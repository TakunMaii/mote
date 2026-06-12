# Package System Redesign TODO

This document tracks the Odin-inspired package redesign for Mote.

## Target Model

- A directory is a package.
- Every `.mote` file must begin with `@package("name");`.
- All `.mote` files in the same directory must declare the same package name.
- `x = @import("package_name");` imports another package by directory/package name.
- Every package directory is also an implicit local package search root for its own files.
  Example: `proj/a.mote` can import `proj/liba/` with `@import("liba")`.
  Nested example: files inside `proj/liba/` can import `proj/liba/libasub/` with `@import("libasub")`.
- Imported package aliases may only be used through member access such as `x.func` or `x.value`.
- Direct member access on an import expression such as `@import("x").func` is rejected.
- Files inside the same package can access each other without explicit import.
- Only `pub` top-level declarations are visible from other packages.
- The compiler builds a package directory, not a single source file.
- Program entry is the `main` function found in the target package.
- CLI changes from `mote <file.mote>` to `mote <dir>`.
- Package collections are supported through `@import("collection:path")`.
- Built-in package collections are `std`, `c`, and `vendor`.
- Only top-level declarations are allowed; top-level executable statements are rejected.
- Top-level declaration order is package-wide and does not matter, but local declaration order inside functions and other nested scopes still matters.
- Cyclic package imports are rejected.

## Current Status

- Implemented:
  - file-level `@package("name");`
  - directory-based package loading
  - package-local visibility with `pub` exports only
  - named package imports and collection imports
  - package-level cycle detection
  - directory-based CLI entry
  - package `main` entry selection
  - std/c/vendor package collection layout
  - harness compatibility layer for legacy single-file tests

## Diagnostics To Keep Covering

- missing package declaration
- malformed `@package`
- mismatched package names within one directory
- non-leading package declaration
- forbidden `@import(...).member`
- private symbol access across packages
- cyclic package imports
- missing package `main` for executable builds

## Execution Model

- Packages are declaration namespaces.
- Entry starts from the selected package `main`.
- Top-level executable statements are not part of the model.

## Remaining Cleanup Ideas

- tighten/expand focused package diagnostics
- add documentation outside `README.md` for package layout and collection usage
- revisit generic/method specialization internals where the current implementation is conservative
