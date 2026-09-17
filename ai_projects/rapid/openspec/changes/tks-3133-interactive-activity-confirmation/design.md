## Context

Декоратор `settle_activity` (`activities/decorator/settle_decorator.py`) выполняет две pre-execution проверки перед запуском activity:

1. **Проверка зависимостей** (строки 120–130): через `self._checker.check_previous()` проверяется, что все операции-зависимости (deps) имеют финальный статус в журнале LOG. При неудаче — `raise ApplicationError(non_retryable=True)`.
2. **Проверка уже выполненной операции** (строки 133–141): через `self._checker.check_current_completed()` проверяется, не была ли операция уже завершена. При неудаче — `raise ApplicationError(non_retryable=True)`.

Проверки выполняются **внутри activity** (worker-side, stateless). Activity не может получать Temporal signals или ждать ответа оператора. Однако `BaseSessionWorkflow._execute_activity_managed` (`workflows/base_session_workflow.py:166-280`) — это workflow-level метод, который обёртывает каждый вызов activity и уже имеет полный lifecycle management: проверка `_should_run_activity`, `workflow.start_activity`, status reporting через signal, graceful shutdown, error handling.

Инфраструктура TKS-2945 предоставляет:
- `ActivityStatus` enum с terminal/transient состояниями
- `_report_activity_status` — child→scheduler signal для reporting
- `cancel_activity_signal` / `stop_activity_signal` — scheduler→child signals
- `cancel_activity` / `stop_activity` — Updates на scheduler для operator→scheduler
- SSE `StepStreamer` — SettleCore → SettleConsole streaming статусов activities
- `MANUAL_DECISION` late policy pattern с `workflow.wait_condition` + timeout

Существующий паттерн `MANUAL_DECISION` (`trading_day_scheduler.py:755-813`) уже реализует wait-for-operator: slot переходит в `PENDING_MANUAL`, `wait_condition` с timeout, при таймауте — `SKIPPED_LATE`. Этот паттерн — модель для activity-level подтверждения.

## Goals / Non-Goals

**Goals:**
- Заменить жёсткий `raise ApplicationError` в декораторе на возврат `CheckResult` с информацией о неудаче проверок
- Реализовать в `_execute_activity_managed` логику интерактивного подтверждения: `PENDING_ANSWER` → wait for operator → confirm/deny/timeout
- Поддержать automatic режим: `already_completed` → `ALREADY_COMPLETED` (continue), `missing_rtypes` → `FAILED` (stop workflow)
- При таймауте в интерактивном режиме применить automatic политику с уведомлением UI
- Добавить `confirm_activity` Update на scheduler и `confirm_activity_signal` на domain-workflow
- Добавить REST endpoint в SettleCore и модальное окно в SettleConsole
- Переиспользовать существующий timeout из `manual_decision_timeout_seconds` (per-slot override → global default 300 сек)

**Non-Goals:**
- Изменение существующих workflow-level операций (`manual_launch`, `cancel_slot`, `stop_workflow`)
- Изменение схемы БД — `CheckResult` не записывается в журнал
- Per-activity override таймаута — используется per-slot `manual_decision_timeout_seconds`
- Параллельное подтверждение нескольких activities — activities выполняются последовательно
- Изменение `_should_run_activity` (фильтрация `activities_to_run` и cancel — отдельный путь, не связанный с pre-execution checks)

## Decisions

### 1. Возврат `CheckResult` вместо raise

**Решение:** Декоратор `settle_activity` при неудаче pre-execution проверок возвращает `CheckResult(missing_rtypes, already_completed)` вместо `raise ApplicationError`. При `params.force=True` проверки пропускаются.

```python
# activities/result.py
@dataclass(frozen=True, slots=True)
class CheckResult:
    missing_rtypes: list[str] = field(default_factory=list)
    already_completed: bool = False

    @property
    def has_failures(self) -> bool:
        return bool(self.missing_rtypes) or self.already_completed
```

```python
# settle_decorator.py — вместо raise:
if not params.force:
    missing_rtypes = []
    if resolved_previous_rtypes:
        missing_rtypes = self._checker.check_previous(
            resolved_previous_rtypes, settlement_date,
        )
    already_completed = self._checker.check_current_completed(
        current_rtypes, settlement_date,
    )
    if missing_rtypes or already_completed:
        return CheckResult(missing_rtypes=missing_rtypes, already_completed=already_completed)

# START записывается только после успешных проверок (или force=True)
```

