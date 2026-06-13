#!/usr/bin/env python3

import argparse
import json
import os
import pathlib
import re
import subprocess
import sys
from typing import Iterable


REPO_ROOT = pathlib.Path(__file__).resolve().parent.parent
IMPORT_RE = re.compile(r'@import\("([^"]+)"\)')


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


def extra_search_roots_for_case(case: dict, input_path: pathlib.Path) -> list[str]:
    roots: list[str] = []
    raw_input = str(case.get("input", "")).replace("\\", "/")

    if "/test/generic/" in f"/{raw_input}/" or raw_input.startswith("test/generic/"):
        roots.append("test/generic")
    if "/test/diagnostic/" in f"/{raw_input}/" and "generic_" in pathlib.Path(raw_input).name:
        roots.append("test/generic")
    if raw_input.startswith("test/multi/"):
        roots.append("test/multi")
        roots.append("test/pkg")

    deduped: list[str] = []
    for root in roots:
        if root not in deduped:
            deduped.append(root)
    return deduped


def resolve_case_input(raw: str) -> pathlib.Path:
    candidate = repo_path(raw)
    if candidate.is_dir():
        return candidate

    if candidate.suffix == ".mote":
        package_dir = candidate.with_suffix("")
        if package_dir.is_dir():
            return package_dir

    return candidate


def package_name_for_case(case_name: str) -> str:
    return case_name.replace("-", "_")


def strip_line_comments(source: str) -> str:
    return re.sub(r"(?m)^[ \t]*//.*(?:\n|$)", "", source)


def split_top_level_statements(source: str) -> list[str]:
    statements: list[str] = []
    start = 0
    i = 0
    paren = 0
    brace = 0
    bracket = 0
    in_string = False
    in_char = False
    while i < len(source):
        ch = source[i]
        nxt = source[i + 1] if i + 1 < len(source) else ""

        if not in_string and not in_char and ch == "/" and nxt == "/":
            i += 2
            while i < len(source) and source[i] != "\n":
                i += 1
            continue

        if ch == '"' and not in_char and (i == 0 or source[i - 1] != "\\"):
            in_string = not in_string
            i += 1
            continue

        if ch == "'" and not in_string and (i == 0 or source[i - 1] != "\\"):
            in_char = not in_char
            i += 1
            continue

        if in_string or in_char:
            i += 1
            continue

        if ch == "(":
            paren += 1
        elif ch == ")":
            paren = max(paren - 1, 0)
        elif ch == "{":
            brace += 1
        elif ch == "}":
            brace = max(brace - 1, 0)
        elif ch == "[":
            bracket += 1
        elif ch == "]":
            bracket = max(bracket - 1, 0)
        elif ch == ";" and paren == 0 and brace == 0 and bracket == 0:
            statements.append(source[start:i + 1])
            start = i + 1
        i += 1

    tail = source[start:]
    if tail.strip():
        statements.append(tail)
    return statements


def statement_declares_top_level_name(statement: str, declared: set[str]) -> bool:
    stripped = strip_line_comments(statement).strip()
    if not stripped:
        return True
    if stripped.startswith("pub "):
        stripped = stripped[4:].lstrip()
    if stripped.startswith("@operator("):
        return True
    if stripped.startswith(("if", "while", "for", "do", "break", "continue", "return", "defer")):
        return False

    match = re.match(r"^([A-Za-z_][A-Za-z0-9_]*)\s*(::|:=|:\s*[^=;\n]+?\s*:|:\s*[^=;\n]+?\s*=)\s*", stripped, re.S)
    if match is None:
        return False

    name = match.group(1)
    op = match.group(2)
    if name in declared and op == ":=":
        return False

    declared.add(name)
    return True


def split_declarations_and_exec(source: str) -> tuple[list[str], list[str]]:
    declarations: list[str] = []
    executable: list[str] = []
    declared: set[str] = set()

    for statement in split_top_level_statements(source):
        if statement_declares_top_level_name(statement, declared):
            declarations.append(statement.rstrip())
        else:
            executable.append(statement.rstrip())

    return declarations, executable


def resolve_legacy_import(source_path: pathlib.Path, raw_import: str) -> pathlib.Path | None:
    if raw_import.startswith(("std:", "c:", "vendor:")):
        return None
    if raw_import.startswith("./") or raw_import.startswith("../"):
        target = (source_path.parent / raw_import).resolve()
    elif "/" in raw_import:
        target = (REPO_ROOT / "test" / raw_import).resolve()
    else:
        return None

    if target.is_dir():
        return target

    candidate = pathlib.Path(str(target) + ".mote")
    if candidate.is_file():
        return candidate
    return None


def rewrite_imports_and_collect_dependencies(source_path: pathlib.Path, source: str) -> tuple[str, list[pathlib.Path]]:
    dependencies: list[pathlib.Path] = []

    def replace(match: re.Match[str]) -> str:
        raw_import = match.group(1)
        resolved = resolve_legacy_import(source_path, raw_import)
        if resolved is None:
            return match.group(0)

        package_name = resolved.stem if resolved.is_file() else resolved.name
        dependencies.append(resolved)
        return f'@import("{package_name}")'

    rewritten = IMPORT_RE.sub(replace, source)
    return rewritten, dependencies


