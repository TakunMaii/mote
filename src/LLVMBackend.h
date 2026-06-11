#ifndef LLVM_BACKEND_H
#define LLVM_BACKEND_H

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "MIR.h"

typedef struct LLVMFunctionEmitContext {
    FILE *stream;
    MirFunction *function;
    int *aliases;
    int temp_counter;
} LLVMFunctionEmitContext;

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

static void llvmEmitFloatLiteral(FILE *stream, ASTDataType *data_type, long double value)
{
    if(data_type != NULL && data_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
       data_type->primary == AST_PRIMARY_DATA_TYPE_F32)
    {
        fprintf(stream, "%.9e", (double)(float)value);
        return;
    }

    if(data_type != NULL && data_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
       data_type->primary == AST_PRIMARY_DATA_TYPE_F16)
    {
        fprintf(stream, "%.6e", (double)value);
        return;
    }

    llvmEmitDoubleLiteral(stream, value);
}

static void llvmEmitFloatConstantInst(FILE *stream, int result_value_id, ASTDataType *data_type, long double value)
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
        fprintf(stream, "\n");
        return;
    }

    llvmEmitInstructionPrefix(stream, result_value_id);
    fprintf(stream, "fadd ");
    llvmEmitType(stream, data_type);
    fprintf(stream, " 0.0, ");
    llvmEmitDoubleLiteral(stream, value);
    fprintf(stream, "\n");
}

static void llvmEmitConstZero(FILE *stream, ASTDataType *data_type)
{
    if(llvmIsFloatDataType(data_type))
        llvmEmitFloatLiteral(stream, data_type, 0.0L);
    else if(data_type != NULL && data_type->kind == AST_DATA_TYPE_KIND_POINTER)
        fprintf(stream, "null");
    else
        fprintf(stream, "0");
}

