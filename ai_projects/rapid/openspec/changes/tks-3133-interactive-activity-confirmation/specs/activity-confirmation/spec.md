## ADDED Requirements

### Requirement: Pre-execution checks возвращают CheckResult вместо исключения

Декоратор `settle_activity` SHALL выполнять pre-execution проверки (зависимости и уже выполненная операция) и при неудаче возвращать `CheckResult(missing_rtypes, already_completed)` вместо `raise ApplicationError`. При `params.force=True` проверки SHALL пропускаться. Запись `START` в журнал SHALL выполняться только после успешных проверок или при `force=True`.

#### Scenario: Проверка зависимостей не пройдена

- **WHEN** activity вызывается с `force=False` и операции-зависимости `"SettleCCP_SaveLimit1"` не имеют финального статуса в журнале LOG
- **THEN** декоратор возвращает `CheckResult(missing_rtypes=["SettleCCP_SaveLimit1"], already_completed=False)` без записи `START` в журнал и без вызова тела activity

#### Scenario: Операция уже выполнена

- **WHEN** activity вызывается с `force=False` и операция уже имеет запись в журнале LOG с ответом "Not completed"
- **THEN** декоратор возвращает `CheckResult(missing_rtypes=[], already_completed=True)` без записи `START` в журнал и без вызова тела activity

#### Scenario: Проверки пройдены успешно

- **WHEN** activity вызывается с `force=False` и все зависимости завершены, операция не была ранее выполнена
- **THEN** декоратор записывает `START` в журнал, выполняет тело activity, возвращает `ActivityResults`

#### Scenario: Принудительный запуск с force=True

- **WHEN** activity вызывается с `force=True` и операции-зависимости не завершены
- **THEN** декоратор пропускает обе проверки, записывает `START` в журнал, выполняет тело activity, возвращает `ActivityResults`

### Requirement: Automatic режим для pre-execution check failures

Система SHALL поддерживать automatic режим (при `WorkflowParams.interactive=False`) для обработки неудач pre-execution проверок. При `already_completed=True` система SHALL переводить activity в `ALREADY_COMPLETED` и продолжать выполнение следующих activities. При `missing_rtypes` непустой система SHALL переводить activity в `FAILED`, помечать все оставшиеся `PENDING` activities как `SKIPPED` с `reason="blocked by failed activity: {activity_id}"`, и прекращать выполнение domain-workflow (re-raise).

#### Scenario: Automatic — операция уже выполнена

- **WHEN** `interactive=False` и `CheckResult.already_completed=True`
- **THEN** domain-workflow отправляет Signal `activity_status_update` с `state=ALREADY_COMPLETED`, `reason="already completed in previous run"`; выполнение продолжается со следующей activity

#### Scenario: Automatic — зависимости не завершены

- **WHEN** `interactive=False` и `CheckResult.missing_rtypes=["SettleCCP_SaveLimit1"]`
- **THEN** domain-workflow отправляет Signal `activity_status_update` с `state=FAILED`, `reason="missing dependencies: SettleCCP_SaveLimit1"`; затем отправляет Signals для всех оставшихся `PENDING` activities с `state=SKIPPED`, `reason="blocked by failed activity: {activity_id}"`; затем re-raise; scheduler переводит slot в `SlotState.FAILED`

### Requirement: Interactive режим с подтверждением оператора

Система SHALL поддерживать interactive режим (при `WorkflowParams.interactive=True`) для обработки неудач pre-execution проверок. При `CheckResult.has_failures` система SHALL переводить activity в `PENDING_ANSWER`, ожидать ответа оператора через `workflow.wait_condition` с таймаутом `confirmation_timeout_seconds`, и обрабатывать ответ: confirm (повторный запуск с `force=True`), deny (SKIPPED или ALREADY_COMPLETED), timeout (применение automatic политики).

#### Scenario: Interactive — оператор подтверждает запуск

- **WHEN** `interactive=True`, `CheckResult.missing_rtypes=["SettleCCP_SaveLimit1"]`, оператор отправляет `confirm_activity(approved=True)` до истечения таймаута
- **THEN** domain-workflow переводит activity в `PENDING_ANSWER`, получает `confirm_activity_signal(approved=True)`, повторно вызывает activity с `force=True`; activity выполняется (запись `START`, тело activity, `COMPLETED`)

