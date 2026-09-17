# Rapid — Python Development Guide

## Database Management (`database/`)

PostgreSQL schema management tooling. Core script: `database/git-pg.py` — versioned schema deployment driven by `.sql` files organized in `sqlrepo/` directories.

**Dependencies:** `psycopg2`, `typing_extensions` (see `database/requirements.txt`).

**DB schemas** are organized under `database/rapid/sqlrepo/` by logical domain:

| Schema | Purpose |
|--------|---------|
| `public` | Common/shared objects |
| `rapid` | Core Rapid tables |
| `rapid_cu`, `cu_oi` | Clearing/settlement schemas |
| `rapid_se`, `se_oi` | Securities schemas |
| `xfers_se` | Transfer schemas |

**Key scripts:**

```bash
cd database

# Create empty database from schema definitions
python3 create-empty-db.py

# Export full database to SQL
python3 export-full-db.py

# Git-based schema management (apply/diff/export)
python3 git-pg.py
```

**Internal library** `database/pysql/` provides `PGConnectionParams`, `SQLStrategy`, `DBStrategy`, `FileStrategy` for connecting to PostgreSQL and applying SQL.

## Python Linting

Ruff is used across all Python code. Each subdirectory may extend the root config:

```bash
# Lint all Python code from project root
ruff check .

# Lint database scripts only
ruff check database/
```

| Scope | Config | Line length | Target |
|-------|--------|-------------|--------|
| Root | `ruff.toml` | 120 | (default) |
| `database/` | `database/ruff.toml` | 160 | Python 3.8 |
| `rapid_testkit/` | `rapid_testkit/ruff.toml` | 120 | (default) |

Indent width is 2 spaces everywhere. Double quotes enforced.
