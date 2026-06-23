import argparse
import json
import os
import shutil
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import List, Optional


ROOT = Path(__file__).resolve().parent.parent
TESTS_ROOT = ROOT / "tests"
CASES_ROOT = TESTS_ROOT / "cases"


@dataclass
class TestCase:
    group: str
    name: str
    directory: Path
    meta: dict

    @property
    def case_id(self) -> str:
        return f"{self.group}/{self.name}"


def load_cases() -> List[TestCase]:
    cases: List[TestCase] = []
    if not CASES_ROOT.exists():
        return cases

    for group_dir in sorted(p for p in CASES_ROOT.iterdir() if p.is_dir()):
        for case_dir in sorted(p for p in group_dir.iterdir() if p.is_dir()):
            meta_path = case_dir / "test.json"
            if not meta_path.exists():
                raise SystemExit(f"missing metadata file: {meta_path}")
            with meta_path.open("r", encoding="utf-8") as handle:
                meta = json.load(handle)
            cases.append(TestCase(group=group_dir.name, name=case_dir.name, directory=case_dir, meta=meta))
    return cases


def find_compiler(path_hint: Optional[str]) -> Path:
    if path_hint:
        candidate = Path(path_hint)
        if not candidate.is_absolute():
            candidate = (ROOT / candidate).resolve()
        if candidate.exists():
            return candidate
        raise SystemExit(f"compiler not found: {candidate}")

    default = ROOT / "mote.exe"
    if default.exists():
        return default

    default = ROOT / "mote"
    if default.exists():
        return default

    raise SystemExit("compiler not found; pass --compiler")


def copy_case_to_workspace(case: TestCase, workspace_root: Path) -> Path:
    package_dir = workspace_root / case.name
    shutil.copytree(case.directory, package_dir, dirs_exist_ok=True)
    metadata_copy = package_dir / "test.json"
    if metadata_copy.exists():
        metadata_copy.unlink()
    return package_dir


def command_to_string(args: List[str]) -> str:
    return " ".join(args)


def run_command(args: List[str], cwd: Path) -> subprocess.CompletedProcess:
    return subprocess.run(
        args,
        cwd=str(cwd),
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
    )


def run_command_with_input(args: List[str], cwd: Path, stdin_text: Optional[str]) -> subprocess.CompletedProcess:
    return subprocess.run(
        args,
        cwd=str(cwd),
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        input=stdin_text,
    )


def collect_case_selection(all_cases: List[TestCase], group: Optional[str], case_id: Optional[str]) -> List[TestCase]:
    selected = all_cases
    if group is not None:
        selected = [case for case in selected if case.group == group]
    if case_id is not None:
        selected = [case for case in selected if case.case_id == case_id]
    return selected


def assert_contains(text: str, needle: str, label: str) -> Optional[str]:
    if needle in text:
        return None
    return f"expected {label} to contain {needle!r}"


def maybe_bool(meta: dict, key: str, default: bool) -> bool:
    value = meta.get(key, default)
    if not isinstance(value, bool):
        raise ValueError(f"{key} must be a boolean")
    return value


