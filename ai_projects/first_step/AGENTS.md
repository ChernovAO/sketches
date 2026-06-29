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
