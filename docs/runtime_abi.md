# Mote Runtime ABI

This document defines the backend-facing runtime ABI that `MIR` assumes.

## Scalars

- `bool`
  - Logical domain: `0` or `1`
  - Storage size: 1 byte
  - Call/return ABI: zero-extended integer with only the low bit semantically relevant
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

- User-visible arguments are passed in source order
- A capturing function body receives a hidden first argument:
  - `__env: *ClosureEnv`
- Non-capturing functions have no hidden environment argument
- `&T` / `&mut T` parameters are lowered as pointers in MIR
- Arrays, structs, enums, bools, chars, and numeric scalars are passed and returned by value at MIR level
- Backend-specific target ABI expansion can happen after MIR, but must preserve the logical layout defined here
