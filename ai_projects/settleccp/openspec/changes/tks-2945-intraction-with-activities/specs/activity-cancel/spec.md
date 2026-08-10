## ADDED Requirements

> **Совместимость:** проект в фазе активной разработки, обратная совместимость не требуется.

### Requirement: Отмена pending activity через Update API

Система SHALL поддерживать отмену не-запущенной (pending) activity внутри domain-workflow через Update `cancel_activity(slot_id: str, activity_id: str) -> ActivityStatusRecord` в `TradingDaySchedulerWorkflow`. Отмена возможна только для activities в состоянии `PENDING`.

Для pre-launch отмены (слот в `PENDING`/`PENDING_MANUAL`): scheduler отмечает `_SlotRuntimeState.activities[activity_id]` как `CANCELLED`. При запуске workflow отменённая activity исключается из `activities_to_run` — domain-workflow не получает информацию об отменённой activity, она просто не входит в список запуска.

Для in-flight отмены (слот в `LAUNCHED`/`RUNNING`): scheduler отправляет Signal `cancel_activity_signal(activity_id)` в child-workflow через `workflow.get_external_workflow_handle(child_workflow_id).signal(...)`.

#### Scenario: Успешная отмена pending activity до запуска workflow

- **WHEN** оператор отправляет Update `cancel_activity` для слота в состоянии `PENDING` и activity в состоянии `PENDING`
- **THEN** scheduler отмечает activity как `CANCELLED` в `_SlotRuntimeState.activities`, Update возвращает `ActivityStatusRecord(state=CANCELLED, reason="cancelled by operator")`; при последующем запуске workflow activity исключается из `activities_to_run`

#### Scenario: Отмена pending activity в запущенном workflow

- **WHEN** оператор отправляет Update `cancel_activity` для слота в состоянии `LAUNCHED`/`RUNNING`, и activity ещё не начала выполнение (состояние `PENDING`)
- **THEN** scheduler отправляет Signal `cancel_activity_signal` в child-workflow через `workflow.get_external_workflow_handle(child_workflow_id).signal(...)`, domain-workflow добавляет activity_id в `_cancelled_activities` set, activity пропускается при достижении, Update возвращает `ActivityStatusRecord(state=CANCELLED, reason="cancelled by operator")`

#### Scenario: Отказ при отмене running activity

- **WHEN** оператор отправляет Update `cancel_activity` для activity в состоянии `RUNNING`
- **THEN** валидатор Update отклоняет запрос с ошибкой "activity already running; use stop_activity instead"

#### Scenario: Отказ при отмене завершённой activity

- **WHEN** оператор отправляет Update `cancel_activity` для activity в состоянии `COMPLETED`
- **THEN** валидатор Update отклоняет запрос с ошибкой "activity already completed; cannot cancel"

#### Scenario: Отказ при отмене уже отменённой activity

- **WHEN** оператор отправляет Update `cancel_activity` для activity в состоянии `CANCELLED`
- **THEN** валидатор Update отклоняет запрос с ошибкой "activity already cancelled"

#### Scenario: Отказ при отмене пропущенной activity

- **WHEN** оператор отправляет Update `cancel_activity` для activity в состоянии `SKIPPED`
- **THEN** валидатор Update отклоняет запрос с ошибкой "activity already skipped"

### Requirement: Пропуск отменённых activities в domain-workflow

Система SHALL пропускать отменённые activities в domain-workflow. Domain-workflow проверяет `activity_id in self._cancelled_activities` (set, пополняемый через `cancel_activity_signal`) перед каждым `workflow.start_activity`. При обнаружении отмены, domain-workflow отправляет Signal `activity_status_update` с `state=CANCELLED` и `reason="cancelled by operator"`, выполнение продолжается со следующей activity.

Pre-launch отменённые activities не попадают в domain-workflow: scheduler исключает их из `activities_to_run` при формировании параметров запуска.

#### Scenario: Пропуск отменённой activity через signal

- **WHEN** domain-workflow доходит до activity `"generate_eqm91_report"`, и `"generate_eqm91_report"` находится в `self._cancelled_activities` (добавлен через `cancel_activity_signal`)
- **THEN** activity `"generate_eqm91_report"` пропускается, её статус устанавливается в `CANCELLED` через Signal `activity_status_update`

#### Scenario: Выполнение не-отменённой activity

- **WHEN** domain-workflow доходит до activity `"generate_eqm94_report"`, и `_cancelled_activities` пуст
- **THEN** activity `"generate_eqm94_report"` выполняется, так как она не входит в `_cancelled_activities`
