Vendor packages bundled with Mote.

Import rules:
- Use collection imports under `vendor:...`.
- Current package names are:
  - `@import("vendor:glfw")`
  - `@import("vendor:opengl")`
  - `@import("vendor:raylib")`
  - `@import("vendor:enet")`
  - `@import("vendor:cgltf")`
  - `@import("vendor:miniaudio")`
  - `@import("vendor:stb/image")`
  - `@import("vendor:stb/truetype")`
  - `@import("vendor:stb/easy_font")`

Integration model:
- `glfw`, `raylib`: linked from vendored platform libraries.
- `enet`, `cgltf`, `miniaudio`, `stb/*`: vendored C sources are compiled automatically when the package is imported.
- `opengl`: pure Mote binding plus runtime function loading.

Notes:
- `vendor` bindings are intentionally thin. Some packages also expose small convenience helpers for the most common workflows.
- Smoke tests for vendor packages live under `tests/cases/vendor`.
