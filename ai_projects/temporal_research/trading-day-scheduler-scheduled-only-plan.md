# Подплан: Scheduled Execution (только автоматический запуск по расписанию)

> Выдержка из `trading-day-scheduler-workflow-plan-reviewed.md`.
> Исключены все ручные операции: pause, resume, request_manual_run, skip_slot,
> retry_slot, MANUAL_DECISION, cross-day overlap, операторские статусы слотов.

---

## 1. Цель (сокращённая)

Реализовать scheduler торгового дня на Temporal, который:

1. запускается один раз на рынок и целевую дату;
2. читает торговый календарь и расписание из операционной БД;
3. создаёт устойчивый снапшот плана дня;
4. периодически перечитывает расписание и корректно применяет intraday-изменения;
5. запускает domain-workflow не более одного раза для одного логического слота;
6. не зависит от жизненного цикла запущенных domain-workflow;
7. сохраняет историю решений по каждому слоту.

---

## 2. Архитектурные решения

### 2.1 Один scheduler workflow на рынок и целевую дату

Temporal Schedule ежедневно запускает:

```python
TradingDaySchedulerWorkflow(
    market_id="MOEX",
    intended_date="YYYY-MM-DD",
)
```

`intended_date` задаётся снаружи. Workflow не вычисляет её через БД.

### 2.2 Источник календаря и расписания

Операционная БД — источник истины. Workflow получает данные только через activity.

### 2.3 Детерминированный workflow ID

```
scheduler:{market_id}:{intended_date}
```

Повторный запуск не создаёт второе исполнение. При старте задать политику конфликта
workflow ID: Running — использовать существующий, Completed — не пересоздавать.

### 2.4 Независимый жизненный цикл domain-workflow

Запуск domain-workflow через activity (TemporalClient.start_workflow), не через
child workflow. Activity возвращает сериализуемый результат:

```python
@dataclass(frozen=True, slots=True)
class DomainWorkflowStartResult:
    workflow_id: str
    started: bool
    already_exists: bool
    run_id: str | None = None
```

### 2.5 Exactly-once через workflow ID

Workflow ID автоматического запуска:

```
scheduled:{market_id}:{task_id}:{intended_date}:{slot_id}
```

Visibility не используется как lock.

---

## 3. Модели

**Файл:** `apps/clearingflow/src/clearingflow/workflows/scheduler_models.py`

```python
from dataclasses import dataclass, field
from enum import StrEnum


class LatePolicy(StrEnum):
    RUN_LATE = "RUN_LATE"
    RUN_WITHIN_WINDOW = "RUN_WITHIN_WINDOW"
    SKIP_IF_LATE = "SKIP_IF_LATE"


class SlotDecisionStatus(StrEnum):
    PENDING = "PENDING"
    WAITING = "WAITING"
    LAUNCHING = "LAUNCHING"
    LAUNCHED = "LAUNCHED"
    SKIPPED_LATE = "SKIPPED_LATE"
    REMOVED_FROM_PLAN = "REMOVED_FROM_PLAN"


class SchedulerStatus(StrEnum):
    RUNNING = "RUNNING"
    COMPLETED = "COMPLETED"
    FAILED = "FAILED"


@dataclass(frozen=True, slots=True)
class ScheduledSlot:
    slot_id: str
    task_id: str
    start_at: str
    late_policy: LatePolicy
    run_window_seconds: int = 0
    enabled: bool = True


@dataclass(frozen=True, slots=True)
class TradingDayPlan:
    market_id: str
    intended_date: str
    operational_date: str
    is_trading_day: bool
    revision: str
    slots: tuple[ScheduledSlot, ...]


@dataclass(slots=True)
class SlotState:
    slot_id: str
    task_id: str
    status: SlotDecisionStatus = SlotDecisionStatus.PENDING
    workflow_id: str | None = None
    reason: str | None = None
    plan_revision: str | None = None


@dataclass(frozen=True, slots=True)
class SchedulerInput:
    market_id: str
    intended_date: str


@dataclass(frozen=True, slots=True)
class SchedulerResult:
    market_id: str
    intended_date: str
    operational_date: str | None
    final_revision: str | None
    status: SchedulerStatus
    slots: tuple[SlotState, ...]
```

