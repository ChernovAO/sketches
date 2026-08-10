## Context

`TradingDaySchedulerWorkflow` (`apps/clearingflow/src/clearingflow/workflows/trading_day_scheduler.py`, 1199 строк) реализован в change `tks-2732-trading-scheduler-interaction` и предоставляет:

- **Update-обработчики**: `manual_launch(task_id, trading_date, request_id) -> SlotStatus`, `cancel_slot(slot_id) -> SlotStatus`, `stop_workflow(slot_id) -> SlotStatus` — каждый с `.validator`.
- **Query**: `get_status() -> SchedulerStatus`.
- **Модели** (`scheduler_models.py`, 181 строк): `SlotState`, `SlotStatus`, `SchedulerStatus`, `LatePolicy`, `ScheduledSlot`, `TradingDayPlan`, `OverlapCheckResult`.
- **Internal state**: `_SlotRuntimeState` (mutable dataclass: `state`, `workflow_id`, `actual_launch_time`, `reason`), `_slot_runtime: dict[str, _SlotRuntimeState]`, `_domain_workflows: dict[str, Any]` (task_id → run_fn).
- **Domain-workflow resolution**: `_resolve_workflow_mapping(mapping) -> dict[str, Any]` через `_import_workflow_run_fn(module_name, class_name)` — динамический импорт `cls.run` с валидацией.
- **Child-workflow launch**: `workflow.start_child_workflow(run_fn, params, id=..., parent_close_policy=ABANDON, id_reuse_policy=REJECT_DUPLICATE, run_timeout=...)`.
- **Child-workflow stop**: `workflow.get_external_workflow_handle(workflow_id).cancel()`.
- **Overlap check**: Activity `SchedulerOverlapActivities.check_active_workflow` через Temporal visibility API.
- **DI pattern**: Constructor-based injection в `SchedulerActivities` / `SchedulerOverlapActivities`; bound-method registration в `settle_worker.py`.

**Domain-workflows** (`after_start_trade_engine_session.py` 437 строк, `morning_session.py` 255 строк):
- Плоская последовательность `await workflow.execute_activity(fn, activity_params, start_to_close_timeout=..., retry_policy=...)`.
- Activities передаются по **function reference** (не по string ID).
- `AfterStartTradeEngineSession`: 38 activities; `MorningSession`: 17 activities.
- `WorkflowParams` (frozen, slots): `date: str = ""`, `session_id: str = ""`.
- Нет фильтрации, signal handlers, status reporting, try/except.

**Чего не хватает для activity-level операций:**
- Нет activity-level Update-обработчиков (`cancel_activity`, `stop_activity`)
- Domain-workflows выполняют все activities без возможности фильтрации или отмены
- Нет механизма reporting статуса activities от child к scheduler
- Нет Signal handlers для communication child↔scheduler
- `_resolve_workflow_mapping` возвращает только `run_fn`, не список activities

## Goals / Non-Goals

**Goals:**
- Добавить activity-level операции параллельно с существующими workflow-level
- Обеспечить фильтрацию activities через `activities_to_run`
- Реализовать communication channel child→scheduler для reporting activity status (через Signal)
- Реализовать communication channel scheduler→child для cancel/stop activity (через Signal)
- Расширить Query `get_status` для возврата статусов activities
- Предоставить scheduler список activities domain-workflow для инициализации и валидации

**Non-Goals:**
- Изменение workflow-level операций (уже реализованы в `tks-2732`)
- Изменение схемы БД
- REST API и RBAC в SettleCore
- Pause/resume activity (только stop)
- Добавление новых Temporal activities (status reporting через signals, не activities)
- Изменение `settle_worker.py` (нет новых activities для регистрации)

## Decisions

### 1. Расширение `_SlotRuntimeState` для activity статусов

**Решение:** Добавить в `_SlotRuntimeState` поля:
- `activities: dict[str, ActivityStatusRecord] = field(default_factory=dict)` — mapping activity_id → статус

`SlotStatus` (frozen public model) расширяется полем `activities: dict[str, ActivityStatusRecord] = field(default_factory=dict)`. `_build_slot_status` копирует `rt.activities` в `SlotStatus.activities`.

**Обоснование:** Все per-slot state в одном месте. `_SlotRuntimeState` уже используется как mutable counterpart of `SlotStatus`. Добавление activity tracking туда — естественное расширение. Frozen `SlotStatus` собирается из mutable `_SlotRuntimeState` в `_build_slot_status`, как и существующие поля.

