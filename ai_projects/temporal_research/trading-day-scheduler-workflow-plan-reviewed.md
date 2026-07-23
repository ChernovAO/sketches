# План реализации TradingDaySchedulerWorkflow

## 1. Цель

Реализовать управляемый scheduler торгового дня на Temporal, который:

1. запускается один раз на рынок и целевую дату;
2. читает торговый календарь и расписание из операционной БД;
3. создаёт устойчивый снапшот плана дня;
4. периодически перечитывает расписание и корректно применяет intraday-изменения;
5. запускает domain-workflow не более одного раза для одного логического слота;
6. учитывает автоматические и ручные запуски через единый механизм координации;
7. не зависит от жизненного цикла запущенных domain-workflow;
8. сохраняет полную историю решений по каждому слоту;
9. допускает паузу, ручное решение и повторную обработку по явно заданным правилам.

## 2. Архитектурные решения

### 2.1 Один scheduler workflow на рынок и целевую дату

Temporal Schedule ежедневно запускает:

```python
TradingDaySchedulerWorkflow(
    market_id="MOEX",
    intended_date="YYYY-MM-DD",
)
```

`intended_date` является датой, для которой создан конкретный scheduler execution. Она не должна определяться внутри workflow запросом «текущей даты» из БД.

Причина: catch-up запуск, задержка Temporal Schedule или восстановление после сбоя не должны смещать scheduler на следующий операционный день.

### 2.2 Источник календаря и расписания

Операционная БД остаётся источником истины для:

- признака торгового дня;
- операционной даты, если она отличается от календарной;
- состава слотов;
- времени запуска;
- политик late execution;
- ревизии расписания.

Workflow получает данные только через activity.

### 2.3 Единая точка координации автоматических и ручных запусков

Manual-запуск не должен обходить scheduler и самостоятельно конкурировать с ним.

Все команды по конкретному торговому дню направляются в `TradingDaySchedulerWorkflow` через Temporal Update или Signal:

- `pause()`;
- `resume()`;
- `request_manual_run(slot_id, request_id, override_policy)`;
- `skip_slot(slot_id, reason)`;
- `retry_slot(slot_id, request_id)`;
- `refresh_plan()`.

Для команд, где вызывающей стороне нужен подтверждённый результат, использовать Temporal Update. Signal допустим только для команд без немедленного результата.

Если scheduler конкретной даты ещё не существует, backend должен сначала получить или запустить workflow с детерминированным ID.

### 2.4 Детерминированный workflow ID scheduler

```text
scheduler:{market_id}:{intended_date}
```

Повторный запуск того же scheduler не должен создавать второе исполнение.

При старте необходимо явно задать политику конфликта workflow ID. Ожидаемая семантика:

- если execution уже Running — использовать существующий workflow;
- если execution завершён — не создавать новый автоматически без отдельной административной операции.

Точные значения `WorkflowIDConflictPolicy` и `WorkflowIDReusePolicy` выбрать по версии Temporal Python SDK, установленной в репозитории.

### 2.5 Независимый жизненный цикл domain-workflow

Domain-workflow не являются логическими child-workflow scheduler-а.

Запуск выполняется через activity:

```text
TradingDaySchedulerWorkflow
    -> start_domain_workflow activity
        -> TemporalClient.start_workflow(...)
```

Это обеспечивает независимый жизненный цикл и устраняет зависимость от `ParentClosePolicy`.

Activity должна возвращать только сериализуемый результат:

```python
@dataclass(frozen=True, slots=True)
class DomainWorkflowStartResult:
    workflow_id: str
    started: bool
    already_exists: bool
    run_id: str | None = None
```

### 2.6 Exactly-once бизнес-семантика через workflow ID

Для автоматического запуска:

```text
scheduled:{market_id}:{task_id}:{intended_date}:{slot_id}
```

Для ручного запуска:

```text
manual:{market_id}:{task_id}:{intended_date}:{request_id}
```

`request_id` обязателен и обеспечивает идемпотентность повторного запроса API.

Visibility не используется как lock и не участвует в доказательстве уникальности запуска.

Проверка Visibility может применяться только для:

