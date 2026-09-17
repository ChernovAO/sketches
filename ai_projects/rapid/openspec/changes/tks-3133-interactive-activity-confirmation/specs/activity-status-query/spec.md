## MODIFIED Requirements

### Requirement: Query для получения статуса activities

Система SHALL поддерживать расширение Query `get_status()` в `TradingDaySchedulerWorkflow` для возврата статуса individual activities. Query возвращает `SchedulerStatus` со списком всех слотов; каждый `SlotStatus` содержит поле `activities: dict[str, ActivityStatusRecord]`, где ключ — activity_id, значение — статус activity (`PENDING`, `RUNNING`, `PENDING_ANSWER`, `COMPLETED`, `ALREADY_COMPLETED`, `CANCELLED`, `FAILED`, `SKIPPED`).

#### Scenario: Получение статуса всех activities

- **WHEN** клиент вызывает Query `get_status()`
- **THEN** возвращается список всех слотов с их activities и статусами каждой activity

#### Scenario: Статус activity в состоянии PENDING

- **WHEN** activity ещё не начала выполнение (слот запущен, `_init_activity_states` создал записи)
- **THEN** статус activity включает: `state=PENDING`, `started_at=None`, `completed_at=None`, `reason=None`

#### Scenario: Статус activity в состоянии RUNNING

- **WHEN** activity выполняется (child-workflow отправил `activity_status_update` с `state=RUNNING`)
- **THEN** статус activity включает: `state=RUNNING`, `started_at={timestamp}`, `completed_at=None`

#### Scenario: Статус activity в состоянии PENDING_ANSWER

- **WHEN** activity ожидает подтверждения оператора (child-workflow отправил `activity_status_update` с `state=PENDING_ANSWER`)
- **THEN** статус activity включает: `state=PENDING_ANSWER`, `started_at={timestamp}`, `reason={check_result_description}`

#### Scenario: Статус activity в состоянии COMPLETED

- **WHEN** activity успешно завершена (child-workflow отправил `activity_status_update` с `state=COMPLETED`)
- **THEN** статус activity включает: `state=COMPLETED`, `started_at={timestamp}`, `completed_at={timestamp}`

#### Scenario: Статус activity в состоянии ALREADY_COMPLETED

- **WHEN** activity уже была выполнена в предыдущем запуске и не повторялась
- **THEN** статус activity включает: `state=ALREADY_COMPLETED`, `reason="already completed in previous run"` или `"operator declined re-run; already completed"` или `"confirmation timeout: applied automatic policy"`

#### Scenario: Статус activity в состоянии CANCELLED

- **WHEN** activity отменена оператором через `cancel_activity` или `stop_activity`
- **THEN** статус activity включает: `state=CANCELLED`, `completed_at={timestamp}`, `reason="cancelled by operator"` или `"stopped by operator"`

#### Scenario: Статус activity в состоянии FAILED

- **WHEN** activity завершилась с ошибкой (child-workflow отправил `activity_status_update` с `state=FAILED`)
- **THEN** статус activity включает: `state=FAILED`, `started_at={timestamp}`, `completed_at={timestamp}`, `reason={error_message}`

#### Scenario: Статус activity в состоянии SKIPPED

- **WHEN** activity пропущена из-за `activities_to_run` фильтра, operator deny, или блокировки неудачной activity
- **THEN** статус activity включает: `state=SKIPPED`, `reason` указывает причину пропуска