**Обоснование:**
- Проверка — ожидаемое, восстанавливаемое условие, не ошибка. Возврат значения семантически корректен.
- Temporal не фиксирует activity как failed — activity успешно выполнила проверки и сообщила результат. Workflow решает следующий шаг.
- `isinstance(result, CheckResult)` — однозначная проверка, `CheckResult` не пересекается с `ActivityResults` (`list[ActivityResult]`).
- `START` в журнал не записывается при неудаче проверок — журнал чист, повторный запуск с `force=True` запишет `START` нормально.

### 2. `ActivityParams.force` — обход проверок

**Решение:** Добавить `force: bool = False` в `ActivityParams` (`activities/params.py`). При `True` декоратор пропускает обе проверки. Workflow устанавливает `force=True` при повторном запуске после подтверждения оператора.

**Обоснование:** Минимальное изменение — один bool-флаг. Декоратор уже принимает `ActivityParams`, не требует нового типа параметра. `force=True` передаётся через `dataclasses.replace(activity_params, force=True)` в `_execute_activity_managed`.

### 3. Новые статусы `PENDING_ANSWER` и `ALREADY_COMPLETED`

**Решение:**

```python
class ActivityStatus(StrEnum):
    PENDING = "PENDING"
    RUNNING = "RUNNING"
    PENDING_ANSWER = "PENDING_ANSWER"          # transient
    COMPLETED = "COMPLETED"
    ALREADY_COMPLETED = "ALREADY_COMPLETED"    # terminal
    CANCELLED = "CANCELLED"
    FAILED = "FAILED"
    SKIPPED = "SKIPPED"
```

- `PENDING_ANSWER` — transient: activity ожидает ответа оператора. Переходит в `COMPLETED`/`FAILED` (confirm), `SKIPPED`/`ALREADY_COMPLETED` (deny), `FAILED`/`ALREADY_COMPLETED` (timeout → automatic policy).
- `ALREADY_COMPLETED` — terminal: операция уже выполнена в предыдущем запуске, не повторяется.

Terminal states: `COMPLETED`, `ALREADY_COMPLETED`, `CANCELLED`, `FAILED`, `SKIPPED`.
Transient states: `PENDING`, `RUNNING`, `PENDING_ANSWER`.

**Обоснование:** `PENDING_ANSWER` позволяет UI показать модальное окно подтверждения. `ALREADY_COMPLETED` отличает «не выполнено, пропущено» (`SKIPPED`) от «уже сделано ранее» — оператор видит корректный статус. Signal handler `activity_status_update` в scheduler уже игнорирует updates для terminal activities; `ALREADY_COMPLETED` добавляется в terminal set.

### 4. `WorkflowParams.interactive` и `confirmation_timeout_seconds`

**Решение:**

```python
# workflows/params.py
@dataclass(frozen=True, slots=True)
class WorkflowParams:
    date: str = ""
    session_id: str = ""
    slot_id: str = ""
    activities_to_run: tuple[str, ...] = ()
    interactive: bool = False
    confirmation_timeout_seconds: int = 300
```

`SchedulerSettings.interactive_mode: bool = False` (env `SCHEDULER_INTERACTIVE_MODE`). Scheduler читает настройку при формировании `WorkflowParams` в `_build_workflow_params`:

```python
interactive = settings.interactive_mode
confirmation_timeout_seconds = (
    slot.manual_decision_timeout_seconds
    if slot.manual_decision_timeout_seconds is not None
    else settings.manual_decision_timeout_seconds
)
```

**Обоснование:**
- Global config — система либо в automatic, либо в interactive режиме. Соответствует «полностью автоматический режим» из Jira-тикета.
- Default `False` — production safety: не блокировать на операторе по умолчанию.
- Переиспользование `manual_decision_timeout_seconds` — тот же таймаут для slot-level `MANUAL_DECISION` и activity-level confirmation. Per-slot override работает для обоих.

### 5. `_execute_activity_managed` — расширение логикой подтверждения

**Решение:** Рефакторинг `_execute_activity_managed` с extraction helper-методов:

**`_run_activity_once`** — один вызов activity с lifecycle management (start_activity, RUNNING report, CancelledError/Exception handling, finally cleanup). Возвращает `ActivityResults` или `CheckResult`.