- диагностики;
- отображения состояния;
- best-effort предупреждений о другом активном execution;
- политики cross-day overlap, если она действительно требуется бизнесом.

### 2.7 Cross-day overlap

Необходимо отдельно определить бизнес-правило:

> Может ли задача одного типа для новой торговой даты стартовать, пока экземпляр предыдущей даты ещё выполняется?

До подтверждения использовать консервативное правило:

- если активен domain-workflow того же `task_id` для другой даты, слот получает `BLOCKED_BY_ACTIVE_EXECUTION`;
- оператор может выполнить manual override через Update;
- проверка является best-effort и не заменяет атомарную уникальность текущего слота.

## 3. Модели

**Файл:** `apps/clearingflow/src/clearingflow/workflows/scheduler_models.py`

```python
from dataclasses import dataclass, field
from enum import StrEnum


class LatePolicy(StrEnum):
    RUN_LATE = "RUN_LATE"
    RUN_WITHIN_WINDOW = "RUN_WITHIN_WINDOW"
    SKIP_IF_LATE = "SKIP_IF_LATE"
    MANUAL_DECISION = "MANUAL_DECISION"


class SlotDecisionStatus(StrEnum):
    PENDING = "PENDING"
    WAITING = "WAITING"
    WAITING_MANUAL_DECISION = "WAITING_MANUAL_DECISION"
    LAUNCHING = "LAUNCHING"
    LAUNCHED = "LAUNCHED"
    SKIPPED_LATE = "SKIPPED_LATE"
    SKIPPED_BY_OPERATOR = "SKIPPED_BY_OPERATOR"
    BLOCKED_BY_ACTIVE_EXECUTION = "BLOCKED_BY_ACTIVE_EXECUTION"
    FAILED_VALIDATION = "FAILED_VALIDATION"
    FAILED_TO_START = "FAILED_TO_START"
    REMOVED_FROM_PLAN = "REMOVED_FROM_PLAN"


class SchedulerStatus(StrEnum):
    RUNNING = "RUNNING"
    PAUSED = "PAUSED"
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
    request_id: str | None = None


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

Перед реализацией проверить, какой data converter используется в проекте и поддерживает ли он `StrEnum`, dataclass и tuple без дополнительной настройки.

## 4. Семантика late policy

Использовать `workflow.now()` и абсолютное время `slot.start_at`.

| Policy | До `start_at` | После `start_at`, внутри окна | После окна |
|---|---|---|---|
| `RUN_LATE` | ждать | запускать | запускать |
| `RUN_WITHIN_WINDOW` | ждать | запускать | `SKIPPED_LATE` |
| `SKIP_IF_LATE` | ждать | `SKIPPED_LATE` | `SKIPPED_LATE` |
| `MANUAL_DECISION` | ждать | `WAITING_MANUAL_DECISION` | `WAITING_MANUAL_DECISION` |

Для `RUN_WITHIN_WINDOW`:

```text
deadline = start_at + run_window_seconds
```

Граница окна включительна: `now <= deadline` допускает запуск.

`MANUAL_DECISION` не завершается автоматически, пока:

- оператор не отправит `request_manual_run` или `skip_slot`;
- либо не наступит общий deadline scheduler-а, определённый конфигурацией.

## 5. Activity

**Каталог:** `apps/clearingflow/src/clearingflow/activities/scheduler/`

### 5.1 `read_trading_day_plan`

```python
read_trading_day_plan(
    market_id: str,
    intended_date: str,
) -> TradingDayPlan
```

Требования:

- read-only;
- возвращает полный план одной ревизии;
- сортирует слоты детерминированно;
- проверяет уникальность `slot_id`;
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

Activity выполняет непосредственно перед запуском:

- повторное чтение актуального слота;
- проверку торгового дня;
- проверку, что слот существует и включён;
- проверку, что task mapping не изменился;
- получение текущей revision;
- при необходимости best-effort cross-day overlap check.

Отдельный `check_db_available()` не создавать: успешное чтение и является проверкой доступности БД.

### 5.3 `start_domain_workflow`

```python
start_domain_workflow(request: DomainWorkflowStartRequest) \
    -> DomainWorkflowStartResult
