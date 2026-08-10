## 1. Модели данных (scheduler_models.py) — расширение

> Существующие модели (`SlotState`, `SlotStatus`, `SchedulerStatus`, `LatePolicy`, `ScheduledSlot`, `TradingDayPlan`, `OverlapCheckResult`) реализованы в `tks-2732`. Добавляются activity-level модели.

- [ ] 1.1 Добавить enum `ActivityStatus(StrEnum)` со значениями `PENDING`, `RUNNING`, `COMPLETED`, `CANCELLED`, `FAILED`, `SKIPPED`
- [ ] 1.2 Добавить frozen-dataclass `ActivityStatusRecord` (frozen=True, slots=True) с полями: `activity_id` (str), `state` (ActivityStatus), `started_at` (str | None = None), `completed_at` (str | None = None), `reason` (str | None = None)
- [ ] 1.3 Расширить `SlotStatus` полем `activities: dict[str, ActivityStatusRecord] = field(default_factory=dict)`
- [ ] 1.4 Экспортировать `ActivityStatus` и `ActivityStatusRecord` из `workflows/__init__.py` (в `__all__`)

## 2. WorkflowParams и ActivityStep (workflows/params.py) — расширение

- [ ] 2.1 Добавить поле `slot_id: str = ""` в `WorkflowParams`: идентификатор слота для reporting статуса child→scheduler
- [ ] 2.2 Добавить поле `activities_to_run: tuple[str, ...] = ()` в `WorkflowParams`: при непустом tuple выполняются только перечисленные activities, остальные пропускаются со статусом `SKIPPED`. При пустом — выполняется полный workflow.
- [ ] 2.3 Добавить frozen-dataclass `ActivityStep` (frozen=True, slots=True) с полями: `activity_id` (str), `fn` (Any), `start_to_close_timeout` (timedelta), `retry_policy` (RetryPolicy). Импортировать `timedelta` из `datetime`, `Any` из `typing`, `RetryPolicy` из `temporalio.common`.

## 3. Константы (constants.py) — расширение

- [ ] 3.1 Добавить `ACTIVITY_GRACEFUL_SHUTDOWN_TIMEOUT = timedelta(seconds=60)` — timeout для graceful shutdown activity перед force cancel
- [ ] 3.2 Добавить signal name константы: `ACTIVITY_STATUS_UPDATE_SIGNAL = "activity_status_update"`, `CANCEL_ACTIVITY_SIGNAL = "cancel_activity_signal"`, `STOP_ACTIVITY_SIGNAL = "stop_activity_signal"`

## 4. Рефакторинг domain-workflows: ActivityStep registry и signal handlers

> Domain-workflows перерабатываются с плоской последовательности `await workflow.execute_activity(...)` на итеративный цикл по `ACTIVITIES` registry.

### 4.1 AfterStartTradeEngineSession (after_start_trade_engine_session.py)

