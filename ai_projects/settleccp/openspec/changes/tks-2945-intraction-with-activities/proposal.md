## Why

`TradingDaySchedulerWorkflow` (change `tks-2732-trading-scheduler-interaction`, полностью реализован) предоставляет workflow-level операции: ручной запуск (`manual_launch`), отмена слота (`cancel_slot`), остановка запущенного domain-workflow (`stop_workflow`), и Query статуса (`get_status`). Однако операторам клиринга требуется более гранулярный контроль — на уровне отдельных activities внутри domain-workflow.

**Бизнес-сценарии:**
- При сбое upstream-системы нужно пропустить загрузку данных, но продолжить последующие activities
- При зависании одной activity — остановить только её, не трогая остальной workflow
- Для тестирования — запустить workflow только с определёнными activities

Текущая архитектура не поддерживает activity-level операции: оператор может управлять только workflow целиком. Domain-workflows (`AfterStartTradeEngineSession` с 38 activities, `MorningSession` с 17 activities) выполняют все activities последовательно без возможности фильтрации, отмены или reporting статуса отдельных steps.

## What Changes

- **Расширение `scheduler_models.py`**: Добавление `ActivityStatus` enum (`PENDING`, `RUNNING`, `COMPLETED`, `CANCELLED`, `FAILED`, `SKIPPED`) и `ActivityStatusRecord` (frozen-dataclass с полями `activity_id`, `state`, `started_at`, `completed_at`, `reason`). Расширение `SlotStatus` полем `activities: dict[str, ActivityStatusRecord] = field(default_factory=dict)`.

- **Расширение `WorkflowParams`** (`workflows/params.py`): Добавление полей `slot_id: str = ""` (для идентификации слота при reporting статуса child→scheduler), `activities_to_run: tuple[str, ...] = ()` (фильтр включения activities; при непустом tuple выполняются только указанные, остальные пропускаются со статусом `SKIPPED`). Добавление `ActivityStep` — frozen-dataclass-дескриптора одной activity (`activity_id`, `fn`, `start_to_close_timeout`, `retry_policy`), используемого domain-workflow для итеративного выполнения. Отменённые pre-launch activities исключаются из `activities_to_run` при формировании параметров запуска.

- **Рефакторинг domain-workflows**: `AfterStartTradeEngineSession` и `MorningSession` перерабатываются с плоской последовательности `await workflow.execute_activity(...)` на итеративный цикл по `ACTIVITIES: tuple[ActivityStep, ...]` (class-level атрибут). Добавляются: (1) фильтрация по `activities_to_run`, (2) пропуск отменённых activities по `cancel_activity_signal`, (3) Signal handlers `cancel_activity_signal(activity_id)` и `stop_activity_signal(activity_id)`, (4) reporting статуса через Signal `activity_status_update` в scheduler (child→parent signal через `workflow.info().parent`), (5) graceful shutdown activity через `workflow.start_activity` + `handle.cancel()`.

- **Расширение `_resolve_workflow_mapping`** в `trading_day_scheduler.py`: Возврат `_ResolvedWorkflow` (run_fn + activity_ids) вместо bare `run_fn`. Scheduler читает class-level `ACTIVITY_IDS` domain-workflow для инициализации activity статусов и валидации `activities_to_run`.

- **Расширение `_SlotRuntimeState`**: Добавление поля `activities: dict[str, ActivityStatusRecord]` для tracking activity-level состояния per-slot.

- **Новые Update-обработчики в scheduler**: `cancel_activity(slot_id, activity_id) -> ActivityStatusRecord` для отмены pending activity, `stop_activity(slot_id, activity_id) -> ActivityStatusRecord` для остановки running activity с graceful shutdown и force cancel после timeout.

- **Расширение `manual_launch`**: Добавление опционального параметра `activities_to_run: tuple[str, ...] = ()`. Валидация activity_id против `ACTIVITY_IDS` domain-workflow.

- **Signal handler `activity_status_update`** в scheduler: принимает `(slot_id, activity_id, state, started_at, completed_at, reason)` от child-workflow, обновляет `_SlotRuntimeState.activities`.

- **Расширение Query**: `get_status()` возвращает `SlotStatus` с `activities: dict[str, ActivityStatusRecord]` для каждого слота.

- **Константы** (`constants.py`): `ACTIVITY_GRACEFUL_SHUTDOWN_TIMEOUT` (timedelta(seconds=60)), signal name constants.

## Capabilities

### New Capabilities

