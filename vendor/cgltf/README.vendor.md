Official cgltf sources vendored for Mote.

Source:
- https://github.com/jkuhlmann/cgltf
- copied via Odin vendor reference, then tracked in-tree as source

Integration model:
- `vendor:cgltf` binds a practical subset of the parser API
- executable builds compile `vendor/cgltf/src/cgltf.c` automatically when the package is imported

License:
- see `vendor/cgltf/src/LICENSE`