### 2. Передача activity context в domain-workflow через `WorkflowParams`

**Решение:** Добавить в `WorkflowParams` поля:
- `slot_id: str = ""` — идентификатор слота для reporting статуса обратно в scheduler
- `activities_to_run: tuple[str, ...] = ()` — при непустом tuple выполняются только указанные activities, остальные пропускаются со статусом `SKIPPED`. При пустом — выполняется полный workflow.

Также добавить `ActivityStep` dataclass (`activity_id`, `fn`, `start_to_close_timeout`, `retry_policy`) в `params.py` для использования domain-workflow.

**Обоснование:** Детерминированная передача через workflow input. Совместимость с Temporal data converter (все поля JSON-serializable: str, tuple[str,...]). `slot_id` нужен child для идентификации слота при отправке `activity_status_update` signal. `ActivityStep` — это internal descriptor domain-workflow, не сериализуется (не передаётся через Temporal).

Отменённые activities не передаются в `WorkflowParams`. Pre-launch отмена обрабатывается в scheduler: activity исключается из `activities_to_run` при формировании параметров запуска. In-flight отмена доставляется через Signal в domain-workflow.

### 3. `ActivityStep` registry в domain-workflows

**Решение:** Каждый domain-workflow class определяет:
- `ACTIVITIES: tuple[ActivityStep, ...]` — class-level tuple дескрипторов activities в порядке выполнения
- `ACTIVITY_IDS: tuple[str, ...]` — производный tuple activity_id (для чтения scheduler-ом)

`run()` метод заменяется с плоской последовательности `await workflow.execute_activity(...)` на итеративный цикл по `self.ACTIVITIES` с проверкой фильтров, отправкой status signals, и graceful shutdown.

**Обоснование:** 
- Единственный source of truth для списка activities — сам domain-workflow class
- Scheduler читает `ACTIVITY_IDS` при `_resolve_workflow_mapping` для инициализации PENDING статусов и валидации `activities_to_run`
- Итеративный цикл обеспечивает единообразную логику фильтрации, отмены и reporting для всех activities
- `ActivityStep` сохраняет timeout и retry_policy per-activity (как в текущем коде)

### 4. Получение списка activities scheduler-ом

**Решение:** Модифицировать `_resolve_workflow_mapping` для возврата `dict[str, _ResolvedWorkflow]` где `_ResolvedWorkflow` — dataclass с `run_fn: Any` и `activity_ids: tuple[str, ...]`. `_import_workflow_run_fn` (или новый `_import_workflow_class`) читает `ACTIVITY_IDS` class attribute.

**Обоснование:** Scheduler нужен список activities для:
1. Инициализации всех activities в `PENDING` при запуске workflow (`_init_activity_states`)
2. Валидации `activities_to_run` в `manual_launch` validator
3. Валидации `activity_id` в `cancel_activity` и `stop_activity`

Class-level `ACTIVITY_IDS` читается один раз при mapping resolution и хранится в `_domain_workflows`. Не требует runtime queries или signals для получения списка.

### 5. Communication child→scheduler через Signal

**Решение:** Domain-workflow отправляет Signal `activity_status_update` в scheduler:
- Child получает parent workflow ID из `workflow.info().parent.workflow_id`
- Отправляет через `workflow.get_external_workflow_handle(parent_workflow_id).signal(ACTIVITY_STATUS_UPDATE_SIGNAL, args=[slot_id, activity_id, state, started_at, completed_at, reason])`
- Signal отправляется: перед запуском activity (RUNNING), после успешного завершения (COMPLETED), при ошибке (FAILED), при пропуске (SKIPPED/CANCELLED)

Scheduler определяет `@workflow.signal def activity_status_update(self, slot_id, activity_id, state, started_at, completed_at, reason)` который обновляет `_SlotRuntimeState.activities[activity_id]`.

**Обоснование:** 
- **Update API недоступен для workflow-to-workflow коммуникации.** Temporal SDK предоставляет Update API только для внешних клиентов (операторов) через `WorkflowHandle.execute_update()`. Изнутри workflow отправить Update в другой workflow невозможно — `ExternalWorkflowHandle` имеет только методы `signal()` и `cancel()`. Scheduler и domain-workflow — оба workflow; между ними доступны только Signal и cancel.
- Temporal Signal — стандартный механизм для child→parent communication
- `workflow.info().parent` доступен для child workflows (запущенных через `start_child_workflow`)
- Push-модель эффективнее polling
- Не требует новой activity (в отличие от proposal в исходном change)
- Signal name constants (`ACTIVITY_STATUS_UPDATE_SIGNAL`) определяются в `constants.py` для избежания stringly-typed ошибок

