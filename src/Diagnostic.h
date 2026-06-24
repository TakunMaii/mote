#ifndef DIAGNOSTIC_H
#define DIAGNOSTIC_H

#include <ctype.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DIAGNOSTIC_MAX_MESSAGE_LENGTH 1024
#define DIAGNOSTIC_MAX_LABEL_LENGTH 512
#define DIAGNOSTIC_MAX_NOTES 8

typedef enum DiagnosticSeverity {
    DIAGNOSTIC_SEVERITY_ERROR,
    DIAGNOSTIC_SEVERITY_NOTE,
    DIAGNOSTIC_SEVERITY_HELP,
} DiagnosticSeverity;

typedef struct SourceSpan {
    const char *filename;
    int start_line;
    int start_column;
    int end_line;
    int end_column;
} SourceSpan;

typedef struct DiagnosticAnnotation {
    DiagnosticSeverity severity;
    SourceSpan span;
    char message[DIAGNOSTIC_MAX_LABEL_LENGTH];
} DiagnosticAnnotation;

typedef struct Diagnostic {
    DiagnosticSeverity severity;
    char code[16];
    char message[DIAGNOSTIC_MAX_MESSAGE_LENGTH];
    SourceSpan primary_span;
    char primary_label[DIAGNOSTIC_MAX_LABEL_LENGTH];
    DiagnosticAnnotation annotations[DIAGNOSTIC_MAX_NOTES];
    int annotation_count;
    char notes[DIAGNOSTIC_MAX_NOTES][DIAGNOSTIC_MAX_MESSAGE_LENGTH];
    int note_count;
} Diagnostic;

typedef struct SourceFileCacheEntry {
    const char *filename;
    char *content;
    struct SourceFileCacheEntry *next;
} SourceFileCacheEntry;

typedef struct DiagnosticTrap {
    jmp_buf jump_buffer;
    Diagnostic diagnostic;
    bool has_diagnostic;
} DiagnosticTrap;

static SourceFileCacheEntry *g_source_file_cache_head = NULL;
static DiagnosticTrap *g_diagnostic_trap = NULL;
static void diagnosticAbortInternal(const char *context, const char *detail);

static SourceSpan makeSourceSpan(const char *filename, int start_line, int start_column, int end_line, int end_column)
{
    SourceSpan span = {0};
    span.filename = filename;
    span.start_line = start_line;
    span.start_column = start_column;
    span.end_line = end_line;
    span.end_column = end_column;
    return span;
}

static SourceSpan makePointSourceSpan(const char *filename, int line, int column)
{
    return makeSourceSpan(filename, line, column, line, column + 1);
}

static int diagnosticDisplayLine(int zero_based_line)
{
    return zero_based_line + 1;
}

static int diagnosticDisplayColumn(int zero_based_column)
{
    return zero_based_column + 1;
}

static int diagnosticCountDigits(int value)
{
    int digits = 1;
    while(value >= 10)
    {
        value /= 10;
        digits++;
    }
    return digits;
}

static const char* diagnosticSeverityToString(DiagnosticSeverity severity)
{
    switch(severity)
    {
        case DIAGNOSTIC_SEVERITY_ERROR: return "error";
        case DIAGNOSTIC_SEVERITY_NOTE: return "note";
        case DIAGNOSTIC_SEVERITY_HELP: return "help";
        default: return "error";
    }
}

static void diagnosticVFormat(char *buffer, size_t buffer_size, const char *format, va_list args)
{
    if(buffer_size == 0)
        return;

    vsnprintf(buffer, buffer_size, format, args);
    buffer[buffer_size - 1] = '\0';
}

static void diagnosticFormat(char *buffer, size_t buffer_size, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    diagnosticVFormat(buffer, buffer_size, format, args);
    va_end(args);
}

static char* diagnosticCloneString(const char *value)
{
    if(value == NULL)
        return NULL;

    size_t length = strlen(value);
    char *copy = (char*) malloc(length + 1);
    if(copy == NULL)
        diagnosticAbortInternal("string allocation failed while building diagnostic context", value);
    memcpy(copy, value, length + 1);
    return copy;
}

