#include <stddef.h>
#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void *mote_stderr_handle(void)
{
    return stderr;
}

float mote_sinf(float x)
{
    return sinf(x);
}

double mote_sin(double x)
{
    return sin(x);
}

float mote_cosf(float x)
{
    return cosf(x);
}

double mote_cos(double x)
{
    return cos(x);
}

float mote_sqrtf(float x)
{
    return sqrtf(x);
}

double mote_sqrt(double x)
{
    return sqrt(x);
}

double mote_fabs(double x)
{
    return fabs(x);
}

float mote_tanf(float x)
{
    return tanf(x);
}

double mote_tan(double x)
{
    return tan(x);
}

float mote_asinf(float x)
{
    return asinf(x);
}

double mote_asin(double x)
{
    return asin(x);
}

float mote_acosf(float x)
{
    return acosf(x);
}

double mote_acos(double x)
{
    return acos(x);
}

float mote_atanf(float x)
{
    return atanf(x);
}

double mote_atan(double x)
{
    return atan(x);
}

float mote_atan2f(float y, float x)
{
    return atan2f(y, x);
}

double mote_atan2(double y, double x)
{
    return atan2(y, x);
}

float mote_expf(float x)
{
    return expf(x);
}

double mote_exp(double x)
{
    return exp(x);
}

float mote_logf(float x)
{
    return logf(x);
}

double mote_log(double x)
{
    return log(x);
}

float mote_log2f(float x)
{
    return log2f(x);
}

double mote_log2(double x)
{
    return log2(x);
}

float mote_log10f(float x)
{
    return log10f(x);
}

double mote_log10(double x)
{
    return log10(x);
}

float mote_powf(float x, float y)
{
    return powf(x, y);
}

double mote_pow(double x, double y)
{
    return pow(x, y);
}

float mote_floorf(float x)
{
    return floorf(x);
}

double mote_floor(double x)
{
    return floor(x);
}

float mote_ceilf(float x)
{
    return ceilf(x);
}

double mote_ceil(double x)
{
    return ceil(x);
}

float mote_roundf(float x)
{
    return roundf(x);
}

double mote_round(double x)
{
    return round(x);
}

float mote_truncf(float x)
{
    return truncf(x);
}

double mote_trunc(double x)
{
    return trunc(x);
}

float mote_fminf(float a, float b)
{
    return fminf(a, b);
}

double mote_fmin(double a, double b)
{
    return fmin(a, b);
}

float mote_fmaxf(float a, float b)
{
    return fmaxf(a, b);
}

double mote_fmax(double a, double b)
{
    return fmax(a, b);
}

void mote_unwrap_null_panic(void)
{
    fputs("runtime panic: @unwrap(null)\n", stderr);
    abort();
}

typedef struct MoteArenaChunk {
    struct MoteArenaChunk *next;
    size_t capacity;
    size_t used;
    unsigned char data[];
} MoteArenaChunk;

typedef struct MoteArenaState {
    MoteArenaChunk *head;
    size_t default_chunk_size;
} MoteArenaState;

static size_t mote_align_forward(size_t value, size_t alignment)
{
    if(alignment == 0)
        return value;
    size_t remainder = value % alignment;
    if(remainder == 0)
        return value;
    return value + (alignment - remainder);
}

static void mote_oom_panic(void)
{
    fputs("runtime panic: out of memory\n", stderr);
    abort();
}

void *mote_alloc(size_t size)
{
    void *ptr = malloc(size);
    if(ptr == NULL && size != 0)
        mote_oom_panic();
    return ptr;
}

void *mote_alloc_zeroed(size_t size)
{
    void *ptr = calloc(1, size);
    if(ptr == NULL && size != 0)
        mote_oom_panic();
    return ptr;
}

void *mote_alloc_array(size_t elem_size, size_t count)
{
    if(elem_size != 0 && count > ((size_t)-1) / elem_size)
        mote_oom_panic();
    return mote_alloc(elem_size * count);
}

void *mote_alloc_array_zeroed(size_t elem_size, size_t count)
{
    if(elem_size != 0 && count > ((size_t)-1) / elem_size)
        mote_oom_panic();
    return mote_alloc_zeroed(elem_size * count);
}

