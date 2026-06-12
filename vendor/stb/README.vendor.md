Official stb sources vendored for Mote.

Source:
- https://github.com/nothings/stb
- `stb_image.h`, `stb_truetype.h`, `stb_easy_font.h`
- Odin vendor layout was used as a reference for package split

Integration model:
- `vendor:stb_image` and `vendor:stb_truetype` compile tiny implementation units automatically
- `vendor:stb_easy_font` uses a small C shim because upstream exposes static functions directly from the header

License:
- public domain or MIT, per upstream stb headers