def run_case(case: TestCase, compiler: Path, clang_available: bool, verbose: bool) -> dict:
    with tempfile.TemporaryDirectory(prefix=f"mote_test_{case.group}_{case.name}_") as temp_dir:
        workspace_root = Path(temp_dir)
        package_dir = copy_case_to_workspace(case, workspace_root)

        mode = case.meta.get("mode", "emit_llvm")
        if mode not in {"emit_llvm", "native", "compile_only"}:
            return {"status": "FAIL", "reason": f"unknown mode {mode!r}"}

        expect_success = maybe_bool(case.meta, "expect_success", True)
        expected_exit = int(case.meta.get("expected_exit_code", 0))
        expected_stdout = case.meta.get("expected_stdout")
        expected_stderr = case.meta.get("expected_stderr")
        expected_diagnostic = case.meta.get("expected_diagnostic")
        stdin_text = case.meta.get("stdin")

        ll_path = workspace_root / f"{case.name}.ll"
        exe_path = workspace_root / f"{case.name}.exe"

        compile_args = [str(compiler)]
        if mode == "emit_llvm" or mode == "native":
            compile_args.extend(["-S", str(package_dir), "-o", str(ll_path)])
        elif mode == "compile_only":
            compile_args.extend([str(package_dir), "-o", str(exe_path)])

        compile_result = run_command(compile_args, ROOT)
        if verbose:
            print(f"[compile] {case.case_id}: {command_to_string(compile_args)}")

        if expect_success:
            if compile_result.returncode != 0:
                return {
                    "status": "FAIL",
                    "reason": "compile failed unexpectedly",
                    "stdout": compile_result.stdout,
                    "stderr": compile_result.stderr,
                }
            if mode == "emit_llvm" and not ll_path.exists():
                return {"status": "FAIL", "reason": "expected LLVM IR output was not produced"}
            if mode == "native" and not ll_path.exists():
                return {"status": "FAIL", "reason": "expected LLVM IR output was not produced"}
        else:
            if compile_result.returncode == 0:
                return {"status": "FAIL", "reason": "compile succeeded unexpectedly"}
            combined = compile_result.stdout + compile_result.stderr
            if expected_diagnostic is not None:
                mismatch = assert_contains(combined, expected_diagnostic, "diagnostic output")
                if mismatch is not None:
                    return {"status": "FAIL", "reason": mismatch, "stdout": compile_result.stdout, "stderr": compile_result.stderr}
            if expected_stderr is not None:
                mismatch = assert_contains(combined, expected_stderr, "compiler output")
                if mismatch is not None:
                    return {"status": "FAIL", "reason": mismatch, "stdout": compile_result.stdout, "stderr": compile_result.stderr}
            return {"status": "PASS"}

        combined_compile = compile_result.stdout + compile_result.stderr
        if expected_diagnostic is not None:
            mismatch = assert_contains(combined_compile, expected_diagnostic, "compiler output")
            if mismatch is not None:
                return {"status": "FAIL", "reason": mismatch, "stdout": compile_result.stdout, "stderr": compile_result.stderr}

        if mode == "emit_llvm":
            return {"status": "PASS"}

        if mode == "compile_only":
            if not clang_available:
                return {"status": "SKIP", "reason": "clang is not available in PATH"}
            if not exe_path.exists():
                return {"status": "PASS"}
            return {"status": "PASS"}

        if not clang_available:
            return {"status": "SKIP", "reason": "clang is not available in PATH"}

        native_args = [str(compiler), str(package_dir), "-o", str(exe_path)]
        native_result = run_command(native_args, ROOT)
        if verbose:
            print(f"[native-compile] {case.case_id}: {command_to_string(native_args)}")
        if native_result.returncode != 0:
            return {
                "status": "FAIL",
                "reason": "native compile failed unexpectedly",
                "stdout": native_result.stdout,
                "stderr": native_result.stderr,
            }
        if not exe_path.exists():
            return {"status": "FAIL", "reason": "native executable was not produced"}

        exec_result = run_command_with_input([str(exe_path)], package_dir, stdin_text)
        if verbose:
            print(f"[run] {case.case_id}: {exe_path}")
        if exec_result.returncode != expected_exit:
            return {
                "status": "FAIL",
                "reason": f"expected exit code {expected_exit}, got {exec_result.returncode}",
                "stdout": exec_result.stdout,
                "stderr": exec_result.stderr,
            }
        if expected_stdout is not None and exec_result.stdout != expected_stdout:
            return {
                "status": "FAIL",
                "reason": f"stdout mismatch: expected {expected_stdout!r}, got {exec_result.stdout!r}",
                "stdout": exec_result.stdout,
                "stderr": exec_result.stderr,
            }
        if expected_stderr is not None and exec_result.stderr != expected_stderr:
            return {
                "status": "FAIL",
                "reason": f"stderr mismatch: expected {expected_stderr!r}, got {exec_result.stderr!r}",
                "stdout": exec_result.stdout,
                "stderr": exec_result.stderr,
            }
        return {"status": "PASS"}


def main() -> int:
    parser = argparse.ArgumentParser(description="Run the Mote language test suite")
    parser.add_argument("--compiler", help="path to mote compiler executable")
    parser.add_argument("--all", action="store_true", help="run all cases")
    parser.add_argument("--group", help="run a single test group")
    parser.add_argument("--case", dest="case_id", help="run a single case using group/name")
    parser.add_argument("--list", action="store_true", help="list available cases")
    parser.add_argument("--verbose", action="store_true", help="print commands while running")
    args = parser.parse_args()

    all_cases = load_cases()
    if args.list:
        for case in all_cases:
            print(case.case_id)
        return 0

    if not args.all and args.group is None and args.case_id is None:
        parser.error("choose --all, --group, --case, or --list")

    selected = collect_case_selection(all_cases, args.group, args.case_id)
    if not selected:
        print("no test cases selected", file=sys.stderr)
        return 1

    compiler = find_compiler(args.compiler)
    clang_available = shutil.which("clang") is not None

    passed = 0
    failed = 0
    skipped = 0

    for case in selected:
        result = run_case(case, compiler, clang_available, args.verbose)
        status = result["status"]
        reason = result.get("reason", "")
        if status == "PASS":
            passed += 1
            print(f"PASS {case.case_id}")
        elif status == "SKIP":
            skipped += 1
            print(f"SKIP {case.case_id}: {reason}")
        else:
            failed += 1
            print(f"FAIL {case.case_id}: {reason}")
            stdout = result.get("stdout")
            stderr = result.get("stderr")
            if stdout:
                print("STDOUT:")
                print(stdout.rstrip())
            if stderr:
                print("STDERR:")
                print(stderr.rstrip())

    print(f"\nSummary: passed={passed} failed={failed} skipped={skipped}")
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
