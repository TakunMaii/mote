# Mote Compiler Usage

This document describes how to invoke the current `mote` compiler binary.

## Build

From the repository root:

```powershell
gcc src\main.c -o mote.exe
```

## Command Forms

```text
mote [--pkg name=path]... <input>
mote [--pkg name=path]... --emit-llvm <input> [output.ll]
mote [--pkg name=path]... --emit-exe <input> [output.exe]
```

## Options

- `--pkg name=path`
  - Registers a package root for `@import("name/...")`
- `--emit-llvm`
  - Emits LLVM IR
- `--emit-exe`
  - Emits LLVM IR, invokes `clang`, and produces an executable
- `--help` / `-h`
  - Prints command help

## Behavior

- With no emit flag, the compiler:
  - Parses modules
  - Runs semantic checks
  - Runs type checks
  - Lowers to MIR
  - Prints AST and MIR for debugging
- With `--emit-llvm`, the compiler writes a `.ll` file.
- With `--emit-exe`, the compiler writes LLVM IR, calls `clang`, and removes the temporary `.ll` file on success.

## Output Naming

- `--emit-llvm test\simple.mote` defaults to `test\simple.ll`
- `--emit-exe test\simple.mote` defaults to `test\simple.exe` on Windows
- You can always override the output path explicitly

## Examples

Inspect AST and MIR only:

```powershell
.\mote.exe test\simple.mote
```

Emit LLVM IR:

```powershell
.\mote.exe --emit-llvm test\simple.mote
```

Emit LLVM IR to a custom file:

```powershell
.\mote.exe --emit-llvm test\simple.mote out.ll
```

Build an executable:

```powershell
.\mote.exe --emit-exe test\simple.mote
```

Build an executable with a custom output path:

```powershell
.\mote.exe --emit-exe test\simple.mote bin\simple.exe
```

Compile with the built-in C FFI package:

```powershell
.\mote.exe --pkg c=lib\c --emit-exe test\ffi_main.mote
```

Compile a package-based multi-file program:

```powershell
.\mote.exe --pkg app=test\pkg\app --emit-exe test\multi\package_main.mote
```

## Import Resolution

- Relative imports like `@import("./foo")` are resolved from the importing file's directory.
- Package imports like `@import("c/io")` require `--pkg c=...`.
- Package roots may point to either a module file tree or a directory containing `root.mote`.

## Toolchain Requirement

`--emit-exe` currently depends on `clang` being available in `PATH`.