- [ ] 4.1.1 Импортировать `ActivityStep` из `clearingflow.workflows.params` (внутри `imports_passed_through`)
- [ ] 4.1.2 Импортировать `ActivityStatus` из `clearingflow.scheduler_models` и signal name константы из `clearingflow.constants` (внутри `imports_passed_through`)
- [ ] 4.1.3 Определить class-level `ACTIVITIES: tuple[ActivityStep, ...]` — 38 entries в порядке выполнения, каждый с `activity_id` (function name), `fn` (function reference), `start_to_close_timeout`, `retry_policy` (перенести из текущих `execute_activity` вызовов)
- [ ] 4.1.4 Определить class-level `ACTIVITY_IDS: tuple[str, ...]` — производный tuple из `step.activity_id for step in ACTIVITIES`
- [ ] 4.1.5 Добавить `__init__` с instance attributes: `_cancelled_activities: set[str] = set()`, `_current_activity_id: str | None = None`, `_current_activity_handle: Any = None`
- [ ] 4.1.6 Рефакторить `run()`: заменить плоскую последовательность на цикл `for step in self.ACTIVITIES:` с проверкой `params.activities_to_run` и `self._cancelled_activities`; отправкой `activity_status_update` signal перед запуском (RUNNING) и после завершения (COMPLETED/CANCELLED/FAILED/SKIPPED); использованием `workflow.start_activity` + `await handle` вместо `workflow.execute_activity`; `try/except asyncio.CancelledError` для graceful shutdown; `try/except Exception` для FAILED + re-raise
- [ ] 4.1.7 Добавить helper `_report_activity_status(parent_workflow_id, slot_id, activity_id, state, started_at, completed_at, reason)`: отправляет signal через `workflow.get_external_workflow_handle(parent_workflow_id).signal(ACTIVITY_STATUS_UPDATE_SIGNAL, args=[...])`; no-op если `parent_workflow_id is None` или `slot_id` пустой
- [ ] 4.1.8 Добавить `@workflow.signal def cancel_activity_signal(self, activity_id: str)`: добавляет `activity_id` в `self._cancelled_activities`
- [ ] 4.1.9 Добавить `@workflow.signal def stop_activity_signal(self, activity_id: str)`: если `self._current_activity_id == activity_id and self._current_activity_handle is not None`, вызывает `self._current_activity_handle.cancel()`

### 4.2 MorningSession (morning_session.py)

- [ ] 4.2.1–4.2.7 Аналогично 4.1.1–4.1.9 для `MorningSession` (17 activities)

## 5. Расширение TradingDaySchedulerWorkflow: mapping, state, signals

### 5.1 _ResolvedWorkflow и _resolve_workflow_mapping

- [ ] 5.1.1 Добавить `_ResolvedWorkflow` dataclass (slots=True) с полями `run_fn: Any` и `activity_ids: tuple[str, ...]`
- [ ] 5.1.2 Модифицировать `_import_workflow_run_fn` (или добавить `_import_workflow_class`) для возврата класса (не только `run_fn`), чтобы читать `ACTIVITY_IDS`
- [ ] 5.1.3 Модифицировать `_resolve_workflow_mapping` для возврата `dict[str, _ResolvedWorkflow]` вместо `dict[str, Any]`: для каждого entry импортировать класс, получить `run_fn = getattr(cls, "run")` и `activity_ids = getattr(cls, "ACTIVITY_IDS", ())`, вернуть `_ResolvedWorkflow(run_fn=run_fn, activity_ids=activity_ids)`
- [ ] 5.1.4 Обновить тип `self._domain_workflows` на `dict[str, _ResolvedWorkflow]`

### 5.2 _SlotRuntimeState расширение

- [ ] 5.2.1 Добавить поле `activities: dict[str, ActivityStatusRecord] = field(default_factory=dict)` в `_SlotRuntimeState`

### 5.3 Activity state initialization

- [ ] 5.3.1 Добавить helper `_init_activity_states(slot_id: str, task_id: str) -> None`: создаёт записи `ActivityStatusRecord(state=PENDING)` для всех `activity_ids` из `self._domain_workflows[task_id].activity_ids` в `self._slot_runtime[slot_id].activities`
- [ ] 5.3.2 Вызывать `_init_activity_states` в `_start_domain_workflow` и `_start_manual_domain_workflow` перед `start_child_workflow`

### 5.4 Signal handler: activity_status_update

- [ ] 5.4.1 Добавить `@workflow.signal def activity_status_update(self, slot_id: str, activity_id: str, state: str, started_at: str | None, completed_at: str | None, reason: str | None) -> None`: обновляет `self._slot_runtime[slot_id].activities[activity_id]` новой `ActivityStatusRecord`; no-op если `slot_id` не найден или activity уже в terminal state (CANCELLED/COMPLETED/FAILED)

### 5.5 _build_workflow_params и _build_slot_status

