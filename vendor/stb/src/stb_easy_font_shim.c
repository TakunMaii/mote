#include "stb_easy_font.h"

int mote_stb_easy_font_width(char *text)
{
    return stb_easy_font_width(text);
}

int mote_stb_easy_font_height(char *text)
{
    return stb_easy_font_height(text);
}

int mote_stb_easy_font_print(float x, float y, char *text, unsigned char color[4], void *vertex_buffer, int vbuf_size)
{
    return stb_easy_font_print(x, y, text, color, vertex_buffer, vbuf_size);
}

void mote_stb_easy_font_spacing(float spacing)
{
    stb_easy_font_spacing(spacing);
}
