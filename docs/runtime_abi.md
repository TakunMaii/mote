# Mote Runtime ABI

This document defines the backend-facing runtime ABI that `MIR` assumes.

## Scalars

- `bool`
  - Logical domain: `0` or `1`
  - In LLVM SSA, boolean computations use `i1`
  - In memory and native extern ABI positions, it occupies 1 byte
  - Call/return ABI uses a zero-extended integer with only the low bit semantically relevant
- `char`
  - Storage size: 1 byte
  - Semantics: raw code unit, not a Unicode scalar guarantee
- Integer and float primaries
  - Use their declared bit width directly

## Enum

- Runtime representation: signed 32-bit tag
- Stored value: zero-based variant ordinal in declaration order
- Payloads are not supported yet, so an enum is only a tag

## Array

- Runtime representation: inline fixed-size aggregate
- Layout: contiguous element storage, no header, no length field, no implicit terminator
- `Array(T, 0)` occupies zero element slots
- A string literal expression has type `Array(char, N)` where `N` is the source byte length, not including any trailing `\0`

## Struct

- Runtime representation: inline aggregate of data fields only
- Layout order: declaration order of data fields
- Methods are compile-time members only and do not occupy runtime storage

## Closure / Function Value

- A function value uses closure ABI even when it captures nothing
- Runtime representation:
  - `code_ptr`
  - `env_ptr`
- `code_ptr` identifies the lowered MIR function body
- `env_ptr` is null/empty for non-capturing functions
- Capturing closures use an environment object whose fields are laid out in capture order

### Closure Environment Layout

- Capture by value stores the capture payload directly in the environment
- Capture by `&T` stores a pointer to `T`
- Capture by `&mut T` stores a mutable pointer to `T`

## Calling Convention

- User-visible arguments are passed in source order after the hidden environment parameter
- Every internal callable entry point receives a hidden first argument:
  - `__env: *ClosureEnv` logically
- Non-capturing functions are still called through the same ABI shape, with a null environment pointer
- `&T` / `&mut T` parameters are lowered as pointers in MIR
- Arrays, structs, enums, bools, chars, and numeric scalars are passed and returned by value at MIR level
- Backend-specific target ABI expansion can happen after MIR, but must preserve the logical layout defined here

## String Literal To `*char`

- String literals can coerce to `*char` in pointer target contexts for C interop
- `@as(*char, "...")` remains a supported explicit form
- The backend materializes a global NUL-terminated byte buffer and returns a pointer to its first element
- This does not change the default type of a string literal expression, which remains `Array(char, N)`

## Native Extern ABI

These rules describe the current LLVM backend behavior for `@extern(...)` calls and wrappers.

- Scalars and pointers are passed directly
- `enum` lowers as `i32`
- Small aggregate parameters (`array` / `struct`) with total size `1`, `2`, `4`, or `8` bytes are integer-coerced and passed as `i8`, `i16`, `i32`, or `i64`
- Larger aggregate parameters are passed indirectly by pointer
- Small aggregate returns with total size `1`, `2`, `4`, or `8` bytes are integer-coerced on the native ABI boundary and reconstructed on the Mote side
- Larger aggregate returns use an `sret` out-pointer on the native ABI boundary

## Native Extern Variadic Promotion

For variadic `@extern` functions such as `printf`:

- `bool`, `char`, `i8`, `i16`, `u8`, `u16`, and `enum` are promoted to `i32`
- `f32` is promoted to `f64`
- Other argument types are passed with their existing ABI form