**`_handle_check_failure`** — обработка `CheckResult`:
- Automatic mode (или interactive timeout fallback): `_apply_automatic_policy(check_result)`
- Interactive mode: `PENDING_ANSWER` → `wait_condition` → confirm/deny/timeout

**`_apply_automatic_policy`** — применяет automatic политику:
- `already_completed` → report `ALREADY_COMPLETED`, return None (continue)
- `missing_rtypes` → report `FAILED`, `_mark_remaining_skipped`, raise (stop workflow)

Полный flow:

```
_execute_activity_managed
  │
  ├── _should_run_activity? No → SKIPPED/CANCELLED, return None
  │
  ├── result = _run_activity_once(params)
  │
  ├── isinstance(result, CheckResult) and has_failures?
  │   │
  │   ├── not interactive:
  │   │   └── _apply_automatic_policy(check_result)
  │   │       already_completed → ALREADY_COMPLETED, return None
  │   │       missing_rtypes   → FAILED, mark remaining, raise
  │   │
  │   └── interactive:
  │       ├── report PENDING_ANSWER, reason=str(check_result)
  │       ├── _pending_confirmations[activity_id] = None
  │       ├── wait_condition(approved is not None, timeout)
  │       │
  │       ├── timeout:
  │       │   ├── log warning
  │       │   ├── _apply_automatic_policy(check_result)
  │       │   └── (FAILED → raise) или (ALREADY_COMPLETED → return None)
  │       │
  │       ├── approved = True:
  │       │   └── _run_activity_once(force=True) → COMPLETED/FAILED
  │       │
  │       └── approved = False (deny):
  │           already_completed → ALREADY_COMPLETED, return None
  │           missing_rtypes    → SKIPPED, return None
  │
  └── result is ActivityResults → report COMPLETED, return result
```

**Обоснование:**
- Extraction `_run_activity_once` устраняет дублирование между initial execution и forced re-execution.
- `_apply_automatic_policy` переиспользуется в automatic mode и при interactive timeout.
- `except asyncio.CancelledError` и `except Exception` остаются в `_run_activity_once` — confirmation логика в `_handle_check_failure` не пересекается с error handling.
- `wait_condition` — стандартный Temporal механизм, уже используемый в `MANUAL_DECISION`.

### 6. `confirm_activity_signal` — signal handler в BaseSessionWorkflow

**Решение:**

```python
@workflow.signal(name=CONFIRM_ACTIVITY_SIGNAL)
def confirm_activity_signal(self, activity_id: str, approved: bool) -> None:
    self._pending_confirmations[activity_id] = approved
```

`_pending_confirmations: dict[str, bool | None]` — `None` = not yet answered, `True`/`False` = operator response. `wait_condition(lambda: self._pending_confirmations.get(activity_id) is not None)` разблокируется при получении signal.

**Обоснование:** Аналогично `cancel_activity_signal` и `stop_activity_signal` — scheduler→child communication через `ExternalWorkflowHandle.signal()`. Update API недоступен для workflow-to-workflow (см. design.md TKS-2945, Decision 7).

### 7. `confirm_activity` Update в TradingDaySchedulerWorkflow

**Решение:**

```python
@workflow.update
async def confirm_activity(
    self, slot_id: str, activity_id: str, approved: bool,
) -> ActivityStatusRecord:
    # validator: slot not terminal, activity in PENDING_ANSWER
    # send CONFIRM_ACTIVITY_SIGNAL to child
    # wait for terminal state (COMPLETED/FAILED/SKIPPED/ALREADY_COMPLETED)
    # return ActivityStatusRecord
```

Validator проверяет: slot не в terminal state, activity в состоянии `PENDING_ANSWER`. Scheduler отправляет signal в child через `workflow.get_external_workflow_handle(child_id).signal(CONFIRM_ACTIVITY_SIGNAL, args=[activity_id, approved])`.

**Обоснование:** Та же архитектура, что `cancel_activity` и `stop_activity` — operator→scheduler через Update (с возвратом результата), scheduler→child через Signal.

### 8. SettleCore REST + SSE

**Решение:**

REST: `POST /api/workflows/{scheduler_id}/confirm` с body `{"slot_id": "...", "activity_id": "...", "approved": true/false}`. SettleCore вызывает `confirm_activity` Update на scheduler workflow через Temporal client.

ActionResolver: routing confirm action для step-level → `confirm_activity` Update.

