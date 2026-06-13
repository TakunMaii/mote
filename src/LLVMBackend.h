#ifndef LLVM_BACKEND_H
#define LLVM_BACKEND_H

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "MIR.h"

typedef struct LLVMDebugBuilder LLVMDebugBuilder;

typedef struct LLVMFunctionEmitContext {
    FILE *stream;
    MirProgram *program;
    MirFunction *function;
    LLVMDebugBuilder *debug;
    int *aliases;
    int temp_counter;
    bool emit_debug_info;
    int debug_subprogram_md;
} LLVMFunctionEmitContext;

typedef struct LLVMDebugFileEntry {
    char *path;
    char *directory;
    char *basename;
    int file_md;
} LLVMDebugFileEntry;

typedef struct LLVMDebugLocationEntry {
    const char *filename;
    int line;
    int column;
    int scope_md;
    int location_md;
} LLVMDebugLocationEntry;

typedef struct LLVMDebugSubprogramEntry {
    const char *filename;
    const char *function_name;
    int line;
    int file_md;
    int subprogram_md;
} LLVMDebugSubprogramEntry;

typedef struct LLVMDebugLexicalBlockEntry {
    int mir_scope_id;
    int parent_scope_md;
    int file_md;
    int line;
    int column;
    int lexical_block_md;
} LLVMDebugLexicalBlockEntry;

typedef struct LLVMDebugLocalEntry {
    const char *filename;
    const char *function_name;
    const char *variable_name;
    int line;
    int scope_md;
    int mir_scope_id;
    int file_md;
    ASTDataType *data_type;
    bool is_parameter;
    int argument_index;
    int local_md;
} LLVMDebugLocalEntry;

typedef struct LLVMDebugTypeEntry {
    char *key;
    char *display_name;
    ASTDataType *data_type;
    int type_md;
    bool is_derived;
} LLVMDebugTypeEntry;

typedef struct LLVMDebugBuilder {
    bool enabled;
    int next_md_id;
    LLVMDebugFileEntry *files;
    int file_count;
    int file_capacity;
    LLVMDebugLocationEntry *locations;
    int location_count;
    int location_capacity;
    LLVMDebugSubprogramEntry *subprograms;
    int subprogram_count;
    int subprogram_capacity;
    LLVMDebugLexicalBlockEntry *lexical_blocks;
    int lexical_block_count;
    int lexical_block_capacity;
    LLVMDebugLocalEntry *locals;
    int local_count;
    int local_capacity;
    LLVMDebugTypeEntry *types;
    int type_count;
    int type_capacity;
    int compile_unit_md;
    int globals_md;
    int imported_entities_md;
    int enum_types_md;
    int retained_types_md;
    int file_enums_md;
    int file_retained_types_md;
    int file_globals_md;
    int file_imported_entities_md;
    int expression_md;
} LLVMDebugBuilder;

typedef enum LLVMExternABIKind {
    LLVM_EXTERN_ABI_DIRECT,
    LLVM_EXTERN_ABI_INTEGER_COERCE,
    LLVM_EXTERN_ABI_INDIRECT_POINTER,
    LLVM_EXTERN_ABI_SRET_POINTER,
} LLVMExternABIKind;

typedef struct LLVMExternABIInfo {
    LLVMExternABIKind kind;
    int integer_bits;
    size_t size;
    size_t align;
} LLVMExternABIInfo;

static bool llvmIsVoidDataType(ASTDataType *data_type)
{
    return data_type != NULL &&
           data_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
           data_type->primary == AST_PRIMARY_DATA_TYPE_VOID;
}

static const char* llvmHostTargetTriple(void)
{
#if defined(_WIN64)
    return "x86_64-pc-windows-msvc";
#elif defined(_WIN32)
    return "i686-pc-windows-msvc";
#elif defined(__APPLE__) && defined(__x86_64__)
    return "x86_64-apple-darwin";
#elif defined(__APPLE__) && defined(__aarch64__)
    return "arm64-apple-darwin";
#elif defined(__linux__) && defined(__x86_64__)
    return "x86_64-pc-linux-gnu";
#elif defined(__linux__) && defined(__aarch64__)
    return "aarch64-pc-linux-gnu";
#else
    return NULL;
#endif
}

static char* llvmDebugCloneRange(const char *start, size_t length)
{
    char *copy = (char*) malloc(length + 1);
    if(copy == NULL)
        diagnosticAbortInternal("llvmDebugCloneRange", "allocation failed");
    memcpy(copy, start, length);
    copy[length] = '\0';
    return copy;
}

static void llvmDebugSplitPath(const char *path, char **directory_out, char **basename_out)
{
    const char *separator = NULL;
    const char *last_backslash = path != NULL ? strrchr(path, '\\') : NULL;
    const char *last_slash = path != NULL ? strrchr(path, '/') : NULL;
    separator = last_backslash;
    if(separator == NULL || (last_slash != NULL && last_slash > separator))
        separator = last_slash;

    if(path == NULL || path[0] == '\0')
    {
        *directory_out = diagnosticCloneString(".");
        *basename_out = diagnosticCloneString("<unknown>");
        return;
    }

    if(separator == NULL)
    {
        *directory_out = diagnosticCloneString(".");
        *basename_out = diagnosticCloneString(path);
        return;
    }

    if(separator == path)
        *directory_out = llvmDebugCloneRange(path, 1);
    else
        *directory_out = llvmDebugCloneRange(path, (size_t) (separator - path));
    *basename_out = diagnosticCloneString(separator + 1);
}

static void llvmDebugEnsureCapacity(void **items, int *capacity, int min_capacity, size_t item_size, const char *label)
{
    if(*capacity >= min_capacity)
        return;
    int next_capacity = *capacity == 0 ? 8 : *capacity;
    while(next_capacity < min_capacity)
        next_capacity *= 2;
    void *grown = realloc(*items, item_size * (size_t) next_capacity);
    if(grown == NULL)
        diagnosticAbortInternal(label, "allocation failed");
    *items = grown;
    *capacity = next_capacity;
}

static int llvmDebugNextMetadataId(LLVMDebugBuilder *debug)
{
    return debug->next_md_id++;
}

static int llvmDebugGetFileMetadata(LLVMDebugBuilder *debug, const char *filename)
{
    if(!debug->enabled)
        return 0;

    const char *effective = (filename != NULL && filename[0] != '\0') ? filename : "<unknown>";
    for(int i = 0; i < debug->file_count; i++)
    {
        if(strcmp(debug->files[i].path, effective) == 0)
            return debug->files[i].file_md;
    }

    llvmDebugEnsureCapacity((void**) &(debug->files), &(debug->file_capacity), debug->file_count + 1,
                            sizeof(LLVMDebugFileEntry), "llvmDebugGetFileMetadata");
    LLVMDebugFileEntry *entry = &(debug->files[debug->file_count++]);
    memset(entry, 0, sizeof(*entry));
    entry->path = diagnosticCloneString(effective);
    llvmDebugSplitPath(effective, &(entry->directory), &(entry->basename));
    entry->file_md = llvmDebugNextMetadataId(debug);
    return entry->file_md;
}

static void llvmDebugInit(LLVMDebugBuilder *debug, bool enabled)
{
    memset(debug, 0, sizeof(*debug));
    debug->enabled = enabled;
    debug->next_md_id = 1;
    if(!enabled)
        return;

    debug->globals_md = llvmDebugNextMetadataId(debug);
    debug->imported_entities_md = llvmDebugNextMetadataId(debug);
    debug->enum_types_md = llvmDebugNextMetadataId(debug);
    debug->retained_types_md = llvmDebugNextMetadataId(debug);
    debug->file_enums_md = llvmDebugNextMetadataId(debug);
    debug->file_retained_types_md = llvmDebugNextMetadataId(debug);
    debug->file_globals_md = llvmDebugNextMetadataId(debug);
    debug->file_imported_entities_md = llvmDebugNextMetadataId(debug);
    debug->expression_md = llvmDebugNextMetadataId(debug);
    debug->compile_unit_md = llvmDebugNextMetadataId(debug);
}

static int llvmDebugCreateSubprogram(LLVMDebugBuilder *debug, const char *filename, const char *function_name, int line)
{
    if(!debug->enabled)
        return 0;
    const char *effective_filename = filename != NULL ? filename : "<unknown>";
    const char *effective_function_name = function_name != NULL ? function_name : "<anon>";
    for(int i = 0; i < debug->subprogram_count; i++)
    {
        LLVMDebugSubprogramEntry *existing = &(debug->subprograms[i]);
        if(existing->line == line &&
           strcmp(existing->filename, effective_filename) == 0 &&
           strcmp(existing->function_name, effective_function_name) == 0)
            return existing->subprogram_md;
    }
    int file_md = llvmDebugGetFileMetadata(debug, effective_filename);
    int subprogram_md = llvmDebugNextMetadataId(debug);
    llvmDebugEnsureCapacity((void**) &(debug->subprograms), &(debug->subprogram_capacity), debug->subprogram_count + 1,
                            sizeof(LLVMDebugSubprogramEntry), "llvmDebugCreateSubprogram");
    LLVMDebugSubprogramEntry *entry = &(debug->subprograms[debug->subprogram_count++]);
    memset(entry, 0, sizeof(*entry));
    entry->filename = diagnosticCloneString(effective_filename);
    entry->function_name = diagnosticCloneString(effective_function_name);
    entry->line = line;
    entry->file_md = file_md;
    entry->subprogram_md = subprogram_md;
    return subprogram_md;
}

static int llvmDebugGetLocationMetadata(LLVMDebugBuilder *debug, const char *filename, int line, int column, int scope_md)
{
    if(!debug->enabled || filename == NULL || line < 0 || column < 0 || scope_md <= 0)
        return 0;

    for(int i = 0; i < debug->location_count; i++)
    {
        LLVMDebugLocationEntry *entry = &(debug->locations[i]);
        if(entry->scope_md == scope_md &&
           entry->line == line + 1 &&
           entry->column == column + 1 &&
           strcmp(entry->filename, filename) == 0)
            return entry->location_md;
    }

    llvmDebugEnsureCapacity((void**) &(debug->locations), &(debug->location_capacity), debug->location_count + 1,
                            sizeof(LLVMDebugLocationEntry), "llvmDebugGetLocationMetadata");
    LLVMDebugLocationEntry *entry = &(debug->locations[debug->location_count++]);
    memset(entry, 0, sizeof(*entry));
    entry->filename = diagnosticCloneString(filename);
    entry->line = line + 1;
    entry->column = column + 1;
    entry->scope_md = scope_md;
    entry->location_md = llvmDebugNextMetadataId(debug);
    return entry->location_md;
}

static int llvmDebugGetScopeMetadata(LLVMDebugBuilder *debug, MirFunction *function, int mir_scope_id, int fallback_scope_md)
{
    if(!debug->enabled || function == NULL || mir_scope_id < 0)
        return fallback_scope_md;

    if(function->debug_scopes == NULL || mir_scope_id >= function->debug_scope_count)
        return fallback_scope_md;

    MirDebugScope *scope = &(function->debug_scopes[mir_scope_id]);
    if(scope->parent_scope_id < 0)
        return fallback_scope_md;

    for(int i = 0; i < debug->lexical_block_count; i++)
    {
        LLVMDebugLexicalBlockEntry *entry = &(debug->lexical_blocks[i]);
        if(entry->mir_scope_id == mir_scope_id)
            return entry->lexical_block_md;
    }

    int parent_scope_md = scope->parent_scope_id >= 0
        ? llvmDebugGetScopeMetadata(debug, function, scope->parent_scope_id, fallback_scope_md)
        : fallback_scope_md;
    int file_md = llvmDebugGetFileMetadata(debug, scope->filename);

    llvmDebugEnsureCapacity((void**) &(debug->lexical_blocks), &(debug->lexical_block_capacity), debug->lexical_block_count + 1,
                            sizeof(LLVMDebugLexicalBlockEntry), "llvmDebugGetScopeMetadata");
    LLVMDebugLexicalBlockEntry *entry = &(debug->lexical_blocks[debug->lexical_block_count++]);
    memset(entry, 0, sizeof(*entry));
    entry->mir_scope_id = mir_scope_id;
    entry->parent_scope_md = parent_scope_md;
    entry->file_md = file_md;
    entry->line = scope->line_number + 1;
    entry->column = scope->column_number + 1;
    entry->lexical_block_md = llvmDebugNextMetadataId(debug);
    return entry->lexical_block_md;
}

static int llvmDebugGetLocalVariableMetadata(LLVMDebugBuilder *debug,
                                             const char *filename,
                                             const char *function_name,
                                             const char *variable_name,
                                             ASTDataType *data_type,
                                             bool is_parameter,
                                             int argument_index,
                                             int line,
                                             int mir_scope_id,
                                             int scope_md)
{
    if(!debug->enabled || filename == NULL || function_name == NULL || variable_name == NULL || scope_md <= 0)
        return 0;

    for(int i = 0; i < debug->local_count; i++)
    {
        LLVMDebugLocalEntry *entry = &(debug->locals[i]);
        if(entry->line == line + 1 &&
           entry->scope_md == scope_md &&
           strcmp(entry->filename, filename) == 0 &&
           strcmp(entry->function_name, function_name) == 0 &&
           strcmp(entry->variable_name, variable_name) == 0)
            return entry->local_md;
    }

    llvmDebugEnsureCapacity((void**) &(debug->locals), &(debug->local_capacity), debug->local_count + 1,
                            sizeof(LLVMDebugLocalEntry), "llvmDebugGetLocalVariableMetadata");
    LLVMDebugLocalEntry *entry = &(debug->locals[debug->local_count++]);
    memset(entry, 0, sizeof(*entry));
    entry->filename = diagnosticCloneString(filename);
    entry->function_name = diagnosticCloneString(function_name);
    entry->variable_name = diagnosticCloneString(variable_name);
    entry->line = line + 1;
    entry->scope_md = scope_md;
    entry->mir_scope_id = mir_scope_id;
    entry->file_md = llvmDebugGetFileMetadata(debug, filename);
    entry->data_type = data_type != NULL ? cloneDataType(data_type) : NULL;
    entry->is_parameter = is_parameter;
    entry->argument_index = argument_index;
    entry->local_md = llvmDebugNextMetadataId(debug);
    return entry->local_md;
}

static MirFunction* llvmFindProgramFunctionByName(MirProgram *program, const char *function_name)
{
    if(program == NULL || function_name == NULL)
        return NULL;
    for(int i = 0; i < program->function_count; i++)
    {
        MirFunction *function = program->functions[i];
        if(function != NULL && strcmp(function->name, function_name) == 0)
            return function;
    }
    return NULL;
}

