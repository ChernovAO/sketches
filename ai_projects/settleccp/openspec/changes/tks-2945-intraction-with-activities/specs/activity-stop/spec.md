## ADDED Requirements

> **Совместимость:** проект в фазе активной разработки, обратная совместимость не требуется.

### Requirement: Остановка running activity через Update API

Система SHALL поддерживать остановку выполняющейся (running) activity через Update `stop_activity(slot_id: str, activity_id: str) -> ActivityStatusRecord` в `TradingDaySchedulerWorkflow`. Остановка возможна только для activities в состоянии `RUNNING`. Scheduler отправляет Signal `stop_activity_signal(activity_id)` в child-workflow через `workflow.get_external_workflow_handle(child_workflow_id).signal(...)`. Domain-workflow инициирует graceful shutdown: `stop_activity_signal` handler вызывает `self._current_activity_handle.cancel()`, activity получает `asyncio.CancelledError` и завершается. После остановки activity переходит в состояние `CANCELLED`, workflow продолжает выполнение других activities.

#### Scenario: Успешная остановка running activity

- **WHEN** оператор отправляет Update `stop_activity` для activity в состоянии `RUNNING`
- **THEN** scheduler отправляет Signal `stop_activity_signal` в child-workflow, domain-workflow вызывает `self._current_activity_handle.cancel()`, activity выполняет graceful shutdown, child-workflow отправляет Signal `activity_status_update` с `state=CANCELLED, reason="stopped by operator"`, Update возвращает `ActivityStatusRecord(state=CANCELLED, reason="stopped by operator")`

#### Scenario: Продолжение выполнения других activities после остановки

- **WHEN** activity `"generate_eqm91_report"` остановлена через `stop_activity`
- **THEN** domain-workflow продолжает выполнение следующих activities (`"generate_eqm91all_report"`, `"generate_eqm14_report"`, и т.д.)

#### Scenario: Отказ при остановке pending activity

- **WHEN** оператор отправляет Update `stop_activity` для activity в состоянии `PENDING`
- **THEN** валидатор Update отклоняет запрос с ошибкой "activity not running; use cancel_activity instead"

#### Scenario: Отказ при остановке завершённой activity

- **WHEN** оператор отправляет Update `stop_activity` для activity в состоянии `COMPLETED`
- **THEN** валидатор Update отклоняет запрос с ошибкой "activity already completed; cannot stop"

#### Scenario: Отказ при повторной остановке

- **WHEN** оператор отправляет Update `stop_activity` для activity в состоянии `CANCELLED`
- **THEN** валидатор Update отклоняет запрос с ошибкой "activity already stopped"

### Requirement: Graceful shutdown activity через handle.cancel()

Система SHALL обеспечивать graceful shutdown activity через Temporal activity cancellation. Domain-workflow использует `workflow.start_activity(fn, ...)` вместо `workflow.execute_activity(fn, ...)` для возможности отмены. `stop_activity_signal` handler проверяет `self._current_activity_id == activity_id` и вызывает `self._current_activity_handle.cancel()`. Activity получает `asyncio.CancelledError`; domain-workflow main loop ловит `CancelledError` и отправляет Signal `activity_status_update` с `state=CANCELLED`.

#### Scenario: Activity получает CancelledError при остановке

- **WHEN** `stop_activity_signal` вызывает `self._current_activity_handle.cancel()` для текущей activity
- **THEN** `await self._current_activity_handle` поднимает `asyncio.CancelledError`, domain-workflow ловит его и отправляет `activity_status_update` с `state=CANCELLED`

#### Scenario: Очистка состояния после остановки activity

- **WHEN** activity остановлена и `CancelledError` обработан
- **THEN** `self._current_activity_id` и `self._current_activity_handle` сбрасываются в `None` в блоке `finally:`, domain-workflow продолжает со следующей activity

### Requirement: Force cancel после timeout

Система SHALL применять принудительную отмену child-workflow через `workflow.get_external_workflow_handle(child_workflow_id).cancel()` если graceful shutdown не завершается в течение `ACTIVITY_GRACEFUL_SHUTDOWN_TIMEOUT` (по умолчанию 60 секунд). Scheduler ждёт `workflow.wait_condition(lambda: activity.state == CANCELLED, timeout=ACTIVITY_GRACEFUL_SHUTDOWN_TIMEOUT)`. При timeout — force cancel всего child-workflow, slot переходит в `STOPPED`.

#### Scenario: Graceful shutdown в пределах timeout

- **WHEN** activity завершает graceful shutdown в течение `ACTIVITY_GRACEFUL_SHUTDOWN_TIMEOUT`
- **THEN** child-workflow отправляет `activity_status_update` с `state=CANCELLED`, `wait_condition` разблокируется, force cancel не применяется, domain-workflow продолжает выполнение других activities

#### Scenario: Force cancel после превышения timeout

- **WHEN** activity не завершает graceful shutdown в течение `ACTIVITY_GRACEFUL_SHUTDOWN_TIMEOUT`
- **THEN** scheduler вызывает `workflow.get_external_workflow_handle(child_workflow_id).cancel()` (force cancel всего child-workflow), slot переходит в `STOPPED` с `reason="force cancelled: activity stop timeout"`, оставшиеся activities не выполняются
