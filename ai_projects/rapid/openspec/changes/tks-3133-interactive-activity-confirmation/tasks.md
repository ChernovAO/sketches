## 1. Модели данных — расширение

- [ ] 1.1 Добавить `CheckResult` (frozen-dataclass, slots=True) в `activities/result.py`: поля `missing_rtypes: list[str] = field(default_factory=list)`, `already_completed: bool = False`; property `has_failures -> bool`
- [ ] 1.2 Добавить поле `force: bool = False` в `ActivityParams` (`activities/params.py`)
- [ ] 1.3 Добавить `PENDING_ANSWER` и `ALREADY_COMPLETED` в `ActivityStatus` enum (`scheduler_models.py`)
- [ ] 1.4 Обновить inline-проверку терминальных статусов в `activity_status_update` signal handler (`trading_day_scheduler.py`): добавить `ALREADY_COMPLETED` в кортеж терминальных статусов `(COMPLETED, FAILED, SKIPPED, ALREADY_COMPLETED)` (не добавлять `PENDING_ANSWER` — transient)
- [ ] 1.5 Добавить поля `interactive: bool = False` и `confirmation_timeout_seconds: int = 300` в `WorkflowParams` (`workflows/params.py`)

## 2. Константы и конфигурация

- [ ] 2.1 Добавить `CONFIRM_ACTIVITY_SIGNAL = "confirm_activity_signal"` в `constants.py`
- [ ] 2.2 Добавить поле `interactive_mode: bool = False` (env `SCHEDULER_INTERACTIVE_MODE`) в `SchedulerSettings` (`config.py`)

## 3. Декоратор settle_activity — возврат CheckResult

- [ ] 3.1 Изменить pre-execution проверки в `settle_decorator.py`: вместо `raise ApplicationError` возвращать `CheckResult(missing_rtypes=..., already_completed=...)` при неудаче
- [ ] 3.2 Добавить проверку `params.force`: при `True` пропускать обе проверки и переходить к записи `START` + выполнению тела
- [ ] 3.3 Убедиться, что `START` в журнал записывается только после успешных проверок (или при `force=True`) — уже так в текущем коде, проверить после изменений
- [ ] 3.4 Обновить тип возврата wrapper: `ActivityResults | CheckResult`; обновить `SettleActivityWrapper` protocol (`__call__` return type → `Awaitable[ActivityResults | CheckResult]`)

## 4. BaseSessionWorkflow — расширение _execute_activity_managed

- [ ] 4.1 Добавить `_pending_confirmations: dict[str, bool | None]` в `__init__`
- [ ] 4.2 Выделить helper `_run_activity_once(activity_id, activity_name, activity_params, workflow_params, all_activity_ids, timeout, retry_policy) -> Any` — один вызов activity с lifecycle management (start_activity, RUNNING report, CancelledError/Exception handling, finally cleanup). Возвращает `ActivityResults` или `CheckResult`. **Поведенческое изменение:** `_run_activity_once` re-raise `asyncio.CancelledError` (в отличие от текущего `_execute_activity_managed`, который его гасит). Перехват `CancelledError` переносится на уровень `_execute_activity_managed` (см. 4.5)
- [ ] 4.3 Выделить helper `_apply_automatic_policy(check_result, activity_id, workflow_params, all_activity_ids) -> None` — применяет automatic политику: `already_completed` → report `ALREADY_COMPLETED`, return; `missing_rtypes` → report `FAILED`, `_mark_remaining_skipped`, raise
- [ ] 4.4 Выделить helper `_handle_check_failure(check_result, activity_id, activity_name, activity_params, workflow_params, all_activity_ids, timeout, retry_policy) -> Any | None` — automatic mode → `_apply_automatic_policy`; interactive mode → `PENDING_ANSWER` + `wait_condition` + confirm/deny/timeout
- [ ] 4.5 Обновить `_execute_activity_managed`: вызвать `_run_activity_once`, при `isinstance(result, CheckResult) and has_failures` → `_handle_check_failure`; иначе → report COMPLETED (существующая логика). **Перехват `asyncio.CancelledError`** на этом уровне: `except asyncio.CancelledError` → report CANCELLED, return None (не re-raise, workflow продолжается) — сохранение существующего поведения, relocated из `_run_activity_once`
- [ ] 4.6 Реализовать interactive flow в `_handle_check_failure`: report `PENDING_ANSWER`, `_pending_confirmations[activity_id] = None`, `workflow.wait_condition(lambda: ... is not None, timeout=confirmation_timeout_seconds)`
- [ ] 4.7 Реализовать timeout в `_handle_check_failure`: log warning, `_apply_automatic_policy(check_result)` (FAILED или ALREADY_COMPLETED с reason="confirmation timeout: ...")
- [ ] 4.8 Реализовать confirm в `_handle_check_failure`: `_run_activity_once` с `dataclasses.replace(activity_params, force=True)`
- [ ] 4.9 Реализовать deny в `_handle_check_failure`: `already_completed` → `ALREADY_COMPLETED`, `missing_rtypes` → `SKIPPED` с reason="operator declined; missing dependencies"
- [ ] 4.10 Добавить signal handler `@workflow.signal(name=CONFIRM_ACTIVITY_SIGNAL) def confirm_activity_signal(self, activity_id: str, approved: bool)` — устанавливает `_pending_confirmations[activity_id] = approved`