### 6. Communication scheduler→child через Signal

**Решение:** Scheduler отправляет signals в child-workflow через `workflow.get_external_workflow_handle(child_workflow_id).signal(...)`:
- `cancel_activity_signal(activity_id)` — для in-flight отмены pending activity
- `stop_activity_signal(activity_id)` — для остановки running activity

Scheduler знает child workflow ID из `_SlotRuntimeState.workflow_id` (устанавливается при `_start_domain_workflow` / `_start_manual_domain_workflow`).

**Обоснование:** Scheduler уже использует `workflow.get_external_workflow_handle(workflow_id)` для `stop_workflow` (cancel). Расширение до signal — естественное развитие. Signal names константы в `constants.py`.

> **Почему Signal, а не Update API:** scheduler и domain-workflow — оба Temporal workflow. Update API доступен только для внешних клиентов (операторов) через `WorkflowHandle.execute_update()`. Между двумя workflow Temporal SDK предоставляет только `ExternalWorkflowHandle` с методами `signal()` и `cancel()`. Поэтому scheduler→child interaction реализован через Signal, а оператор→scheduler — через Update API (возврат результата, validators).

### 7. Обработка cancel_activity: pre-launch и in-flight

**Решение:**
- **Pre-launch отмена** (слот в `PENDING`/`PENDING_MANUAL`): scheduler отмечает `_SlotRuntimeState.activities[activity_id]` как `CANCELLED`. При запуске workflow отменённая activity исключается из `activities_to_run` (scheduler формирует список запуска без отменённых activities). Отменённая activity не передаётся в domain-workflow.
- **In-flight отмена** (слот в `LAUNCHED`/`RUNNING`): scheduler отправляет Signal `cancel_activity_signal(activity_id)` в child-workflow. Domain-workflow добавляет activity_id в `_cancelled_activities` set. При достижении activity проверяет set и пропускает.

**Обоснование:** `WorkflowParams` immutable после старта workflow — для уже запущенного нужен Signal. Для ещё не запущенного — scheduler просто исключает activity из списка запуска. Не нужен `activities_to_cancel` в `WorkflowParams`.

### 8. Graceful shutdown activity через `start_activity` + `handle.cancel()`

**Решение:** Domain-workflow использует `workflow.start_activity(fn, ...)` вместо `workflow.execute_activity(fn, ...)` для возможности отмены:
- `self._current_activity_handle = workflow.start_activity(step.fn, activity_params, ...)`
- Signal handler `stop_activity_signal(activity_id)`: если `self._current_activity_id == activity_id`, вызывает `self._current_activity_handle.cancel()`
- Activity получает `asyncio.CancelledError`; main loop ловит его и сообщает `CANCELLED`
- `finally:` очищает `_current_activity_id` и `_current_activity_handle`

**Обоснование:** `workflow.execute_activity` — convenience wrapper вокруг `start_activity` + `await`. `start_activity` возвращает `ActivityHandle` (наследник `asyncio.Future`) с методом `cancel()`. Это стандартный Temporal механизм для cooperative cancellation. Activity functions могут опционально ловить `CancelledError` для cleanup, но это не требуется — Temporal force-cancels activity после cancellation timeout.

### 9. Force cancel после timeout в `stop_activity`

**Решение:** `stop_activity` Update handler:
1. Отправляет `stop_activity_signal(activity_id)` в child
2. Ждёт `workflow.wait_condition(lambda: activity.state == CANCELLED, timeout=ACTIVITY_GRACEFUL_SHUTDOWN_TIMEOUT)`
3. При timeout — `workflow.get_external_workflow_handle(child_workflow_id).cancel()` (force cancel всего child-workflow), slot → `STOPPED`

**Обоснование:** Если activity не отвечает на graceful shutdown (зависла, долгая I/O операция), force cancel — последняя мера. Cancel всего child-workflow означает, что оставшиеся activities также отменяются. Slot переходит в `STOPPED` (не только activity). Это безопаснее, чем ждать бесконечно.

### 10. `_build_workflow_params` расширение