#### Scenario: Interactive — оператор отклоняет запуск при missing deps

- **WHEN** `interactive=True`, `CheckResult.missing_rtypes=["SettleCCP_SaveLimit1"]`, оператор отправляет `confirm_activity(approved=False)`
- **THEN** domain-workflow переводит activity в `PENDING_ANSWER`, получает `confirm_activity_signal(approved=False)`, переводит activity в `SKIPPED` с `reason="operator declined; missing dependencies"`; выполнение продолжается со следующей activity

#### Scenario: Interactive — оператор отклоняет запуск при already completed

- **WHEN** `interactive=True`, `CheckResult.already_completed=True`, оператор отправляет `confirm_activity(approved=False)`
- **THEN** domain-workflow переводит activity в `PENDING_ANSWER`, получает `confirm_activity_signal(approved=False)`, переводит activity в `ALREADY_COMPLETED` с `reason="operator declined re-run; already completed"`; выполнение продолжается со следующей activity

#### Scenario: Interactive — оператор подтверждает повторный запуск already completed

- **WHEN** `interactive=True`, `CheckResult.already_completed=True`, оператор отправляет `confirm_activity(approved=True)`
- **THEN** domain-workflow повторно вызывает activity с `force=True`; activity выполняется независимо от предыдущего результата

#### Scenario: Interactive — подтверждённый повторный запуск завершается с ошибкой

- **WHEN** `interactive=True`, оператор подтверждает запуск (`approved=True`), activity выполняется с `force=True` и завершается с исключением `DatabaseError("connection refused")`
- **THEN** domain-workflow отправляет Signal `activity_status_update` с `state=FAILED`, `reason="connection refused"`; помечает оставшиеся `PENDING` activities как `SKIPPED` с `reason="blocked by failed activity: {activity_id}"`; re-raise; scheduler переводит slot в `SlotState.FAILED`

### Requirement: Таймаут подтверждения с применением automatic политики

Система SHALL применять automatic политику при истечении таймаута ожидания ответа оператора в interactive режиме. При таймауте система SHALL перевести activity из `PENDING_ANSWER` в финальный статус согласно automatic политике: `already_completed` → `ALREADY_COMPLETED` с `reason="confirmation timeout: applied automatic policy"`, `missing_rtypes` → `FAILED` с `reason="confirmation timeout: missing dependencies: {rtypes}"` + mark remaining SKIPPED + re-raise. Система SHALL логировать warning с информацией о таймауте.

#### Scenario: Таймаут при ожидании подтверждения — already completed

- **WHEN** `interactive=True`, `CheckResult.already_completed=True`, оператор не отвечает в течение `confirmation_timeout_seconds`
- **THEN** domain-workflow переводит activity в `ALREADY_COMPLETED` с `reason="confirmation timeout: applied automatic policy"`; выполнение продолжается со следующей activity; логируется warning "Activity confirmation timeout — applying automatic policy"

#### Scenario: Таймаут при ожидании подтверждения — missing deps

- **WHEN** `interactive=True`, `CheckResult.missing_rtypes=["SettleCCP_SaveLimit1"]`, оператор не отвечает в течение `confirmation_timeout_seconds`
- **THEN** domain-workflow переводит activity в `FAILED` с `reason="confirmation timeout: missing dependencies: SettleCCP_SaveLimit1"`; помечает оставшиеся `PENDING` activities как `SKIPPED`; re-raise; scheduler переводит slot в `SlotState.FAILED`

### Requirement: Конфигурация interactive режима

Система SHALL использовать global config `SchedulerSettings.interactive_mode` (env `SCHEDULER_INTERACTIVE_MODE`, default `False`) для определения режима подтверждения. Scheduler SHALL передавать значение в `WorkflowParams.interactive` при формировании параметров запуска domain-workflow. Таймаут подтверждения SHALL использовать per-slot `manual_decision_timeout_seconds` (если задан) или global `SchedulerSettings.manual_decision_timeout_seconds` (default 300 сек).