- [ ] 5.5.1 Обновить `_build_workflow_params(slot, operational_date, activities_to_run=()) -> WorkflowParams`: добавляет `slot_id=slot.slot_id`, `activities_to_run=activities_to_run` (с исключением отменённых activities из `rt.activities`).
- [ ] 5.5.2 Обновить `_build_slot_status` для включения `activities=dict(rt.activities)` в возвращаемый `SlotStatus`

### 5.6 _start_domain_workflow и _start_manual_domain_workflow

- [ ] 5.6.1 В `_start_domain_workflow`: получить `resolved = self._domain_workflows.get(slot.task_id)` (теперь `_ResolvedWorkflow`); использовать `resolved.run_fn` для `start_child_workflow`; вызвать `self._init_activity_states(slot.slot_id, slot.task_id)` перед запуском; передать `activities_to_run` (с исключением отменённых activities из `rt.activities`) в `_build_workflow_params`
- [ ] 5.6.2 В `_start_manual_domain_workflow`: аналогично 5.6.1; дополнительно передать `activities_to_run` из `manual_launch` параметров

## 6. Update-обработчики для activity-level операций

### 6.1 cancel_activity

- [ ] 6.1.1 Реализовать `@workflow.update async def cancel_activity(self, slot_id: str, activity_id: str) -> ActivityStatusRecord`: найти slot; получить `rt = self._slot_runtime[slot_id]`; получить activity record из `rt.activities`; проверить состояние (только `PENDING` разрешён); обновить `rt.activities[activity_id]` с `state=CANCELLED, reason="cancelled by operator", completed_at=now`; если slot в `LAUNCHED`/`RUNNING` и `rt.workflow_id` — отправить `CANCEL_ACTIVITY_SIGNAL` в child через `workflow.get_external_workflow_handle(rt.workflow_id).signal(...)`; если slot в `PENDING`/`PENDING_MANUAL` — activity будет исключена из `activities_to_run` при запуске workflow; вернуть `ActivityStatusRecord`
- [ ] 6.1.2 Реализовать `@cancel_activity.validator def _validate_cancel_activity(self, slot_id, activity_id)`: проверить существование slot; проверить существование activity_id в `rt.activities`; проверить состояние: `RUNNING` → error "activity already running; use stop_activity instead", `COMPLETED` → error "activity already completed; cannot cancel", `CANCELLED` → error "activity already cancelled", `SKIPPED` → error "activity already skipped"
- [ ] 6.1.3 Обеспечить идемпотентность: повторный `cancel_activity` для `CANCELLED` → ошибка (в валидаторе)
- [ ] 6.1.4 Добавить логирование через `workflow.logger` (structlog): slot_id, activity_id, operation_time

### 6.2 stop_activity

- [ ] 6.2.1 Реализовать `@workflow.update async def stop_activity(self, slot_id: str, activity_id: str) -> ActivityStatusRecord`: найти slot; получить `rt`; получить activity record; отправить `STOP_ACTIVITY_SIGNAL` в child через `workflow.get_external_workflow_handle(rt.workflow_id).signal(...)`; ждать `workflow.wait_condition(lambda: rt.activities[activity_id].state == ActivityStatus.CANCELLED, timeout=ACTIVITY_GRACEFUL_SHUTDOWN_TIMEOUT)`; при timeout — `workflow.get_external_workflow_handle(rt.workflow_id).cancel()` (force cancel), `_update_slot(slot_id, SlotState.STOPPED, reason="force cancelled: activity stop timeout")`; вернуть `ActivityStatusRecord`
- [ ] 6.2.2 Реализовать `@stop_activity.validator def _validate_stop_activity(self, slot_id, activity_id)`: проверить существование slot; проверить существование activity_id; проверить состояние: только `RUNNING` разрешено; `PENDING` → error "activity not running; use cancel_activity instead", `COMPLETED` → error "activity already completed; cannot stop", `CANCELLED` → error "activity already stopped"
- [ ] 6.2.3 Обеспечить идемпотентность: повторный `stop_activity` для `CANCELLED` → ошибка (в валидаторе)
- [ ] 6.2.4 Добавить логирование через `workflow.logger`: slot_id, activity_id, workflow_id, operation_time