```

Activity:

- использует Temporal Client;
- запускает domain-workflow с детерминированным ID;
- явно задаёт ID conflict/reuse policy;
- корректно обрабатывает AlreadyStarted;
- не ожидает завершения domain-workflow;
- возвращает сериализуемый результат.

### 5.4 `emit_scheduler_alert`

Добавлять только при наличии существующей инфраструктуры alerting.

Если такой инфраструктуры в репозитории нет, в первой версии:

- записывать structured log;
- сохранять статус и reason в workflow state;
- alerting вынести в отдельную задачу.

Код-агент не должен самостоятельно проектировать новый alert subsystem.

## 6. Scheduler workflow

**Файл:** `apps/clearingflow/src/clearingflow/workflows/trading_day_scheduler.py`

Workflow должен быть event loop, а не `for slot in initial_plan.slots`.

Упрощённый алгоритм:

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
            if self._status == SchedulerStatus.PAUSED:
                await workflow.wait_condition(
                    lambda: self._status != SchedulerStatus.PAUSED
                    or self._force_refresh
                )

            await self._refresh_if_due_or_requested(input)
            await self._process_due_slots(input)

            if self._all_slots_terminal():
                break

            timeout = self._seconds_until_next_wakeup()
            try:
                await workflow.wait_condition(
                    lambda: self._force_refresh
                    or self._status == SchedulerStatus.PAUSED,
                    timeout=timeout,
                )
            except asyncio.TimeoutError:
                pass

        return self._complete()
```

Точная форма ожидания должна соответствовать версии Temporal Python SDK и существующим соглашениям проекта.

## 7. Reconciliation intraday-изменений

План перечитывается:

- не реже `SCHEDULER_PLAN_REFRESH_INTERVAL`;
- непосредственно перед запуском слота;
- по команде `refresh_plan`;
- после `resume`.

Reconciliation выполняется по `slot_id`.

Правила:

1. Новый слот добавляется как `PENDING`.
2. Изменение времени применяется, пока слот не перешёл в terminal state.
3. Удалённый `PENDING`/`WAITING` слот получает `REMOVED_FROM_PLAN`.
4. Удаление уже `LAUNCHED` слота не отменяет domain-workflow.
5. Изменение `task_id` для существующего слота считается несовместимым изменением:
   - слот получает `FAILED_VALIDATION`;
   - требуется ручное решение.
6. Terminal state не сбрасывается одной только сменой revision.
7. Повторный запуск terminal slot возможен только через явную manual-команду с новым `request_id`.

## 8. Updates, Signals и Queries

### 8.1 Updates

Использовать для команд, которым нужен подтверждённый ответ:

```python
pause() -> CommandResult
resume() -> CommandResult
request_manual_run(slot_id, request_id, override_policy) -> CommandResult
skip_slot(slot_id, reason) -> CommandResult
retry_slot(slot_id, request_id) -> CommandResult
```

Update handler должен валидировать:

- существование `slot_id`;
- допустимость перехода состояния;
- уникальность `request_id`;
- отсутствие повторного запуска без явного override.

### 8.2 Signals

Допустимо использовать для:

```python
refresh_plan()
```

где вызывающей стороне не требуется синхронный результат.

### 8.3 Queries

```python
get_status() -> SchedulerView
get_slots() -> tuple[SlotState, ...]
```

Query не выполняет I/O и возвращает только текущее workflow state.

## 9. Маппинг domain-workflow

Не использовать динамический импорт или строковую рефлексию внутри workflow.

Создать детерминированный статический registry:

```python
DOMAIN_WORKFLOWS = {
    "AfterStartTradeEngineSession": DomainWorkflowSpec(...),
    "MorningSession": DomainWorkflowSpec(...),
}
```

Перед реализацией код-агент обязан проверить:

- реальные workflow type names;
- сигнатуры input;
- тип и смысл `session_id`;
- task queue;
- timeout и retry semantics;
- существующие места запуска этих workflow.

Нельзя использовать `session_id=trading_date` без подтверждения существующего контракта.

## 10. Timeout и retry

Добавить в `constants.py` только подтверждённые значения:

```python
SCHEDULER_ACTIVITY_TIMEOUT = timedelta(minutes=1)
SCHEDULER_PLAN_REFRESH_INTERVAL = timedelta(minutes=5)
SCHEDULER_MAX_LIFETIME = timedelta(hours=6)
```