#### Scenario: Interactive режим включён через конфигурацию

- **WHEN** `SCHEDULER_INTERACTIVE_MODE=true` и scheduler запускает domain-workflow
- **THEN** `WorkflowParams.interactive=True` передаётся в domain-workflow; все activities используют interactive подтверждение при неудаче проверок

#### Scenario: Automatic режим по умолчанию

- **WHEN** `SCHEDULER_INTERACTIVE_MODE` не задан (default `False`)
- **THEN** `WorkflowParams.interactive=False`; все activities используют automatic политику при неудаче проверок

#### Scenario: Per-slot таймаут подтверждения

- **WHEN** slot имеет `manual_decision_timeout_seconds=600` и `interactive=True`
- **THEN** `WorkflowParams.confirmation_timeout_seconds=600`; `wait_condition` ожидает ответ оператора до 600 сек

### Requirement: confirm_activity Update в TradingDaySchedulerWorkflow

Система SHALL поддерживать Update `confirm_activity(slot_id, activity_id, approved: bool) -> ActivityStatusRecord` в `TradingDaySchedulerWorkflow`. Validator SHALL отклонять вызов если slot в terminal state или activity не в `PENDING_ANSWER`. Scheduler SHALL отправлять `CONFIRM_ACTIVITY_SIGNAL` в child-workflow через `workflow.get_external_workflow_handle(child_workflow_id).signal(...)`. Ошибки отправки signal в завершённый child-workflow SHALL обрабатываться (warning + return текущего статуса).

#### Scenario: Оператор подтверждает activity

- **WHEN** оператор вызывает `confirm_activity(slot_id="morning", activity_id="load_trad_deals_to_core", approved=True)`
- **THEN** scheduler отправляет `CONFIRM_ACTIVITY_SIGNAL` с `activity_id="load_trad_deals_to_core", approved=True` в child-workflow; child-workflow разблокирует `wait_condition` и повторно выполняет activity с `force=True`

#### Scenario: Оператор отклоняет activity

- **WHEN** оператор вызывает `confirm_activity(slot_id="morning", activity_id="load_trad_deals_to_core", approved=False)`
- **THEN** scheduler отправляет `CONFIRM_ACTIVITY_SIGNAL` с `approved=False`; child-workflow переводит activity в `SKIPPED` или `ALREADY_COMPLETED`

#### Scenario: Валидатор отклоняет confirm для non-PENDING_ANSWER activity

- **WHEN** оператор вызывает `confirm_activity` для activity в состоянии `RUNNING`
- **THEN** validator отклоняет вызов с ошибкой "activity is not in PENDING_ANSWER state"

#### Scenario: Валидатор отклоняет confirm для terminal slot

- **WHEN** оператор вызывает `confirm_activity` для slot в `SlotState.STOPPED`
- **THEN** validator отклоняет вызов с ошибкой "slot is in terminal state STOPPED; cannot confirm activities"

#### Scenario: Confirm при завершённом child-workflow

- **WHEN** оператор вызывает `confirm_activity`, но child-workflow завершился до отправки signal (timeout, crash, run_timeout)
- **THEN** scheduler ловит `ApplicationError` при отправке signal, логирует warning "confirm_activity: child workflow already completed", возвращает текущий `ActivityStatusRecord` (terminal state) без ошибки

### Requirement: confirm_activity_signal в BaseSessionWorkflow

Система SHALL поддерживать signal handler `confirm_activity_signal(activity_id: str, approved: bool)` в `BaseSessionWorkflow`. Signal устанавливает `_pending_confirmations[activity_id] = approved`, что разблокирует `workflow.wait_condition` в `_execute_activity_managed`.

#### Scenario: Signal подтверждения разблокирует wait_condition

- **WHEN** activity `"load_trad_deals_to_core"` в `PENDING_ANSWER` и child-workflow получает `confirm_activity_signal(activity_id="load_trad_deals_to_core", approved=True)`
- **THEN** `_pending_confirmations["load_trad_deals_to_core"]` устанавливается в `True`; `wait_condition` разблокируется; domain-workflow повторно вызывает activity с `force=True`