static void llvmEmitZeroValueInst(FILE *stream, LLVMFunctionEmitContext *context, int result_value_id, ASTDataType *data_type)
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
        fprintf(stream, "trunc i8 %s to i1\n", load_name);
    }
    else
    {
        llvmEmitInstructionPrefix(stream, result_value_id);
        fprintf(stream, "load ");
        llvmEmitType(stream, data_type);
        fprintf(stream, ", ptr %s\n", slot_name);
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

static bool llvmIsAggregateType(ASTDataType *data_type)
{
    if(data_type == NULL)
        return false;

    return data_type->kind == AST_DATA_TYPE_KIND_ARRAY ||
           data_type->kind == AST_DATA_TYPE_KIND_STRUCT ||
           data_type->kind == AST_DATA_TYPE_KIND_FUNCTION;
}

static void llvmEmitInsertValueSequence(FILE *stream, LLVMFunctionEmitContext *context, int result_value_id,
                                        ASTDataType *aggregate_type, MirOperandList values)
{
    char current_name[32];
    bool has_current = false;

    for(int i = 0; i < values.count; i++)
    {
        ASTDataType *element_type = context->function->values[llvmResolveAlias(context, values.items[i])].data_type;
        char stored_value_name[32];
        const char *stored_value_ref = llvmPrepareStoredValueRef(stream, context, values.items[i],
                                                                 stored_value_name, sizeof(stored_value_name));
        char next_name[32];
        if(i + 1 == values.count)
            llvmEmitInstructionPrefix(stream, result_value_id);
        else
        {
            llvmMakeTempName(context, next_name, sizeof(next_name));
            llvmEmitTempAssignPrefix(stream, next_name);
        }

        fprintf(stream, "insertvalue ");
        llvmEmitStorageType(stream, aggregate_type);
        if(!has_current)
            fprintf(stream, " undef, ");
        else
            fprintf(stream, " %s, ", current_name);

        llvmEmitStorageType(stream, element_type);
        fprintf(stream, " ");
        fprintf(stream, "%s", stored_value_ref);
        fprintf(stream, ", %d\n", i);

        if(i + 1 != values.count)
            strcpy(current_name, next_name);
        has_current = true;
    }

    if(values.count == 0)
    {
        llvmEmitInstructionPrefix(stream, result_value_id);
        fprintf(stream, "insertvalue ");
        llvmEmitStorageType(stream, aggregate_type);
        fprintf(stream, " undef, i8 0, 0\n");
        llvmBackendError("zero-field insertvalue fallback was reached unexpectedly", NULL, 0, 0);
    }
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
                                     const char *function_name, const char *env_name)
{
    char first_name[32];
    llvmMakeTempName(context, first_name, sizeof(first_name));
    llvmEmitTempAssignPrefix(stream, first_name);
    fprintf(stream, "insertvalue { ptr, ptr } undef, ptr @%s, 0\n", function_name);

    llvmEmitInstructionPrefix(stream, result_value_id);
    fprintf(stream, "insertvalue { ptr, ptr } %s, ptr %s, 1\n", first_name, env_name != NULL ? env_name : "null");
}

static void llvmEmitArrayLiteral(FILE *stream, LLVMFunctionEmitContext *context, MirInst *inst)
{
    if(inst->data.array_literal.elements.count == 0)
    {
        llvmEmitZeroValueInst(stream, context, inst->result, inst->result_type);
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
        fprintf(stream, ", %d\n", i);
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
        llvmEmitZeroValueInst(stream, context, inst->result, inst->result_type);
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
        else if(inst->result_type != NULL && inst->result_type->kind == AST_DATA_TYPE_KIND_SLICE)
        {
            if(strcmp(field->identifier, "ptr") == 0)
            {
                field_type = newWrappedDataType(AST_DATA_TYPE_KIND_POINTER, true,
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
        fprintf(stream, ", %d\n", field_index);
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

static bool llvmProgramHasExternSymbol(MirProgram *program, const char *symbol_name)
{
    for(int i = 0; i < program->extern_function_count; i++)
    {
        if(strcmp(program->extern_functions[i].symbol_name, symbol_name) == 0)
            return true;
    }
    return false;
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

    if(return_abi.kind == LLVM_EXTERN_ABI_SRET_POINTER)
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
            llvmEmitZeroValueInst(stream, context, inst->result, inst->result_type);
            return;
        case MIR_INST_CONST_BOOL:
            llvmEmitInstructionPrefix(stream, inst->result);
            fprintf(stream, "or i1 false, %s\n", inst->data.const_bool.value ? "true" : "false");
            return;
        case MIR_INST_CONST_CHAR:
        case MIR_INST_CONST_INT:
            if(inst->result_type != NULL && inst->result_type->kind == AST_DATA_TYPE_KIND_POINTER)
            {
                llvmEmitInstructionPrefix(stream, inst->result);
                fprintf(stream, "inttoptr i64 %lld to ptr\n",
                        inst->kind == MIR_INST_CONST_CHAR
                            ? (long long int)(unsigned char)inst->data.const_char.value
                            : inst->data.const_int.value);
                return;
            }

            if(inst->result_type != NULL &&
               inst->result_type->kind == AST_DATA_TYPE_KIND_FUNCTION &&
               inst->kind == MIR_INST_CONST_INT &&
               inst->data.const_int.value == 0)
            {
                llvmEmitZeroValueInst(stream, context, inst->result, inst->result_type);
                return;
            }

            if(llvmIsFloatDataType(inst->result_type))
            {
                llvmEmitFloatConstantInst(stream, inst->result, inst->result_type,
                                         inst->kind == MIR_INST_CONST_CHAR
                                              ? (long double)(unsigned char)inst->data.const_char.value
                                              : (long double)inst->data.const_int.value);
                return;
            }

            llvmEmitInstructionPrefix(stream, inst->result);
            fprintf(stream, "add ");
            llvmEmitType(stream, inst->result_type);
            fprintf(stream, " 0, %lld\n", inst->kind == MIR_INST_CONST_CHAR
                    ? (long long int)(unsigned char)inst->data.const_char.value
                    : inst->data.const_int.value);
            return;
        case MIR_INST_CONST_FLOAT:
            llvmEmitFloatConstantInst(stream, inst->result, inst->result_type, inst->data.const_float.value);
            return;
        case MIR_INST_CONST_STRING: {
            int length = (int)inst->result_type->array_length;
            if(length == 0)
            {
                llvmEmitZeroValueInst(stream, context, inst->result, inst->result_type);
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
                fprintf(stream, "i8 %d, %d\n", (unsigned char)inst->data.const_string.value[i], i);
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
            fprintf(stream, "\n");
            return;
        case MIR_INST_NOT:
            llvmEmitInstructionPrefix(stream, inst->result);
            fprintf(stream, "xor i1 ");
            llvmEmitValueRef(stream, context, inst->data.unary.operand);
            fprintf(stream, ", true\n");
            return;
        case MIR_INST_BIT_NOT:
            llvmEmitInstructionPrefix(stream, inst->result);
            fprintf(stream, "xor ");
            llvmEmitType(stream, inst->result_type);
            fprintf(stream, " ");
            llvmEmitValueRef(stream, context, inst->data.unary.operand);
            fprintf(stream, ", ");
            llvmEmitConstAllOnes(stream, inst->result_type);
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
            fprintf(stream, "\n");
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
                fprintf(stream, "trunc i8 %s to i1\n", load_name);
            }
            else
            {
                llvmEmitInstructionPrefix(stream, inst->result);
                fprintf(stream, "load ");
                llvmEmitType(stream, inst->result_type);
                fprintf(stream, ", ptr ");
                llvmEmitValueRef(stream, context, inst->data.load.address);
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
            fprintf(stream, "\n");
            return;
        }
        case MIR_INST_GLOBAL_ADDR:
            llvmEmitInstructionPrefix(stream, inst->result);
            fprintf(stream, "getelementptr ");
            llvmEmitStorageType(stream, llvmPointeeType(inst->result_type));
            fprintf(stream, ", ptr @%s, i32 0\n", inst->data.global_addr.global_name);
            return;
        case MIR_INST_FUNCTION_REF:
            llvmEmitClosureAggregate(stream, context, inst->result, inst->data.function_ref.function_name, NULL);
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
            llvmEmitClosureAggregate(stream, context, inst->result, inst->data.make_closure.function_name, env_ref);
            return;
        }
        case MIR_INST_FIELD_PTR: {
            ASTDataType *pointee_type = llvmPointeeType(llvmResolvedValueType(context, inst->data.field_ptr.base_address));
            llvmEmitInstructionPrefix(stream, inst->result);
            fprintf(stream, "getelementptr ");
            llvmEmitStorageType(stream, pointee_type);
            fprintf(stream, ", ptr ");
            llvmEmitValueRef(stream, context, inst->data.field_ptr.base_address);
            fprintf(stream, ", i32 0, i32 %d\n", inst->data.field_ptr.field_index);
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
            fprintf(stream, "sdiv i64 %s, %d\n", byte_diff_name,
                    (int) llvmExternABITypeSize(llvmPointeeType(llvmResolvedValueType(context, inst->data.ptr_diff.lhs))));
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
            fprintf(stream, "add i32 0, %d\n", inst->data.enum_literal.ordinal);
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
            fprintf(stream, ")\n");
            return;
        }
        case MIR_INST_EXTERN_CALL: {
            LLVMExternABIInfo return_abi = llvmDescribeExternReturnABI(inst->result_type);
            ASTDataType *function_type = inst->data.extern_call.function_type;
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
            fprintf(stream, ")\n");

            if(return_abi.kind == LLVM_EXTERN_ABI_SRET_POINTER && inst->result >= 0)
            {
                llvmEmitInstructionPrefix(stream, inst->result);
                fprintf(stream, "load ");
                llvmEmitType(stream, inst->result_type);
                fprintf(stream, ", ptr %%v%d_ret_slot\n", temp_base);
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
            fprintf(stream, "    br label %%%s\n", context->function->blocks[terminator->data.br.target].name);
            return;
        case MIR_TERM_COND_BR:
            fprintf(stream, "    br i1 ");
            llvmEmitValueRef(stream, context, terminator->data.cond_br.condition);
            fprintf(stream, ", label %%%s, label %%%s\n",
                    context->function->blocks[terminator->data.cond_br.then_block].name,
                    context->function->blocks[terminator->data.cond_br.else_block].name);
            return;
        case MIR_TERM_RET:
            if(terminator->data.ret.has_value)
            {
                fprintf(stream, "    ret ");
                llvmEmitType(stream, llvmResolvedValueType(context, terminator->data.ret.value));
                fprintf(stream, " ");
                llvmEmitValueRef(stream, context, terminator->data.ret.value);
                fprintf(stream, "\n");
            }
            else
                fprintf(stream, "    ret void\n");
            return;
        case MIR_TERM_NONE:
            llvmBackendError("unterminated MIR block cannot be emitted to LLVM IR", NULL, 0, 0);
            return;
    }
}

static void llvmEmitFunctionDefinition(FILE *stream, MirFunction *function)
{
    LLVMFunctionEmitContext context = {0};
    context.stream = stream;
    context.function = function;
    context.aliases = (int*) malloc(sizeof(int) * function->value_count);
    for(int i = 0; i < function->value_count; i++)
        context.aliases[i] = i;

    fprintf(stream, "define ");
    llvmEmitFunctionSignature(stream, function);
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

static void llvmEmitEntryPoint(FILE *stream, MirProgram *program)
{
    fprintf(stream, "define i32 @main() {\n");
    fprintf(stream, "entry:\n");
    fprintf(stream, "    call void @__mote_init(ptr null)\n");
    if(llvmProgramHasGlobal(program, "m0__main"))
    {
        fprintf(stream, "    %%mote_user_main_slot = getelementptr { ptr, ptr }, ptr @m0__main, i32 0\n");
        fprintf(stream, "    %%mote_user_main = load { ptr, ptr }, ptr %%mote_user_main_slot\n");
        fprintf(stream, "    %%mote_user_main_fn = extractvalue { ptr, ptr } %%mote_user_main, 0\n");
        fprintf(stream, "    %%mote_user_main_env = extractvalue { ptr, ptr } %%mote_user_main, 1\n");
        fprintf(stream, "    %%mote_user_main_ret = call i32 %%mote_user_main_fn(ptr %%mote_user_main_env)\n");
        fprintf(stream, "    ret i32 %%mote_user_main_ret\n");
    }
    else
        fprintf(stream, "    ret i32 0\n");
    fprintf(stream, "}\n\n");
}

static void emitLLVMProgramToFile(MirProgram *program, const char *module_name, const char *output_path)
{
    FILE *stream = fopen(output_path, "wb");
    if(stream == NULL)
        llvmBackendErrorFormatted("L2002", NULL, 0, 0,
                                  NULL,
                                  "failed to open LLVM output file `%s`",
                                  output_path);

    fprintf(stream, "; ModuleID = '%s'\n", module_name != NULL ? module_name : "mote");
    fprintf(stream, "source_filename = \"%s\"\n\n", module_name != NULL ? module_name : "mote");
    if(llvmHostTargetTriple() != NULL)
        fprintf(stream, "target triple = \"%s\"\n\n", llvmHostTargetTriple());

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
        llvmEmitFunctionDefinition(stream, program->functions[i]);

    llvmEmitEntryPoint(stream, program);

    fclose(stream);
}

#endif /* LLVM_BACKEND_H */