Проверить поддержку `StrEnum`, dataclass и tuple data converter-ом проекта.

---

## 4. Семантика late policy

Использовать `workflow.now()` и абсолютное время `slot.start_at`.

| Policy | До `start_at` | После `start_at`, внутри окна | После окна |
|---|---|---|---|
| `RUN_LATE` | ждать | запускать | запускать |
| `RUN_WITHIN_WINDOW` | ждать | запускать | `SKIPPED_LATE` |
| `SKIP_IF_LATE` | ждать | `SKIPPED_LATE` | `SKIPPED_LATE` |

`deadline = start_at + run_window_seconds`. Граница включительна: `now <= deadline`.

---

## 5. Activity

**Каталог:** `apps/clearingflow/src/clearingflow/activities/scheduler/`

### 5.1 `read_trading_day_plan`

```python
read_trading_day_plan(
    market_id: str,
    intended_date: str,
) -> TradingDayPlan
```

- read-only, возвращает полный план одной ревизии;
- сортирует слоты детерминированно, проверяет уникальность `slot_id`;
- преобразует время в ISO datetime с timezone offset;
- не возвращает DB connection, ORM model или Temporal client object.

### 5.2 `validate_slot_for_launch`

```python
validate_slot_for_launch(
    market_id: str,
    intended_date: str,
    slot_id: str,
    expected_revision: str,
) -> LaunchValidationResult
```

Выполняется непосредственно перед запуском:
- повторное чтение актуального слота;
- проверка торгового дня;
- проверка, что слот существует и включён;
- проверка, что task mapping не изменился;
- получение текущей revision.

### 5.3 `start_domain_workflow`

```python
start_domain_workflow(request: DomainWorkflowStartRequest) \
    -> DomainWorkflowStartResult
```

- использует Temporal Client;
- запускает domain-workflow с детерминированным ID;
- задаёт ID conflict/reuse policy;
- обрабатывает AlreadyStarted;
- не ожидает завершения domain-workflow;
- возвращает сериализуемый результат.

### 5.4 `emit_scheduler_alert`

Только если в проекте уже есть alerting-инфраструктура. Иначе — structured log
и сохранение статуса в workflow state. Alert subsystem не проектировать.

---

## 6. Scheduler Workflow

**Файл:** `apps/clearingflow/src/clearingflow/workflows/trading_day_scheduler.py`

Event loop, а не `for slot in initial_plan.slots`:

```python
@workflow.defn
class TradingDaySchedulerWorkflow:
    def __init__(self) -> None:
        self._status = SchedulerStatus.RUNNING
        self._plan: TradingDayPlan | None = None
        self._slots: dict[str, SlotState] = {}
        self._force_refresh = False

    @workflow.run
    async def run(self, input: SchedulerInput) -> SchedulerResult:
        self._plan = await self._read_plan(input)
        self._reconcile_plan(self._plan)

        if not self._plan.is_trading_day:
            return self._complete()

        while not self._all_slots_terminal():
            await self._refresh_if_due_or_requested(input)
            await self._process_due_slots(input)

            if self._all_slots_terminal():
                break

            timeout = self._seconds_until_next_wakeup()
            try:
                await workflow.wait_condition(
                    lambda: self._force_refresh,
                    timeout=timeout,
                )
            except asyncio.TimeoutError:
                pass

        return self._complete()
```

Форма ожидания должна соответствовать версии Temporal Python SDK в проекте.

### Queries (read-only)

```python
get_status() -> SchedulerView
get_slots() -> tuple[SlotState, ...]
```

Query не выполняет I/O и возвращает только workflow state.

---

## 7. Reconciliation intraday-изменений

План перечитывается:
- не реже `SCHEDULER_PLAN_REFRESH_INTERVAL`;
- непосредственно перед запуском слота.

Правила по `slot_id`:

1. Новый слот добавляется как `PENDING`.
2. Изменение времени применяется, пока слот не в terminal state.
3. Удалённый `PENDING`/`WAITING` слот получает `REMOVED_FROM_PLAN`.
4. Удаление уже `LAUNCHED` слота не отменяет domain-workflow.
5. Изменение `task_id` для существующего слота — несовместимое изменение: слот получает статус `FAILED_VALIDATION` (добавить в SlotDecisionStatus).