#### Scenario: Signal отклонения разблокирует wait_condition

- **WHEN** activity в `PENDING_ANSWER` и child-workflow получает `confirm_activity_signal(activity_id=..., approved=False)`
- **THEN** `_pending_confirmations[activity_id]` устанавливается в `False`; `wait_condition` разблокируется; domain-workflow применяет deny-логику

### Requirement: REST API для подтверждения activity

SettleCore SHALL предоставлять REST endpoint `POST /api/workflows/{scheduler_id}/confirm` с body `{"slot_id": str, "activity_id": str, "approved": bool}`. Endpoint SHALL вызывать `confirm_activity` Update на scheduler workflow через Temporal client и возвращать результат `ActivityStatusRecord`.

#### Scenario: Успешный вызов confirm через REST

- **WHEN** клиент отправляет `POST /api/workflows/{scheduler_id}/confirm` с `{"slot_id": "morning", "activity_id": "load_trad_deals_to_core", "approved": true}`
- **THEN** SettleCore вызывает `confirm_activity` Update на scheduler workflow; возвращает `ActivityStatusRecord` с финальным статусом activity

#### Scenario: Confirm для несуществующего activity

- **WHEN** клиент отправляет confirm с несуществующим `activity_id`
- **THEN** SettleCore возвращает ошибку (Update отклонён validator-ом scheduler-а)

### Requirement: SSE эмиссия новых статусов

SettleCore SSE `StepStreamer` SHALL эммитить статусы `PENDING_ANSWER` и `ALREADY_COMPLETED` через существующий SSE pipeline. `StatusChangeTracker` SHALL обнаруживать изменения статуса на `PENDING_ANSWER` и `ALREADY_COMPLETED` и отправлять SSE events.

#### Scenario: SSE отправляет PENDING_ANSWER

- **WHEN** activity переходит в `PENDING_ANSWER` (scheduler получил signal от child-workflow)
- **THEN** `StepStreamer` эммитит SSE event с `status="PENDING_ANSWER"` и `reason` из `ActivityStatusRecord`

#### Scenario: SSE отправляет ALREADY_COMPLETED

- **WHEN** activity переходит в `ALREADY_COMPLETED`
- **THEN** `StepStreamer` эммитит SSE event с `status="ALREADY_COMPLETED"` и `reason`

### Requirement: SettleConsole UI для подтверждения activity

SettleConsole SHALL отображать модальное окно подтверждения при получении SSE event со `status="PENDING_ANSWER"` для step. Модальное окно SHALL показывать `reason` (missing deps / already completed) и кнопки «Continue» (approved=true) и «Cancel» (approved=false). При клике SHALL вызывать `POST /api/workflows/{scheduler_id}/confirm`.

#### Scenario: Отображение модального окна подтверждения

- **WHEN** SettleConsole получает SSE event со `status="PENDING_ANSWER"` для step `"load_trad_deals_to_core"` с `reason="missing_rtypes: SettleCCP_SaveLimit1"`
- **THEN** отображается модальное окно с текстом причины и кнопками «Continue» / «Cancel»

#### Scenario: Оператор подтверждает через UI

- **WHEN** оператор нажимает «Continue» в модальном окне
- **THEN** SettleConsole отправляет `POST /confirm` с `approved=true`; модальное окно закрывается; step переходит в `RUNNING` (повторный запуск) по следующему SSE event

#### Scenario: Оператор отклоняет через UI

- **WHEN** оператор нажимает «Cancel» в модальном окне
- **THEN** SettleConsole отправляет `POST /confirm` с `approved=false`; модальное окно закрывается; step переходит в `SKIPPED` или `ALREADY_COMPLETED` по следующему SSE event

#### Scenario: Таймаут отображается в UI

- **WHEN** оператор не отвечает в течение таймаута и activity переходит в `FAILED` или `ALREADY_COMPLETED` с `reason="confirmation timeout: ..."`
- **THEN** SettleConsole получает SSE event с финальным статусом; модальное окно (если открыто) закрывается; статус отображается в step list