- `activity-manual-launch`: Запуск domain-workflow с параметром `activities_to_run` через расширение Update `manual_launch`. При непустом списке выполняются только указанные activities, остальные пропускаются со статусом `SKIPPED`. При пустом — выполняется полный workflow (существующее поведение).
- `activity-cancel`: Отмена pending activity через Update `cancel_activity(slot_id, activity_id)`. Отмена возможна только для activities в состоянии `PENDING`. Для pre-launch отмены (слот в `PENDING`/`PENDING_MANUAL`) scheduler отмечает activity как `CANCELLED` и исключает её из `activities_to_run` при запуске workflow. Для in-flight отмены (слот в `LAUNCHED`/`RUNNING`) scheduler отправляет Signal `cancel_activity_signal` в child-workflow.
- `activity-stop`: Остановка running activity через Update `stop_activity(slot_id, activity_id)`. Scheduler отправляет Signal `stop_activity_signal` в child-workflow, domain-workflow отменяет activity handle (graceful shutdown через `asyncio.CancelledError`). Force cancel через `cancel_external_workflow` после `ACTIVITY_GRACEFUL_SHUTDOWN_TIMEOUT` (60 сек). Остановка возможна только для activities в состоянии `RUNNING`.
- `activity-status-query`: Query `get_status()` возвращает `SlotStatus` с полем `activities: dict[str, ActivityStatusRecord]` для каждого слота. Статусы activities обновляются в реальном времени через Signal `activity_status_update` от child-workflow.

### Modified Capabilities

- `scheduler-manual-launch`: Расширение Update `manual_launch` с опциональным параметром `activities_to_run: tuple[str, ...] = ()`. Валидация activity_id в `ACTIVITY_IDS` domain-workflow.
- `scheduler-status-query`: Расширение Query `get_status` для возврата статусов individual activities наряду со статусами слотов.

## Impact

- **ClearingFlow** (`apps/clearingflow/`):
  - `scheduler_models.py` — добавление `ActivityStatus`, `ActivityStatusRecord`, расширение `SlotStatus` полем `activities`.
  - `workflows/params.py` — добавление `slot_id`, `activities_to_run` в `WorkflowParams`; добавление `ActivityStep` dataclass.
  - `workflows/trading_day_scheduler.py` — расширение `_SlotRuntimeState`; `_ResolvedWorkflow` и модификация `_resolve_workflow_mapping`; Signal handler `activity_status_update`; Update-обработчики `cancel_activity`, `stop_activity`; расширение `manual_launch`; helper `_init_activity_states`; обновление `_build_workflow_params`, `_build_slot_status`, `_start_domain_workflow`, `_start_manual_domain_workflow`.
  - `workflows/after_start_trade_engine_session.py` и `morning_session.py` — рефакторинг на `ACTIVITIES` registry; Signal handlers; status reporting; graceful shutdown.
  - `constants.py` — `ACTIVITY_GRACEFUL_SHUTDOWN_TIMEOUT`, signal name constants.
  - `workers/settle_worker.py` — без изменений (новые activities не добавляются; status reporting через signals, не activities).
- **Тесты** (`apps/clearingflow/tests/`) — добавление тестов activity-level операций, тестов фильтрации activities, graceful shutdown, reporting статуса, race conditions. Обновление test workflows (`FastDomainWorkflow`, `WaitingDomainWorkflow`) для поддержки `ACTIVITY_IDS`.
- Не требует изменений схемы БД (activity context хранится в workflow state).
- Не требует новых activities (status reporting реализован через child→parent Signal, не через activity).
- REST API, RBAC и аудит в SettleCore не входят в область изменения.

## Notes

- Проект в фазе активной разработки. **Обратная совместимость не требуется**: модели и сигнатуры могут изменяться без миграции.
- Workflow-level операции (`manual_launch`, `cancel_slot`, `stop_workflow`, `get_status`) уже реализованы в `tks-2732-trading-scheduler-interaction`. Этот change расширяет их activity-level аналогами.
- Domain-workflows перерабатываются: плоская последовательность `execute_activity` заменяется на итеративный цикл по `ACTIVITIES` registry с поддержкой фильтрации, отмены, status reporting и graceful shutdown.
- Communication child→scheduler реализован через Temporal Signal (`workflow.get_external_workflow_handle(parent_workflow_id).signal(...)`), не через activity. Child получает parent workflow ID из `workflow.info().parent`.
- Communication scheduler→child реализован через `workflow.get_external_workflow_handle(child_workflow_id).signal(...)`. Scheduler знает child workflow ID из `_SlotRuntimeState.workflow_id`.
- Scheduler получает список activities domain-workflow из class-level `ACTIVITY_IDS` атрибута, читаемого при `_resolve_workflow_mapping`. Это позволяет инициализировать все activities в `PENDING` при запуске workflow и валидировать `activities_to_run` в `manual_launch`.