## 7. Расширение manual_launch

- [ ] 7.1 Расширить сигнатуру `manual_launch(self, task_id, trading_date, request_id, activities_to_run: tuple[str, ...] = ()) -> SlotStatus`
- [ ] 7.2 Расширить `_validate_manual_launch` с параметром `activities_to_run`: при непустом `activities_to_run` проверить каждый `activity_id` против `self._domain_workflows[task_id].activity_ids`; неизвестный activity_id → `ApplicationError(non_retryable=True)`
- [ ] 7.3 Передать `activities_to_run` в `_start_manual_domain_workflow` → `_build_workflow_params`

## 8. Расширение Query: get_status с activity статусами

- [ ] 8.1 Обновить `_build_slot_status` для включения `activities=dict(rt.activities)` — уже выполнено в 5.5.2, убедиться что `get_status` Query возвращает activities для каждого слота
- [ ] 8.2 Обеспечить доступность Query на любом этапе: до запуска (activities в PENDING после `_init_activity_states`), во время выполнения, после завершения

## 9. Тесты

> Тесты используют встроенный Temporal dev-сервер (`WorkflowEnvironment.start_local`). Существующие stubs: `StubTradingDayPlanRepository`, `StubOverlapActivities`, `StubDomainWorkflowMappingRepository`. Test workflows: `FastDomainWorkflow`, `WaitingDomainWorkflow`.

- [ ] 9.1 Обновить test workflows: добавить `ACTIVITY_IDS` и `ACTIVITIES` в `FastDomainWorkflow` и `WaitingDomainWorkflow`; добавить signal handlers `cancel_activity_signal`, `stop_activity_signal`; добавить `_report_activity_status` helper; добавить `_cancelled_activities` set и `_current_activity_handle`
- [ ] 9.2 Добавить тесты `manual_launch` с `activities_to_run`: успешный запуск subset, валидация activity_id, пропуск неуказанных (SKIPPED), идемпотентность
- [ ] 9.3 Добавить тесты `cancel_activity`: успешная отмена pending (pre-launch), успешная отмена pending (in-flight через signal), отказ для RUNNING/COMPLETED/CANCELLED/SKIPPED, отправка сигнала в child-workflow
- [ ] 9.4 Добавить тесты `stop_activity`: успешная остановка running (graceful shutdown), force cancel после timeout, отказ для PENDING/COMPLETED/CANCELLED, продолжение выполнения других activities после остановки
- [ ] 9.5 Добавить тесты `get_status` с activity статусами: статусы всех activities (PENDING/RUNNING/COMPLETED/CANCELLED/FAILED/SKIPPED), формат `ActivityStatusRecord`, обновление в реальном времени через signal
- [ ] 9.6 Добавить тесты domain-workflow: фильтрация по `activities_to_run`, пропуск отменённых (через signal), graceful shutdown при `stop_activity_signal`, reporting статуса через signal
- [ ] 9.7 Добавить тесты на race conditions: конкурентные `cancel_activity` и `stop_activity` при завершении activity, `stop_activity` при уже завершённой activity, комбинированные сценарии
- [ ] 9.8 Обновить существующие тесты `_resolve_workflow_mapping` под новый возврат `_ResolvedWorkflow`
- [ ] 9.9 Добавить тест replay-детерминизма для activity-level операций (Update + Signal + Query)

## 10. Проверка качества

- [ ] 10.1 `uv run ruff check .`
- [ ] 10.2 `uv run mypy .`
- [ ] 10.3 `uv run pytest`
- [ ] 10.4 Проверка покрытия тестами модулей scheduler-а и domain-workflows — минимум 80%
