Official miniaudio sources vendored for Mote.

Source:
- https://github.com/mackron/miniaudio
- copied via Odin vendor reference, then tracked in-tree as source

Integration model:
- `vendor:miniaudio` binds directly against the C API in `src/miniaudio.h`
- executable builds compile `vendor/miniaudio/src/miniaudio.c` automatically when the package is imported

License:
- public domain or MIT-0, per upstream `miniaudio.h`
