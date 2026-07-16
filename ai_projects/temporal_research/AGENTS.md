# Python Development Guidelines

## General Principles
- Prefer clarity over cleverness.
- Keep functions small and focused.
- Follow existing project patterns when present.

## Code Style
- Use Python 3.11+ features when helpful.
- Format with 4 spaces; no tabs.
- Use type hints for public functions and methods.
- Keep imports grouped: standard library, third-party, local.

## Testing
- Add or update tests for behavior changes.
- Keep tests deterministic and fast.
- Name tests to describe the behavior.

## Error Handling and Logging
- Raise specific exceptions.
- Avoid bare except.
- Use structured, actionable log messages.

## Project Hygiene
- Document non-obvious decisions in code.
- Avoid adding new dependencies unless necessary.
- Keep files small; split modules when they grow.

---

# Temporal Workers and Workflows

## Research Guidelines

- Use https://docs.temporal.io as the **primary source** for all web searches
  about Temporal (concepts, SDK usage, APIs, configuration, best practices).
- Prefer fetching official docs pages over blog posts, tutorials, or AI
  summaries. Only use other sources when the docs do not cover the topic.
- Always cite the specific docs page URL when surfacing Temporal information.

## Project Layout

```
activities/
    payment.py       # Activity definitions (one file per domain)
    notification.py
workflows/
    order_workflow.py  # Workflow definitions (one file per workflow)
    onboarding.py
workers/
    worker.py          # Main worker entrypoint
    run_worker.py      # Worker runner with CLI args
tests/
    test_workflows.py  # Workflow unit/integration tests
    test_activities.py
shared/
    objects.py         # Shared data classes / ValueObjects
    constants.py       # Task queue names, retry configs
```

## Determinism Rules (Must Follow)

- **No non-deterministic calls inside workflows.** No `random`, `datetime.now()`,
  `uuid4()`, or filesystem/network I/O directly in workflow code.
- Use `workflow.now()`, `workflow.random()`, and `workflow.uuid4()`.
- All external interaction goes through **activities**. Workflows orchestrate;
  activities execute.
- Workflow logic must be **idempotent by replay**. Side effects (calls, sleeps,
  signals) must go through the Temporal SDK so the replay produces the same
  command sequence.
- Avoid mutable global state inside workflow classes/functions.

## Writing Activities

- Activities should be **stateless** or use only idempotent external calls.
- Set retry policies explicitly per activity:

```python
from temporalio.common import RetryPolicy

@activity.defn
async def charge_payment(amount: float) -> str:
    ...

# In workflow:
await workflow.execute_activity(
    charge_payment,
    29.99,
    start_to_close_timeout=timedelta(seconds=10),
    retry_policy=RetryPolicy(
        initial_interval=timedelta(seconds=1),
        maximum_attempts=3,
        non_retryable_error_types=["InvalidCardError"],
    ),
)
```

- Define custom exception types for different failure modes.
- Distinguish retryable (transient) from non-retryable (invalid input) errors.

## Writing Workflows

- Define a single `@workflow.defn` class per workflow file.
- Use `@workflow.run` for the main entry method.
- Use `@workflow.signal`, `@workflow.query`, and `@workflow.update` for
  interaction patterns.
- Keep workflow methods short; delegate complex logic to activities or helper
  functions.
- Always set timeouts on `execute_activity` and `execute_child_workflow` calls.
- Use `workflow.continue_as_new` when history may grow large (suggested when
  event count exceeds 10k–50k).

```python
@workflow.defn
class OrderWorkflow:
    @workflow.run
    async def run(self, order: OrderInput) -> str:
        result = await workflow.execute_activity(
            process_order,
            order,
            start_to_close_timeout=timedelta(seconds=30),
        )
        return result
```

## Worker Configuration

- Start workers with a dedicated task queue per service domain.
- Configure client and worker in `workers/worker.py`:

```python
from temporalio.client import Client
from temporalio.worker import Worker

client = await Client.connect("localhost:7233")
worker = Worker(
    client,
    task_queue="my-task-queue",
    workflows=[OrderWorkflow],
    activities=[charge_payment, send_email],
)
await worker.run()
```

- Use `workers/run_worker.py` for CLI entry with argparse or env vars for
  Temporal host, namespace, and task queue overrides.
- Set `max_concurrent_activities` and `max_concurrent_workflow_tasks` to tune
  throughput.
- Enable the Temporal SDK logger bridge for structured logs:

```python
temporalio.runtime.Runtime.default().logger = ...
```

## Testing

- Use `temporalio.testing.WorkflowEnvironment` for integration-like tests with
  an embedded Temporal server.
- Use `temporalio.testing.ActivityEnvironment` for unit testing activities in
  isolation.

```python
from temporalio.testing import WorkflowEnvironment

async with await WorkflowEnvironment.start_time_skipping() as env:
    handle = await env.client.start_workflow(
        OrderWorkflow.run,
        OrderInput(...),
        id="test-order",
        task_queue="my-task-queue",
    )
    assert await handle.result() == "charged"
```

- Tests must be deterministic and fast. Use `start_time_skipping()` to avoid
  real-time waits.
- Mock external services (HTTP, DB) inside activity implementations during
  tests using dependency injection or patching.

## Versioning Workflows

- When modifying workflow code after it runs in production, use `get_version`
  (Patching API) or define a new workflow type with a version suffix.
- Prefer `get_version` for small changes; use new workflow type for structural
  reworks.

```python
v = workflow.get_version("add-gratuity", workflow.DEFAULT_VERSION, 1)
if v == workflow.DEFAULT_VERSION:
    total = subtotal
else:
    total = subtotal * 1.15
```

## Common Pitfalls

- Calling blocking `time.sleep()` inside a workflow. Use `asyncio.sleep` only
  through `workflow.wait_condition`, or better, call `workflow.sleep()`.
- Mutating arguments after passing them to `execute_activity`. The SDK takes a
  reference; changes may affect replay.
- Forgetting to register an activity or workflow with the worker — runtime
  errors only appear when the workflow executes.
- Using `pickle`-unsafe objects as activity/workflow arguments. Prefer
  dataclasses with plain JSON-serializable fields.