static const char* llvmDebugPrimaryEncoding(ASTPrimaryDataType primary)
{
    switch(primary)
    {
        case AST_PRIMARY_DATA_TYPE_BOOL: return "DW_ATE_boolean";
        case AST_PRIMARY_DATA_TYPE_F16:
        case AST_PRIMARY_DATA_TYPE_F32:
        case AST_PRIMARY_DATA_TYPE_F64: return "DW_ATE_float";
        case AST_PRIMARY_DATA_TYPE_CHAR:
        case AST_PRIMARY_DATA_TYPE_U8:
        case AST_PRIMARY_DATA_TYPE_U16:
        case AST_PRIMARY_DATA_TYPE_U32:
        case AST_PRIMARY_DATA_TYPE_U64: return "DW_ATE_unsigned";
        case AST_PRIMARY_DATA_TYPE_I8:
        case AST_PRIMARY_DATA_TYPE_I16:
        case AST_PRIMARY_DATA_TYPE_I32:
        case AST_PRIMARY_DATA_TYPE_I64: return "DW_ATE_signed";
        default: return "DW_ATE_unsigned";
    }
}

static int llvmDebugGetTypeMetadata(LLVMDebugBuilder *debug, ASTDataType *data_type)
{
    if(!debug->enabled || data_type == NULL)
        return 0;

    char key[512] = {0};
    appendASTDataTypeString(data_type, key, sizeof(key));
    for(int i = 0; i < debug->type_count; i++)
    {
        if(strcmp(debug->types[i].key, key) == 0)
            return debug->types[i].type_md;
    }

    llvmDebugEnsureCapacity((void**) &(debug->types), &(debug->type_capacity), debug->type_count + 1,
                            sizeof(LLVMDebugTypeEntry), "llvmDebugGetTypeMetadata");
    LLVMDebugTypeEntry *entry = &(debug->types[debug->type_count++]);
    memset(entry, 0, sizeof(*entry));
    entry->key = diagnosticCloneString(key);
    entry->display_name = diagnosticCloneString(key);
    entry->data_type = data_type;
    entry->type_md = llvmDebugNextMetadataId(debug);

    if(data_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
       data_type->primary != AST_PRIMARY_DATA_TYPE_VOID &&
       data_type->primary != AST_PRIMARY_DATA_TYPE_TYPE &&
       data_type->primary != AST_PRIMARY_DATA_TYPE_F8)
    {
        entry->is_derived = false;
        return entry->type_md;
    }

    if(data_type->kind == AST_DATA_TYPE_KIND_POINTER || data_type->kind == AST_DATA_TYPE_KIND_REFERENCE)
    {
        entry->is_derived = true;
        (void) llvmDebugGetTypeMetadata(debug, data_type->child);
        return entry->type_md;
    }

    if(data_type->kind == AST_DATA_TYPE_KIND_FUNCTION)
    {
        entry->is_derived = true;
        return entry->type_md;
    }

    entry->is_derived = false;
    return entry->type_md;
}

static void llvmDebugDispose(LLVMDebugBuilder *debug)
{
    if(debug == NULL)
        return;
    for(int i = 0; i < debug->file_count; i++)
    {
        free(debug->files[i].path);
        free(debug->files[i].directory);
        free(debug->files[i].basename);
    }
    for(int i = 0; i < debug->location_count; i++)
        free((char*) debug->locations[i].filename);
    for(int i = 0; i < debug->subprogram_count; i++)
    {
        free((char*) debug->subprograms[i].filename);
        free((char*) debug->subprograms[i].function_name);
    }
    for(int i = 0; i < debug->local_count; i++)
    {
        free((char*) debug->locals[i].filename);
        free((char*) debug->locals[i].function_name);
        free((char*) debug->locals[i].variable_name);
    }
    for(int i = 0; i < debug->type_count; i++)
    {
        free(debug->types[i].key);
        free(debug->types[i].display_name);
    }
    free(debug->files);
    free(debug->locations);
    free(debug->subprograms);
    free(debug->locals);
    free(debug->types);
    memset(debug, 0, sizeof(*debug));
}

static void llvmBackendError(const char *message, const char *filename, int line, int column)
{
    SourceSpan span = filename != NULL
        ? makePointSourceSpan(filename, line, column)
        : makeSourceSpan(NULL, 0, 0, 0, 0);
    diagnosticAbortSimple("L2001", message, span,
                          filename != NULL ? "LLVM backend failed here" : NULL);
}

static void llvmBackendErrorFormatted(const char *code, const char *filename, int line, int column,
                                      const char *label, const char *format, ...)
{
    SourceSpan span = filename != NULL
        ? makePointSourceSpan(filename, line, column)
        : makeSourceSpan(NULL, 0, 0, 0, 0);
    Diagnostic diagnostic = diagnosticMake(DIAGNOSTIC_SEVERITY_ERROR, code, span, "");
    va_list args;
    va_start(args, format);
    diagnosticVFormat(diagnostic.message, sizeof(diagnostic.message), format, args);
    va_end(args);
    if(label != NULL)
        diagnosticSetPrimaryLabel(&diagnostic, "%s", label);
    diagnosticAbort(diagnostic);
}

static bool llvmIsIntegerLikePrimary(ASTPrimaryDataType primary)
{
    return isIntegerPrimary(primary) || isBoolPrimary(primary);
}

static bool llvmIsBoolDataType(ASTDataType *data_type)
{
    return data_type != NULL &&
           data_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
           data_type->primary == AST_PRIMARY_DATA_TYPE_BOOL;
}

static int llvmIntegerBitWidth(ASTPrimaryDataType primary)
{
    if(primary == AST_PRIMARY_DATA_TYPE_BOOL)
        return 1;
    return getIntegerPrimaryWidth(primary);
}

static bool llvmIsFloatDataType(ASTDataType *data_type)
{
    return data_type != NULL &&
           data_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
           isFloatPrimary(data_type->primary);
}

static bool llvmIsIntegerDataType(ASTDataType *data_type)
{
    return data_type != NULL &&
           data_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
           llvmIsIntegerLikePrimary(data_type->primary);
}

static bool llvmIsSignedIntegerDataType(ASTDataType *data_type)
{
    if(data_type == NULL || data_type->kind != AST_DATA_TYPE_KIND_PRIMARY)
        return false;
    if(data_type->primary == AST_PRIMARY_DATA_TYPE_BOOL || data_type->primary == AST_PRIMARY_DATA_TYPE_CHAR)
        return false;
    return isSignedIntegerPrimary(data_type->primary);
}

static ASTDataType* llvmPointeeType(ASTDataType *data_type)
{
    if(data_type == NULL ||
       (data_type->kind != AST_DATA_TYPE_KIND_POINTER && data_type->kind != AST_DATA_TYPE_KIND_REFERENCE))
        return NULL;
    return data_type->child;
}

static size_t llvmAlignTo(size_t value, size_t alignment)
{
    if(alignment == 0)
        return value;
    size_t remainder = value % alignment;
    if(remainder == 0)
        return value;
    return value + (alignment - remainder);
}

static size_t llvmExternABITypeAlignment(ASTDataType *data_type);

static size_t llvmExternABIPrimaryTypeSize(ASTPrimaryDataType primary)
{
    switch(primary)
    {
        case AST_PRIMARY_DATA_TYPE_VOID: return 0;
        case AST_PRIMARY_DATA_TYPE_BOOL: return 1;
        case AST_PRIMARY_DATA_TYPE_CHAR:
        case AST_PRIMARY_DATA_TYPE_I8:
        case AST_PRIMARY_DATA_TYPE_U8: return 1;
        case AST_PRIMARY_DATA_TYPE_I16:
        case AST_PRIMARY_DATA_TYPE_U16:
        case AST_PRIMARY_DATA_TYPE_F16: return 2;
        case AST_PRIMARY_DATA_TYPE_I32:
        case AST_PRIMARY_DATA_TYPE_U32:
        case AST_PRIMARY_DATA_TYPE_F32: return 4;
        case AST_PRIMARY_DATA_TYPE_I64:
        case AST_PRIMARY_DATA_TYPE_U64:
        case AST_PRIMARY_DATA_TYPE_F64: return 8;
        case AST_PRIMARY_DATA_TYPE_F8: return 1;
        case AST_PRIMARY_DATA_TYPE_TYPE: return sizeof(void*);
    }

    return 0;
}

static size_t llvmExternABIPrimaryTypeAlignment(ASTPrimaryDataType primary)
{
    size_t size = llvmExternABIPrimaryTypeSize(primary);
    if(size == 0)
        return 1;
    if(size > sizeof(void*))
        return sizeof(void*);
    return size;
}

static bool llvmIsExternAggregateType(ASTDataType *data_type)
{
    return data_type != NULL &&
           (data_type->kind == AST_DATA_TYPE_KIND_ARRAY ||
            data_type->kind == AST_DATA_TYPE_KIND_SLICE ||
            data_type->kind == AST_DATA_TYPE_KIND_STRING ||
            data_type->kind == AST_DATA_TYPE_KIND_STRUCT ||
            data_type->kind == AST_DATA_TYPE_KIND_OPTIONAL);
}

static size_t llvmExternABITypeSize(ASTDataType *data_type)
{
    if(data_type == NULL)
        return 0;

    switch(data_type->kind)
    {
        case AST_DATA_TYPE_KIND_PRIMARY:
            return llvmExternABIPrimaryTypeSize(data_type->primary);
        case AST_DATA_TYPE_KIND_POINTER:
        case AST_DATA_TYPE_KIND_REFERENCE:
        case AST_DATA_TYPE_KIND_FUNCTION:
            return sizeof(void*);
        case AST_DATA_TYPE_KIND_OPTIONAL: {
            size_t flag_align = llvmExternABIPrimaryTypeAlignment(AST_PRIMARY_DATA_TYPE_BOOL);
            size_t flag_size = llvmExternABIPrimaryTypeSize(AST_PRIMARY_DATA_TYPE_BOOL);
            size_t child_align = llvmExternABITypeAlignment(data_type->child);
            size_t child_size = llvmExternABITypeSize(data_type->child);
            size_t offset = llvmAlignTo(flag_size, child_align);
            size_t max_align = flag_align > child_align ? flag_align : child_align;
            return llvmAlignTo(offset + child_size, max_align);
        }
        case AST_DATA_TYPE_KIND_ENUM:
            return 4;
        case AST_DATA_TYPE_KIND_ARRAY: {
            size_t child_size = llvmExternABITypeSize(data_type->child);
            return child_size * (size_t) data_type->array_length;
        }
        case AST_DATA_TYPE_KIND_SLICE: {
            size_t ptr_size = sizeof(void*);
            size_t len_align = llvmExternABITypeAlignment(newPrimaryDataType(AST_PRIMARY_DATA_TYPE_I64));
            size_t len_size = llvmExternABITypeSize(newPrimaryDataType(AST_PRIMARY_DATA_TYPE_I64));
            size_t offset = llvmAlignTo(ptr_size, len_align);
            size_t max_align = sizeof(void*) > len_align ? sizeof(void*) : len_align;
            return llvmAlignTo(offset + len_size, max_align);
        }
        case AST_DATA_TYPE_KIND_STRING: {
            size_t ptr_size = sizeof(void*);
            size_t len_align = llvmExternABITypeAlignment(newPrimaryDataType(AST_PRIMARY_DATA_TYPE_I64));
            size_t len_size = llvmExternABITypeSize(newPrimaryDataType(AST_PRIMARY_DATA_TYPE_I64));
            size_t offset = llvmAlignTo(ptr_size, len_align);
            size_t max_align = sizeof(void*) > len_align ? sizeof(void*) : len_align;
            return llvmAlignTo(offset + len_size, max_align);
        }
        case AST_DATA_TYPE_KIND_STRUCT: {
            size_t offset = 0;
            size_t max_align = 1;
            ASTStructMember *member = data_type->members;
            while(member)
            {
                if(member->value == NULL)
                {
                    size_t member_align = llvmExternABITypeAlignment(member->data_type);
                    size_t member_size = llvmExternABITypeSize(member->data_type);
                    offset = llvmAlignTo(offset, member_align);
                    offset += member_size;
                    if(member_align > max_align)
                        max_align = member_align;
                }
                member = member->next;
            }
            return llvmAlignTo(offset, max_align);
        }
        default:
            return 0;
    }
}

static size_t llvmExternABITypeAlignment(ASTDataType *data_type)
{
    if(data_type == NULL)
        return 1;

    switch(data_type->kind)
    {
        case AST_DATA_TYPE_KIND_PRIMARY:
            return llvmExternABIPrimaryTypeAlignment(data_type->primary);
        case AST_DATA_TYPE_KIND_POINTER:
        case AST_DATA_TYPE_KIND_REFERENCE:
        case AST_DATA_TYPE_KIND_FUNCTION:
            return sizeof(void*);
        case AST_DATA_TYPE_KIND_OPTIONAL: {
            size_t flag_align = llvmExternABIPrimaryTypeAlignment(AST_PRIMARY_DATA_TYPE_BOOL);
            size_t child_align = llvmExternABITypeAlignment(data_type->child);
            return flag_align > child_align ? flag_align : child_align;
        }
        case AST_DATA_TYPE_KIND_ENUM:
            return 4;
        case AST_DATA_TYPE_KIND_ARRAY:
            return llvmExternABITypeAlignment(data_type->child);
        case AST_DATA_TYPE_KIND_SLICE: {
            size_t len_align = llvmExternABITypeAlignment(newPrimaryDataType(AST_PRIMARY_DATA_TYPE_I64));
            return sizeof(void*) > len_align ? sizeof(void*) : len_align;
        }
        case AST_DATA_TYPE_KIND_STRING: {
            size_t len_align = llvmExternABITypeAlignment(newPrimaryDataType(AST_PRIMARY_DATA_TYPE_I64));
            return sizeof(void*) > len_align ? sizeof(void*) : len_align;
        }
        case AST_DATA_TYPE_KIND_STRUCT: {
            size_t max_align = 1;
            ASTStructMember *member = data_type->members;
            while(member)
            {
                if(member->value == NULL)
                {
                    size_t member_align = llvmExternABITypeAlignment(member->data_type);
                    if(member_align > max_align)
                        max_align = member_align;
                }
                member = member->next;
            }
            return max_align;
        }
        default:
            return 1;
    }
}