SSE: `StepStreamer` и `StatusChangeTracker` уже сравнивают status и эммитят изменения. Новые статусы `PENDING_ANSWER` и `ALREADY_COMPLETED` передаются как строки через существующий SSE payload (`status` field). Frontend `useWorkflowStore` обрабатывает их наравне с существующими.

**Обоснование:** Минимальные изменения SettleCore — новый REST endpoint + существующий SSE pipeline работает с новыми строковыми статусами без изменений в streamer логике.

### 9. SettleConsole UI

**Решение:**

- `useWorkflowStore`: при получении SSE event со `status === "PENDING_ANSWER"` — установка флага `pendingConfirmation` для step. `ALREADY_COMPLETED` обрабатывается как terminal status.
- Компонент `ConfirmationModal`: модальное окно, показывающее `reason` (missing deps / already completed) и кнопки «Continue» / «Cancel».
- API client: `POST /confirm` вызов при клике на кнопку.
- `NotificationBell`: badge для ожидающих подтверждений (parallel to error notifications).

**Обоснование:** Существующий SSE pipeline и `useWorkflowStore` уже обрабатывают step-level статусы. `PENDING_ANSWER` — новый trigger для modal, аналогичный тому, как `failed`/`stopped` trigger notification.

## Risks / Trade-offs

- **Двойной вызов activity при подтверждении:** Activity вызывается дважды — первый раз для проверок (возвращает `CheckResult`), второй раз с `force=True` для выполнения. → Mitigation: первый вызов быстрый (только проверки, без тела activity); `START` не записывается при неудаче проверок.

- **`wait_condition` блокирует workflow:** Пока activity в `PENDING_ANSWER`, domain-workflow не выполняет следующие activities. → Mitigation: timeout (300 сек по умолчанию) → automatic policy. Это ожидаемое поведение — оператор должен ответить.

- **`confirm_activity` блокирует scheduler:** Update handler может ждать terminal state до 300 сек. → Mitigation: то же ограничение, что и `stop_activity` (до 60 сек graceful + force cancel). Документировано в design.md TKS-2945 (Risks).

- **Replay детерминизм:** `wait_condition` с timeout — детерминирован при replay (Temporal replay воспроизводит ту же последовательность commands). Signal `confirm_activity_signal` воспроизводится при replay. → Mitigation: стандартный Temporal pattern, уже используется в `MANUAL_DECISION`.

- **Сигнал в завершённый child-workflow:** Если child-workflow завершился (timeout, crash) до получения `confirm_activity_signal`, signal не доставляется. → Mitigation: `try/except` вокруг `handle.signal()`, аналогично Decision 7.1 в TKS-2945. Scheduler возвращает текущий статус activity.

- **Multiple pending confirmations:** Activities выполняются последовательно — одновременно может быть только один `PENDING_ANSWER` per domain-workflow. → Не требует специальной обработки.

- **SSE задержка (5 сек poll):** Оператор видит `PENDING_ANSWER` с задержкой до 5 сек. → Mitigation: существующее ограничение SSE pipeline, приемлемо для clearing operations.

## Migration Plan

Проект в фазе активной разработки. Обратная совместимость не требуется (см. proposal.md TKS-2945, Notes).

1. `CheckResult`, `ActivityParams.force` — новые поля с дефолтами, не влияют на существующий код.
2. Декоратор: при `force=False` (default) поведение меняется с `raise` на `return CheckResult`. Все вызывающие стороны, которые ловили `ApplicationError`, должны быть обновлены. Единственный вызывающий — `_execute_activity_managed` — обновляется одновременно.
3. `interactive_mode: bool = False` (default) — система работает в automatic режиме, поведение для оператора не меняется (вместо raise → FAILED/ALREADY_COMPLETED с тем же outcome: workflow останавливается или продолжается).
4. SettleCore REST + SettleConsole UI — добавляются новые endpoints и компоненты, не затрагивают существующие.

## Open Questions

- **Нет открытых вопросов.** Все ключевые решения приняты в explore-сессии:
  - Возврат `CheckResult` вместо raise — принято
  - Automatic: `already_completed` → ALREADY_COMPLETED, `missing_rtypes` → FAILED + stop workflow — принято
  - Interactive timeout → automatic policy с уведомлением UI — принято
  - Global config `interactive_mode` — принято
  - Переиспользование `manual_decision_timeout_seconds` — принято