def materialize_compat_package_tree(case: dict, source_path: pathlib.Path, artifacts_root: pathlib.Path) -> pathlib.Path:
    case_root = artifacts_root / "generated" / case["name"]
    case_root.mkdir(parents=True, exist_ok=True)
    emitted: set[pathlib.Path] = set()

    def emit_module(original_path: pathlib.Path, is_root: bool) -> str:
        canonical = original_path.resolve()
        if canonical in emitted:
            return canonical.stem

        package_name = canonical.stem.replace("-", "_")
        package_dir = case_root / package_name
        package_dir.mkdir(parents=True, exist_ok=True)
        output_path = package_dir / "main.mote"

        source = canonical.read_text(encoding="utf-8")
        rewritten, dependencies = rewrite_imports_and_collect_dependencies(canonical, source)
        for dependency in dependencies:
            if dependency.is_file():
                emit_module(dependency, False)

        body = rewritten.lstrip()
        if body.startswith("@package("):
            content = rewritten
        else:
            prefix = f'@package("{package_name}");\n\n'
            if is_root:
                declarations, executable = split_declarations_and_exec(rewritten)
                content = prefix
                if declarations:
                    content += "\n\n".join(declarations).rstrip() + "\n\n"
                if executable:
                    executable_body = "\n\n".join(executable).rstrip()
                    content += "main :: fn() void {\n"
                    content += executable_body + "\n"
                    content += "};\n"
            else:
                content = prefix + rewritten

        output_path.write_text(content, encoding="utf-8")
        emitted.add(canonical)
        return package_name

    root_package_name = emit_module(source_path, True)
    return case_root / root_package_name


def compiler_inputs() -> list[pathlib.Path]:
    inputs = [REPO_ROOT / "src" / "main.c"]
    inputs.extend(sorted((REPO_ROOT / "src").rglob("*.h")))
    return inputs


def newest_mtime(paths: Iterable[pathlib.Path]) -> float:
    latest = 0.0
    for path in paths:
        try:
            latest = max(latest, path.stat().st_mtime)
        except FileNotFoundError:
            return float("inf")
    return latest


def should_rebuild_compiler(path: pathlib.Path) -> bool:
    if not path.exists():
        return True
    return newest_mtime(compiler_inputs()) > path.stat().st_mtime


def ensure_compiler(path: pathlib.Path, should_build: bool) -> None:
    if should_build or should_rebuild_compiler(path):
        print(f"Building compiler -> {path}")
        result = subprocess.run(
            ["gcc", "-std=c11", "-Wall", "-Wextra", "-g", "src/main.c", "-o", str(path)],
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


def load_output_text(output_path: pathlib.Path) -> str:
    try:
        return output_path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return ""


def contains_needle(haystack: str, needle: str) -> bool:
    if needle in haystack:
        return True
    return needle.replace("\\", "/") in haystack.replace("\\", "/")


def new_generated_test_input(case: dict, artifacts_root: pathlib.Path) -> pathlib.Path:
    generated = case.get("generated")
    if not generated:
        input_path = resolve_case_input(case["input"])
        if input_path.is_dir():
            return input_path
        if input_path.suffix == ".mote":
            return materialize_compat_package_tree(case, input_path, artifacts_root)
        return input_path

    generated_dir = artifacts_root / "generated"
    generated_dir.mkdir(parents=True, exist_ok=True)
    package_name = case["name"].replace("-", "_")
    case_root = generated_dir / case["name"]
    package_dir = case_root / package_name
    package_dir.mkdir(parents=True, exist_ok=True)
    path = package_dir / "main.mote"

    kind = generated["kind"]
    count = int(generated["count"])
    if kind == "scope_variable_overflow":
        header = '@package("' + package_name + '");\n\nHost = struct {\n    boom: fn(self: Self'
        params = "".join(f", p{i}: i32" for i in range(count))
        content = f"{header}{params}) void {{\n    }},\n}};\n"
    elif kind == "scope_type_overflow":
        lines = [f'@package("{package_name}");', ""]
        for i in range(count):
            lines.append(f"T{i} :: struct {{")
            lines.append("    value: i32,")
            lines.append("};")
            lines.append("")
        content = "\n".join(lines)
    else:
        fail(f"unknown generated test kind: {kind}")

    path.write_text(content, encoding="utf-8")
    return package_dir


def build_case_args(case: dict, input_path: pathlib.Path, artifacts_dir: pathlib.Path) -> tuple[list[str], pathlib.Path | None]:
    compiler_args: list[str] = []
    output_path: pathlib.Path | None = None

    mode = case.get("mode")
    if mode == "llvm" or (case.get("expect") == "failure" and case.get("run_mode") != "exe"):
        output_name = input_path.name + ".ll"
        output_path = artifacts_dir / output_name
        compiler_args.extend(["-S", str(input_path), "-o", str(output_path)])
    else:
        compiler_args.append(str(input_path))

    for arg in case.get("args", []):
        compiler_args.append(normalize_arg(str(arg)))

    generated_root = artifacts_dir / "generated"
    try:
        if input_path.is_dir() and generated_root in input_path.parents:
            compiler_args.extend(["-I", str(input_path.parent)])
    except FileNotFoundError:
        pass

    existing_args = set(compiler_args)
    for root in extra_search_roots_for_case(case, input_path):
        normalized = normalize_arg(root)
        if "-I" not in existing_args or normalized not in existing_args:
            compiler_args.extend(["-I", normalized])
            existing_args.add("-I")
            existing_args.add(normalized)

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
            elif output_path is not None and case.get("output_contains"):
                output_text = load_output_text(output_path)
                for needle in case.get("output_contains", []):
                    if not contains_needle(output_text, needle):
                        ok = False
                        break
        elif case["expect"] == "failure":
            if exit_code == 0:
                ok = False
            for needle in case.get("contains", []):
                if not contains_needle(output, needle):
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