static LLVMExternABIInfo llvmDescribeExternParameterABI(ASTDataType *data_type)
{
    LLVMExternABIInfo info = {0};
    info.kind = LLVM_EXTERN_ABI_DIRECT;
    info.align = llvmExternABITypeAlignment(data_type);
    info.size = llvmExternABITypeSize(data_type);

    if(llvmIsExternAggregateType(data_type))
    {
        if(info.size == 1 || info.size == 2 || info.size == 4 || info.size == 8)
        {
            info.kind = LLVM_EXTERN_ABI_INTEGER_COERCE;
            info.integer_bits = (int) (info.size * 8);
        }
        else
            info.kind = LLVM_EXTERN_ABI_INDIRECT_POINTER;
    }

    return info;
}

static LLVMExternABIInfo llvmDescribeExternReturnABI(ASTDataType *data_type)
{
    LLVMExternABIInfo info = llvmDescribeExternParameterABI(data_type);
    if(llvmIsVoidDataType(data_type))
        info.kind = LLVM_EXTERN_ABI_DIRECT;
    else if(llvmIsExternAggregateType(data_type) &&
            !(info.size == 1 || info.size == 2 || info.size == 4 || info.size == 8))
        info.kind = LLVM_EXTERN_ABI_SRET_POINTER;
    return info;
}

static void llvmEmitType(FILE *stream, ASTDataType *data_type);
static void llvmEmitStorageType(FILE *stream, ASTDataType *data_type);

static void llvmEmitStructRuntimeType(FILE *stream, ASTDataType *data_type, bool storage_mode)
{
    fprintf(stream, "{ ");
    bool need_comma = false;
    ASTStructMember *member = data_type->members;
    while(member)
    {
        if(member->value == NULL)
        {
            if(need_comma)
                fprintf(stream, ", ");
            if(storage_mode)
                llvmEmitStorageType(stream, member->data_type);
            else
                llvmEmitType(stream, member->data_type);
            need_comma = true;
        }
        member = member->next;
    }
    fprintf(stream, " }");
}

static void llvmEmitOptionalRuntimeType(FILE *stream, ASTDataType *data_type, bool storage_mode)
{
    fprintf(stream, "{ ");
    if(storage_mode)
        fprintf(stream, "i8, ");
    else
        fprintf(stream, "i1, ");
    if(storage_mode)
        llvmEmitStorageType(stream, data_type->child);
    else
        llvmEmitType(stream, data_type->child);
    fprintf(stream, " }");
}

static void llvmEmitSliceRuntimeType(FILE *stream)
{
    fprintf(stream, "{ ptr, i64 }");
}

static void llvmEmitType(FILE *stream, ASTDataType *data_type)
{
    if(data_type == NULL)
        llvmBackendError("missing runtime type", NULL, 0, 0);

    switch(data_type->kind)
    {
        case AST_DATA_TYPE_KIND_PRIMARY:
            switch(data_type->primary)
            {
                case AST_PRIMARY_DATA_TYPE_VOID: fprintf(stream, "void"); return;
                case AST_PRIMARY_DATA_TYPE_BOOL: fprintf(stream, "i1"); return;
                case AST_PRIMARY_DATA_TYPE_CHAR: fprintf(stream, "i8"); return;
                case AST_PRIMARY_DATA_TYPE_I8:
                case AST_PRIMARY_DATA_TYPE_U8: fprintf(stream, "i8"); return;
                case AST_PRIMARY_DATA_TYPE_I16:
                case AST_PRIMARY_DATA_TYPE_U16: fprintf(stream, "i16"); return;
                case AST_PRIMARY_DATA_TYPE_I32:
                case AST_PRIMARY_DATA_TYPE_U32: fprintf(stream, "i32"); return;
                case AST_PRIMARY_DATA_TYPE_I64:
                case AST_PRIMARY_DATA_TYPE_U64: fprintf(stream, "i64"); return;
                case AST_PRIMARY_DATA_TYPE_F16: fprintf(stream, "half"); return;
                case AST_PRIMARY_DATA_TYPE_F32: fprintf(stream, "float"); return;
                case AST_PRIMARY_DATA_TYPE_F64: fprintf(stream, "double"); return;
                case AST_PRIMARY_DATA_TYPE_F8:
                    llvmBackendError("f8 is not supported by the textual LLVM backend yet", NULL, 0, 0);
                    return;
                case AST_PRIMARY_DATA_TYPE_TYPE:
                    llvmBackendError("Type values are compile-time only and cannot be lowered to LLVM IR", NULL, 0, 0);
                    return;
            }
            break;
        case AST_DATA_TYPE_KIND_POINTER:
        case AST_DATA_TYPE_KIND_REFERENCE:
            fprintf(stream, "ptr");
            return;
        case AST_DATA_TYPE_KIND_FUNCTION:
            fprintf(stream, "{ ptr, ptr }");
            return;
        case AST_DATA_TYPE_KIND_ARRAY:
            fprintf(stream, "[%lld x ", data_type->array_length);
            llvmEmitStorageType(stream, data_type->child);
            fprintf(stream, "]");
            return;
        case AST_DATA_TYPE_KIND_SLICE:
        case AST_DATA_TYPE_KIND_STRING:
            llvmEmitSliceRuntimeType(stream);
            return;
        case AST_DATA_TYPE_KIND_OPTIONAL:
            llvmEmitOptionalRuntimeType(stream, data_type, true);
            return;
        case AST_DATA_TYPE_KIND_ENUM:
            fprintf(stream, "i32");
            return;
        case AST_DATA_TYPE_KIND_STRUCT:
            llvmEmitStructRuntimeType(stream, data_type, true);
            return;
        default:
            break;
    }

    llvmBackendError("unsupported runtime type shape", NULL, 0, 0);
}

static void llvmEmitStorageType(FILE *stream, ASTDataType *data_type)
{
    if(data_type == NULL)
        llvmBackendError("missing storage type", NULL, 0, 0);

    switch(data_type->kind)
    {
        case AST_DATA_TYPE_KIND_PRIMARY:
            switch(data_type->primary)
            {
                case AST_PRIMARY_DATA_TYPE_VOID: fprintf(stream, "void"); return;
                case AST_PRIMARY_DATA_TYPE_BOOL: fprintf(stream, "i8"); return;
                case AST_PRIMARY_DATA_TYPE_CHAR: fprintf(stream, "i8"); return;
                case AST_PRIMARY_DATA_TYPE_I8:
                case AST_PRIMARY_DATA_TYPE_U8: fprintf(stream, "i8"); return;
                case AST_PRIMARY_DATA_TYPE_I16:
                case AST_PRIMARY_DATA_TYPE_U16: fprintf(stream, "i16"); return;
                case AST_PRIMARY_DATA_TYPE_I32:
                case AST_PRIMARY_DATA_TYPE_U32: fprintf(stream, "i32"); return;
                case AST_PRIMARY_DATA_TYPE_I64:
                case AST_PRIMARY_DATA_TYPE_U64: fprintf(stream, "i64"); return;
                case AST_PRIMARY_DATA_TYPE_F16: fprintf(stream, "half"); return;
                case AST_PRIMARY_DATA_TYPE_F32: fprintf(stream, "float"); return;
                case AST_PRIMARY_DATA_TYPE_F64: fprintf(stream, "double"); return;
                case AST_PRIMARY_DATA_TYPE_F8:
                    llvmBackendError("f8 is not supported by the textual LLVM backend yet", NULL, 0, 0);
                    return;
                case AST_PRIMARY_DATA_TYPE_TYPE:
                    llvmBackendError("Type values are compile-time only and cannot be lowered to LLVM IR", NULL, 0, 0);
                    return;
            }
            break;
        case AST_DATA_TYPE_KIND_POINTER:
        case AST_DATA_TYPE_KIND_REFERENCE:
            fprintf(stream, "ptr");
            return;
        case AST_DATA_TYPE_KIND_FUNCTION:
            fprintf(stream, "{ ptr, ptr }");
            return;
        case AST_DATA_TYPE_KIND_ARRAY:
            fprintf(stream, "[%lld x ", data_type->array_length);
            llvmEmitStorageType(stream, data_type->child);
            fprintf(stream, "]");
            return;
        case AST_DATA_TYPE_KIND_SLICE:
        case AST_DATA_TYPE_KIND_STRING:
            llvmEmitSliceRuntimeType(stream);
            return;
        case AST_DATA_TYPE_KIND_OPTIONAL:
            llvmEmitOptionalRuntimeType(stream, data_type, true);
            return;
        case AST_DATA_TYPE_KIND_ENUM:
            fprintf(stream, "i32");
            return;
        case AST_DATA_TYPE_KIND_STRUCT:
            llvmEmitStructRuntimeType(stream, data_type, true);
            return;
        default:
            break;
    }

    llvmBackendError("unsupported storage type shape", NULL, 0, 0);
}

static void llvmEmitRuntimeParameterType(FILE *stream, ASTDataType *source_type)
{
    if(source_type != NULL && source_type->kind == AST_DATA_TYPE_KIND_REFERENCE)
    {
        fprintf(stream, "ptr");
        return;
    }

    llvmEmitType(stream, source_type);
}

static void llvmMakeTempName(LLVMFunctionEmitContext *context, char *buffer, size_t buffer_size);
static void llvmEmitTempAssignPrefix(FILE *stream, const char *name);
static void llvmEmitValueRef(FILE *stream, LLVMFunctionEmitContext *context, int value_id);
static const char* llvmPrepareStoredValueRef(FILE *stream, LLVMFunctionEmitContext *context, int value_id,
                                             char *buffer, size_t buffer_size);

static void llvmEmitIntegerCoerceType(FILE *stream, int bits)
{
    fprintf(stream, "i%d", bits);
}

static void llvmEmitFunctionReturnABIType(FILE *stream, ASTDataType *data_type)
{
    if(llvmIsBoolDataType(data_type))
        fprintf(stream, "zeroext ");
    llvmEmitType(stream, data_type);
}

static void llvmEmitCallReturnABIType(FILE *stream, ASTDataType *data_type)
{
    llvmEmitFunctionReturnABIType(stream, data_type);
}

static void llvmEmitParameterABIType(FILE *stream, ASTDataType *source_type)
{
    llvmEmitRuntimeParameterType(stream, source_type);
    if(llvmIsBoolDataType(source_type))
        fprintf(stream, " zeroext");
}

static void llvmEmitCallArgumentABIType(FILE *stream, ASTDataType *data_type)
{
    llvmEmitType(stream, data_type);
    if(llvmIsBoolDataType(data_type))
        fprintf(stream, " zeroext");
}

static void llvmEmitNativeExternReturnType(FILE *stream, ASTDataType *data_type)
{
    LLVMExternABIInfo abi = llvmDescribeExternReturnABI(data_type);
    if(abi.kind == LLVM_EXTERN_ABI_SRET_POINTER)
    {
        fprintf(stream, "void");
        return;
    }
    if(abi.kind == LLVM_EXTERN_ABI_INTEGER_COERCE)
    {
        llvmEmitIntegerCoerceType(stream, abi.integer_bits);
        return;
    }

    llvmEmitFunctionReturnABIType(stream, data_type);
}

static void llvmEmitNativeExternParameterType(FILE *stream, ASTDataType *data_type)
{
    LLVMExternABIInfo abi = llvmDescribeExternParameterABI(data_type);
    if(abi.kind == LLVM_EXTERN_ABI_INTEGER_COERCE)
    {
        llvmEmitIntegerCoerceType(stream, abi.integer_bits);
        return;
    }
    if(abi.kind == LLVM_EXTERN_ABI_INDIRECT_POINTER)
    {
        fprintf(stream, "ptr");
        return;
    }

    llvmEmitParameterABIType(stream, data_type);
}

static int llvmResolveAlias(LLVMFunctionEmitContext *context, int value_id)
{
    int current = value_id;
    while(context->aliases[current] != current)
        current = context->aliases[current];
    return current;
}

static ASTDataType* llvmResolvedValueType(LLVMFunctionEmitContext *context, int value_id)
{
    int resolved = llvmResolveAlias(context, value_id);
    return context->function->values[resolved].data_type;
}

static ASTDataType* llvmResolvedFunctionValueType(LLVMFunctionEmitContext *context, int value_id)
{
    ASTDataType *data_type = llvmResolvedValueType(context, value_id);
    if(data_type != NULL &&
       data_type->kind == AST_DATA_TYPE_KIND_FUNCTION)
        return data_type;
    return NULL;
}

static MirInst* llvmFindValueProducer(LLVMFunctionEmitContext *context, int value_id)
{
    int resolved = llvmResolveAlias(context, value_id);
    for(int block_index = 0; block_index < context->function->block_count; block_index++)
    {
        MirBlock *block = &(context->function->blocks[block_index]);
        for(int inst_index = 0; inst_index < block->inst_count; inst_index++)
        {
            MirInst *inst = &(block->insts[inst_index]);
            if(inst->result == resolved)
                return inst;
        }
    }
    return NULL;
}

static void llvmEmitValueRef(FILE *stream, LLVMFunctionEmitContext *context, int value_id)
{
    fprintf(stream, "%%v%d", llvmResolveAlias(context, value_id));
}

static const char* llvmPrepareStoredValueRef(FILE *stream, LLVMFunctionEmitContext *context, int value_id,
                                             char *buffer, size_t buffer_size)
{
    ASTDataType *data_type = llvmResolvedValueType(context, value_id);
    if(!llvmIsBoolDataType(data_type))
    {
        snprintf(buffer, buffer_size, "%%v%d", llvmResolveAlias(context, value_id));
        return buffer;
    }

    llvmMakeTempName(context, buffer, buffer_size);
    llvmEmitTempAssignPrefix(stream, buffer);
    fprintf(stream, "zext i1 ");
    llvmEmitValueRef(stream, context, value_id);
    fprintf(stream, " to i8\n");
    return buffer;
}

static void llvmMakeTempName(LLVMFunctionEmitContext *context, char *buffer, size_t buffer_size)
{
    snprintf(buffer, buffer_size, "%%t%d", context->temp_counter++);
}

static void llvmEmitInstructionPrefix(FILE *stream, int value_id)
{
    fprintf(stream, "    %%v%d = ", value_id);
}

static void llvmEmitTempAssignPrefix(FILE *stream, const char *name)
{
    fprintf(stream, "    %s = ", name);
}

static void llvmEmitDebugLocationSuffixInScope(FILE *stream, LLVMFunctionEmitContext *context,
                                               const char *filename, int line, int column, int mir_scope_id)
{
    if(context == NULL || !context->emit_debug_info || context->debug_subprogram_md <= 0)
        return;
    int scope_md = llvmDebugGetScopeMetadata(context->debug, context->function, mir_scope_id, context->debug_subprogram_md);
    int location_md = llvmDebugGetLocationMetadata(context->debug, filename, line, column,
                                                   scope_md);
    if(location_md > 0)
        fprintf(stream, ", !dbg !%d", location_md);
}

