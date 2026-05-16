# Mote Compiler Usage

This document describes how to invoke the current `mote` compiler binary.

## Build

From the repository root:

```powershell
gcc src\main.c -o mote.exe
```

## Command Forms

```text
mote [options] <input.mote>
```

## Options

- `-o <file>`
  - Writes output to an explicit file path
- `-S`
  - Emits LLVM IR and stops before linking
- `-I <dir>`
  - Registers a module search root for `@import("name/...")`
- `-L <dir>`
  - Adds a library search directory for the linker
- `-l<name>`
  - Links against `name`
- `-Wl,<args>`
  - Forwards comma-separated arguments directly to the linker
- `--dump-ast`
  - Prints the AST after parsing and rewriting
- `--dump-mir`
  - Prints the MIR after lowering
- `--help` / `-h`
  - Prints command help

## Behavior

- With no emit flag, the compiler emits an executable.
- With `-S`, the compiler writes a `.ll` file and stops.
- AST and MIR are only printed when `--dump-ast` or `--dump-mir` are passed explicitly.
- When executable linking succeeds, the temporary `.ll` file is removed.
- If `clang` link fails, the compiler keeps the intermediate `.ll` file for debugging.

## Output Naming

- `-S test\basic\simple.mote` defaults to `test\basic\simple.ll`
- `test\basic\simple.mote` defaults to `test\basic\simple.exe` on Windows
- `-o <file>` overrides the inferred output path

## Examples

Build an executable:

```powershell
.\mote.exe test\basic\simple.mote
```

Emit LLVM IR:

```powershell
.\mote.exe -S test\basic\simple.mote
```

Emit LLVM IR to a custom file:

```powershell
.\mote.exe -S test\basic\simple.mote -o out.ll
```

Build an executable with a custom output path:

```powershell
.\mote.exe test\basic\simple.mote -o bin\simple.exe
```

Print AST and MIR while still stopping at LLVM IR:

```powershell
.\mote.exe --dump-ast --dump-mir -S test\basic\simple.mote
```

Build an executable with extra linker arguments:

```powershell
.\mote.exe app.mote -lopengl32 -luser32
```

Compile with the built-in C FFI package:

```powershell
.\mote.exe test\ffi\ffi_main.mote -I lib
```

Compile a package-based multi-file program:

```powershell
.\mote.exe test\multi\package_main.mote -I test\pkg
```

Compile the `notgate` test package:

```powershell
.\mote.exe test\game\notgate_main.mote -I lib -I test\game -L test\game\notgate\build -lraylib -lopengl32 -lgdi32 -lwinmm -luser32 -lshell32 -o test\artifacts\notgate.exe
```

## Import Resolution

- Relative imports like `@import("./foo")` are resolved from the importing file's directory.
- Search-root imports like `@import("c/io")` require `-I <dir>` where `<dir>` contains `c\root.mote` or `c\io.mote`.
- Search roots may point to either a module file tree or a directory containing nested packages with `root.mote`.

## Current String Interop

- A plain string literal currently has type `Array(char, N)`.
- It does not auto-coerce to `*char`.
- For C-style NUL-terminated strings, use an explicit cast:

```mote
c.printf(@as(*char, "hello %d\n"), 42);
```

## Toolchain Requirement

Executable emission currently depends on `clang` being available in `PATH`.