## 5. TradingDaySchedulerWorkflow — confirm_activity Update

- [ ] 5.1 Реализовать `@workflow.update async def confirm_activity(self, slot_id: str, activity_id: str, approved: bool) -> ActivityStatusRecord`
- [ ] 5.2 Реализовать `@confirm_activity.validator def _validate_confirm_activity(self, slot_id, activity_id, approved)` — slot не в terminal state, activity в `PENDING_ANSWER`
- [ ] 5.3 Отправка `CONFIRM_ACTIVITY_SIGNAL` в child-workflow через `workflow.get_external_workflow_handle(child_workflow_id).signal(CONFIRM_ACTIVITY_SIGNAL, args=[activity_id, approved])`
- [ ] 5.4 Обработка ошибки signal в завершённый child-workflow: `try/except ApplicationError`, warning + return текущего статуса
- [ ] 5.5 После отправки signal: `workflow.wait_condition(lambda: activity.state in (COMPLETED, FAILED, SKIPPED, ALREADY_COMPLETED, CANCELLED), timeout=confirmation_timeout_seconds + grace_period)` — ожидание terminal state activity (аналогично `stop_activity` в TKS-2945 Decision 10). При timeout — return текущего статуса с warning
- [ ] 5.6 Логирование через `workflow.logger`: slot_id, activity_id, approved, operation_time

## 6. Scheduler — передача interactive и timeout в WorkflowParams

- [ ] 6.1 Обновить `_build_workflow_params`: добавить `interactive=settings.interactive_mode`, `confirmation_timeout_seconds` = per-slot `manual_decision_timeout_seconds` ?? `settings.manual_decision_timeout_seconds`
- [ ] 6.2 Обновить `activity_status_update` signal handler: добавить `PENDING_ANSWER` и `ALREADY_COMPLETED` в список статусов, которые принимаются (не no-op). `PENDING_ANSWER` — non-terminal, обновляется. `ALREADY_COMPLETED` — terminal, обновляется

## 7. SettleCore — REST API и SSE

- [ ] 7.1 Добавить REST endpoint `POST /api/workflows/{scheduler_id}/confirm` в `api/workflows.py` с body `{"slot_id": str, "activity_id": str, "approved": bool}` — вызывает `confirm_activity` Update на scheduler workflow
- [ ] 7.2 Расширить `ActionResolver` (`api/action_resolver.py`) routing для confirm action (step-level → `confirm_activity` Update)
- [ ] 7.3 Убедиться, что SSE `StepStreamer` и `StatusChangeTracker` корректно обрабатывают строки `PENDING_ANSWER` и `ALREADY_COMPLETED` (передаются как `status` field в SSE payload)

## 8. SettleConsole — UI

- [ ] 8.1 Обработать `status === "PENDING_ANSWER"` в `useWorkflowStore.updateStep` — установить флаг `pendingConfirmation` для step
- [ ] 8.2 Обработать `status === "ALREADY_COMPLETED"` в `useWorkflowStore` как terminal status
- [ ] 8.3 Создать компонент `ConfirmationModal` — модальное окно с `reason` и кнопками «Continue» / «Cancel»
- [ ] 8.4 Добавить API client функцию для `POST /confirm` в `api/rest/workflows/`
- [ ] 8.5 Интегрировать `ConfirmationModal` в ClearingSessionsPage (или DashboardPage) — показывать при `pendingConfirmation` на step
- [ ] 8.6 При клике «Continue» → `POST /confirm` с `approved=true`, закрыть модальное окно
- [ ] 8.7 При клике «Cancel» → `POST /confirm` с `approved=false`, закрыть модальное окно
- [ ] 8.8 При получении SSE event с финальным статусом (timeout) — закрыть модальное окно, отобразить финальный статус
- [ ] 8.9 Интегрировать с `NotificationBell` — badge для ожидающих подтверждений (опционально, parallel to error notifications)