static void llvmEmitDebugLocationSuffix(FILE *stream, LLVMFunctionEmitContext *context, const char *filename, int line, int column)
{
    llvmEmitDebugLocationSuffixInScope(stream, context, filename, line, column, -1);
}

static void llvmEmitDebugMetadata(FILE *stream, LLVMDebugBuilder *debug, MirProgram *program)
{
    if(debug == NULL || !debug->enabled)
        return;

    (void) program;

    fprintf(stream, "!llvm.dbg.cu = !{!%d}\n", debug->compile_unit_md);
    int debug_version_md = llvmDebugNextMetadataId(debug);
    int dwarf_version_md = llvmDebugNextMetadataId(debug);
    fprintf(stream, "!llvm.module.flags = !{!%d, !%d}\n", debug_version_md, dwarf_version_md);
    fprintf(stream, "!%d = !{i32 2, !\"Debug Info Version\", i32 3}\n", debug_version_md);
    fprintf(stream, "!%d = !{i32 7, !\"Dwarf Version\", i32 4}\n", dwarf_version_md);

    for(int i = 0; i < debug->file_count; i++)
    {
        LLVMDebugFileEntry *entry = &(debug->files[i]);
        fprintf(stream, "!%d = !DIFile(filename: \"%s\", directory: \"%s\")\n",
                entry->file_md, entry->basename, entry->directory);
    }

    fprintf(stream, "!%d = !{}\n", debug->globals_md);
    fprintf(stream, "!%d = !{}\n", debug->imported_entities_md);
    fprintf(stream, "!%d = !{}\n", debug->enum_types_md);
    fprintf(stream, "!%d = !{}\n", debug->retained_types_md);
    fprintf(stream, "!%d = !{}\n", debug->file_enums_md);
    fprintf(stream, "!%d = !{}\n", debug->file_retained_types_md);
    fprintf(stream, "!%d = !{}\n", debug->file_globals_md);
    fprintf(stream, "!%d = !{}\n", debug->file_imported_entities_md);
    fprintf(stream, "!%d = !DIExpression()\n", debug->expression_md);

    if(debug->file_count > 0)
    {
        int primary_file_md = debug->files[0].file_md;
        fprintf(stream,
                "!%d = distinct !DICompileUnit(language: DW_LANG_C, file: !%d, producer: \"mote\", isOptimized: false, "
                "runtimeVersion: 0, emissionKind: FullDebug, enums: !%d, retainedTypes: !%d, globals: !%d, imports: !%d)\n",
                debug->compile_unit_md, primary_file_md, debug->enum_types_md, debug->retained_types_md,
                debug->globals_md, debug->imported_entities_md);
    }

    for(int i = 0; i < program->function_count; i++)
    {
        MirFunction *function = program->functions[i];
        if(function == NULL || function->source_function == NULL)
            continue;
        int return_type_md = llvmDebugGetTypeMetadata(debug, function->return_data_type);
        int subroutine_types_md = llvmDebugNextMetadataId(debug);
        int subroutine_type_md = llvmDebugNextMetadataId(debug);
        int file_md = llvmDebugGetFileMetadata(debug, function->source_function->filename);
        int subprogram_md = llvmDebugCreateSubprogram(debug,
                                                      function->source_function->filename,
                                                      function->name,
                                                      function->source_function->line_number + 1);
        fprintf(stream, "!%d = !{", subroutine_types_md);
        if(return_type_md > 0)
            fprintf(stream, "!%d", return_type_md);
        else
            fprintf(stream, "null");
        for(int parameter_index = 0; parameter_index < function->parameter_count; parameter_index++)
        {
            int parameter_type_md = llvmDebugGetTypeMetadata(debug, function->parameters[parameter_index].source_data_type);
            if(parameter_type_md > 0)
                fprintf(stream, ", !%d", parameter_type_md);
            else
                fprintf(stream, ", null");
        }
        fprintf(stream, "}\n");
        fprintf(stream, "!%d = !DISubroutineType(types: !%d)\n", subroutine_type_md, subroutine_types_md);
        fprintf(stream,
                "!%d = distinct !DISubprogram(name: \"%s\", linkageName: \"%s\", scope: !%d, file: !%d, line: %d, "
                "type: !%d, scopeLine: %d, spFlags: DISPFlagDefinition, unit: !%d)\n",
                subprogram_md,
                function->name,
                function->name,
                file_md,
                file_md,
                function->source_function->line_number + 1,
                subroutine_type_md,
                function->source_function->line_number + 1,
                debug->compile_unit_md);
    }

    for(int i = 0; i < debug->local_count; i++)
    {
        LLVMDebugLocalEntry *entry = &(debug->locals[i]);
        int type_md = llvmDebugGetTypeMetadata(debug, entry->data_type);
        MirFunction *scope_function = llvmFindProgramFunctionByName(program, entry->function_name);
        int scope_md = entry->mir_scope_id >= 0
            ? llvmDebugGetScopeMetadata(debug, scope_function, entry->mir_scope_id, entry->scope_md)
            : entry->scope_md;
        if(entry->is_parameter)
            fprintf(stream,
                    "!%d = !DILocalVariable(name: \"%s\", arg: %d, scope: !%d, file: !%d, line: %d, type: !%d)\n",
                    entry->local_md,
                    entry->variable_name,
                    entry->argument_index,
                    scope_md,
                    entry->file_md,
                    entry->line,
                    type_md > 0 ? type_md : debug->compile_unit_md);
        else
            fprintf(stream,
                    "!%d = !DILocalVariable(name: \"%s\", scope: !%d, file: !%d, line: %d, type: !%d)\n",
                    entry->local_md,
                    entry->variable_name,
                    scope_md,
                    entry->file_md,
                    entry->line,
                    type_md > 0 ? type_md : debug->compile_unit_md);
    }

    for(int i = 0; i < debug->type_count; i++)
    {
        LLVMDebugTypeEntry *entry = &(debug->types[i]);
        if(entry->data_type == NULL)
            continue;
        if(entry->data_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
           entry->data_type->primary != AST_PRIMARY_DATA_TYPE_VOID &&
           entry->data_type->primary != AST_PRIMARY_DATA_TYPE_TYPE &&
           entry->data_type->primary != AST_PRIMARY_DATA_TYPE_F8)
        {
            fprintf(stream,
                    "!%d = !DIBasicType(name: \"%s\", size: %zu, encoding: %s)\n",
                    entry->type_md,
                    entry->display_name,
                    llvmExternABIPrimaryTypeSize(entry->data_type->primary) * 8,
                    llvmDebugPrimaryEncoding(entry->data_type->primary));
            continue;
        }
        if(entry->data_type->kind == AST_DATA_TYPE_KIND_POINTER || entry->data_type->kind == AST_DATA_TYPE_KIND_REFERENCE)
        {
            int base_type_md = llvmDebugGetTypeMetadata(debug, entry->data_type->child);
            fprintf(stream,
                    "!%d = !DIDerivedType(tag: DW_TAG_pointer_type, name: \"%s\", baseType: !%d, size: %zu)\n",
                    entry->type_md,
                    entry->display_name,
                    base_type_md,
                    sizeof(void*) * 8);
        }
    }

    for(int i = 0; i < debug->location_count; i++)
    {
        LLVMDebugLocationEntry *entry = &(debug->locations[i]);
        fprintf(stream, "!%d = !DILocation(line: %d, column: %d, scope: !%d)\n",
                entry->location_md, entry->line, entry->column, entry->scope_md);
    }

    for(int i = 0; i < debug->lexical_block_count; i++)
    {
        LLVMDebugLexicalBlockEntry *entry = &(debug->lexical_blocks[i]);
        fprintf(stream, "!%d = distinct !DILexicalBlock(scope: !%d, file: !%d, line: %d, column: %d)\n",
                entry->lexical_block_md,
                entry->parent_scope_md,
                entry->file_md,
                entry->line,
                entry->column);
    }
}

static MirDebugLocal* llvmFindDebugLocalBySlot(LLVMFunctionEmitContext *context, int slot_value_id)
{
    if(context == NULL || context->function == NULL)
        return NULL;
    for(int i = 0; i < context->function->debug_local_count; i++)
    {
        if(context->function->debug_locals[i].slot_value == slot_value_id)
            return &(context->function->debug_locals[i]);
    }
    return NULL;
}

static void llvmEmitDebugDeclare(FILE *stream, LLVMFunctionEmitContext *context, int slot_value_id)
{
    if(context == NULL || !context->emit_debug_info || context->debug_subprogram_md <= 0)
        return;
    MirDebugLocal *local = llvmFindDebugLocalBySlot(context, slot_value_id);
    if(local == NULL || local->filename == NULL)
        return;
    int local_md = llvmDebugGetLocalVariableMetadata(context->debug,
                                                     local->filename,
                                                     context->function->name,
                                                     local->identifier,
                                                     context->function->values[slot_value_id].data_type != NULL
                                                         ? llvmPointeeType(context->function->values[slot_value_id].data_type)
                                                         : NULL,
                                                     local->is_parameter,
                                                     local->argument_index,
                                                     local->line_number,
                                                     local->debug_scope_id,
                                                     context->debug_subprogram_md);
    if(local_md <= 0)
        return;
    fprintf(stream, "    call void @llvm.dbg.declare(metadata ptr %%v%d, metadata !%d, metadata !%d)",
            slot_value_id, local_md, context->debug->expression_md);
    llvmEmitDebugLocationSuffixInScope(stream, context, local->filename, local->line_number, local->column_number,
                                       local->debug_scope_id);
    fprintf(stream, "\n");
}

static void llvmEmitDoubleLiteral(FILE *stream, long double value)
{
    if(isnan((double)value))
    {
        fprintf(stream, "0x7FF8000000000000");
        return;
    }

    if(isinf((double)value))
    {
        if(signbit((double)value))
            fprintf(stream, "0xFFF0000000000000");
        else
            fprintf(stream, "0x7FF0000000000000");
        return;
    }

    fprintf(stream, "%.17e", (double)value);
}

static void llvmEmitFloatConstantInst(FILE *stream, LLVMFunctionEmitContext *context, int result_value_id,
                                      ASTDataType *data_type, long double value,
                                      const char *filename, int line, int column)
{
    if(data_type != NULL &&
       data_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
       (data_type->primary == AST_PRIMARY_DATA_TYPE_F16 ||
        data_type->primary == AST_PRIMARY_DATA_TYPE_F32))
    {
        llvmEmitInstructionPrefix(stream, result_value_id);
        fprintf(stream, "fptrunc double ");
        llvmEmitDoubleLiteral(stream, value);
        fprintf(stream, " to ");
        llvmEmitType(stream, data_type);
        llvmEmitDebugLocationSuffix(stream, context, filename, line, column);
        fprintf(stream, "\n");
        return;
    }

    llvmEmitInstructionPrefix(stream, result_value_id);
    fprintf(stream, "fadd ");
    llvmEmitType(stream, data_type);
    fprintf(stream, " 0.0, ");
    llvmEmitDoubleLiteral(stream, value);
    llvmEmitDebugLocationSuffix(stream, context, filename, line, column);
    fprintf(stream, "\n");
}

static void llvmEmitZeroValueInst(FILE *stream, LLVMFunctionEmitContext *context, int result_value_id, ASTDataType *data_type,
                                  const char *filename, int line, int column)
{
    char slot_name[32];
    llvmMakeTempName(context, slot_name, sizeof(slot_name));

    llvmEmitTempAssignPrefix(stream, slot_name);
    fprintf(stream, "alloca ");
    llvmEmitStorageType(stream, data_type);
    fprintf(stream, "\n");

    fprintf(stream, "    store ");
    llvmEmitStorageType(stream, data_type);
    fprintf(stream, " zeroinitializer, ptr %s\n", slot_name);

    if(llvmIsBoolDataType(data_type))
    {
        char load_name[32];
        llvmMakeTempName(context, load_name, sizeof(load_name));
        llvmEmitTempAssignPrefix(stream, load_name);
        fprintf(stream, "load i8, ptr %s\n", slot_name);
        llvmEmitInstructionPrefix(stream, result_value_id);
        fprintf(stream, "trunc i8 %s to i1", load_name);
        llvmEmitDebugLocationSuffix(stream, context, filename, line, column);
        fprintf(stream, "\n");
    }
    else
    {
        llvmEmitInstructionPrefix(stream, result_value_id);
        fprintf(stream, "load ");
        llvmEmitType(stream, data_type);
        fprintf(stream, ", ptr %s", slot_name);
        llvmEmitDebugLocationSuffix(stream, context, filename, line, column);
        fprintf(stream, "\n");
    }
}

static void llvmEmitConstAllOnes(FILE *stream, ASTDataType *data_type)
{
    if(!llvmIsIntegerDataType(data_type))
        llvmBackendError("bitwise not requires an integer-like type", NULL, 0, 0);

    int width = llvmIntegerBitWidth(data_type->primary);
    if(width == 1)
        fprintf(stream, "true");
    else
        fprintf(stream, "-1");
}

static bool llvmProgramNeedsMalloc(MirProgram *program)
{
    for(int i = 0; i < program->function_count; i++)
    {
        MirFunction *function = program->functions[i];
        for(int block_index = 0; block_index < function->block_count; block_index++)
        {
            MirBlock *block = &(function->blocks[block_index]);
            for(int inst_index = 0; inst_index < block->inst_count; inst_index++)
            {
                MirInst *inst = &(block->insts[inst_index]);
                if(inst->kind == MIR_INST_MAKE_CLOSURE && inst->data.make_closure.captures.count > 0)
                    return true;
            }
        }
    }
    return false;
}

static void llvmEmitEnvAllocation(FILE *stream, LLVMFunctionEmitContext *context, MirInst *inst, const char *env_name)
{
    ASTDataType *env_type = inst->data.make_closure.environment_type;
    char size_ptr_name[32];
    char size_name[32];
    llvmMakeTempName(context, size_ptr_name, sizeof(size_ptr_name));
    llvmMakeTempName(context, size_name, sizeof(size_name));

    llvmEmitTempAssignPrefix(stream, size_ptr_name);
    fprintf(stream, "getelementptr ");
    llvmEmitType(stream, env_type);
    fprintf(stream, ", ptr null, i32 1\n");

    llvmEmitTempAssignPrefix(stream, size_name);
    fprintf(stream, "ptrtoint ptr %s to i64\n", size_ptr_name);

    llvmEmitTempAssignPrefix(stream, env_name);
    fprintf(stream, "call ptr @malloc(i64 %s)\n", size_name);

    ASTStructMember *member = env_type->members;
    int capture_index = 0;
    while(member)
    {
        if(member->value == NULL)
        {
            char field_ptr_name[32];
            llvmMakeTempName(context, field_ptr_name, sizeof(field_ptr_name));

            llvmEmitTempAssignPrefix(stream, field_ptr_name);
            fprintf(stream, "getelementptr ");
            llvmEmitType(stream, env_type);
            fprintf(stream, ", ptr %s, i32 0, i32 %d\n", env_name, capture_index);

            char stored_capture_name[32];
            const char *stored_capture_ref = llvmPrepareStoredValueRef(stream, context,
                                                                       inst->data.make_closure.captures.items[capture_index],
                                                                       stored_capture_name, sizeof(stored_capture_name));
            fprintf(stream, "    store ");
            llvmEmitStorageType(stream, member->data_type);
            fprintf(stream, " ");
            fprintf(stream, "%s", stored_capture_ref);
            fprintf(stream, ", ptr %s\n", field_ptr_name);

            capture_index++;
        }
        member = member->next;
    }
}