void *mote_try_alloc(size_t size)
{
    return malloc(size);
}

void *mote_try_alloc_zeroed(size_t size)
{
    return calloc(1, size);
}

void *mote_try_alloc_array(size_t elem_size, size_t count)
{
    if(elem_size != 0 && count > ((size_t)-1) / elem_size)
        return NULL;
    return malloc(elem_size * count);
}

void *mote_try_alloc_array_zeroed(size_t elem_size, size_t count)
{
    if(elem_size != 0 && count > ((size_t)-1) / elem_size)
        return NULL;
    return calloc(count, elem_size);
}

void mote_dealloc(void *ptr)
{
    free(ptr);
}

void *mote_memdup(const void *src, size_t size)
{
    void *dst = mote_alloc(size);
    if(size != 0)
        memcpy(dst, src, size);
    return dst;
}

void *mote_try_memdup(const void *src, size_t size)
{
    void *dst = mote_try_alloc(size);
    if(dst == NULL)
        return NULL;
    if(size != 0)
        memcpy(dst, src, size);
    return dst;
}

void *mote_arena_create(size_t default_chunk_size)
{
    MoteArenaState *arena = (MoteArenaState*) malloc(sizeof(MoteArenaState));
    if(arena == NULL)
        mote_oom_panic();
    arena->head = NULL;
    arena->default_chunk_size = default_chunk_size == 0 ? 4096 : default_chunk_size;
    return arena;
}

static MoteArenaChunk *mote_arena_new_chunk(size_t capacity)
{
    MoteArenaChunk *chunk = (MoteArenaChunk*) malloc(sizeof(MoteArenaChunk) + capacity);
    if(chunk == NULL)
        mote_oom_panic();
    chunk->next = NULL;
    chunk->capacity = capacity;
    chunk->used = 0;
    return chunk;
}

static void *mote_arena_alloc_internal(MoteArenaState *arena, size_t size, size_t align, int zeroed)
{
    if(arena == NULL)
        mote_oom_panic();

    if(size == 0)
        size = 1;
    if(align == 0)
        align = sizeof(void*);

    MoteArenaChunk *chunk = arena->head;
    size_t offset = 0;
    if(chunk != NULL)
        offset = mote_align_forward(chunk->used, align);

    if(chunk == NULL || offset + size > chunk->capacity)
    {
        size_t chunk_capacity = arena->default_chunk_size;
        size_t needed = size + align;
        if(chunk_capacity < needed)
            chunk_capacity = needed;
        MoteArenaChunk *new_chunk = mote_arena_new_chunk(chunk_capacity);
        new_chunk->next = arena->head;
        arena->head = new_chunk;
        chunk = new_chunk;
        offset = mote_align_forward(chunk->used, align);
    }

    void *result = chunk->data + offset;
    chunk->used = offset + size;
    if(zeroed)
        memset(result, 0, size);
    return result;
}

void *mote_arena_alloc(void *arena_ptr, size_t size, size_t align)
{
    return mote_arena_alloc_internal((MoteArenaState*) arena_ptr, size, align, 0);
}

void *mote_arena_alloc_zeroed(void *arena_ptr, size_t size, size_t align)
{
    return mote_arena_alloc_internal((MoteArenaState*) arena_ptr, size, align, 1);
}

void mote_arena_reset(void *arena_ptr)
{
    MoteArenaState *arena = (MoteArenaState*) arena_ptr;
    if(arena == NULL)
        return;

    MoteArenaChunk *chunk = arena->head;
    while(chunk != NULL)
    {
        chunk->used = 0;
        chunk = chunk->next;
    }
}

void mote_arena_destroy(void *arena_ptr)
{
    MoteArenaState *arena = (MoteArenaState*) arena_ptr;
    if(arena == NULL)
        return;

    MoteArenaChunk *chunk = arena->head;
    while(chunk != NULL)
    {
        MoteArenaChunk *next = chunk->next;
        free(chunk);
        chunk = next;
    }
    free(arena);
}
