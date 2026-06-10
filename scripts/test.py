#!/usr/bin/env python3

import argparse
import json
import os
import pathlib
import subprocess
import sys
import tempfile


REPO_ROOT = pathlib.Path(__file__).resolve().parent.parent


def fail(message: str) -> None:
    print(f"FAIL: {message}")
    sys.exit(1)


def repo_path(raw: str) -> pathlib.Path:
    return REPO_ROOT / pathlib.Path(raw.replace("\\", os.sep))


def normalize_arg(raw: str) -> str:
    candidate = repo_path(raw)
    if any(sep in raw for sep in ("\\", "/")) and candidate.exists():
        return str(candidate)
    return raw


def ensure_compiler(path: pathlib.Path, should_build: bool) -> None:
    if should_build or not path.exists():
        print(f"Building compiler -> {path}")
        result = subprocess.run(
            ["gcc", "src/main.c", "-o", str(path)],
            cwd=REPO_ROOT,
            text=True,
            capture_output=True,
        )
        if result.returncode != 0:
            if result.stdout:
                print(result.stdout, end="")
            if result.stderr:
                print(result.stderr, end="")
            fail("compiler build failed")


def invoke_compiler(compiler: pathlib.Path, compiler_args: list[str]) -> tuple[int, str]:
    process = subprocess.run(
        [str(compiler), *compiler_args],
        cwd=REPO_ROOT,
        text=True,
        capture_output=True,
    )
    return process.returncode, process.stdout + process.stderr


def new_generated_test_input(case: dict, artifacts_root: pathlib.Path) -> pathlib.Path:
    generated = case.get("generated")
    if not generated:
        return repo_path(case["input"])

    generated_dir = artifacts_root / "generated"
    generated_dir.mkdir(parents=True, exist_ok=True)
    path = generated_dir / f"{case['name']}.mote"

    kind = generated["kind"]
    count = int(generated["count"])
    if kind == "scope_variable_overflow":
        header = "Host = struct {\n    boom: fn(self: Self"
        params = "".join(f", p{i}: i32" for i in range(count))
        content = f"{header}{params}) void {{\n    }},\n}};\n"
    elif kind == "scope_type_overflow":
        lines = []
        for i in range(count):
            lines.append(f"T{i} = struct {{")
            lines.append("    value: i32,")
            lines.append("};")
            lines.append("")
        content = "\n".join(lines)
    else:
        fail(f"unknown generated test kind: {kind}")

    path.write_text(content, encoding="utf-8")
    return path


def build_case_args(case: dict, input_path: pathlib.Path, artifacts_dir: pathlib.Path) -> tuple[list[str], pathlib.Path | None]:
    compiler_args: list[str] = []
    output_path: pathlib.Path | None = None

    if case.get("mode") == "llvm":
        output_name = input_path.stem + ".ll"
        output_path = artifacts_dir / output_name
        compiler_args.extend(["-S", str(input_path), "-o", str(output_path)])
    else:
        compiler_args.append(str(input_path))

    for arg in case.get("args", []):
        compiler_args.append(normalize_arg(str(arg)))

    return compiler_args, output_path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler-path", default="./mote_test")
    parser.add_argument("--manifest-path", default="test/harness/manifest.json")
    parser.add_argument("--artifacts-dir", default="test/artifacts/harness")
    parser.add_argument("--build", action="store_true")
    parser.add_argument("--filter")
    args = parser.parse_args()

    manifest_path = repo_path(args.manifest_path)
    if not manifest_path.exists():
        fail(f"manifest not found: {manifest_path}")

    compiler_path = repo_path(args.compiler_path)
    artifacts_dir = repo_path(args.artifacts_dir)

    ensure_compiler(compiler_path, args.build)
    artifacts_dir.mkdir(parents=True, exist_ok=True)

    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    cases = manifest.get("tests", [])
    if args.filter:
        cases = [case for case in cases if args.filter in case["name"]]

    if not cases:
        fail("no test cases matched")

    passed = 0
    failed = 0

    for case in cases:
        input_path = new_generated_test_input(case, artifacts_dir)
        compiler_args, output_path = build_case_args(case, input_path, artifacts_dir)
        exit_code, output = invoke_compiler(compiler_path, compiler_args)
        ok = True

        if case["expect"] == "success":
            if exit_code != 0:
                ok = False
            elif case.get("mode") == "llvm" and (output_path is None or not output_path.exists()):
                ok = False
        elif case["expect"] == "failure":
            if exit_code == 0:
                ok = False
            for needle in case.get("contains", []):
                if needle not in output:
                    ok = False
                    break
        else:
            fail(f"unknown expectation in manifest for {case['name']}")

        if ok:
            passed += 1
            print(f"PASS [{case['name']}]")
        else:
            failed += 1
            print(f"FAIL [{case['name']}]")
            print(f"  args: {' '.join(compiler_args)}")
            print(f"  exit: {exit_code}")
            if output:
                print("  output:")
                print(output.rstrip())

    print()
    print(f"Summary: {passed} passed, {failed} failed")
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