---

## 8. Маппинг domain-workflow

Детерминированный статический registry:

```python
DOMAIN_WORKFLOWS = {
    "AfterStartTradeEngineSession": DomainWorkflowSpec(...),
    "MorningSession": DomainWorkflowSpec(...),
}
```

Перед реализацией проверить:
- реальные workflow type names;
- сигнатуры input;
- тип и смысл `session_id`;
- task queue;
- timeout и retry semantics;
- существующие места запуска этих workflow.

Не использовать `session_id=trading_date` без подтверждения контракта.

---

## 9. Timeout и retry

В `constants.py`:

```python
SCHEDULER_ACTIVITY_TIMEOUT = timedelta(minutes=1)
SCHEDULER_PLAN_REFRESH_INTERVAL = timedelta(minutes=5)
SCHEDULER_MAX_LIFETIME = timedelta(hours=6)
```

- Для read-only activity — отдельная retry policy.
- Для `start_domain_workflow` — retry, совместимый с идемпотентным workflow ID.
- Domain workflow run timeout — отдельное решение для каждого workflow type.

---

## 10. Temporal Schedule

Создание Schedule — инфраструктурная операция.

Schedule запускает workflow с input:

```json
{
  "market_id": "MOEX",
  "intended_date": "<date derived for scheduled occurrence>"
}
```

Если Temporal Schedule не подставляет дату в input динамически — bootstrap workflow:

```
DailyScheduleBootstrapWorkflow(market_id)
    -> вычисляет intended_date из workflow.now() и timezone рынка
    -> запускает/получает scheduler:{market}:{date}
    -> завершается
```

Overlap policy: `Skip` или `BufferOne`.

Скрипт управления: `create`, `update`, `pause`, `unpause`, `delete`, `describe`.
Создание идемпотентно.

---

## 11. Structured logging и observability

События: `scheduler_started`, `plan_loaded`, `plan_revision_changed`,
`slot_waiting`, `slot_launch_requested`, `slot_launched`, `slot_skipped`,
`scheduler_completed`.

Обязательные поля: `market_id`, `intended_date`, `operational_date`, `slot_id`,
`task_id`, `plan_revision`, `workflow_id`, `reason`.

Search Attributes (если приняты в проекте): `MarketId`, `TradingDate`, `SchedulerStatus`.

---

## 12. Регистрация

Изменить:
- `workflows/__init__.py`
- `activities/__init__.py`
- `workers/settle_worker.py`

Зарегистрировать:
- scheduler workflow
- bootstrap workflow (если нужен)
- scheduler activity

---

## 13. Тестирование

### 13.1 Unit tests

Покрыть чистые функции:
- построение workflow ID;
- late policy;
- вычисление deadline;
- reconciliation plans;
- state transitions;
- определение следующего wake-up.

### 13.2 Workflow tests

`WorkflowEnvironment.start_time_skipping()`.

Сценарии:

1. Неторговый день — scheduler завершается без запусков.
2. Обычный торговый день — каждый слот запускается один раз.
3. Scheduler стартовал после времени слота с `RUN_LATE` — слот запускается.
4. `RUN_WITHIN_WINDOW` внутри окна — запускается.
5. `RUN_WITHIN_WINDOW` после окна — `SKIPPED_LATE`.
6. `SKIP_IF_LATE` после `start_at` — `SKIPPED_LATE`.
7. В расписание добавлен новый слот — появляется и обрабатывается.
8. Слот удалён до запуска — `REMOVED_FROM_PLAN`.
9. Время слота изменено — используется новое время.
10. `task_id` изменён — `FAILED_VALIDATION`.
11. Activity `start_domain_workflow` повторена после retry — второго workflow нет.
12. Scheduler завершается, domain-workflow продолжает выполняться.
13. Replay test после изменения workflow state machine.

### 13.3 Integration tests

С реальным test Temporal server:
- ID conflict/reuse policy;
- AlreadyStarted handling;
- Temporal Schedule creation;
- Search Attributes;
- поведение после worker restart.

---

## 14. Acceptance criteria

### AC-1. Уникальность scheduler

