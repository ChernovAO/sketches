## ADDED Requirements

> **Совместимость:** проект в фазе активной разработки, обратная совместимость не требуется.

### Requirement: Запуск domain-workflow с фильтром activities

Система SHALL поддерживать запуск domain-workflow с параметром `activities_to_run` через расширение Update `manual_launch` в `TradingDaySchedulerWorkflow`. При указании непустого `activities_to_run` domain-workflow выполняет только перечисленные activities, остальные пропускаются со статусом `SKIPPED`. При пустом `activities_to_run` выполняется полный workflow (существующее поведение). Фильтрация реализуется в domain-workflow через проверку `params.activities_to_run` перед каждым `workflow.start_activity`.

#### Scenario: Успешный запуск с подмножеством activities

- **WHEN** оператор отправляет Update `manual_launch` с `activities_to_run=("load_trad_deals_to_core", "generate_eqm91_report")`
- **THEN** scheduler передаёт `activities_to_run` в `WorkflowParams`, domain-workflow выполняет только указанные activities, Update возвращает `SlotStatus` со `state=LAUNCHED`

#### Scenario: Запуск полного workflow при пустом activities_to_run

- **WHEN** оператор отправляет Update `manual_launch` без `activities_to_run` или с пустым tuple
- **THEN** domain-workflow выполняет все activities (существующее поведение)

#### Scenario: Валидация activity_id в activities_to_run

- **WHEN** оператор отправляет Update `manual_launch` с `activities_to_run=("nonexistent_activity",)` и `task_id="AfterStartTradeEngineSession"`
- **THEN** валидатор Update отклоняет запрос с ошибкой "activity_id не найден в domain-workflow", проверка выполняется против `ACTIVITY_IDS` class attribute domain-workflow, доступного scheduler-у через `_resolve_workflow_mapping`

#### Scenario: Идемпотентность с activities_to_run

- **WHEN** оператор отправляет повторный Update `manual_launch` с тем же `request_id` и `activities_to_run`
- **THEN** новый workflow не создаётся, Update возвращает `SlotStatus` со `state=LAUNCHED` и `reason="already exists"`

### Requirement: Передача activities_to_run в WorkflowParams

Система SHALL передавать `activities_to_run: tuple[str, ...]` в domain-workflow через `WorkflowParams.activities_to_run`. Domain-workflow SHALL проверять `activities_to_run` перед каждым `workflow.start_activity`: если tuple непустой и activity_id не входит в список, activity пропускается, child-workflow отправляет Signal `activity_status_update` с `state=SKIPPED` и `reason="filtered by activities_to_run"`.

#### Scenario: Пропуск activity не из списка

- **WHEN** domain-workflow доходит до activity `"generate_eqm14_report"`, а `activities_to_run=("generate_eqm91_report", "generate_eqm94_report")`
- **THEN** activity `"generate_eqm14_report"` пропускается, её статус устанавливается в `SKIPPED` через Signal `activity_status_update`

#### Scenario: Выполнение activity из списка

- **WHEN** domain-workflow доходит до activity `"generate_eqm91_report"`, а `activities_to_run=("generate_eqm91_report", "generate_eqm94_report")`
- **THEN** activity `"generate_eqm91_report"` выполняется через `workflow.start_activity`

### Requirement: Передача slot_id в WorkflowParams

Система SHALL передавать `slot_id` в domain-workflow через `WorkflowParams.slot_id`. Domain-workflow использует `slot_id` при отправке Signal `activity_status_update` в scheduler для идентификации слота. Scheduler устанавливает `slot_id` в `_build_workflow_params` из `slot.slot_id`.

#### Scenario: Child-workflow получает slot_id

- **WHEN** scheduler запускает domain-workflow для слота `slot_id="morning"`
- **THEN** `WorkflowParams.slot_id` равен `"morning"`, child-workflow использует его при отправке `activity_status_update` signal
