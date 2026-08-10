## ADDED Requirements

> **Совместимость:** проект в фазе активной разработки, обратная совместимость не требуется.

### Requirement: Query для получения статуса activities

Система SHALL поддерживать расширение Query `get_status()` в `TradingDaySchedulerWorkflow` для возврата статуса individual activities. Query возвращает `SchedulerStatus` со списком всех слотов; каждый `SlotStatus` содержит поле `activities: dict[str, ActivityStatusRecord]`, где ключ — activity_id, значение — статус activity (`PENDING`, `RUNNING`, `COMPLETED`, `CANCELLED`, `FAILED`, `SKIPPED`).

#### Scenario: Получение статуса всех activities

- **WHEN** клиент вызывает Query `get_status()`
- **THEN** возвращается список всех слотов с их activities и статусами каждой activity

#### Scenario: Статус activity в состоянии PENDING

- **WHEN** activity ещё не начала выполнение (слот запущен, `_init_activity_states` создал записи)
- **THEN** статус activity включает: `state=PENDING`, `started_at=None`, `completed_at=None`, `reason=None`

#### Scenario: Статус activity в состоянии RUNNING

- **WHEN** activity выполняется (child-workflow отправил `activity_status_update` с `state=RUNNING`)
- **THEN** статус activity включает: `state=RUNNING`, `started_at={timestamp}`, `completed_at=None`

#### Scenario: Статус activity в состоянии COMPLETED

- **WHEN** activity успешно завершена (child-workflow отправил `activity_status_update` с `state=COMPLETED`)
- **THEN** статус activity включает: `state=COMPLETED`, `started_at={timestamp}`, `completed_at={timestamp}`

#### Scenario: Статус activity в состоянии CANCELLED

- **WHEN** activity отменена оператором через `cancel_activity` или `stop_activity`
- **THEN** статус activity включает: `state=CANCELLED`, `completed_at={timestamp}`, `reason="cancelled by operator"` или `"stopped by operator"`

#### Scenario: Статус activity в состоянии FAILED

- **WHEN** activity завершилась с ошибкой (child-workflow отправил `activity_status_update` с `state=FAILED`)
- **THEN** статус activity включает: `state=FAILED`, `started_at={timestamp}`, `completed_at={timestamp}`, `reason={error_message}`

#### Scenario: Статус activity в состоянии SKIPPED

- **WHEN** activity пропущена из-за `activities_to_run` фильтра (child-workflow отправил `activity_status_update` с `state=SKIPPED`)
- **THEN** статус activity включает: `state=SKIPPED`, `reason="filtered by activities_to_run"`

### Requirement: Формат ответа Query с activity статусами

Система SHALL возвращать ответ Query в формате `SchedulerStatus`. Каждый `SlotStatus` содержит: `slot_id`, `task_id`, `planned_time`, `late_policy`, `state`, `actual_launch_time`, `workflow_id`, `reason`, `activities: dict[str, ActivityStatusRecord]`. Каждый `ActivityStatusRecord` содержит: `activity_id` (str), `state` (ActivityStatus enum), `started_at` (ISO 8601 | None), `completed_at` (ISO 8601 | None), `reason` (str | None).

#### Scenario: Формат объекта ActivityStatusRecord

- **WHEN** Query возвращает статус
- **THEN** каждая activity содержит перечисленные поля указанных типов

#### Scenario: Порядок activities в ответе

- **WHEN** Query возвращает статус activities
- **THEN** activities представлены в порядке выполнения в domain-workflow (порядок `ACTIVITY_IDS`), так как `_init_activity_states` создаёт записи в порядке `ACTIVITY_IDS` и dict сохраняет insertion order

### Requirement: Обновление статуса activities в реальном времени через Signal

Система SHALL обновлять статус activities в state scheduler-а по мере выполнения domain-workflow. Child-workflow отправляет Signal `activity_status_update(slot_id, activity_id, state, started_at, completed_at, reason)` в scheduler через `workflow.get_external_workflow_handle(parent_workflow_id).signal(...)`, где `parent_workflow_id` получается из `workflow.info().parent.workflow_id`. Scheduler определяет `@workflow.signal def activity_status_update(...)` который обновляет `_SlotRuntimeState.activities[activity_id]`.

#### Scenario: Обновление после завершения activity

- **WHEN** child-workflow завершает activity `"generate_eqm91_report"` с состоянием `COMPLETED`
- **THEN** child-workflow отправляет Signal `activity_status_update` с `activity_id="generate_eqm91_report"`, `state=COMPLETED`, `completed_at={timestamp}`; scheduler обновляет статус в `_SlotRuntimeState.activities`

#### Scenario: Обновление при старте activity

- **WHEN** child-workflow начинает выполнение activity `"generate_eqm94_report"`
- **THEN** child-workflow отправляет Signal `activity_status_update` с `activity_id="generate_eqm94_report"`, `state=RUNNING`, `started_at={timestamp}`; scheduler обновляет статус

#### Scenario: Query во время выполнения workflow

- **WHEN** Query вызван во время выполнения domain-workflow
- **THEN** Query возвращает актуальные статусы всех activities: выполненные (`COMPLETED`), текущая (`RUNNING`), ожидающие (`PENDING`)

### Requirement: Инициализация activity статусов при запуске workflow

Система SHALL инициализировать все activities domain-workflow в состоянии `PENDING` при запуске workflow. Scheduler вызывает `_init_activity_states(slot_id, task_id)` в `_start_domain_workflow` и `_start_manual_domain_workflow` перед `start_child_workflow`. `_init_activity_states` создаёт `ActivityStatusRecord(state=PENDING)` для каждого `activity_id` из `_domain_workflows[task_id].activity_ids` (полученных из class-level `ACTIVITY_IDS` domain-workflow).

#### Scenario: Activities в PENDING после запуска

- **WHEN** scheduler запускает domain-workflow для слота `slot_id="morning"` с `task_id="MorningSession"`
- **THEN** `_init_activity_states` создаёт 17 записей `ActivityStatusRecord(state=PENDING)` (по количеству activities в `MorningSession.ACTIVITY_IDS`), все доступны через Query `get_status`

#### Scenario: Activities отсутствуют до запуска

- **WHEN** слот в состоянии `PENDING` (workflow ещё не запущен)
- **THEN** `SlotStatus.activities` пуст (`{}`), так как `_init_activity_states` ещё не вызван
