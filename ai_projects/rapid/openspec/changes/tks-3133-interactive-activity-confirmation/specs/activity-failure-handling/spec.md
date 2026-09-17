## MODIFIED Requirements

### Requirement: Обработка ошибки activity с остановкой workflow и marking оставшихся activities

Система SHALL поддерживать обработку ошибки activity в domain-workflow с остановкой выполнения и явным указанием статуса для оставшихся activities. При завершении activity с ошибкой (`FAILED`): domain-workflow отправляет Signal `activity_status_update` с `state=FAILED`, `reason=str(exc)`, `completed_at={timestamp}`; затем проходит по всем оставшимся activities со `state=PENDING` (из `ACTIVITY_IDS` после текущей) и отправляет для каждой Signal `activity_status_update` с `state=SKIPPED`, `reason="blocked by failed activity: {failed_activity_id}"`; затем re-raise исключения. Scheduler при получении Signal с `state=FAILED` переводит slot в `SlotState.FAILED` с `reason="activity {activity_id} failed: {reason}"`.

При неудаче pre-execution проверок зависимостей в automatic режиме (или при таймауте в interactive режиме) система SHALL переводить activity в `FAILED` с `reason="missing dependencies: {rtypes}"` (или `"confirmation timeout: missing dependencies: {rtypes}"` при таймауте), помечать оставшиеся `PENDING` activities как `SKIPPED` с `reason="blocked by failed activity: {activity_id}"`, и re-raise.

#### Scenario: Успешная обработка ошибки activity

- **WHEN** activity `"load_trad_deals_to_core"` завершается с исключением `DatabaseError("connection timeout")`
- **THEN** domain-workflow отправляет Signal `activity_status_update` с `activity_id="load_trad_deals_to_core"`, `state=FAILED`, `reason="connection timeout"`, `completed_at={timestamp}`; затем отправляет Signals для всех оставшихся `PENDING` activities с `state=SKIPPED`, `reason="blocked by failed activity: load_trad_deals_to_core"`; затем re-raise исключения; scheduler обновляет `rt.activities["load_trad_deals_to_core"]` и переводит slot в `SlotState.FAILED` с `reason="activity load_trad_deals_to_core failed: connection timeout"`

#### Scenario: Automatic режим — зависимости не завершены

- **WHEN** `interactive=False` и `CheckResult.missing_rtypes=["SettleCCP_SaveLimit1"]`
- **THEN** domain-workflow отправляет Signal `activity_status_update` с `state=FAILED`, `reason="missing dependencies: SettleCCP_SaveLimit1"`; затем отправляет Signals для всех оставшихся `PENDING` activities с `state=SKIPPED`, `reason="blocked by failed activity: {activity_id}"`; затем re-raise; scheduler переводит slot в `SlotState.FAILED` с `reason="activity {activity_id} failed: missing dependencies: SettleCCP_SaveLimit1"`

#### Scenario: Interactive таймаут — зависимости не завершены

- **WHEN** `interactive=True`, `CheckResult.missing_rtypes=["SettleCCP_SaveLimit1"]`, оператор не отвечает в течение `confirmation_timeout_seconds`
- **THEN** domain-workflow отправляет Signal `activity_status_update` с `state=FAILED`, `reason="confirmation timeout: missing dependencies: SettleCCP_SaveLimit1"`; затем помечает оставшиеся `PENDING` activities как `SKIPPED`; re-raise; scheduler переводит slot в `SlotState.FAILED`

#### Scenario: Формат reason для заблокированных activities

- **WHEN** domain-workflow отправляет Signal для оставшейся `PENDING` activity после ошибки
- **THEN** `reason` имеет формат `"blocked by failed activity: {failed_activity_id}"` где `{failed_activity_id}` — activity_id упавшей activity

#### Scenario: Различие между FAILED activity и FAILED запуском workflow

- **WHEN** scheduler получает Signal с `state=FAILED` от activity
- **THEN** slot переходит в `SlotState.FAILED` с `reason="activity {activity_id} failed: {reason}"`
- **WHEN** scheduler не может запустить domain-workflow (ошибка импорта, конфигурации)
- **THEN** slot переходит в `SlotState.FAILED` с `reason="failed to launch: {error}"`

#### Scenario: Оставшиеся activities получают SKIPPED с правильным reason

- **WHEN** activity `"generate_data_for_form_0031_0800"` (15-я из 27) падает с ошибкой
- **THEN** activities 1–14 имеют свои фактические статусы (COMPLETED/FAILED/CANCELLED), activity 15 имеет `state=FAILED`, activities 16–27 получают `state=SKIPPED` с `reason="blocked by failed activity: generate_data_for_form_0031_0800"`

#### Scenario: Scheduler обновляет slot state при получении FAILED signal

- **WHEN** scheduler получает Signal `activity_status_update` с `state=FAILED` для activity
- **THEN** scheduler обновляет `rt.activities[activity_id]` с `state=FAILED`, `reason={reason}`, `completed_at={timestamp}`; проверяет текущее состояние slot — если slot в `LAUNCHED` или `RUNNING`, переводит в `SlotState.FAILED` с `reason="activity {activity_id} failed: {reason}"`; если slot уже в терминальном состоянии — no-op

### Requirement: Формат reason для различных состояний SKIPPED

Система SHALL различать reason для `SKIPPED` activities в зависимости от причины пропуска:
- `reason="filtered by activities_to_run"` — activity пропущена из-за фильтра `activities_to_run` в `manual_launch`
- `reason="cancelled by operator"` — activity отменена оператором через `cancel_activity`
- `reason="blocked by failed activity: {activity_id}"` — activity не запустилась из-за ошибки предыдущей activity
- `reason="operator declined; missing dependencies"` — оператор отклонил запуск при неудаче проверок зависимостей в interactive режиме
- `reason="confirmation timeout: applied automatic policy"` — таймаут подтверждения при `already_completed`, применена automatic политика (финальный статус `ALREADY_COMPLETED`)
- `reason="confirmation timeout: missing dependencies: {rtypes}"` — таймаут подтверждения при `missing_rtypes`, применена automatic политика (финальный статус `FAILED`)

#### Scenario: Query возвращает правильный reason для SKIPPED activities

- **WHEN** оператор вызывает Query `get_status()` после завершения workflow с ошибкой activity
- **THEN** `SlotStatus.activities` содержит: выполненные activities со `state=COMPLETED`, упавшая activity со `state=FAILED` и `reason={error_message}`, оставшиеся activities со `state=SKIPPED` и `reason="blocked by failed activity: {activity_id}"`

#### Scenario: Query различает SKIPPED по фильтру и SKIPPED по ошибке

- **WHEN** оператор запускает workflow с `activities_to_run=("load_trad_deals_to_core", "generate_data_for_form_0031_0800")` и `"load_trad_deals_to_core"` падает с ошибкой
- **THEN** activities не из `activities_to_run` имеют `state=SKIPPED`, `reason="filtered by activities_to_run"`; `"generate_data_for_form_0031_0800"` и последующие имеют `state=SKIPPED`, `reason="blocked by failed activity: load_trad_deals_to_core"`

#### Scenario: Query возвращает reason для operator-denied SKIPPED

- **WHEN** оператор отклонил запуск activity с missing deps в interactive режиме
- **THEN** activity имеет `state=SKIPPED`, `reason="operator declined; missing dependencies"`