static char* diagnosticReadFileContent(const char *filename)
{
    FILE *f = fopen(filename, "rb");
    if(f == NULL)
        return NULL;

    if(fseek(f, 0, SEEK_END) != 0)
    {
        fclose(f);
        return NULL;
    }

    long size = ftell(f);
    if(size < 0)
    {
        fclose(f);
        return NULL;
    }

    rewind(f);
    char *buffer = (char*) malloc((size_t)size + 1);
    if(buffer == NULL)
    {
        fclose(f);
        return NULL;
    }

    size_t read_bytes = fread(buffer, 1, (size_t)size, f);
    fclose(f);
    if(read_bytes != (size_t)size)
    {
        free(buffer);
        return NULL;
    }

    buffer[size] = '\0';
    return buffer;
}

static char* diagnosticGetSourceFileContent(const char *filename)
{
    if(filename == NULL)
        return NULL;

    SourceFileCacheEntry *entry = g_source_file_cache_head;
    while(entry != NULL)
    {
        if(strcmp(entry->filename, filename) == 0)
            return entry->content;
        entry = entry->next;
    }

    char *content = diagnosticReadFileContent(filename);
    if(content == NULL)
        return NULL;

    SourceFileCacheEntry *new_entry = (SourceFileCacheEntry*) malloc(sizeof(SourceFileCacheEntry));
    if(new_entry == NULL)
        return content;

    new_entry->filename = filename;
    new_entry->content = content;
    new_entry->next = g_source_file_cache_head;
    g_source_file_cache_head = new_entry;
    return content;
}

static bool diagnosticGetLineText(const char *filename, int zero_based_line, const char **out_line_start, size_t *out_line_length)
{
    char *content = diagnosticGetSourceFileContent(filename);
    if(content == NULL)
        return false;

    const char *cursor = content;
    int current_line = 0;
    while(*cursor != '\0' && current_line < zero_based_line)
    {
        if(*cursor == '\n')
            current_line++;
        cursor++;
    }

    if(current_line != zero_based_line)
        return false;

    const char *line_start = cursor;
    while(*cursor != '\0' && *cursor != '\n' && *cursor != '\r')
        cursor++;

    *out_line_start = line_start;
    *out_line_length = (size_t)(cursor - line_start);
    return true;
}

static void diagnosticPrintSourceSpan(SourceSpan span, const char *label)
{
    if(span.filename == NULL)
        return;

    printf("  --> %s:%d:%d\n",
           span.filename,
           diagnosticDisplayLine(span.start_line),
           diagnosticDisplayColumn(span.start_column));

    const char *line_text = NULL;
    size_t line_length = 0;
    if(!diagnosticGetLineText(span.filename, span.start_line, &line_text, &line_length))
    {
        if(label != NULL && label[0] != '\0')
            printf("      %s\n", label);
        return;
    }

    int display_line = diagnosticDisplayLine(span.start_line);
    int line_digits = diagnosticCountDigits(display_line);
    printf("%*s |\n", line_digits, "");
    printf("%*d | %.*s\n", line_digits, display_line, (int)line_length, line_text);

    int marker_start = span.start_column;
    int marker_end = span.end_column;
    if(marker_end <= marker_start)
        marker_end = marker_start + 1;

    printf("%*s | ", line_digits, "");
    for(int i = 0; i < marker_start; i++)
    {
        if(i < (int)line_length && line_text[i] == '\t')
            putchar('\t');
        else
            putchar(' ');
    }

    putchar('^');
    for(int i = marker_start + 1; i < marker_end; i++)
        putchar('~');

    if(label != NULL && label[0] != '\0')
        printf(" %s", label);
    printf("\n");
}

static Diagnostic diagnosticMake(DiagnosticSeverity severity, const char *code, SourceSpan span, const char *message)
{
    Diagnostic diagnostic;
    memset(&diagnostic, 0, sizeof(Diagnostic));
    diagnostic.severity = severity;
    diagnostic.primary_span = span;
    if(code != NULL)
        diagnosticFormat(diagnostic.code, sizeof(diagnostic.code), "%s", code);
    if(message != NULL)
        diagnosticFormat(diagnostic.message, sizeof(diagnostic.message), "%s", message);
    return diagnostic;
}