static void llvmEmitClosureAggregate(FILE *stream, LLVMFunctionEmitContext *context, int result_value_id,
                                     const char *function_name, const char *env_name,
                                     const char *filename, int line, int column)
{
    char first_name[32];
    llvmMakeTempName(context, first_name, sizeof(first_name));
    llvmEmitTempAssignPrefix(stream, first_name);
    fprintf(stream, "insertvalue { ptr, ptr } undef, ptr @%s, 0\n", function_name);

    llvmEmitInstructionPrefix(stream, result_value_id);
    fprintf(stream, "insertvalue { ptr, ptr } %s, ptr %s, 1", first_name, env_name != NULL ? env_name : "null");
    llvmEmitDebugLocationSuffix(stream, context, filename, line, column);
    fprintf(stream, "\n");
}

static void llvmEmitArrayLiteral(FILE *stream, LLVMFunctionEmitContext *context, MirInst *inst)
{
    if(inst->data.array_literal.elements.count == 0)
    {
        llvmEmitZeroValueInst(stream, context, inst->result, inst->result_type,
                              inst->filename, inst->line_number, inst->column_number);
        return;
    }

    char current_name[32];
    bool has_current = false;
    for(int i = 0; i < inst->data.array_literal.elements.count; i++)
    {
        char stored_element_name[32];
        const char *stored_element_ref = llvmPrepareStoredValueRef(stream, context,
                                                                   inst->data.array_literal.elements.items[i],
                                                                   stored_element_name, sizeof(stored_element_name));
        char next_name[32];
        if(i + 1 == inst->data.array_literal.elements.count)
            llvmEmitInstructionPrefix(stream, inst->result);
        else
        {
            llvmMakeTempName(context, next_name, sizeof(next_name));
            llvmEmitTempAssignPrefix(stream, next_name);
        }

        fprintf(stream, "insertvalue ");
        llvmEmitStorageType(stream, inst->result_type);
        if(!has_current)
            fprintf(stream, " zeroinitializer, ");
        else
            fprintf(stream, " %s, ", current_name);
        llvmEmitStorageType(stream, inst->result_type->child);
        fprintf(stream, " ");
        fprintf(stream, "%s", stored_element_ref);
        fprintf(stream, ", %d", i);
        if(i + 1 == inst->data.array_literal.elements.count)
            llvmEmitDebugLocationSuffixInScope(stream, context, inst->filename, inst->line_number, inst->column_number, inst->debug_scope_id);
        fprintf(stream, "\n");
        if(i + 1 != inst->data.array_literal.elements.count)
            strcpy(current_name, next_name);
        has_current = true;
    }
}

static void llvmEmitStructLiteral(FILE *stream, LLVMFunctionEmitContext *context, MirInst *inst)
{
    int field_count = inst->data.struct_literal.fields.count;
    if(field_count == 0)
    {
        llvmEmitZeroValueInst(stream, context, inst->result, inst->result_type,
                              inst->filename, inst->line_number, inst->column_number);
        return;
    }

    char current_name[32];
    bool has_current = false;
    for(int i = 0; i < field_count; i++)
    {
        MirFieldValue *field = &(inst->data.struct_literal.fields.items[i]);
        ASTDataType *field_type = NULL;
        int field_index = -1;
        if(inst->result_type != NULL && inst->result_type->kind == AST_DATA_TYPE_KIND_OPTIONAL)
        {
            if(i == 0)
            {
                field_type = newPrimaryDataType(AST_PRIMARY_DATA_TYPE_BOOL);
                field_index = 0;
            }
            else if(i == 1)
            {
                field_type = inst->result_type->child;
                field_index = 1;
            }
        }
        else if(inst->result_type != NULL &&
                (inst->result_type->kind == AST_DATA_TYPE_KIND_SLICE ||
                 inst->result_type->kind == AST_DATA_TYPE_KIND_STRING))
        {
            if(strcmp(field->identifier, "ptr") == 0)
            {
                field_type = newWrappedDataType(AST_DATA_TYPE_KIND_POINTER,
                                                cloneDataType(inst->result_type->child));
                field_index = 0;
            }
            else if(strcmp(field->identifier, "len") == 0)
            {
                field_type = newPrimaryDataType(AST_PRIMARY_DATA_TYPE_I64);
                field_index = 1;
            }
        }
        else
        {
            ASTStructMember *member = findStructMember(inst->result_type, field->identifier);
            if(member != NULL)
            {
                field_type = member->data_type;
                field_index = findStructDataFieldIndex(inst->result_type, field->identifier);
            }
        }

        if(field_type == NULL || field_index < 0)
            llvmBackendError("unknown struct literal field in LLVM backend", inst->filename, inst->line_number, inst->column_number);

        char stored_field_name[32];
        const char *stored_field_ref = llvmPrepareStoredValueRef(stream, context, field->value,
                                                                 stored_field_name, sizeof(stored_field_name));
        char next_name[32];
        if(i + 1 == field_count)
            llvmEmitInstructionPrefix(stream, inst->result);
        else
        {
            llvmMakeTempName(context, next_name, sizeof(next_name));
            llvmEmitTempAssignPrefix(stream, next_name);
        }
        fprintf(stream, "insertvalue ");
        llvmEmitStorageType(stream, inst->result_type);
        if(!has_current)
            fprintf(stream, " zeroinitializer, ");
        else
            fprintf(stream, " %s, ", current_name);
        llvmEmitStorageType(stream, field_type);
        fprintf(stream, " ");
        fprintf(stream, "%s", stored_field_ref);
        fprintf(stream, ", %d", field_index);
        if(i + 1 == field_count)
            llvmEmitDebugLocationSuffixInScope(stream, context, inst->filename, inst->line_number, inst->column_number, inst->debug_scope_id);
        fprintf(stream, "\n");
        if(i + 1 != field_count)
            strcpy(current_name, next_name);
        has_current = true;
    }
}

static void llvmEmitFunctionSignature(FILE *stream, MirFunction *function)
{
    llvmEmitFunctionReturnABIType(stream, function->return_data_type);
    fprintf(stream, " @%s(ptr ", function->name);
    if(function->closure_env_type != NULL)
        fprintf(stream, "%%v%d", function->closure_env_input);
    else
        fprintf(stream, "%%env");

    for(int i = 0; i < function->parameter_count; i++)
    {
        fprintf(stream, ", ");
        llvmEmitParameterABIType(stream, function->parameters[i].runtime_data_type);
        fprintf(stream, " %%v%d", function->parameters[i].input_value);
    }

    fprintf(stream, ")");
}

static void llvmEmitNativeExternSignature(FILE *stream, const char *symbol_name, ASTDataType *function_type)
{
    LLVMExternABIInfo return_abi = llvmDescribeExternReturnABI(function_type->return_data_type);
    llvmEmitNativeExternReturnType(stream, function_type->return_data_type);
    fprintf(stream, " @%s(", symbol_name);

    ASTFunctionParameter *parameter = function_type->parameters;
    bool need_comma = false;
    if(return_abi.kind == LLVM_EXTERN_ABI_SRET_POINTER)
    {
        fprintf(stream, "ptr dead_on_unwind writable sret(");
        llvmEmitType(stream, function_type->return_data_type);
        fprintf(stream, ") align %zu", return_abi.align);
        need_comma = true;
    }
    while(parameter)
    {
        if(need_comma)
            fprintf(stream, ", ");
        llvmEmitNativeExternParameterType(stream, parameter->data_type);
        need_comma = true;
        parameter = parameter->next;
    }

    if(function_type->is_variadic)
    {
        if(need_comma)
            fprintf(stream, ", ");
        fprintf(stream, "...");
    }

    fprintf(stream, ")");
}

static void llvmEmitNativeExternCallTarget(FILE *stream, const char *symbol_name, ASTDataType *function_type)
{
    (void) function_type;
    fprintf(stream, " @%s", symbol_name);
}

static void llvmEmitDynamicFunctionPointerSignature(FILE *stream, ASTDataType *function_type)
{
    fprintf(stream, "(");

    ASTFunctionParameter *parameter = function_type->parameters;
    bool need_comma = false;
    while(parameter)
    {
        if(need_comma)
            fprintf(stream, ", ");
        llvmEmitType(stream, parameter->data_type);
        need_comma = true;
        parameter = parameter->next;
    }

    fprintf(stream, ")");
}

static bool llvmExternSymbolIsNoReturn(const char *symbol_name)
{
    return symbol_name != NULL && strcmp(symbol_name, "mote_unwrap_null_panic") == 0;
}

static bool llvmProgramHasExternSymbol(MirProgram *program, const char *symbol_name)
{
    for(int i = 0; i < program->extern_function_count; i++)
    {
        if(strcmp(program->extern_functions[i].symbol_name, symbol_name) == 0)
            return true;
    }
    return false;
}

static MirExternFunction* llvmFindExternFunction(MirProgram *program, const char *symbol_name)
{
    for(int i = 0; i < program->extern_function_count; i++)
    {
        MirExternFunction *extern_function = &(program->extern_functions[i]);
        if(strcmp(extern_function->symbol_name, symbol_name) == 0 ||
           strcmp(extern_function->wrapper_name, symbol_name) == 0)
            return extern_function;
    }
    return NULL;
}

static void llvmEmitExternDeclarations(FILE *stream, MirProgram *program)
{
    for(int i = 0; i < program->extern_function_count; i++)
    {
        MirExternFunction *extern_function = &(program->extern_functions[i]);
        if(extern_function->kind != MIR_EXTERN_FUNCTION_NATIVE)
            continue;
        fprintf(stream, "declare ");
        llvmEmitNativeExternSignature(stream, extern_function->symbol_name, extern_function->function_type);
        if(llvmExternSymbolIsNoReturn(extern_function->symbol_name))
            fprintf(stream, " noreturn");
        fprintf(stream, "\n");
    }
}

static void llvmEmitExternWrapperDefinition(FILE *stream, MirExternFunction *extern_function)
{
    ASTDataType *function_type = extern_function->function_type;
    if(extern_function->is_direct)
        return;

    LLVMExternABIInfo return_abi = llvmDescribeExternReturnABI(function_type->return_data_type);

    fprintf(stream, "define ");
    llvmEmitFunctionReturnABIType(stream, function_type->return_data_type);
    fprintf(stream, " @%s(ptr %%env", extern_function->wrapper_name);

    ASTFunctionParameter *parameter = function_type->parameters;
    int parameter_index = 0;
    while(parameter)
    {
        fprintf(stream, ", ");
        llvmEmitParameterABIType(stream, parameter->data_type);
        fprintf(stream, " %%arg%d", parameter_index++);
        parameter = parameter->next;
    }

    fprintf(stream, ") {\n");
    fprintf(stream, "entry:\n");

    if(return_abi.kind == LLVM_EXTERN_ABI_SRET_POINTER)
    {
        fprintf(stream, "    %%ret_slot = alloca ");
        llvmEmitType(stream, function_type->return_data_type);
        fprintf(stream, "\n");
    }

    parameter = function_type->parameters;
    parameter_index = 0;
    while(parameter)
    {
        LLVMExternABIInfo parameter_abi = llvmDescribeExternParameterABI(parameter->data_type);
        if(parameter_abi.kind == LLVM_EXTERN_ABI_INTEGER_COERCE ||
           parameter_abi.kind == LLVM_EXTERN_ABI_INDIRECT_POINTER)
        {
            fprintf(stream, "    %%arg%d.slot = alloca ", parameter_index);
            llvmEmitType(stream, parameter->data_type);
            fprintf(stream, "\n");
            fprintf(stream, "    store ");
            llvmEmitType(stream, parameter->data_type);
            fprintf(stream, " %%arg%d, ptr %%arg%d.slot\n", parameter_index, parameter_index);
            if(parameter_abi.kind == LLVM_EXTERN_ABI_INTEGER_COERCE)
            {
                fprintf(stream, "    %%arg%d.coerce = load ", parameter_index);
                llvmEmitIntegerCoerceType(stream, parameter_abi.integer_bits);
                fprintf(stream, ", ptr %%arg%d.slot\n", parameter_index);
            }
        }
        parameter_index++;
        parameter = parameter->next;
    }

    if(extern_function->kind == MIR_EXTERN_FUNCTION_DYNAMIC_POINTER)
    {
        fprintf(stream, "    %%fn_ptr_slot = getelementptr { ptr }, ptr %%env, i32 0, i32 0\n");
        fprintf(stream, "    %%fn_ptr = load ptr, ptr %%fn_ptr_slot\n");
    }

    bool is_noreturn = llvmExternSymbolIsNoReturn(extern_function->symbol_name);

    if(llvmIsVoidDataType(function_type->return_data_type) || return_abi.kind == LLVM_EXTERN_ABI_SRET_POINTER)
        fprintf(stream, "    call ");
    else
        fprintf(stream, "    %%ret = call ");

    llvmEmitNativeExternReturnType(stream, function_type->return_data_type);
    if(extern_function->kind == MIR_EXTERN_FUNCTION_DYNAMIC_POINTER)
    {
        fprintf(stream, " ");
        llvmEmitDynamicFunctionPointerSignature(stream, function_type);
        fprintf(stream, " %%fn_ptr");
    }
    else
        llvmEmitNativeExternCallTarget(stream, extern_function->symbol_name, function_type);
    fprintf(stream, "(");

    bool need_comma = false;
    if(return_abi.kind == LLVM_EXTERN_ABI_SRET_POINTER)
    {
        fprintf(stream, "ptr sret(");
        llvmEmitType(stream, function_type->return_data_type);
        fprintf(stream, ") align %zu %%ret_slot", return_abi.align);
        need_comma = true;
    }

    parameter = function_type->parameters;
    parameter_index = 0;
    while(parameter)
    {
        if(need_comma)
            fprintf(stream, ", ");
        LLVMExternABIInfo parameter_abi = llvmDescribeExternParameterABI(parameter->data_type);
        llvmEmitNativeExternParameterType(stream, parameter->data_type);
        fprintf(stream, " ");
        if(parameter_abi.kind == LLVM_EXTERN_ABI_INTEGER_COERCE)
            fprintf(stream, "%%arg%d.coerce", parameter_index);
        else if(parameter_abi.kind == LLVM_EXTERN_ABI_INDIRECT_POINTER)
            fprintf(stream, "%%arg%d.slot", parameter_index);
        else
            fprintf(stream, "%%arg%d", parameter_index);
        need_comma = true;
        parameter_index++;
        parameter = parameter->next;
    }

    fprintf(stream, ")\n");

    if(is_noreturn)
        fprintf(stream, "    unreachable\n");
    else if(return_abi.kind == LLVM_EXTERN_ABI_SRET_POINTER)
    {
        fprintf(stream, "    %%ret = load ");
        llvmEmitType(stream, function_type->return_data_type);
        fprintf(stream, ", ptr %%ret_slot\n");
        fprintf(stream, "    ret ");
        llvmEmitType(stream, function_type->return_data_type);
        fprintf(stream, " %%ret\n");
    }
    else if(llvmIsVoidDataType(function_type->return_data_type))
        fprintf(stream, "    ret void\n");
    else if(return_abi.kind == LLVM_EXTERN_ABI_INTEGER_COERCE)
    {
        fprintf(stream, "    %%ret.slot = alloca ");
        llvmEmitType(stream, function_type->return_data_type);
        fprintf(stream, "\n");
        fprintf(stream, "    store ");
        llvmEmitIntegerCoerceType(stream, return_abi.integer_bits);
        fprintf(stream, " %%ret, ptr %%ret.slot\n");
        fprintf(stream, "    %%ret.value = load ");
        llvmEmitType(stream, function_type->return_data_type);
        fprintf(stream, ", ptr %%ret.slot\n");
        fprintf(stream, "    ret ");
        llvmEmitType(stream, function_type->return_data_type);
        fprintf(stream, " %%ret.value\n");
    }
    else
    {
        fprintf(stream, "    ret ");
        llvmEmitType(stream, function_type->return_data_type);
        fprintf(stream, " %%ret\n");
    }

    fprintf(stream, "}\n\n");
}

