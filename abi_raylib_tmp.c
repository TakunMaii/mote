#include "Notgate/libs/raylib/src/raylib.h"
void test_DrawRectangleV(Vector2 a, Vector2 b, Color c) { DrawRectangleV(a, b, c); }
void test_DrawTextureRec(Texture2D t, Rectangle s, Vector2 p, Color c) { DrawTextureRec(t, s, p, c); }
void test_DrawTexturePro(Texture2D t, Rectangle s, Rectangle d, Vector2 o, float r, Color c) { DrawTexturePro(t, s, d, o, r, c); }
Color test_Fade(Color c, float a) { return Fade(c, a); }
bool test_CheckCollisionRecs(Rectangle a, Rectangle b) { return CheckCollisionRecs(a, b); }
Texture2D test_LoadTexture(const char *p) { return LoadTexture(p); }
RenderTexture2D test_LoadRenderTexture(int w, int h) { return LoadRenderTexture(w, h); }
Shader test_LoadShader(const char *vs, const char *fs) { return LoadShader(vs, fs); }
Material test_LoadMaterialDefault(void) { return LoadMaterialDefault(); }
Music test_LoadMusicStream(const char *p) { return LoadMusicStream(p); }
Sound test_LoadSound(const char *p) { return LoadSound(p); }
