# Python Development Guidelines

## General Principles
- Prefer clarity over cleverness.
- Keep functions small and focused.
- Follow existing project patterns when present.
- Write self-documenting code; use comments for intent, not mechanics.

## Code Style
- Use Python 3.11+ features when helpful.
- Format with 4 spaces; no tabs.
- Use type hints for public functions and methods.
- Keep imports grouped: standard library, third-party, local.
- Use `ruff` or `black` + `isort` for consistent formatting.
- Run `mypy` or `pyright` for static type checking.

## Project Structure
```
src/
    my_package/
        __init__.py
        module.py
        subpackage/
            __init__.py
            ...
tests/
    test_module.py
    conftest.py
pyproject.toml
```

- Keep package source under `src/` for proper isolation.
- Mirror the package structure in `tests/`.
- Use `conftest.py` for shared fixtures.
- Define project metadata, dependencies, and tool configs in `pyproject.toml`.

## Testing
- Use `pytest` as the test runner.
- Add or update tests for behavior changes.
- Keep tests deterministic and fast.
- Name tests to describe the behavior (`test_<what>_<condition>_<expected>`).
- Use fixtures for shared setup; avoid fixture overuse.
- Prefer functional tests for logic, integration tests sparingly for I/O.

## Error Handling and Logging
- Raise specific exceptions; avoid bare `except`.
- Define custom exception classes when built-in ones are insufficient.
- Use structured logging with `logging` or `structlog`.
- Log at appropriate levels: DEBUG for diagnostics, INFO for key events, WARNING for recoverable issues, ERROR for failures.

## Dependency Management
- Declare dependencies in `pyproject.toml` (PEP 621).
- Pin dev dependencies separately from runtime dependencies.
- Avoid adding new dependencies unless necessary.
- Use a lock file (`requirements.lock`, `poetry.lock`, or `uv.lock`) for reproducibility.

## Project Hygiene
- Document non-obvious decisions in code.
- Keep files small; split modules when they exceed ~500 lines.
- Use `dataclasses` or `pydantic` for data containers.
- Validate inputs at system boundaries (API handlers, CLI entrypoints, external data).
- Run `ruff check` and `mypy` before committing.

## CLI / Entrypoints
- Define entrypoints in `pyproject.toml` under `[project.scripts]`.
- Use `argparse` or `click` for CLI argument parsing.
- Keep CLI logic thin; delegate to library code.

## Common Pitfalls
- Mutable default arguments (`def f(items=[])`) — use `None` and initialize inside.
- Catching `Exception` broadly — catch specific exception types.
- Modifying a list/dict while iterating over it — iterate over a copy or use comprehensions.
- Forgetting to close resources — use `with` statements and context managers.
- Leaking secrets into logs or version control — use `.env` files and `.gitignore`.
