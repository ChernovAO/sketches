# Rapid — Python Testing Guide

## Integration Test Kit (`rapid_testkit/`)

Python-based end-to-end test runner for Rapid ping-pong and protocol tests. Uses `pytest` + `paramiko` for remote execution.

```bash
cd rapid_testkit
pip install -r requirements.txt
export PYTHONPATH=$PYTHONPATH:$(pwd)
python3 runner.py tests/
```

Test suites: `lgw_tests/` (light gateway), `twime_tests/` (TWIME protocol), `rpt0x_tests/` (report tests).

Results (logs, zips) appear in `rapid_testkit/logs/`.
