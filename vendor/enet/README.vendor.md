# ENet vendor package

This package vendors the official [ENet](https://github.com/lsalzman/enet) C sources and exposes a minimal Mote FFI layer as `@import("vendor:enet")`.

## Layout

- `main.mote`: thin Mote bindings for the most useful public ENet API surface
- `src/enet/*.h`: upstream ENet public headers
- `src/*.c`: upstream ENet implementation files
- `src/LICENSE`: upstream license

## Build model

ENet is compiled from vendored source by the Mote driver when `vendor:enet` is imported.
This keeps the integration portable across macOS, Linux, and Windows without maintaining separate prebuilt archives.

## Notes

- The current binding intentionally covers the core API needed for game and multimedia networking workflows:
  initialization, address resolution, host lifecycle, events, peers, and packets.
- On Windows the driver links the required system libraries `ws2_32` and `winmm`.