**Given** scheduler для `MOEX:2026-07-21` уже существует
**When** Schedule или backend пытается запустить его повторно
**Then** второе логическое исполнение не создаётся.

### AC-2. Уникальность automatic slot

**Given** automatic slot уже запущен
**When** activity повторяется после retry
**Then** второй domain-workflow не создаётся.

### AC-3. Intraday update

**Given** время pending slot изменено в БД
**When** scheduler получает новую revision
**Then** он использует новое время без restart workflow.

### AC-4. Независимость domain workflow

**Given** scheduler завершился
**When** domain-workflow всё ещё выполняется
**Then** domain-workflow не отменяется и не завершается автоматически.

---

## 15. Порядок реализации

### Этап 1. Исследование репозитория

Найти и зафиксировать:
- версию `temporalio`;
- data converter;
- способ создания Temporal Client;
- реальные сигнатуры `AfterStartTradeEngineSession` и `MorningSession`;
- смысл `WorkflowParams.session_id`;
- существующие retry/timeout policies;
- структуру DB repositories;
- conventions structured logging;
- существующие tests с `WorkflowEnvironment`.

Не начинать реализацию без контракта входных параметров domain-workflow.

### Этап 2. Модели и pure state machine

- enum, dataclass;
- state transitions;
- late policy;
- reconciliation;
- unit tests.

### Этап 3. Activity

- `read_trading_day_plan`;
- `validate_slot_for_launch`;
- `start_domain_workflow`.

Сначала mock repository interfaces, если точная DB схема не готова.

### Этап 4. Workflow

- event loop;
- Query handlers;
- refresh timer;
- launch orchestration;
- durable slot state.

### Этап 5. Schedule/bootstrap

После проверки Temporal CLI: прямой input с intended date либо bootstrap workflow.

### Этап 6. Интеграция и тесты

Регистрация, integration tests, operational script.

---

## 16. Файлы

### Создать

- `apps/clearingflow/src/clearingflow/workflows/scheduler_models.py`
- `apps/clearingflow/src/clearingflow/workflows/trading_day_scheduler.py`
- `apps/clearingflow/src/clearingflow/workflows/daily_schedule_bootstrap.py` — только если требуется
- `apps/clearingflow/src/clearingflow/activities/scheduler/__init__.py`
- `apps/clearingflow/src/clearingflow/activities/scheduler/read_trading_day_plan.py`
- `apps/clearingflow/src/clearingflow/activities/scheduler/validate_slot_for_launch.py`
- `apps/clearingflow/src/clearingflow/activities/scheduler/start_domain_workflow.py`
- `apps/clearingflow/scripts/manage_clearing_schedule.sh`
- `apps/clearingflow/tests/workflows/test_trading_day_scheduler.py`
- `apps/clearingflow/tests/activities/test_scheduler_activities.py`
- `apps/clearingflow/tests/unit/test_scheduler_state_machine.py`

### Изменить

- `apps/clearingflow/src/clearingflow/constants.py`
- `apps/clearingflow/src/clearingflow/workflows/__init__.py`
- `apps/clearingflow/src/clearingflow/activities/__init__.py`
- `apps/clearingflow/src/clearingflow/workers/settle_worker.py`

---

## 17. Не делать в первой реализации

- Не использовать Visibility как lock;
- Не вводить таблицу состояния scheduler в БД;
- Не дублировать Temporal state в операционной БД;
- Не создавать alerting subsystem;
- Не вычислять intended_date через БД;
- Не использовать child workflow;
- Не задавать `session_id` на основании предположения;
- Не писать динамический workflow registry;
- Не добавлять `check_db_available`.

---

## 18. Что исключено из полного плана (manual operations)

- Ручные команды: pause, resume, request_manual_run, skip_slot, retry_slot;
- Все Update handlers;
- Signal `refresh_plan` (заменён на периодический refresh по таймеру);
- Статусы: `WAITING_MANUAL_DECISION`, `SKIPPED_BY_OPERATOR`, `BLOCKED_BY_ACTIVE_EXECUTION`;
- Late policy `MANUAL_DECISION`;
- Cross-day overlap (секция 2.7 исходного плана);
- Manual request idempotency;
- Правила reconciliation 6 и 7 (terminal state не сбрасывается, ручной перезапуск).