static void diagnosticSetPrimaryLabel(Diagnostic *diagnostic, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    diagnosticVFormat(diagnostic->primary_label, sizeof(diagnostic->primary_label), format, args);
    va_end(args);
}

static void diagnosticAddNote(Diagnostic *diagnostic, const char *format, ...)
{
    if(diagnostic->note_count >= DIAGNOSTIC_MAX_NOTES)
        return;

    va_list args;
    va_start(args, format);
    diagnosticVFormat(diagnostic->notes[diagnostic->note_count], sizeof(diagnostic->notes[diagnostic->note_count]), format, args);
    va_end(args);
    diagnostic->note_count++;
}

static void diagnosticEmit(const Diagnostic *diagnostic)
{
    if(diagnostic->code[0] != '\0')
        printf("%s[%s]: %s\n", diagnosticSeverityToString(diagnostic->severity), diagnostic->code, diagnostic->message);
    else
        printf("%s: %s\n", diagnosticSeverityToString(diagnostic->severity), diagnostic->message);

    if(diagnostic->primary_span.filename != NULL)
        diagnosticPrintSourceSpan(diagnostic->primary_span, diagnostic->primary_label);

    for(int i = 0; i < diagnostic->annotation_count; i++)
    {
        const DiagnosticAnnotation *annotation = &(diagnostic->annotations[i]);
        printf("%s: %s\n", diagnosticSeverityToString(annotation->severity), annotation->message);
        diagnosticPrintSourceSpan(annotation->span, "");
    }

    for(int i = 0; i < diagnostic->note_count; i++)
        printf("note: %s\n", diagnostic->notes[i]);
}

static void diagnosticTrapPush(DiagnosticTrap *trap)
{
    g_diagnostic_trap = trap;
    if(trap != NULL)
        trap->has_diagnostic = false;
}

static void diagnosticTrapPop(DiagnosticTrap *trap)
{
    if(g_diagnostic_trap == trap)
        g_diagnostic_trap = NULL;
}

#if defined(__GNUC__) || defined(__clang__)
#define MOTE_NORETURN __attribute__((noreturn))
#else
#define MOTE_NORETURN
#endif

static MOTE_NORETURN void diagnosticAbort(Diagnostic diagnostic)
{
    if(g_diagnostic_trap != NULL)
    {
        g_diagnostic_trap->diagnostic = diagnostic;
        g_diagnostic_trap->has_diagnostic = true;
        longjmp(g_diagnostic_trap->jump_buffer, 1);
    }

    diagnosticEmit(&diagnostic);
    exit(1);
}

static MOTE_NORETURN void diagnosticAbortSimple(const char *code, const char *message, SourceSpan span, const char *label)
{
    Diagnostic diagnostic = diagnosticMake(DIAGNOSTIC_SEVERITY_ERROR, code, span, message);
    if(label != NULL)
        diagnosticSetPrimaryLabel(&diagnostic, "%s", label);
    diagnosticAbort(diagnostic);
}

static MOTE_NORETURN void diagnosticAbortFormatted(const char *code, SourceSpan span, const char *label, const char *format, ...)
{
    Diagnostic diagnostic = diagnosticMake(DIAGNOSTIC_SEVERITY_ERROR, code, span, "");

    va_list args;
    va_start(args, format);
    diagnosticVFormat(diagnostic.message, sizeof(diagnostic.message), format, args);
    va_end(args);

    if(label != NULL)
        diagnosticSetPrimaryLabel(&diagnostic, "%s", label);
    diagnosticAbort(diagnostic);
}

static MOTE_NORETURN void diagnosticAbortInternal(const char *context, const char *detail)
{
    SourceSpan span = {0};
    Diagnostic diagnostic = diagnosticMake(DIAGNOSTIC_SEVERITY_ERROR, "ICE0001", span, "compiler internal error");
    if(context != NULL && context[0] != '\0')
        diagnosticAddNote(&diagnostic, "%s", context);
    if(detail != NULL && detail[0] != '\0')
        diagnosticAddNote(&diagnostic, "%s", detail);
    diagnosticAbort(diagnostic);
}

#endif /* DIAGNOSTIC_H */