Для scheduler activity использовать отдельную retry policy для read-only операций.

Для `start_domain_workflow` retry должен быть совместим с идемпотентным workflow ID: повтор activity не должен создавать второе логическое исполнение.

Не задавать domain workflow run timeout на основании приблизительной бизнес-длительности без проверки существующих SLA. Если timeout требуется, он должен быть отдельным решением для каждого workflow type.

## 11. Temporal Schedule

Создание Schedule остаётся инфраструктурной операцией.

Предпочтительно использовать timezone-aware Schedule Spec, если установленная версия Temporal CLI это поддерживает, вместо ручного преобразования MSK в UTC.

Schedule запускает workflow с input:

```json
{
  "market_id": "MOEX",
  "intended_date": "<date derived for scheduled occurrence>"
}
```

Если Temporal Schedule не умеет динамически подставлять дату в input, использовать короткий bootstrap workflow:

```text
DailyScheduleBootstrapWorkflow(market_id)
    -> вычисляет intended_date из workflow.now() и timezone рынка
    -> запускает/получает scheduler:{market}:{date}
    -> завершается
```

Bootstrap не читает «текущую дату» из БД.

Для Schedule overlap policy по умолчанию использовать `Skip` или `BufferOne`, а не `AllowAll`, если bootstrap гарантированно короткий.

`AllowAll` допускается только при доказанной необходимости и уникальном scheduler ID по дате.

Скрипт должен поддерживать:

```text
create
update
pause
unpause
delete
describe
```

Создание должно быть идемпотентным либо сопровождаться явным сообщением, что Schedule уже существует.

## 12. Structured logging и observability

Workflow и activity должны писать структурированные события:

```text
scheduler_started
plan_loaded
plan_revision_changed
slot_waiting
slot_launch_requested
slot_launched
slot_skipped
slot_blocked
manual_command_received
scheduler_paused
scheduler_resumed
scheduler_completed
```

Обязательные поля:

```text
market_id
intended_date
operational_date
slot_id
task_id
plan_revision
workflow_id
request_id
reason
```

Не дублировать одно событие одновременно в нескольких слоях без дополнительного смысла.

Для поиска scheduler execution добавить Search Attributes, если это уже принято в проекте:

- `MarketId`;
- `TradingDate`;
- `SchedulerStatus`.

## 13. Регистрация

Изменить:

- `workflows/__init__.py`;
- `activities/__init__.py`;
- `workers/settle_worker.py`.

Зарегистрировать:

- scheduler workflow;
- bootstrap workflow, если он нужен;
- scheduler activity;
- Update/Signal/Query handlers входят в сам workflow и отдельно не регистрируются.

## 14. Тестирование

### 14.1 Unit tests

Покрыть чистые функции:

- построение workflow ID;
- late policy;
- вычисление deadline;
- reconciliation plans;
- state transitions;
- manual request idempotency;
- определение следующего wake-up.

### 14.2 Workflow tests

Использовать `WorkflowEnvironment.start_time_skipping()`.

Обязательные сценарии:

1. Неторговый день — scheduler завершается без запусков.
2. Обычный торговый день — каждый слот запускается один раз.
3. Scheduler стартовал после времени слота с `RUN_LATE` — слот запускается.
4. `RUN_WITHIN_WINDOW` внутри окна — запускается.
5. `RUN_WITHIN_WINDOW` после окна — `SKIPPED_LATE`.
6. `SKIP_IF_LATE` после `start_at` — `SKIPPED_LATE`.
7. `MANUAL_DECISION` — workflow ждёт Update.
8. Pause до слота — автоматический запуск не происходит.
9. Resume после слота — применяется late policy.
10. В расписание добавлен новый слот — он появляется и обрабатывается.
11. Слот удалён до запуска — `REMOVED_FROM_PLAN`.
12. Время слота изменено — используется новое время.
13. `task_id` изменён — `FAILED_VALIDATION`.
14. Activity `start_domain_workflow` повторена после retry — второго workflow нет.
15. Повторный manual request с тем же `request_id` идемпотентен.
16. Manual и automatic события происходят одновременно — решение остаётся единственным и согласованным.
17. Scheduler завершается, domain-workflow продолжает выполняться.
18. Replay test проходит после изменения workflow state machine.

