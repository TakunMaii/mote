# Mote Compiler Usage

This document describes how to invoke the current `mote` compiler binary.

## Build

From the repository root:

```powershell
gcc src\main.c -o mote.exe
```

## Command Forms

```text
mote [options] <dir>
```

## Options

- `-o <file>`
  - Writes output to an explicit file path
- `-S`
  - Emits LLVM IR and stops before linking
- `-I <dir>`
  - Registers a package search root for `@import("pkg")` or `@import("collection:path")`
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
- When emitting an executable, the compiler resolves `runtime/mote_runtime.c` relative to the `mote` executable location, not the caller's current working directory.
- The compiler also auto-registers official package roots relative to the executable location.
- In the current layout, it automatically checks the executable's sibling directory and sibling `lib/` directory.
- That means built-in packages like `std` and `c` should not require users to manually pass `-I`.
- User-facing diagnostics print source names where possible instead of internal package-mangled identifiers.

## Debug Output

- `@debug(x, y, ...)` writes to `stderr`.
- The output format is `file:line: value ; value ; ...`.
- Current debug formatting covers scalars, pointers/references, arrays, slices, optionals, structs, enums, function values, and type values.

Example:

```text
path/to/file.mote:12: Color(Blue) ; Function([v: i32], i32)(code=0x..., env=0x0) ; Type(Pair)
```

## Entry Model

- The compiler builds a package directory, not a single source file.
- The target package must provide a top-level `main` function for executable builds.
- The compiler synthesizes the native entry point and dispatches into that package `main`.
- With `-S`, the compiler may emit LLVM IR for diagnostic or inspection purposes without requiring a runnable entry point.

## Output Naming

- `-S test\basic\simple` defaults to `test\basic\simple.ll`
- `test\basic\simple` defaults to `test\basic\simple.exe` on Windows
- `-o <file>` overrides the inferred output path

## Examples

Build an executable:

```powershell
.\mote.exe test\basic\simple
```

Emit LLVM IR:

```powershell
.\mote.exe -S test\basic\simple
```

Emit LLVM IR to a custom file:

```powershell
.\mote.exe -S test\basic\simple -o out.ll
```

Build an executable with a custom output path:

```powershell
.\mote.exe test\basic\simple -o bin\simple.exe
```

Print AST and MIR while still stopping at LLVM IR:

```powershell
.\mote.exe --dump-ast --dump-mir -S test\basic\simple
```

Build an executable with extra linker arguments:

```powershell
.\mote.exe app -lopengl32 -luser32
```

Compile with the built-in C FFI package:

```powershell
.\mote.exe test\ffi\ffi_main
```

Compile a package-based multi-file program:

```powershell
.\mote.exe test\multi\package_main -I test\pkg
```

Compile the `notgate` test package:

```powershell
.\mote.exe test\game\notgate -I . -o test\artifacts\notgate.exe
```

## Import Resolution

- `@import("pkg")` resolves a package by name.
- `@import("collection:path")` resolves a package inside a collection.
- Built-in package collections are `std`, `c`, and `vendor`.
- The current package directory is also an implicit local package search root for its own child packages.
- Additional package roots can be added with `-I <dir>`.
- Official packages such as `std` and `c` are normally found through the compiler's automatic relative search roots.
- Vendor collection imports such as `@import("vendor:raylib")` or `@import("vendor:opengl")` typically use the repository root as a search root, for example `-I .`.
- For officially vendored native libraries such as `vendor/raylib` and `vendor/glfw`, the compiler also searches prebuilt libraries relative to the compiler executable automatically.
- Direct member access on an import expression such as `@import("std:mem").copy` is rejected; bind the import to a name first.

## Vendor Packages

- `lib/` is reserved for the built-in standard library and core packages such as `std` and `c`.
- Third-party bindings live under `vendor/`.
- Current vendor collections include `vendor:raylib`, `vendor:glfw`, and `vendor:opengl`.
- `vendor/opengl` uses an explicit runtime loading model. After creating a GL context, load common APIs with `gl.LoadWith(glfw.GetProcAddress)`.

## Package Rules

- A directory is a package.
- Every `.mote` file must begin with `@package("name");`.
- All files in the same directory must declare the same package name.
- Only top-level declarations are allowed.
- Files in the same package share visibility automatically.
- Only `pub` top-level declarations are visible across package boundaries.
- Top-level declaration order is package-wide and does not matter.
- Local declaration order inside functions and nested scopes still matters.
- Cyclic package imports are rejected.

## Current String Interop

- A plain string literal currently has type `Array(char, N)`.
- In `*char` / `*mut char` target contexts, it can now coerce implicitly for C-style string interop.
- Explicit `@as(*char, "...")` is still accepted but usually unnecessary.

```mote
c.printf("hello %d\n", 42);
```

## Toolchain Requirement

Executable emission currently depends on `clang` being available in `PATH`.

The distributed compiler must keep `runtime/mote_runtime.c` next to `mote.exe` using that relative layout.
