Official cgltf sources vendored for Mote.

Source:
- https://github.com/jkuhlmann/cgltf
- copied via Odin vendor reference, then tracked in-tree as source

Integration model:
- import path: `@import("vendor:cgltf")`
- executable builds compile `vendor/cgltf/src/cgltf.c` and `vendor/cgltf/src/mote_cgltf_shim.c` automatically when the package is imported
- the package exposes:
  - thin direct FFI for the upstream parser entry points
  - a small helper layer for file parsing, scene counts, accessor lookup, and float unpacking

License:
- see `vendor/cgltf/src/LICENSE`
