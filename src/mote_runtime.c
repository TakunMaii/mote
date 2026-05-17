#include <stddef.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

void *mote_stderr_handle(void)
{
    return stderr;
}

float mote_sinf(float x)
{
    return sinf(x);
}

float mote_cosf(float x)
{
    return cosf(x);
}

float mote_sqrtf(float x)
{
    return sqrtf(x);
}

double mote_fabs(double x)
{
    return fabs(x);
}

float mote_fminf(float a, float b)
{
    return fminf(a, b);
}

void mote_unwrap_null_panic(void)
{
    fputs("runtime panic: @unwrap(null)\n", stderr);
    abort();
}
