#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>
#include <math.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32)
#include <windows.h>
#include <fileapi.h>
#include <direct.h>
#else
#include <time.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <sched.h>
#endif

typedef int (*MoteThreadEntryFn)(void *env);

typedef struct MoteClosureValue {
    MoteThreadEntryFn code_ptr;
    void *env_ptr;
} MoteClosureValue;

typedef struct MoteThreadHandle {
#if defined(_WIN32)
    HANDLE native_handle;
    DWORD thread_id;
#else
    pthread_t native_handle;
#endif
    atomic_int finished;
    atomic_int owner_state; // 0 = joinable, 1 = detached, 2 = joined
    atomic_int ref_count;
    int exit_code;
} MoteThreadHandle;

static void mote_oom_panic(void);

#if defined(_WIN32)
static DWORD WINAPI mote_thread_trampoline(LPVOID param)
#else
static void *mote_thread_trampoline(void *param)
#endif
{
    MoteThreadHandle *handle = (MoteThreadHandle*) param;
    if(handle == NULL)
    {
#if defined(_WIN32)
        return 0;
#else
        return NULL;
#endif
    }

    MoteClosureValue *closure = (MoteClosureValue*) (((unsigned char*) handle) + sizeof(MoteThreadHandle));
    int result = 0;
    if(closure->code_ptr != NULL)
        result = closure->code_ptr(closure->env_ptr);
    handle->exit_code = result;
    atomic_store(&(handle->finished), 1);
    if(atomic_fetch_sub(&(handle->ref_count), 1) == 1)
        free(handle);

#if defined(_WIN32)
    return (DWORD) result;
#else
    return NULL;
#endif
}

void *mote_stderr_handle(void)
{
    return stderr;
}

void *mote_stdout_handle(void)
{
    return stdout;
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

void mote_panic(const char *ptr, long long len)
{
    fputs("runtime panic: ", stderr);
    if(ptr != NULL && len > 0)
        fwrite(ptr, 1, (size_t) len, stderr);
    fputc('\n', stderr);
    abort();
}

void mote_assert_fail(const char *ptr, long long len)
{
    mote_panic(ptr, len);
}

void mote_debug_begin(const char *file, long long line)
{
    fprintf(stderr, "%s:%lld: ", file != NULL ? file : "<unknown>", line);
}

void mote_debug_sep(void)
{
    fputs(" ; ", stderr);
}

void mote_debug_end(void)
{
    fputc('\n', stderr);
}

void mote_debug_write_cstr(const char *s)
{
    fputs(s != NULL ? s : "<null>", stderr);
}

void mote_debug_write_char(int ch)
{
    fputc(ch, stderr);
}

void mote_debug_write_i64(long long value)
{
    fprintf(stderr, "%lld", value);
}

void mote_debug_write_u64(unsigned long long value)
{
    fprintf(stderr, "%llu", value);
}

void mote_debug_write_f64(double value)
{
    fprintf(stderr, "%.17g", value);
}

void mote_debug_write_ptr(const void *ptr)
{
    fprintf(stderr, "%p", ptr);
}

unsigned long long mote_time_monotonic_ns(void)
{
#if defined(_WIN32)
    static LARGE_INTEGER frequency;
    static int initialized = 0;
    LARGE_INTEGER counter;

    if(!initialized)
    {
        QueryPerformanceFrequency(&frequency);
        initialized = 1;
    }

    QueryPerformanceCounter(&counter);
    return (unsigned long long) ((counter.QuadPart * 1000000000ull) / frequency.QuadPart);
#else
    struct timespec ts;
#if defined(CLOCK_MONOTONIC)
    clock_gettime(CLOCK_MONOTONIC, &ts);
#else
    clock_gettime(CLOCK_REALTIME, &ts);
#endif
    return (unsigned long long) ts.tv_sec * 1000000000ull + (unsigned long long) ts.tv_nsec;
#endif
}

void mote_sleep_ms(long long milliseconds)
{
    if(milliseconds <= 0)
        return;

#if defined(_WIN32)
    Sleep((DWORD) milliseconds);
#else
    struct timespec req;
    req.tv_sec = (time_t) (milliseconds / 1000);
    req.tv_nsec = (long) ((milliseconds % 1000) * 1000000ll);

    for(;;)
    {
        struct timespec rem;
        if(nanosleep(&req, &rem) == 0)
            return;
        if(errno != EINTR)
            return;
        req = rem;
    }
#endif
}

long long mote_thread_current_id(void)
{
#if defined(_WIN32)
    return (long long) GetCurrentThreadId();
#else
    return (long long) (uintptr_t) pthread_self();
#endif
}

void mote_thread_yield(void)
{
#if defined(_WIN32)
    SwitchToThread();
#else
    sched_yield();
#endif
}

void *mote_thread_spawn(void *entry_ptr)
{
    if(entry_ptr == NULL)
        return NULL;

    MoteThreadHandle *handle = (MoteThreadHandle*) malloc(sizeof(MoteThreadHandle) + sizeof(MoteClosureValue));
    if(handle == NULL)
        mote_oom_panic();
    memset(handle, 0, sizeof(MoteThreadHandle) + sizeof(MoteClosureValue));
    atomic_init(&(handle->finished), 0);
    atomic_init(&(handle->owner_state), 0);
    atomic_init(&(handle->ref_count), 2);

    MoteClosureValue *closure = (MoteClosureValue*) (((unsigned char*) handle) + sizeof(MoteThreadHandle));
    memcpy(closure, entry_ptr, sizeof(MoteClosureValue));

#if defined(_WIN32)
    handle->native_handle = CreateThread(NULL, 0, mote_thread_trampoline, handle, 0, &(handle->thread_id));
    if(handle->native_handle == NULL)
    {
        free(handle);
        return NULL;
    }
#else
    if(pthread_create(&(handle->native_handle), NULL, mote_thread_trampoline, handle) != 0)
    {
        free(handle);
        return NULL;
    }
#endif

    return handle;
}

long long mote_thread_join(void *thread_handle, int *out_exit_code)
{
    MoteThreadHandle *handle = (MoteThreadHandle*) thread_handle;
    int expected_state = 0;
    if(handle == NULL)
        return 0;
    if(!atomic_compare_exchange_strong(&(handle->owner_state), &expected_state, 2))
        return 0;

#if defined(_WIN32)
    DWORD wait_result = WaitForSingleObject(handle->native_handle, INFINITE);
    if(wait_result != WAIT_OBJECT_0)
        return 0;
    CloseHandle(handle->native_handle);
#else
    if(pthread_join(handle->native_handle, NULL) != 0)
        return 0;
#endif

    if(out_exit_code != NULL)
        *out_exit_code = handle->exit_code;
    if(atomic_fetch_sub(&(handle->ref_count), 1) == 1)
        free(handle);
    return 1;
}

long long mote_thread_detach(void *thread_handle)
{
    MoteThreadHandle *handle = (MoteThreadHandle*) thread_handle;
    int expected_state = 0;
    if(handle == NULL)
        return 0;
    if(!atomic_compare_exchange_strong(&(handle->owner_state), &expected_state, 1))
        return 0;

#if defined(_WIN32)
    CloseHandle(handle->native_handle);
#else
    if(pthread_detach(handle->native_handle) != 0)
        return 0;
#endif
    if(atomic_fetch_sub(&(handle->ref_count), 1) == 1)
        free(handle);
    return 1;
}

long long mote_directory_create(const char *path)
{
#if defined(_WIN32)
    return _mkdir(path) == 0 ? 1 : 0;
#else
    return mkdir(path, 0777) == 0 ? 1 : 0;
#endif
}

long long mote_directory_remove(const char *path)
{
#if defined(_WIN32)
    return _rmdir(path) == 0 ? 1 : 0;
#else
    return rmdir(path) == 0 ? 1 : 0;
#endif
}

long long mote_directory_exists(const char *path)
{
#if defined(_WIN32)
    DWORD attrs = GetFileAttributesA(path);
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) ? 1 : 0;
#else
    struct stat st;
    if(stat(path, &st) != 0)
        return 0;
    return S_ISDIR(st.st_mode) ? 1 : 0;
#endif
}

