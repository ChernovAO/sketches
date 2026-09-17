## Why

Декоратор `settle_activity` (`settle_decorator.py`) перед выполнением activity проверяет: (1) завершены ли все операции-зависимости и (2) не была ли текущая операция уже завершена. При нарушении проверок декоратор выбрасывает `ApplicationError(non_retryable=True)` — жёсткая остановка без возможности интерактивного подтверждения. В существующем SettleCCP оператору показывается информативное предупреждение с вопросом о подтверждении продолжения, а также поддерживается полностью автоматический режим. TKS-3133 требует реализовать поддержку интерактивного взаимодействия ClearingFlow/SettleCore с SettleConsole для этих проверок.

## What Changes

- **`ActivityParams`** (`activities/params.py`): Добавление поля `force: bool = False` — при `True` декоратор пропускает pre-execution проверки (для повторного запуска после подтверждения оператора).

- **`CheckResult`** (`activities/result.py`): Новый frozen-dataclass — результат pre-execution проверок при неудаче. Возвращается декоратором вместо `ActivityResults`, когда проверки не пройдены и `force=False`. Содержит `missing_rtypes: list[str]` и `already_completed: bool`.

- **`settle_activity` декоратор** (`settle_decorator.py`): Вместо `raise ApplicationError` при неудаче проверок возвращается `CheckResult`. При `params.force=True` проверки пропускаются. Запись `START` в журнал выполняется только после успешных проверок (или при `force=True`).

- **`ActivityStatus`** (`scheduler_models.py`): Добавление `PENDING_ANSWER` (transient — ожидание ответа оператора) и `ALREADY_COMPLETED` (terminal — операция уже завершена в предыдущем запуске).

- **`WorkflowParams`** (`workflows/params.py`): Добавление полей `interactive: bool = False` (режим интерактивного подтверждения) и `confirmation_timeout_seconds: int = 300` (таймаут ожидания ответа оператора).

- **`SchedulerSettings`** (`config.py`): Добавление поля `interactive_mode: bool = False` (env `SCHEDULER_INTERACTIVE_MODE`). При `True` все запуски domain-workflows используют интерактивный режим. Таймаут переиспользует существующий `manual_decision_timeout_seconds` (per-slot override) или `DEFAULT_MANUAL_DECISION_TIMEOUT_SECONDS` (300 сек).

- **`BaseSessionWorkflow`** (`workflows/base_session_workflow.py`): Расширение `_execute_activity_managed` логикой подтверждения: при `CheckResult.has_failures` в интерактивном режиме — переход в `PENDING_ANSWER`, `workflow.wait_condition` с таймаутом, обработка confirm/deny/timeout. При таймауте — применение автоматической политики (FAILED для missing_rtypes, ALREADY_COMPLETED для already_completed) с информативным `reason`. Новый helper `_run_activity_once` (один вызов activity с lifecycle management, без логики подтверждения). Новый signal handler `confirm_activity_signal(activity_id, approved: bool)`.

- **`TradingDaySchedulerWorkflow`** (`workflows/trading_day_scheduler.py`): Новый Update `confirm_activity(slot_id, activity_id, approved: bool)` — оператор подтверждает или отклоняет. Scheduler отправляет `CONFIRM_ACTIVITY_SIGNAL` в child-workflow (аналогично `cancel_activity`/`stop_activity`).

- **Константы** (`constants.py`): `CONFIRM_ACTIVITY_SIGNAL = "confirm_activity_signal"`.

- **SettleCore** (`apps/settlecore/`): Новый REST endpoint `POST /api/workflows/{scheduler_id}/confirm` для подтверждения activity. Расширение `ActionResolver` маршрутизацией step-level confirm. SSE `StepStreamer` — эмиссия статусов `PENDING_ANSWER`, `ALREADY_COMPLETED`.

- **SettleConsole** (`apps/settleconsole/`): Обработка статуса `PENDING_ANSWER` в `useWorkflowStore` — отображение модального окна подтверждения с информацией о причине (missing deps / already completed). API-вызов `POST /confirm`. Интеграция с NotificationBell для отображения ожидающих подтверждений.

## Capabilities

### New Capabilities
- `activity-confirmation`: Интерактивное подтверждение выполнения activity при неудаче pre-execution проверок (deps не завершены, операция уже выполнена). Поддержка automatic и interactive режимов. При таймауте в интерактивном режиме применяется автоматическая политика с уведомлением UI.

### Modified Capabilities
- `activity-status-query`: Добавление статусов `PENDING_ANSWER` и `ALREADY_COMPLETED` в `ActivityStatus` enum. Расширение терминальных состояний.
- `activity-failure-handling`: При неудаче проверок зависимостей в automatic режиме activity переходит в `FAILED` (а не выбрасывает `ApplicationError`). При `already_completed` в automatic режиме — `ALREADY_COMPLETED` без выполнения. `_execute_activity_managed` обрабатывает `CheckResult` вместо исключения.

## Impact

- **ClearingFlow** (`apps/clearingflow/`):
  - `activities/params.py` — поле `force`
  - `activities/result.py` — `CheckResult`
  - `activities/decorator/settle_decorator.py` — возврат `CheckResult` вместо raise, проверка `force`
  - `scheduler_models.py` — `PENDING_ANSWER`, `ALREADY_COMPLETED` в `ActivityStatus`; обновление inline-проверки терминальных статусов в `activity_status_update` signal handler (добавить `ALREADY_COMPLETED`)
  - `workflows/params.py` — `interactive`, `confirmation_timeout_seconds`
  - `config.py` — `interactive_mode` в `SchedulerSettings`
  - `constants.py` — `CONFIRM_ACTIVITY_SIGNAL`
  - `workflows/base_session_workflow.py` — расширение `_execute_activity_managed`, `_run_activity_once`, `_handle_check_failure`, `_apply_automatic_policy`, `confirm_activity_signal`, `_pending_confirmations`
  - `workflows/trading_day_scheduler.py` — `confirm_activity` Update + validator, передача `interactive` и `confirmation_timeout_seconds` в `WorkflowParams`
  - `workers/settle_worker.py` — без изменений (новый signal, не activity)

- **SettleCore** (`apps/settlecore/`):
  - `api/workflows.py` — `POST /confirm` endpoint
  - `api/action_resolver.py` — routing для confirm action
  - `sse/step_streamer.py`, `sse/models.py` — поддержка новых статусов

- **SettleConsole** (`apps/settleconsole/`):
  - `store/workflow/useWorkflowStore.ts` — обработка `PENDING_ANSWER`
  - Новый компонент `ConfirmationModal`
  - `api/rest/workflows/` — confirm endpoint client
  - `app/components/Header/NotificationBell.tsx` — индикатор ожидающих подтверждений

- **Тесты** — тесты декоратора (возврат `CheckResult`), `_execute_activity_managed` (все пути confirm/deny/timeout/automatic), `confirm_activity` Update, интеграционные тести SSE для новых статусов.
- Не требует изменений схемы БД. `CheckResult` не записывается в журнал — только `START`/`COMPLETED`/`ERROR` после успешных проверок.