static void llvmEmitExternWrapperDefinitions(FILE *stream, MirProgram *program)
{
    for(int i = 0; i < program->extern_function_count; i++)
        llvmEmitExternWrapperDefinition(stream, &(program->extern_functions[i]));
}

static void llvmEmitBinaryInst(FILE *stream, LLVMFunctionEmitContext *context, MirInst *inst, const char *float_op,
                               const char *signed_op, const char *unsigned_op)
{
    ASTDataType *operand_type = llvmResolvedValueType(context, inst->data.binary.lhs);
    llvmEmitInstructionPrefix(stream, inst->result);

    if(llvmIsFloatDataType(operand_type))
        fprintf(stream, "%s ", float_op);
    else if(llvmIsSignedIntegerDataType(operand_type))
        fprintf(stream, "%s ", signed_op);
    else
        fprintf(stream, "%s ", unsigned_op);

    llvmEmitType(stream, operand_type);
    fprintf(stream, " ");
    llvmEmitValueRef(stream, context, inst->data.binary.lhs);
    fprintf(stream, ", ");
    llvmEmitValueRef(stream, context, inst->data.binary.rhs);
    llvmEmitDebugLocationSuffixInScope(stream, context, inst->filename, inst->line_number, inst->column_number, inst->debug_scope_id);
    fprintf(stream, "\n");
}

static void llvmEmitCompareInst(FILE *stream, LLVMFunctionEmitContext *context, MirInst *inst, const char *float_op,
                                const char *signed_op, const char *unsigned_op)
{
    ASTDataType *operand_type = llvmResolvedValueType(context, inst->data.binary.lhs);

    if(operand_type != NULL && operand_type->kind == AST_DATA_TYPE_KIND_FUNCTION)
    {
        char lhs_code_name[32];
        char rhs_code_name[32];
        llvmMakeTempName(context, lhs_code_name, sizeof(lhs_code_name));
        llvmMakeTempName(context, rhs_code_name, sizeof(rhs_code_name));

        llvmEmitTempAssignPrefix(stream, lhs_code_name);
        fprintf(stream, "extractvalue { ptr, ptr } ");
        llvmEmitValueRef(stream, context, inst->data.binary.lhs);
        fprintf(stream, ", 0\n");

        llvmEmitTempAssignPrefix(stream, rhs_code_name);
        fprintf(stream, "extractvalue { ptr, ptr } ");
        llvmEmitValueRef(stream, context, inst->data.binary.rhs);
        fprintf(stream, ", 0\n");

        llvmEmitInstructionPrefix(stream, inst->result);
        fprintf(stream, "icmp %s ptr %s, %s\n", unsigned_op, lhs_code_name, rhs_code_name);
        return;
    }

    llvmEmitInstructionPrefix(stream, inst->result);

    if(llvmIsFloatDataType(operand_type))
        fprintf(stream, "fcmp %s ", float_op);
    else if(llvmIsSignedIntegerDataType(operand_type))
        fprintf(stream, "icmp %s ", signed_op);
    else
        fprintf(stream, "icmp %s ", unsigned_op);

    llvmEmitType(stream, operand_type);
    fprintf(stream, " ");
    llvmEmitValueRef(stream, context, inst->data.binary.lhs);
    fprintf(stream, ", ");
    llvmEmitValueRef(stream, context, inst->data.binary.rhs);
    llvmEmitDebugLocationSuffixInScope(stream, context, inst->filename, inst->line_number, inst->column_number, inst->debug_scope_id);
    fprintf(stream, "\n");
}

static void llvmEmitConvertInst(FILE *stream, LLVMFunctionEmitContext *context, MirInst *inst)
{
    ASTDataType *source_type = llvmResolvedValueType(context, inst->data.convert.operand);
    ASTDataType *target_type = inst->data.convert.target_type;

    if(source_type->kind == AST_DATA_TYPE_KIND_POINTER &&
       source_type->child != NULL &&
       source_type->child->kind == AST_DATA_TYPE_KIND_ARRAY &&
       source_type->child->child != NULL &&
       source_type->child->child->kind == AST_DATA_TYPE_KIND_PRIMARY &&
       source_type->child->child->primary == AST_PRIMARY_DATA_TYPE_CHAR &&
       target_type->kind == AST_DATA_TYPE_KIND_POINTER &&
       target_type->child != NULL &&
       target_type->child->kind == AST_DATA_TYPE_KIND_PRIMARY &&
       target_type->child->primary == AST_PRIMARY_DATA_TYPE_CHAR)
    {
        llvmEmitInstructionPrefix(stream, inst->result);
        fprintf(stream, "getelementptr ");
        llvmEmitStorageType(stream, source_type->child);
        fprintf(stream, ", ptr ");
        llvmEmitValueRef(stream, context, inst->data.convert.operand);
        fprintf(stream, ", i32 0, i32 0\n");
        return;
    }

    if(source_type->kind == AST_DATA_TYPE_KIND_POINTER || source_type->kind == AST_DATA_TYPE_KIND_REFERENCE)
    {
        context->aliases[inst->result] = llvmResolveAlias(context, inst->data.convert.operand);
        return;
    }

    if((source_type->kind == AST_DATA_TYPE_KIND_SLICE || source_type->kind == AST_DATA_TYPE_KIND_STRING) &&
       (target_type->kind == AST_DATA_TYPE_KIND_SLICE || target_type->kind == AST_DATA_TYPE_KIND_STRING))
    {
        context->aliases[inst->result] = llvmResolveAlias(context, inst->data.convert.operand);
        return;
    }

    if(source_type->kind == AST_DATA_TYPE_KIND_PRIMARY && target_type->kind == AST_DATA_TYPE_KIND_PRIMARY)
    {
        if(source_type->primary == target_type->primary)
        {
            context->aliases[inst->result] = llvmResolveAlias(context, inst->data.convert.operand);
            return;
        }

        if(llvmIsIntegerDataType(source_type) && llvmIsIntegerDataType(target_type))
        {
            int src_bits = llvmIntegerBitWidth(source_type->primary);
            int dst_bits = llvmIntegerBitWidth(target_type->primary);
            if(src_bits == dst_bits)
            {
                context->aliases[inst->result] = llvmResolveAlias(context, inst->data.convert.operand);
                return;
            }
            llvmEmitInstructionPrefix(stream, inst->result);
            if(src_bits < dst_bits)
                fprintf(stream, "%s ", llvmIsSignedIntegerDataType(source_type) ? "sext" : "zext");
            else
                fprintf(stream, "trunc ");
            llvmEmitType(stream, source_type);
            fprintf(stream, " ");
            llvmEmitValueRef(stream, context, inst->data.convert.operand);
            fprintf(stream, " to ");
            llvmEmitType(stream, target_type);
            fprintf(stream, "\n");
            return;
        }

        if(llvmIsFloatDataType(source_type) && llvmIsFloatDataType(target_type))
        {
            int src_bits = getFloatPrimaryWidth(source_type->primary);
            int dst_bits = getFloatPrimaryWidth(target_type->primary);
            llvmEmitInstructionPrefix(stream, inst->result);
            fprintf(stream, "%s ", src_bits < dst_bits ? "fpext" : "fptrunc");
            llvmEmitType(stream, source_type);
            fprintf(stream, " ");
            llvmEmitValueRef(stream, context, inst->data.convert.operand);
            fprintf(stream, " to ");
            llvmEmitType(stream, target_type);
            fprintf(stream, "\n");
            return;
        }

        if(llvmIsIntegerDataType(source_type) && llvmIsFloatDataType(target_type))
        {
            llvmEmitInstructionPrefix(stream, inst->result);
            fprintf(stream, "%s ", llvmIsSignedIntegerDataType(source_type) ? "sitofp" : "uitofp");
            llvmEmitType(stream, source_type);
            fprintf(stream, " ");
            llvmEmitValueRef(stream, context, inst->data.convert.operand);
            fprintf(stream, " to ");
            llvmEmitType(stream, target_type);
            fprintf(stream, "\n");
            return;
        }

        if(llvmIsFloatDataType(source_type) && llvmIsIntegerDataType(target_type))
        {
            llvmEmitInstructionPrefix(stream, inst->result);
            fprintf(stream, "%s ", llvmIsSignedIntegerDataType(target_type) ? "fptosi" : "fptoui");
            llvmEmitType(stream, source_type);
            fprintf(stream, " ");
            llvmEmitValueRef(stream, context, inst->data.convert.operand);
            fprintf(stream, " to ");
            llvmEmitType(stream, target_type);
            fprintf(stream, "\n");
            return;
        }
    }

    llvmBackendError("unsupported conversion in textual LLVM backend", inst->filename, inst->line_number, inst->column_number);
}

