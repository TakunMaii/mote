Official miniaudio sources vendored for Mote.

Source:
- https://github.com/mackron/miniaudio
- copied via Odin vendor reference, then tracked in-tree as source

Integration model:
- import path: `@import("vendor:miniaudio")`
- executable builds compile `vendor/miniaudio/src/miniaudio.c` and `vendor/miniaudio/src/mote_miniaudio_shim.c` automatically when the package is imported
- the package exposes:
  - version queries
  - a decoder wrapper for file and memory decoding
  - whole-file decode helpers for PCM extraction

License:
- public domain or MIT-0, per upstream `miniaudio.h`