## 9. Тесты — ClearingFlow

- [ ] 9.1 Тесты декоратора: возврат `CheckResult` при неудаче проверок, пропуск проверок при `force=True`, запись `START` только после успешных проверок
- [ ] 9.2 Тесты `_run_activity_once`: успешный запуск (return ActivityResults), CancelledError (report CANCELLED, raise), Exception (report FAILED, mark remaining, raise)
- [ ] 9.3 Тесты `_apply_automatic_policy`: `already_completed` → ALREADY_COMPLETED + continue; `missing_rtypes` → FAILED + mark remaining + raise
- [ ] 9.4 Тесты `_handle_check_failure` automatic mode: делегирование к `_apply_automatic_policy`
- [ ] 9.5 Тесты `_handle_check_failure` interactive confirm: `PENDING_ANSWER` → `confirm_activity_signal(approved=True)` → `_run_activity_once(force=True)` → COMPLETED
- [ ] 9.5.1 Тесты `_handle_check_failure` interactive confirm с ошибкой повторного запуска: `PENDING_ANSWER` → `confirm_activity_signal(approved=True)` → `_run_activity_once(force=True)` → Exception → FAILED + mark remaining + raise
- [ ] 9.6 Тесты `_handle_check_failure` interactive deny missing deps: `PENDING_ANSWER` → `confirm_activity_signal(approved=False)` → SKIPPED
- [ ] 9.7 Тесты `_handle_check_failure` interactive deny already completed: `PENDING_ANSWER` → `confirm_activity_signal(approved=False)` → ALREADY_COMPLETED
- [ ] 9.8 Тесты `_handle_check_failure` interactive timeout already completed: `PENDING_ANSWER` → timeout → ALREADY_COMPLETED с reason="confirmation timeout: ..."
- [ ] 9.9 Тесты `_handle_check_failure` interactive timeout missing deps: `PENDING_ANSWER` → timeout → FAILED с reason="confirmation timeout: ..." + mark remaining + raise
- [ ] 9.10 Тесты `confirm_activity_signal`: устанавливает `_pending_confirmations[activity_id]`, разблокирует `wait_condition`
- [ ] 9.11 Тесты `confirm_activity` Update: validator (terminal slot, non-PENDING_ANSWER activity), отправка signal, обработка ошибки signal в завершённый workflow, `wait_condition` ожидание terminal state, timeout при ожидании
- [ ] 9.12 Тесты `_build_workflow_params`: передача `interactive` и `confirmation_timeout_seconds` (per-slot override, global default)
- [ ] 9.13 Тесты `activity_status_update` signal handler: приём `PENDING_ANSWER` и `ALREADY_COMPLETED`, обновление state
- [ ] 9.14 Тест replay-детерминизма: `wait_condition` + `confirm_activity_signal` воспроизводятся при replay

## 10. Тесты — SettleCore

- [ ] 10.1 Тест `POST /api/workflows/{id}/confirm` — успешный вызов, возврат `ActivityStatusRecord`
- [ ] 10.2 Тест `POST /confirm` с несуществующим activity_id — ошибка
- [ ] 10.3 Тест SSE эмиссии `PENDING_ANSWER` и `ALREADY_COMPLETED` через `StepStreamer`

## 11. Тесты — SettleConsole

- [ ] 11.1 Тест `useWorkflowStore` обработки `PENDING_ANSWER` — установка `pendingConfirmation`
- [ ] 11.2 Тест `ConfirmationModal` рендеринга с reason и кнопками
- [ ] 11.3 Тест клик «Continue» → вызов API с `approved=true`
- [ ] 11.4 Тест клик «Cancel» → вызов API с `approved=false`
- [ ] 11.5 Тест получение финального статуса при timeout — закрытие модального окна

## 12. Проверка качества

- [ ] 12.1 `uv run ruff check .`
- [ ] 12.2 `uv run mypy .`
- [ ] 12.3 `uv run pytest`
- [ ] 12.4 Проверка покрытия тестами модулей decorator, `_execute_activity_managed`, `confirm_activity` — минимум 80%