static void llvmEmitInst(FILE *stream, LLVMFunctionEmitContext *context, MirInst *inst)
{
    switch(inst->kind)
    {
        case MIR_INST_ZERO:
            llvmEmitZeroValueInst(stream, context, inst->result, inst->result_type,
                                  inst->filename, inst->line_number, inst->column_number);
            return;
        case MIR_INST_CONST_BOOL:
            llvmEmitInstructionPrefix(stream, inst->result);
            fprintf(stream, "or i1 false, %s", inst->data.const_bool.value ? "true" : "false");
            llvmEmitDebugLocationSuffixInScope(stream, context, inst->filename, inst->line_number, inst->column_number, inst->debug_scope_id);
            fprintf(stream, "\n");
            return;
        case MIR_INST_CONST_CHAR:
        case MIR_INST_CONST_INT:
            if(inst->result_type != NULL && inst->result_type->kind == AST_DATA_TYPE_KIND_POINTER)
            {
                llvmEmitInstructionPrefix(stream, inst->result);
                fprintf(stream, "inttoptr i64 %llu to ptr",
                        inst->kind == MIR_INST_CONST_CHAR
                            ? (long long int)(unsigned char)inst->data.const_char.value
                            : inst->data.const_int.value);
                llvmEmitDebugLocationSuffixInScope(stream, context, inst->filename, inst->line_number, inst->column_number, inst->debug_scope_id);
                fprintf(stream, "\n");
                return;
            }

            if(inst->result_type != NULL &&
               inst->result_type->kind == AST_DATA_TYPE_KIND_FUNCTION &&
               inst->kind == MIR_INST_CONST_INT &&
               inst->data.const_int.value == 0ull)
            {
                llvmEmitZeroValueInst(stream, context, inst->result, inst->result_type,
                                      inst->filename, inst->line_number, inst->column_number);
                return;
            }

            if(llvmIsFloatDataType(inst->result_type))
            {
                llvmEmitFloatConstantInst(stream, context, inst->result, inst->result_type,
                                          inst->kind == MIR_INST_CONST_CHAR
                                               ? (long double)(unsigned char)inst->data.const_char.value
                                               : (long double)inst->data.const_int.value,
                                          inst->filename, inst->line_number, inst->column_number);
                return;
            }

            llvmEmitInstructionPrefix(stream, inst->result);
            fprintf(stream, "add ");
            llvmEmitType(stream, inst->result_type);
            fprintf(stream, " 0, %llu", inst->kind == MIR_INST_CONST_CHAR
                    ? (long long int)(unsigned char)inst->data.const_char.value
                    : inst->data.const_int.value);
            llvmEmitDebugLocationSuffixInScope(stream, context, inst->filename, inst->line_number, inst->column_number, inst->debug_scope_id);
            fprintf(stream, "\n");
            return;
        case MIR_INST_CONST_FLOAT:
            llvmEmitFloatConstantInst(stream, context, inst->result, inst->result_type, inst->data.const_float.value,
                                      inst->filename, inst->line_number, inst->column_number);
            return;
        case MIR_INST_CONST_STRING: {
            int length = (int)inst->result_type->array_length;
            if(length == 0)
            {
                llvmEmitZeroValueInst(stream, context, inst->result, inst->result_type,
                                      inst->filename, inst->line_number, inst->column_number);
                return;
            }

            char current_name[32];
            bool has_current = false;
            for(int i = 0; i < length; i++)
            {
                char next_name[32];
                if(i + 1 == length)
                    llvmEmitInstructionPrefix(stream, inst->result);
                else
                {
                    llvmMakeTempName(context, next_name, sizeof(next_name));
                    llvmEmitTempAssignPrefix(stream, next_name);
                }

                fprintf(stream, "insertvalue ");
                llvmEmitType(stream, inst->result_type);
                if(!has_current)
                    fprintf(stream, " zeroinitializer, ");
                else
                    fprintf(stream, " %s, ", current_name);
                fprintf(stream, "i8 %d, %d", (unsigned char)inst->data.const_string.value[i], i);
                if(i + 1 == length)
                    llvmEmitDebugLocationSuffixInScope(stream, context, inst->filename, inst->line_number, inst->column_number, inst->debug_scope_id);
                fprintf(stream, "\n");
                if(i + 1 != length)
                    strcpy(current_name, next_name);
                has_current = true;
            }
            return;
        }
        case MIR_INST_CONVERT:
            llvmEmitConvertInst(stream, context, inst);
            return;
        case MIR_INST_NEG:
            llvmEmitInstructionPrefix(stream, inst->result);
            if(llvmIsFloatDataType(inst->result_type))
                fprintf(stream, "fneg ");
            else
                fprintf(stream, "sub ");
            llvmEmitType(stream, inst->result_type);
            fprintf(stream, " ");
            if(llvmIsFloatDataType(inst->result_type))
                llvmEmitValueRef(stream, context, inst->data.unary.operand);
            else
            {
                fprintf(stream, "0, ");
                llvmEmitValueRef(stream, context, inst->data.unary.operand);
            }
            llvmEmitDebugLocationSuffixInScope(stream, context, inst->filename, inst->line_number, inst->column_number, inst->debug_scope_id);
            fprintf(stream, "\n");
            return;
        case MIR_INST_NOT:
            llvmEmitInstructionPrefix(stream, inst->result);
            fprintf(stream, "xor i1 ");
            llvmEmitValueRef(stream, context, inst->data.unary.operand);
            fprintf(stream, ", true");
            llvmEmitDebugLocationSuffixInScope(stream, context, inst->filename, inst->line_number, inst->column_number, inst->debug_scope_id);
            fprintf(stream, "\n");
            return;
        case MIR_INST_BIT_NOT:
            llvmEmitInstructionPrefix(stream, inst->result);
            fprintf(stream, "xor ");
            llvmEmitType(stream, inst->result_type);
            fprintf(stream, " ");
            llvmEmitValueRef(stream, context, inst->data.unary.operand);
            fprintf(stream, ", ");
            llvmEmitConstAllOnes(stream, inst->result_type);
            llvmEmitDebugLocationSuffixInScope(stream, context, inst->filename, inst->line_number, inst->column_number, inst->debug_scope_id);
            fprintf(stream, "\n");
            return;
        case MIR_INST_ADD: llvmEmitBinaryInst(stream, context, inst, "fadd", "add", "add"); return;
        case MIR_INST_SUB: llvmEmitBinaryInst(stream, context, inst, "fsub", "sub", "sub"); return;
        case MIR_INST_MUL: llvmEmitBinaryInst(stream, context, inst, "fmul", "mul", "mul"); return;
        case MIR_INST_DIV: llvmEmitBinaryInst(stream, context, inst, "fdiv", "sdiv", "udiv"); return;
        case MIR_INST_MOD: llvmEmitBinaryInst(stream, context, inst, "frem", "srem", "urem"); return;
        case MIR_INST_SHIFT_LEFT: llvmEmitBinaryInst(stream, context, inst, "shl", "shl", "shl"); return;
        case MIR_INST_SHIFT_RIGHT: llvmEmitBinaryInst(stream, context, inst, "ashr", "ashr", "lshr"); return;
        case MIR_INST_BIT_AND: llvmEmitBinaryInst(stream, context, inst, "and", "and", "and"); return;
        case MIR_INST_BIT_OR: llvmEmitBinaryInst(stream, context, inst, "or", "or", "or"); return;
        case MIR_INST_BIT_XOR: llvmEmitBinaryInst(stream, context, inst, "xor", "xor", "xor"); return;
        case MIR_INST_EQ: llvmEmitCompareInst(stream, context, inst, "oeq", "eq", "eq"); return;
        case MIR_INST_NE: llvmEmitCompareInst(stream, context, inst, "one", "ne", "ne"); return;
        case MIR_INST_LT: llvmEmitCompareInst(stream, context, inst, "olt", "slt", "ult"); return;
        case MIR_INST_LE: llvmEmitCompareInst(stream, context, inst, "ole", "sle", "ule"); return;
        case MIR_INST_GT: llvmEmitCompareInst(stream, context, inst, "ogt", "sgt", "ugt"); return;
        case MIR_INST_GE: llvmEmitCompareInst(stream, context, inst, "oge", "sge", "uge"); return;
        case MIR_INST_ALLOCA:
            llvmEmitInstructionPrefix(stream, inst->result);
            fprintf(stream, "alloca ");
            llvmEmitStorageType(stream, inst->data.alloca_inst.alloca_type);
            llvmEmitDebugLocationSuffixInScope(stream, context, inst->filename, inst->line_number, inst->column_number, inst->debug_scope_id);
            fprintf(stream, "\n");
            llvmEmitDebugDeclare(stream, context, inst->result);
            return;
        case MIR_INST_LOAD:
            if(llvmIsBoolDataType(inst->result_type))
            {
                char load_name[32];
                llvmMakeTempName(context, load_name, sizeof(load_name));
                llvmEmitTempAssignPrefix(stream, load_name);
                fprintf(stream, "load i8, ptr ");
                llvmEmitValueRef(stream, context, inst->data.load.address);
                fprintf(stream, "\n");
                llvmEmitInstructionPrefix(stream, inst->result);
                fprintf(stream, "trunc i8 %s to i1", load_name);
                llvmEmitDebugLocationSuffixInScope(stream, context, inst->filename, inst->line_number, inst->column_number, inst->debug_scope_id);
                fprintf(stream, "\n");
            }
            else
            {
                llvmEmitInstructionPrefix(stream, inst->result);
                fprintf(stream, "load ");
                llvmEmitType(stream, inst->result_type);
                fprintf(stream, ", ptr ");
                llvmEmitValueRef(stream, context, inst->data.load.address);
                llvmEmitDebugLocationSuffixInScope(stream, context, inst->filename, inst->line_number, inst->column_number, inst->debug_scope_id);
                fprintf(stream, "\n");
            }
            return;
        case MIR_INST_STORE: {
            MirInst *stored_value_inst = llvmFindValueProducer(context, inst->data.store.value);
            ASTDataType *stored_value_type = llvmResolvedValueType(context, inst->data.store.value);
            char stored_value_name[32];
            const char *stored_value_ref = NULL;
            if(!(stored_value_inst != NULL && stored_value_inst->kind == MIR_INST_ZERO))
                stored_value_ref = llvmPrepareStoredValueRef(stream, context, inst->data.store.value,
                                                             stored_value_name, sizeof(stored_value_name));
            fprintf(stream, "    store ");
            llvmEmitStorageType(stream, stored_value_type);
            fprintf(stream, " ");
            if(stored_value_inst != NULL && stored_value_inst->kind == MIR_INST_ZERO)
                fprintf(stream, "zeroinitializer");
            else
                fprintf(stream, "%s", stored_value_ref);
            fprintf(stream, ", ptr ");
            llvmEmitValueRef(stream, context, inst->data.store.address);
            llvmEmitDebugLocationSuffixInScope(stream, context, inst->filename, inst->line_number, inst->column_number, inst->debug_scope_id);
            fprintf(stream, "\n");
            return;
        }
        case MIR_INST_GLOBAL_ADDR:
            llvmEmitInstructionPrefix(stream, inst->result);
            fprintf(stream, "getelementptr ");
            llvmEmitStorageType(stream, llvmPointeeType(inst->result_type));
            fprintf(stream, ", ptr @%s, i32 0", inst->data.global_addr.global_name);
            llvmEmitDebugLocationSuffixInScope(stream, context, inst->filename, inst->line_number, inst->column_number, inst->debug_scope_id);
            fprintf(stream, "\n");
            return;
        case MIR_INST_FUNCTION_REF:
            llvmEmitClosureAggregate(stream, context, inst->result, inst->data.function_ref.function_name, NULL,
                                     inst->filename, inst->line_number, inst->column_number);
            return;
        case MIR_INST_MAKE_CLOSURE: {
            char env_name[32];
            const char *env_ref = NULL;
            if(inst->data.make_closure.captures.count > 0)
            {
                llvmMakeTempName(context, env_name, sizeof(env_name));
                llvmEmitEnvAllocation(stream, context, inst, env_name);
                env_ref = env_name;
            }
            llvmEmitClosureAggregate(stream, context, inst->result, inst->data.make_closure.function_name, env_ref,
                                     inst->filename, inst->line_number, inst->column_number);
            return;
        }
        case MIR_INST_FIELD_PTR: {
            ASTDataType *pointee_type = llvmPointeeType(llvmResolvedValueType(context, inst->data.field_ptr.base_address));
            llvmEmitInstructionPrefix(stream, inst->result);
            fprintf(stream, "getelementptr ");
            llvmEmitStorageType(stream, pointee_type);
            fprintf(stream, ", ptr ");
            llvmEmitValueRef(stream, context, inst->data.field_ptr.base_address);
            fprintf(stream, ", i32 0, i32 %d", inst->data.field_ptr.field_index);
            llvmEmitDebugLocationSuffixInScope(stream, context, inst->filename, inst->line_number, inst->column_number, inst->debug_scope_id);
            fprintf(stream, "\n");
            return;
        }
        case MIR_INST_INDEX_PTR: {
            ASTDataType *pointee_type = llvmPointeeType(llvmResolvedValueType(context, inst->data.index_ptr.base_address));
            llvmEmitInstructionPrefix(stream, inst->result);
            fprintf(stream, "getelementptr ");
            llvmEmitStorageType(stream, pointee_type);
            fprintf(stream, ", ptr ");
            llvmEmitValueRef(stream, context, inst->data.index_ptr.base_address);
            if(!inst->data.index_ptr.base_is_element_pointer)
                fprintf(stream, ", i32 0, ");
            else
                fprintf(stream, ", ");
            llvmEmitType(stream, llvmResolvedValueType(context, inst->data.index_ptr.index_value));
            fprintf(stream, " ");
            llvmEmitValueRef(stream, context, inst->data.index_ptr.index_value);
            llvmEmitDebugLocationSuffixInScope(stream, context, inst->filename, inst->line_number, inst->column_number, inst->debug_scope_id);
            fprintf(stream, "\n");
            return;
        }
        case MIR_INST_PTR_DIFF: {
            char lhs_int_name[32];
            char rhs_int_name[32];
            llvmMakeTempName(context, lhs_int_name, sizeof(lhs_int_name));
            llvmMakeTempName(context, rhs_int_name, sizeof(rhs_int_name));

            fprintf(stream, "    %s = ptrtoint ptr ", lhs_int_name);
            llvmEmitValueRef(stream, context, inst->data.ptr_diff.lhs);
            fprintf(stream, " to i64\n");

            fprintf(stream, "    %s = ptrtoint ptr ", rhs_int_name);
            llvmEmitValueRef(stream, context, inst->data.ptr_diff.rhs);
            fprintf(stream, " to i64\n");

            char byte_diff_name[32];
            llvmMakeTempName(context, byte_diff_name, sizeof(byte_diff_name));
            fprintf(stream, "    %s = sub i64 %s, %s\n", byte_diff_name, lhs_int_name, rhs_int_name);

            llvmEmitInstructionPrefix(stream, inst->result);
            fprintf(stream, "sdiv i64 %s, %d", byte_diff_name,
                    (int) llvmExternABITypeSize(llvmPointeeType(llvmResolvedValueType(context, inst->data.ptr_diff.lhs))));
            llvmEmitDebugLocationSuffixInScope(stream, context, inst->filename, inst->line_number, inst->column_number, inst->debug_scope_id);
            fprintf(stream, "\n");
            return;
        }
        case MIR_INST_ARRAY_LITERAL:
            llvmEmitArrayLiteral(stream, context, inst);
            return;
        case MIR_INST_STRUCT_LITERAL:
            llvmEmitStructLiteral(stream, context, inst);
            return;
        case MIR_INST_ENUM_LITERAL:
            llvmEmitInstructionPrefix(stream, inst->result);
            fprintf(stream, "add i32 0, %d", inst->data.enum_literal.ordinal);
            llvmEmitDebugLocationSuffixInScope(stream, context, inst->filename, inst->line_number, inst->column_number, inst->debug_scope_id);
            fprintf(stream, "\n");
            return;
        case MIR_INST_CALL: {
            char code_name[32];
            char env_name[32];
            ASTDataType *callee_type = llvmResolvedFunctionValueType(context, inst->data.call.callee);
            ASTFunctionParameter *parameter = callee_type != NULL ? callee_type->parameters : NULL;
            llvmMakeTempName(context, code_name, sizeof(code_name));
            llvmMakeTempName(context, env_name, sizeof(env_name));

            llvmEmitTempAssignPrefix(stream, code_name);
            fprintf(stream, "extractvalue { ptr, ptr } ");
            llvmEmitValueRef(stream, context, inst->data.call.callee);
            fprintf(stream, ", 0\n");

            llvmEmitTempAssignPrefix(stream, env_name);
            fprintf(stream, "extractvalue { ptr, ptr } ");
            llvmEmitValueRef(stream, context, inst->data.call.callee);
            fprintf(stream, ", 1\n");

            if(inst->result >= 0)
                llvmEmitInstructionPrefix(stream, inst->result);
            else
                fprintf(stream, "    ");

            fprintf(stream, "call ");
            llvmEmitCallReturnABIType(stream, inst->result_type);
            fprintf(stream, " %s(ptr %s", code_name, env_name);
            for(int i = 0; i < inst->data.call.arguments.count; i++)
            {
                ASTDataType *arg_type = llvmResolvedValueType(context, inst->data.call.arguments.items[i]);
                fprintf(stream, ", ");
                llvmEmitCallArgumentABIType(stream, parameter != NULL ? parameter->data_type : arg_type);
                fprintf(stream, " ");
                llvmEmitValueRef(stream, context, inst->data.call.arguments.items[i]);
                if(parameter != NULL)
                    parameter = parameter->next;
            }
            fprintf(stream, ")");
            llvmEmitDebugLocationSuffixInScope(stream, context, inst->filename, inst->line_number, inst->column_number, inst->debug_scope_id);
            fprintf(stream, "\n");
            return;
        }
        case MIR_INST_EXTERN_CALL: {
            LLVMExternABIInfo return_abi = llvmDescribeExternReturnABI(inst->result_type);
            ASTDataType *function_type = inst->data.extern_call.function_type;
            MirExternFunction *extern_function_info = llvmFindExternFunction(context->program, inst->data.extern_call.symbol_name);
            bool call_wrapper = extern_function_info != NULL &&
                                extern_function_info->kind == MIR_EXTERN_FUNCTION_NATIVE &&
                                !extern_function_info->is_direct &&
                                strcmp(extern_function_info->wrapper_name, inst->data.extern_call.symbol_name) == 0;
            ASTFunctionParameter *parameter = function_type != NULL ? function_type->parameters : NULL;
            int temp_base = inst->result >= 0 ? inst->result : 900000;

            if(return_abi.kind == LLVM_EXTERN_ABI_SRET_POINTER)
            {
                fprintf(stream, "    %%v%d_ret_slot = alloca ", temp_base);
                llvmEmitType(stream, inst->result_type);
                fprintf(stream, "\n");
            }

            for(int i = 0; i < inst->data.extern_call.arguments.count && parameter != NULL; i++, parameter = parameter->next)
            {
                LLVMExternABIInfo parameter_abi = llvmDescribeExternParameterABI(parameter->data_type);
                if(parameter_abi.kind == LLVM_EXTERN_ABI_INTEGER_COERCE ||
                   parameter_abi.kind == LLVM_EXTERN_ABI_INDIRECT_POINTER)
                {
                    fprintf(stream, "    %%v%d_arg%d_slot = alloca ", temp_base, i);
                    llvmEmitType(stream, parameter->data_type);
                    fprintf(stream, "\n");
                    fprintf(stream, "    store ");
                    llvmEmitType(stream, parameter->data_type);
                    fprintf(stream, " ");
                    llvmEmitValueRef(stream, context, inst->data.extern_call.arguments.items[i]);
                    fprintf(stream, ", ptr %%v%d_arg%d_slot\n", temp_base, i);
                    if(parameter_abi.kind == LLVM_EXTERN_ABI_INTEGER_COERCE)
                    {
                        fprintf(stream, "    %%v%d_arg%d_coerce = load ", temp_base, i);
                        llvmEmitIntegerCoerceType(stream, parameter_abi.integer_bits);
                        fprintf(stream, ", ptr %%v%d_arg%d_slot\n", temp_base, i);
                    }
                }
            }

            if(return_abi.kind == LLVM_EXTERN_ABI_SRET_POINTER)
                fprintf(stream, "    call ");
            else if(inst->result >= 0)
                llvmEmitInstructionPrefix(stream, inst->result);
            else
                fprintf(stream, "    ");

            if(return_abi.kind != LLVM_EXTERN_ABI_SRET_POINTER)
                fprintf(stream, "call ");
            if(function_type != NULL && function_type->is_variadic)
            {
                llvmEmitNativeExternReturnType(stream, inst->result_type);
                fprintf(stream, " (");
                ASTFunctionParameter *typed_parameter = function_type->parameters;
                bool typed_need_comma = false;
                while(typed_parameter)
                {
                    if(typed_need_comma)
                        fprintf(stream, ", ");
                    llvmEmitNativeExternParameterType(stream, typed_parameter->data_type);
                    typed_need_comma = true;
                    typed_parameter = typed_parameter->next;
                }
                if(function_type->is_variadic)
                {
                    if(typed_need_comma)
                        fprintf(stream, ", ");
                    fprintf(stream, "...");
                }
                fprintf(stream, ") @%s", inst->data.extern_call.symbol_name);
            }
            else if(call_wrapper)
            {
                llvmEmitFunctionReturnABIType(stream, inst->result_type);
                fprintf(stream, " @%s", inst->data.extern_call.symbol_name);
            }
            else
            {
                llvmEmitNativeExternReturnType(stream, inst->result_type);
                fprintf(stream, " ");
                llvmEmitNativeExternCallTarget(stream, inst->data.extern_call.symbol_name, function_type);
            }
            fprintf(stream, "(");

            bool need_comma = false;
            if(return_abi.kind == LLVM_EXTERN_ABI_SRET_POINTER)
            {
                fprintf(stream, "ptr sret(");
                llvmEmitType(stream, inst->result_type);
                fprintf(stream, ") align %zu %%v%d_ret_slot", return_abi.align, temp_base);
                need_comma = true;
            }

            if(call_wrapper)
            {
                if(need_comma)
                    fprintf(stream, ", ");
                fprintf(stream, "ptr null");
                need_comma = true;
            }

            parameter = function_type != NULL ? function_type->parameters : NULL;
            for(int i = 0; i < inst->data.extern_call.arguments.count; i++)
            {
                ASTDataType *arg_type = llvmResolvedValueType(context, inst->data.extern_call.arguments.items[i]);
                LLVMExternABIInfo parameter_abi = parameter != NULL
                    ? llvmDescribeExternParameterABI(parameter->data_type)
                    : llvmDescribeExternParameterABI(arg_type);
                if(need_comma)
                    fprintf(stream, ", ");
                if(parameter != NULL)
                    llvmEmitNativeExternParameterType(stream, parameter->data_type);
                else
                    llvmEmitCallArgumentABIType(stream, arg_type);
                fprintf(stream, " ");
                if(parameter_abi.kind == LLVM_EXTERN_ABI_INTEGER_COERCE)
                    fprintf(stream, "%%v%d_arg%d_coerce", temp_base, i);
                else if(parameter_abi.kind == LLVM_EXTERN_ABI_INDIRECT_POINTER)
                    fprintf(stream, "%%v%d_arg%d_slot", temp_base, i);
                else
                    llvmEmitValueRef(stream, context, inst->data.extern_call.arguments.items[i]);
                need_comma = true;
                if(parameter != NULL)
                    parameter = parameter->next;
            }
            fprintf(stream, ")");
            llvmEmitDebugLocationSuffixInScope(stream, context, inst->filename, inst->line_number, inst->column_number, inst->debug_scope_id);
            fprintf(stream, "\n");

            if(return_abi.kind == LLVM_EXTERN_ABI_SRET_POINTER && inst->result >= 0)
            {
                llvmEmitInstructionPrefix(stream, inst->result);
                fprintf(stream, "load ");
                llvmEmitType(stream, inst->result_type);
                fprintf(stream, ", ptr %%v%d_ret_slot", temp_base);
                llvmEmitDebugLocationSuffixInScope(stream, context, inst->filename, inst->line_number, inst->column_number, inst->debug_scope_id);
                fprintf(stream, "\n");
            }
            return;
        }
    }

    llvmBackendError("unsupported MIR instruction in textual LLVM backend", inst->filename, inst->line_number, inst->column_number);
}