### 14.3 Integration tests

Проверить с реальным test Temporal server:

- ID conflict/reuse policy;
- AlreadyStarted handling;
- Temporal Schedule creation;
- Update handlers;
- Search Attributes;
- поведение после worker restart.

## 15. Acceptance criteria

### AC-1. Уникальность scheduler

**Given** scheduler для `MOEX:2026-07-21` уже существует  
**When** Schedule или backend пытается запустить его повторно  
**Then** второе логическое исполнение не создаётся.

### AC-2. Уникальность automatic slot

**Given** automatic slot уже запущен  
**When** activity повторяется после retry  
**Then** второй domain-workflow не создаётся.

### AC-3. Manual idempotency

**Given** manual request с `request_id=X` уже обработан  
**When** backend повторяет тот же запрос  
**Then** возвращается существующий результат без нового workflow.

### AC-4. Intraday update

**Given** время pending slot изменено в БД  
**When** scheduler получает новую revision  
**Then** он использует новое время без restart workflow.

### AC-5. Независимость domain workflow

**Given** scheduler завершился  
**When** domain-workflow всё ещё выполняется  
**Then** domain-workflow не отменяется и не завершается автоматически.

### AC-6. Управление оператором

**Given** scheduler поставлен на pause  
**When** наступает время слота  
**Then** автоматический запуск не происходит до resume или manual command.

## 16. Порядок реализации для код-агента

### Этап 1. Исследование репозитория

До изменения кода найти и зафиксировать в отчёте:

- версию `temporalio`;
- существующий Temporal data converter;
- способ создания Temporal Client;
- реальные сигнатуры `AfterStartTradeEngineSession` и `MorningSession`;
- смысл `WorkflowParams.session_id`;
- существующие retry/timeout policies;
- структуру DB repositories;
- наличие alerting;
- conventions structured logging;
- существующие tests с `WorkflowEnvironment`.

Не начинать реализацию, если невозможно определить контракт входных параметров domain-workflow.

### Этап 2. Модели и pure state machine

Реализовать:

- enum;
- dataclass;
- state transitions;
- late policy;
- reconciliation;
- unit tests.

### Этап 3. Activity

Реализовать:

- `read_trading_day_plan`;
- `validate_slot_for_launch`;
- `start_domain_workflow`.

Сначала использовать mock repository interfaces, если точная DB схема ещё не готова, но не придумывать SQL без подтверждения.

### Этап 4. Workflow

Реализовать:

- event loop;
- Update/Signal/Query;
- refresh timer;
- launch orchestration;
- durable slot state.

### Этап 5. Schedule/bootstrap

После проверки возможностей установленного Temporal CLI выбрать:

- прямой input с intended date;
- либо bootstrap workflow.

### Этап 6. Интеграция и тесты

Добавить регистрацию, integration tests и operational script.

## 17. Файлы

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

## 18. Не делать в рамках первой реализации

- не использовать Visibility как lock;
- не запускать manual workflow напрямую в обход coordinator;
- не вводить отдельную таблицу состояния scheduler в БД;
- не дублировать Temporal state в операционной БД;
- не создавать новый alerting subsystem;
- не вычислять intended date через текущую дату из БД;
- не использовать child workflow без явной необходимости;
- не задавать `session_id` на основании предположения;
- не писать динамический workflow registry;
- не добавлять `check_db_available`, если тот же результат даёт реальное чтение.

## 19. Открытые вопросы, требующие бизнес-решения

1. Допускается ли cross-day overlap одного `task_id`?
2. Может ли оператор принудительно запустить слот при активном workflow предыдущей даты?
3. Какой максимальный срок ожидания для `MANUAL_DECISION`?
4. Можно ли повторно запускать успешно завершённый automatic slot?
5. Что является `session_id` для каждого domain-workflow?
6. Нужен ли отдельный operational date помимо intended calendar date?
7. Какие изменения расписания разрешены после начала торгового дня?
8. Требуется ли реальный alert или достаточно UI/status/structured log в первой версии?
