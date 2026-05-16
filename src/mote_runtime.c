#include <stddef.h>
#include <stdio.h>

void *mote_stderr_handle(void)
{
    return stderr;
}

int mote_snprintf0(char *buffer, long long size, const char *format)
{
    return snprintf(buffer, (size_t)size, format);
}

int mote_snprintf_i32(char *buffer, long long size, const char *format, int value)
{
    return snprintf(buffer, (size_t)size, format, value);
}

int mote_snprintf_str(char *buffer, long long size, const char *format, const char *value)
{
    return snprintf(buffer, (size_t)size, format, value);
}

int mote_snprintf_f64_i32(char *buffer, long long size, const char *format, double value0, int value1)
{
    return snprintf(buffer, (size_t)size, format, value0, value1);
}

int mote_printf_i32_str(const char *format, int value0, const char *value1)
{
    return printf(format, value0, value1);
}

int mote_fprintf0(void *stream, const char *format)
{
    return fprintf((FILE*)stream, format);
}

int mote_fprintf_str(void *stream, const char *format, const char *value)
{
    return fprintf((FILE*)stream, format, value);
}

int mote_fprintf_char_i32_i32_str(void *stream, const char *format, int value0, int value1, int value2, const char *value3)
{
    return fprintf((FILE*)stream, format, value0, value1, value2, value3);
}