**Решение:** Обновить `_build_workflow_params(slot, operational_date, activities_to_run=()) -> WorkflowParams` для передачи `slot_id` и `activities_to_run`. При формировании `activities_to_run` scheduler исключает activities, уже отмеченные как `CANCELLED` в `_SlotRuntimeState.activities`.

**Обоснование:** `slot_id` нужен child для reporting. `activities_to_run` передаётся только из `manual_launch` (automatic launch не фильтрует activities). Отменённые pre-launch activities исключаются из списка запуска, а не передаются как отдельный параметр.

## Risks / Trade-offs

- **TOCTOU race при отмене activity:** Между проверкой `_cancelled_activities` и `start_activity` activity может запуститься. → Mitigation: signal handler обновляет set до начала следующей activity; для running activity используется `stop_activity` (через `handle.cancel()`).
- **Signal ordering не гарантирован:** `activity_status_update` может прийти после `stop_activity`. → Mitigation: `wait_condition` в `stop_activity` проверяет `state == CANCELLED`, что устанавливается signal handler-ом; scheduler игнорирует status updates для activities уже в `CANCELLED`/`STOPPED`.
- **Graceful shutdown не гарантирован:** Activity может не успеть обработать `CancelledError`. → Mitigation: `ACTIVITY_GRACEFUL_SHUTDOWN_TIMEOUT` (60 сек) → force cancel всего child-workflow.
- **Parent может завершиться до получения signals:** Scheduler с `ABANDON` child-workflows может завершиться до того, как child отправит все status updates. → Mitigation: scheduler вызывает `workflow.wait_condition(lambda: workflow.all_handlers_finished())` перед возвратом; signals прибывшие до этого обрабатываются. После завершения scheduler-а activity statuses недоступны (Query не работает) — это то же ограничение, что и для workflow-level operations.
- **`_resolve_workflow_mapping` изменение возврата:** Меняется тип возвращаемого значения с `dict[str, Any]` на `dict[str, _ResolvedWorkflow]`. → Mitigation: internal function, используется только в scheduler; тесты обновляются.
- **Domain-workflow рефакторинг:** Изменение обоих domain-workflows может занять время. → Mitigation: `ActivityStep` registry делает рефакторинг механическим — каждый `execute_activity` вызов заменяется на `ActivityStep` entry.

## Migration Plan

1. Расширить `scheduler_models.py` (`ActivityStatus`, `ActivityStatusRecord`, `SlotStatus.activities`)
2. Расширить `workflows/params.py` (`WorkflowParams` fields, `ActivityStep`)
3. Добавить константы в `constants.py`
4. Расширить `trading_day_scheduler.py`:
   - `_SlotRuntimeState`, `_ResolvedWorkflow`, `_resolve_workflow_mapping`
   - Signal handler `activity_status_update`
   - `_init_activity_states`, `_build_workflow_params`, `_build_slot_status`
   - Update `cancel_activity`, `stop_activity` с validators
   - Расширение `manual_launch` с `activities_to_run`
   - Обновление `_start_domain_workflow`, `_start_manual_domain_workflow`
5. Рефакторить domain-workflows по одному:
   - `after_start_trade_engine_session.py` — `ACTIVITIES` registry, signals, reporting
   - `morning_session.py` — аналогично
6. Добавить тесты для activity-level операций
7. Обновить существующие тесты под новые модели

## Open Questions

- **[Resolved]** Формат `activity_id`: строка — имя activity function (например, `"load_trad_deals_to_core"`, `"generate_eqm91_report"`). Совпадает с `fn.__name__` импортируемой функции. Детерминированно, читаемо для оператора.
- **[Resolved]** Timeout для graceful shutdown: 60 секунд (`ACTIVITY_GRACEFUL_SHUTDOWN_TIMEOUT`). После timeout — force cancel всего child-workflow.
- **[Resolved]** Communication mechanism: child→scheduler через Signal (не activity). Child получает parent ID из `workflow.info().parent`.
- **[Resolved]** Scheduler список activities: class-level `ACTIVITY_IDS` domain-workflow, читаемый при `_resolve_workflow_mapping`.
- **[Resolved]** `report_activity_status` activity: не нужна. Status reporting через child→parent Signal.
- **[Resolved]** `ACTIVITY_CANCEL_TIMEOUT` константа: не нужна. `cancel_activity` — мгновенная операция (запись в set + опционально signal). Только `ACTIVITY_GRACEFUL_SHUTDOWN_TIMEOUT` для `stop_activity`.
