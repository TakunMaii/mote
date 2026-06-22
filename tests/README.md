# Mote Test Suite

This directory contains the Python-based test harness and language test cases.

## Layout

- `runner.py`: main test runner
- `cases/<group>/<case_name>/`
  - `main.mote` and optional additional `.mote` files
  - `test.json` describing expected behavior

## Typical usage

```bash
python tests/runner.py --compiler ./mote --all
python tests/runner.py --compiler ./mote --group parser
python tests/runner.py --compiler ./mote --case parser/invalid_top_level_statement
```

## Execution model

The runner supports three layers:

1. Parse / semantic / type tests by invoking the compiler and asserting success or failure.
2. LLVM IR emission tests by invoking the compiler with `-S`.
3. Native execution tests when `clang` is available in `PATH`.

If `clang` is not available, tests that require native execution are skipped.