long long mote_directory_entry_count(const char *path)
{
    long long count = 0;
#if defined(_WIN32)
    char pattern[1024];
    snprintf(pattern, sizeof(pattern), "%s\\*", path);
    WIN32_FIND_DATAA find_data;
    HANDLE handle = FindFirstFileA(pattern, &find_data);
    if(handle == INVALID_HANDLE_VALUE)
        return -1;

    do
    {
        if(strcmp(find_data.cFileName, ".") == 0 || strcmp(find_data.cFileName, "..") == 0)
            continue;
        count++;
    } while(FindNextFileA(handle, &find_data));

    FindClose(handle);
#else
    DIR *directory = opendir(path);
    if(directory == NULL)
        return -1;

    struct dirent *entry = NULL;
    while((entry = readdir(directory)) != NULL)
    {
        if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        count++;
    }

    closedir(directory);
#endif
    return count;
}

long long mote_directory_list_names(const char *path, char *buffer, long long buffer_size)
{
    long long used = 0;
    bool query_only = buffer == NULL;
    if(buffer_size < 0)
        return -1;
    if(query_only && buffer_size != 0)
        return -1;

#if defined(_WIN32)
    char pattern[1024];
    snprintf(pattern, sizeof(pattern), "%s\\*", path);
    WIN32_FIND_DATAA find_data;
    HANDLE handle = FindFirstFileA(pattern, &find_data);
    if(handle == INVALID_HANDLE_VALUE)
        return -1;

    do
    {
        const char *name = find_data.cFileName;
        if(strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
            continue;
        size_t len = strlen(name);
        if(used + (long long) len + 1 > buffer_size)
        {
            if(!query_only)
            {
                FindClose(handle);
                return -1;
            }
        }
        if(!query_only)
            memcpy(buffer + used, name, len);
        used += (long long) len;
        if(!query_only)
            buffer[used] = '\0';
        used++;
    } while(FindNextFileA(handle, &find_data));

    FindClose(handle);
#else
    DIR *directory = opendir(path);
    if(directory == NULL)
        return -1;

    struct dirent *entry = NULL;
    while((entry = readdir(directory)) != NULL)
    {
        const char *name = entry->d_name;
        if(strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
            continue;
        size_t len = strlen(name);
        if(used + (long long) len + 1 > buffer_size)
        {
            if(!query_only)
            {
                closedir(directory);
                return -1;
            }
        }
        if(!query_only)
            memcpy(buffer + used, name, len);
        used += (long long) len;
        if(!query_only)
            buffer[used] = '\0';
        used++;
    }

    closedir(directory);
#endif
    return used;
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
