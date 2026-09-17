# Rapid — Development Guide

This document covers cross-cutting development concerns. For language-specific guidelines see:

| File | Scope |
|------|-------|
| [`DEVELOPMENT_CPP.md`](DEVELOPMENT_CPP.md) | C++ architecture, coding standards, build system, debugging |
| [`DEVELOPMENT_JAVA.md`](DEVELOPMENT_JAVA.md) | Java module structure, Maven build, dependencies |
| [`DEVELOPMENT_PYTHON.md`](DEVELOPMENT_PYTHON.md) | Python database tooling, linting |

## Build System Overview

| Language | Build System | Location |
|----------|-------------|----------|
| C++ | CMake + Ninja | Project root |
| Java | Maven | `app_java/` |
| Python | pip + ruff | `database/`, `rapid_testkit/` |

## Tools and Utilities

### Git Hooks

Install pre-push hooks for quality checks:
```bash
tool/git-hooks/set-git-hooks-directory.sh
```

### Wireshark Dissector

Custom PGM/LLM protocol dissector located in `tool/wireshark/` for packet analysis.