static void llvmEmitTerminator(FILE *stream, LLVMFunctionEmitContext *context, MirTerminator *terminator)
{
    switch(terminator->kind)
    {
        case MIR_TERM_BR:
            fprintf(stream, "    br label %%%s", context->function->blocks[terminator->data.br.target].name);
            llvmEmitDebugLocationSuffixInScope(stream, context, terminator->filename, terminator->line_number, terminator->column_number, terminator->debug_scope_id);
            fprintf(stream, "\n");
            return;
        case MIR_TERM_COND_BR:
            fprintf(stream, "    br i1 ");
            llvmEmitValueRef(stream, context, terminator->data.cond_br.condition);
            fprintf(stream, ", label %%%s, label %%%s",
                    context->function->blocks[terminator->data.cond_br.then_block].name,
                    context->function->blocks[terminator->data.cond_br.else_block].name);
            llvmEmitDebugLocationSuffixInScope(stream, context, terminator->filename, terminator->line_number, terminator->column_number, terminator->debug_scope_id);
            fprintf(stream, "\n");
            return;
        case MIR_TERM_RET:
            if(terminator->data.ret.has_value)
            {
                fprintf(stream, "    ret ");
                llvmEmitType(stream, llvmResolvedValueType(context, terminator->data.ret.value));
                fprintf(stream, " ");
                llvmEmitValueRef(stream, context, terminator->data.ret.value);
                llvmEmitDebugLocationSuffixInScope(stream, context, terminator->filename, terminator->line_number, terminator->column_number, terminator->debug_scope_id);
                fprintf(stream, "\n");
            }
            else
            {
                fprintf(stream, "    ret void");
                llvmEmitDebugLocationSuffixInScope(stream, context, terminator->filename, terminator->line_number, terminator->column_number, terminator->debug_scope_id);
                fprintf(stream, "\n");
            }
            return;
        case MIR_TERM_UNREACHABLE:
            fprintf(stream, "    unreachable");
            llvmEmitDebugLocationSuffixInScope(stream, context, terminator->filename, terminator->line_number, terminator->column_number, terminator->debug_scope_id);
            fprintf(stream, "\n");
            return;
        case MIR_TERM_NONE:
            llvmBackendError("unterminated MIR block cannot be emitted to LLVM IR", NULL, 0, 0);
            return;
    }
}

static void llvmEmitFunctionDefinition(FILE *stream, MirProgram *program, MirFunction *function, LLVMDebugBuilder *debug)
{
    LLVMFunctionEmitContext context = {0};
    context.stream = stream;
    context.program = program;
    context.function = function;
    context.debug = debug;
    context.emit_debug_info = debug != NULL && debug->enabled;
    context.aliases = (int*) malloc(sizeof(int) * function->value_count);
    for(int i = 0; i < function->value_count; i++)
        context.aliases[i] = i;
    if(context.emit_debug_info && function->source_function != NULL)
        context.debug_subprogram_md = llvmDebugCreateSubprogram(debug,
                                                                function->source_function->filename,
                                                                function->name,
                                                                function->source_function->line_number + 1);

    fprintf(stream, "define ");
    llvmEmitFunctionSignature(stream, function);
    if(context.debug_subprogram_md > 0)
        fprintf(stream, " !dbg !%d", context.debug_subprogram_md);
    fprintf(stream, " {\n");

    for(int block_index = 0; block_index < function->block_count; block_index++)
    {
        MirBlock *block = &(function->blocks[block_index]);
        fprintf(stream, "%s:\n", block->name);
        for(int inst_index = 0; inst_index < block->inst_count; inst_index++)
            llvmEmitInst(stream, &context, &(block->insts[inst_index]));
        llvmEmitTerminator(stream, &context, &(block->terminator));
    }

    fprintf(stream, "}\n\n");
    free(context.aliases);
}

static bool llvmProgramHasGlobal(const MirProgram *program, const char *name)
{
    if(program == NULL || name == NULL)
        return false;

    for(int i = 0; i < program->global_count; i++)
    {
        if(strcmp(program->globals[i].name, name) == 0)
            return true;
    }
    return false;
}

static void llvmEmitEntryPoint(FILE *stream, MirProgram *program, ASTNode *root)
{
    fprintf(stream, "define i32 @main() {\n");
    fprintf(stream, "entry:\n");
    fprintf(stream, "    call void @__mote_init(ptr null)\n");
    if(root != NULL && root->entry_symbol[0] != '\0' && llvmProgramHasGlobal(program, root->entry_symbol))
    {
        fprintf(stream, "    %%mote_user_main_slot = getelementptr { ptr, ptr }, ptr @%s, i32 0\n", root->entry_symbol);
        fprintf(stream, "    %%mote_user_main = load { ptr, ptr }, ptr %%mote_user_main_slot\n");
        fprintf(stream, "    %%mote_user_main_fn = extractvalue { ptr, ptr } %%mote_user_main, 0\n");
        fprintf(stream, "    %%mote_user_main_env = extractvalue { ptr, ptr } %%mote_user_main, 1\n");
        if(root->entry_returns_void)
        {
            fprintf(stream, "    call void %%mote_user_main_fn(ptr %%mote_user_main_env)\n");
            fprintf(stream, "    ret i32 0\n");
        }
        else
        {
            fprintf(stream, "    %%mote_user_main_ret = call i32 %%mote_user_main_fn(ptr %%mote_user_main_env)\n");
            fprintf(stream, "    ret i32 %%mote_user_main_ret\n");
        }
    }
    else
        fprintf(stream, "    ret i32 0\n");
    fprintf(stream, "}\n\n");
}

static void emitLLVMProgramToFile(MirProgram *program, ASTNode *root, const char *module_name, const char *output_path,
                                  bool emit_debug_info)
{
    FILE *stream = fopen(output_path, "wb");
    if(stream == NULL)
        llvmBackendErrorFormatted("L2002", NULL, 0, 0,
                                  NULL,
                                  "failed to open LLVM output file `%s`",
                                  output_path);

    LLVMDebugBuilder debug = {0};
    llvmDebugInit(&debug, emit_debug_info);

    fprintf(stream, "; ModuleID = '%s'\n", module_name != NULL ? module_name : "mote");
    fprintf(stream, "source_filename = \"%s\"\n\n", module_name != NULL ? module_name : "mote");
    if(llvmHostTargetTriple() != NULL)
        fprintf(stream, "target triple = \"%s\"\n\n", llvmHostTargetTriple());

    if(emit_debug_info)
        fprintf(stream, "declare void @llvm.dbg.declare(metadata, metadata, metadata)\n\n");

    if(llvmProgramNeedsMalloc(program) && !llvmProgramHasExternSymbol(program, "malloc"))
        fprintf(stream, "declare ptr @malloc(i64)\n\n");

    if(program->extern_function_count > 0)
    {
        llvmEmitExternDeclarations(stream, program);
        fprintf(stream, "\n");
    }

    for(int i = 0; i < program->global_count; i++)
    {
        MirGlobal *global = &(program->globals[i]);
        fprintf(stream, "@%s = global ", global->name);
        llvmEmitType(stream, global->data_type);
        if(global->has_const_string_initializer)
        {
            int length = (int)global->data_type->array_length;
            fprintf(stream, " c\"");
            for(int j = 0; j < length; j++)
            {
                unsigned char ch = j + 1 == length
                    ? 0
                    : (unsigned char)global->const_string_initializer[j];
                if(ch >= 32 && ch <= 126 && ch != '\\' && ch != '"')
                    fputc(ch, stream);
                else
                    fprintf(stream, "\\%02X", ch);
            }
            fprintf(stream, "\"\n");
        }
        else
            fprintf(stream, " zeroinitializer\n");
    }

    if(program->global_count > 0)
        fprintf(stream, "\n");

    if(program->extern_function_count > 0)
        llvmEmitExternWrapperDefinitions(stream, program);

    for(int i = 0; i < program->function_count; i++)
        llvmEmitFunctionDefinition(stream, program, program->functions[i], &debug);

    llvmEmitEntryPoint(stream, program, root);

    if(emit_debug_info)
    {
        fprintf(stream, "\n");
        llvmEmitDebugMetadata(stream, &debug, program);
    }

    fclose(stream);
    llvmDebugDispose(&debug);
}

#endif /* LLVM_BACKEND_H */
