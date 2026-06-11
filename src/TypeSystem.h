#ifndef TYPE_SYSTEM_H
#define TYPE_SYSTEM_H

#include "Diagnostic.h"
#include "AST.h"
#include "SymbolTable.h"
#include <stdbool.h>
#include <stdio.h>

typedef enum TypeSystemExprTypeKind {
    TYPE_SYSTEM_EXPR_TYPE_VALUE,
    TYPE_SYSTEM_EXPR_TYPE_TYPE,
    TYPE_SYSTEM_EXPR_TYPE_NULL,
    TYPE_SYSTEM_EXPR_TYPE_LITERAL_INTEGER,
    TYPE_SYSTEM_EXPR_TYPE_LITERAL_FLOAT,
} TypeSystemExprTypeKind;

typedef struct TypeSystemExprType {
    TypeSystemExprTypeKind kind;
    ASTDataType *data_type;
} TypeSystemExprType;

TypeSystemExprType inferExprType(ASTNode *node, ScopeFrame *scope);
bool isSameDataType(ASTDataType *lhs, ASTDataType *rhs);
ASTDataType* inferDeclaredTypeFromExpr(ASTNode *expr, ScopeFrame *scope);
ASTStructMember* resolveStructMembers(ASTStructMember *member, ScopeFrame *scope, ASTDataType *self_data_type);
ASTDataType* resolveNamedDataType(ASTDataType *data_type, ScopeFrame *scope, ASTDataType *self_data_type);
void bindCapturedValuesForInstantiation(ASTFunctionCapture *capture, ScopeFrame *inst_scope, ScopeFrame *outer_scope);
void bindCallArgumentsForInstantiation(ASTFunctionParameter *parameter, ASTNode *argument, ScopeFrame *inst_scope, ScopeFrame *outer_scope);
ASTNode* findReturnedExpr(ASTNode *function_expr);
ASTDataType* instantiateTypeExprValue(ASTNode *expr, ScopeFrame *inst_scope);
TypeSystemExprType instantiateFunctionCallExprType(ASTNode *function_value, ASTNode *call_arguments, ScopeFrame *outer_scope);
ASTNode* buildTypeLiteralArgumentExprs(ASTTypeArgument *argument, ScopeFrame *scope, ASTDataType *self_data_type);
ASTNode* resolveFunctionValueExpr(ASTNode *expr, ScopeFrame *scope);
ASTNode* resolveExternValueExpr(ASTNode *expr, ScopeFrame *scope);
bool canImplicitConvertExprToType(ASTNode *expr, ScopeFrame *scope, ASTDataType *target_type);
bool canBindReferenceArgument(ASTNode *argument, ScopeFrame *scope, ASTDataType *parameter_type);

typedef struct ResolvedOperatorOverload {
    ASTNode *function_value;
    ASTDataType *result_type;
} ResolvedOperatorOverload;

static bool resolveOperatorOverload(ASTOperatorKind operator_kind,
                                    ASTNode *lhs_expr,
                                    ASTNode *rhs_expr,
                                    ScopeFrame *scope,
                                    ResolvedOperatorOverload *out);

typedef struct ResolveDataTypeEntry {
    ASTDataType *source;
    ASTDataType *resolved;
    struct ResolveDataTypeEntry *next;
} ResolveDataTypeEntry;

static ScopeFrame* findInstantiatingFunctionScope(ScopeFrame *scope, ASTNode *function_value)
{
    ScopeFrame *current = scope;
    while(current != NULL)
    {
        if(current->instantiating_function == function_value)
            return current;
        current = current->parent;
    }
    return NULL;
}

static ASTDataType* resolveNamedDataTypeInternal(ASTDataType *data_type, ScopeFrame *scope,
                                                 ASTDataType *self_data_type,
                                                 ResolveDataTypeEntry **memo,
                                                 bool allow_recursive_factory_result);
static ASTStructMember* resolveStructMembersInternal(ASTStructMember *member, ScopeFrame *scope,
                                                     ASTDataType *self_data_type,
                                                     ResolveDataTypeEntry **memo,
                                                     bool allow_recursive_factory_result);

TypeSystemExprType newValueExprType(ASTDataType *data_type)
{
    TypeSystemExprType expr_type = {0};
    expr_type.kind = TYPE_SYSTEM_EXPR_TYPE_VALUE;
    expr_type.data_type = cloneDataType(data_type);
    return expr_type;
}

TypeSystemExprType newTypeExprType(ASTDataType *data_type)
{
    TypeSystemExprType expr_type = {0};
    expr_type.kind = TYPE_SYSTEM_EXPR_TYPE_TYPE;
    expr_type.data_type = cloneDataType(data_type);
    return expr_type;
}

TypeSystemExprType newLiteralIntegerExprType()
{
    TypeSystemExprType expr_type = {0};
    expr_type.kind = TYPE_SYSTEM_EXPR_TYPE_LITERAL_INTEGER;
    return expr_type;
}

TypeSystemExprType newNullExprType()
{
    TypeSystemExprType expr_type = {0};
    expr_type.kind = TYPE_SYSTEM_EXPR_TYPE_NULL;
    return expr_type;
}

TypeSystemExprType newLiteralFloatExprType()
{
    TypeSystemExprType expr_type = {0};
    expr_type.kind = TYPE_SYSTEM_EXPR_TYPE_LITERAL_FLOAT;
    return expr_type;
}

MOTE_NORETURN void typeSystemAbortNode(const char *code, ASTNode *node, const char *message, const char *label)
{
    diagnosticAbortSimple(code, message, astNodeSourceSpan(node), label);
}

MOTE_NORETURN void typeSystemAbortFormatted(const char *code, ASTNode *node, const char *label, const char *format, ...)
{
    Diagnostic diagnostic = diagnosticMake(DIAGNOSTIC_SEVERITY_ERROR, code, astNodeSourceSpan(node), "");
    va_list args;
    va_start(args, format);
    diagnosticVFormat(diagnostic.message, sizeof(diagnostic.message), format, args);
    va_end(args);
    if(label != NULL)
        diagnosticSetPrimaryLabel(&diagnostic, "%s", label);
    diagnosticAbort(diagnostic);
}

MOTE_NORETURN void typeSystemAbortNoSpan(const char *code, const char *message, const char *detail)
{
    diagnosticAbortSimple(code, message, makeSourceSpan(NULL, 0, 0, 0, 0), detail);
}

void typeSystemDescribeExprType(TypeSystemExprType expr_type, char *buffer, size_t buffer_size)
{
    if(buffer_size == 0)
        return;

    if(expr_type.kind == TYPE_SYSTEM_EXPR_TYPE_LITERAL_INTEGER)
    {
        diagnosticFormat(buffer, buffer_size, "literal integer");
        return;
    }
    if(expr_type.kind == TYPE_SYSTEM_EXPR_TYPE_LITERAL_FLOAT)
    {
        diagnosticFormat(buffer, buffer_size, "literal float");
        return;
    }
    if(expr_type.kind == TYPE_SYSTEM_EXPR_TYPE_TYPE)
    {
        diagnosticFormat(buffer, buffer_size, "type");
        return;
    }
    if(expr_type.kind == TYPE_SYSTEM_EXPR_TYPE_NULL)
    {
        diagnosticFormat(buffer, buffer_size, "`null`");
        return;
    }
    if(expr_type.data_type == NULL)
    {
        diagnosticFormat(buffer, buffer_size, "<unknown>");
        return;
    }
    appendASTDataTypeString(expr_type.data_type, buffer, buffer_size);
}

bool isInferDataType(ASTDataType *data_type)
{
    return data_type != NULL && data_type->kind == AST_DATA_TYPE_KIND_INFER;
}

bool isPrimaryValueDataType(ASTDataType *data_type)
{
    return data_type != NULL && data_type->kind == AST_DATA_TYPE_KIND_PRIMARY;
}

bool isIntegerPrimary(ASTPrimaryDataType primary)
{
    return primary == AST_PRIMARY_DATA_TYPE_I8 ||
           primary == AST_PRIMARY_DATA_TYPE_I16 ||
           primary == AST_PRIMARY_DATA_TYPE_I32 ||
           primary == AST_PRIMARY_DATA_TYPE_I64 ||
           primary == AST_PRIMARY_DATA_TYPE_U8 ||
           primary == AST_PRIMARY_DATA_TYPE_U16 ||
           primary == AST_PRIMARY_DATA_TYPE_U32 ||
           primary == AST_PRIMARY_DATA_TYPE_U64 ||
           primary == AST_PRIMARY_DATA_TYPE_CHAR;
}

bool isSignedIntegerPrimary(ASTPrimaryDataType primary)
{
    return primary == AST_PRIMARY_DATA_TYPE_I8 ||
           primary == AST_PRIMARY_DATA_TYPE_I16 ||
           primary == AST_PRIMARY_DATA_TYPE_I32 ||
           primary == AST_PRIMARY_DATA_TYPE_I64;
}

bool isUnsignedIntegerPrimary(ASTPrimaryDataType primary)
{
    return primary == AST_PRIMARY_DATA_TYPE_U8 ||
           primary == AST_PRIMARY_DATA_TYPE_U16 ||
           primary == AST_PRIMARY_DATA_TYPE_U32 ||
           primary == AST_PRIMARY_DATA_TYPE_U64;
}

bool isFloatPrimary(ASTPrimaryDataType primary)
{
    return primary == AST_PRIMARY_DATA_TYPE_F8 ||
           primary == AST_PRIMARY_DATA_TYPE_F16 ||
           primary == AST_PRIMARY_DATA_TYPE_F32 ||
           primary == AST_PRIMARY_DATA_TYPE_F64;
}

static bool isBuiltinNumericOperandType(TypeSystemExprType expr_type)
{
    return expr_type.kind == TYPE_SYSTEM_EXPR_TYPE_LITERAL_INTEGER ||
           expr_type.kind == TYPE_SYSTEM_EXPR_TYPE_LITERAL_FLOAT ||
           (expr_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE &&
            expr_type.data_type != NULL &&
            expr_type.data_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
            (isIntegerPrimary(expr_type.data_type->primary) || isFloatPrimary(expr_type.data_type->primary)));
}

bool isBoolPrimary(ASTPrimaryDataType primary)
{
    return primary == AST_PRIMARY_DATA_TYPE_BOOL;
}

bool isCharPrimary(ASTPrimaryDataType primary)
{
    return primary == AST_PRIMARY_DATA_TYPE_CHAR;
}

bool isVoidPrimary(ASTPrimaryDataType primary)
{
    return primary == AST_PRIMARY_DATA_TYPE_VOID;
}

bool isTypePrimary(ASTPrimaryDataType primary)
{
    return primary == AST_PRIMARY_DATA_TYPE_TYPE;
}

ASTDataType* builtinIdentifierToDataType(const char *identifier)
{
    if(strcmp(identifier, "i8") == 0) return newPrimaryDataType(AST_PRIMARY_DATA_TYPE_I8);
    if(strcmp(identifier, "i16") == 0) return newPrimaryDataType(AST_PRIMARY_DATA_TYPE_I16);
    if(strcmp(identifier, "i32") == 0) return newPrimaryDataType(AST_PRIMARY_DATA_TYPE_I32);
    if(strcmp(identifier, "i64") == 0) return newPrimaryDataType(AST_PRIMARY_DATA_TYPE_I64);
    if(strcmp(identifier, "u8") == 0) return newPrimaryDataType(AST_PRIMARY_DATA_TYPE_U8);
    if(strcmp(identifier, "u16") == 0) return newPrimaryDataType(AST_PRIMARY_DATA_TYPE_U16);
    if(strcmp(identifier, "u32") == 0) return newPrimaryDataType(AST_PRIMARY_DATA_TYPE_U32);
    if(strcmp(identifier, "u64") == 0) return newPrimaryDataType(AST_PRIMARY_DATA_TYPE_U64);
    if(strcmp(identifier, "f8") == 0) return newPrimaryDataType(AST_PRIMARY_DATA_TYPE_F8);
    if(strcmp(identifier, "f16") == 0) return newPrimaryDataType(AST_PRIMARY_DATA_TYPE_F16);
    if(strcmp(identifier, "f32") == 0) return newPrimaryDataType(AST_PRIMARY_DATA_TYPE_F32);
    if(strcmp(identifier, "f64") == 0) return newPrimaryDataType(AST_PRIMARY_DATA_TYPE_F64);
    if(strcmp(identifier, "char") == 0) return newPrimaryDataType(AST_PRIMARY_DATA_TYPE_CHAR);
    if(strcmp(identifier, "bool") == 0) return newPrimaryDataType(AST_PRIMARY_DATA_TYPE_BOOL);
    if(strcmp(identifier, "void") == 0) return newPrimaryDataType(AST_PRIMARY_DATA_TYPE_VOID);
    if(strcmp(identifier, "Type") == 0) return newPrimaryDataType(AST_PRIMARY_DATA_TYPE_TYPE);
    if(strcmp(identifier, "opaque") == 0) return newOpaqueDataType("");
    return NULL;
}

bool isStructDataType(ASTDataType *data_type)
{
    return data_type != NULL && data_type->kind == AST_DATA_TYPE_KIND_STRUCT;
}

bool isOpaqueDataType(ASTDataType *data_type)
{
    return data_type != NULL && data_type->kind == AST_DATA_TYPE_KIND_OPAQUE;
}

bool isRuntimeOpaqueDataType(ASTDataType *data_type)
{
    return data_type != NULL && data_type->kind == AST_DATA_TYPE_KIND_OPAQUE;
}

typedef struct TypeSystemVisitedDataTypeEntry {
    ASTDataType *data_type;
    struct TypeSystemVisitedDataTypeEntry *next;
} TypeSystemVisitedDataTypeEntry;

static bool typeSystemVisitedDataTypeContains(TypeSystemVisitedDataTypeEntry *memo, ASTDataType *data_type)
{
    while(memo != NULL)
    {
        if(memo->data_type == data_type)
            return true;
        memo = memo->next;
    }
    return false;
}

static void typeSystemRememberVisitedDataType(TypeSystemVisitedDataTypeEntry **memo, ASTDataType *data_type)
{
    TypeSystemVisitedDataTypeEntry *entry = (TypeSystemVisitedDataTypeEntry*) malloc(sizeof(TypeSystemVisitedDataTypeEntry));
    entry->data_type = data_type;
    entry->next = *memo;
    *memo = entry;
}

static void typeSystemEnsureNoBareOpaqueInternal(ASTDataType *data_type, ASTNode *node, const char *code,
                                                 const char *context, TypeSystemVisitedDataTypeEntry **memo)
{
    if(data_type == NULL)
        return;

    if(typeSystemVisitedDataTypeContains(*memo, data_type))
        return;
    typeSystemRememberVisitedDataType(memo, data_type);

    switch(data_type->kind)
    {
        case AST_DATA_TYPE_KIND_OPAQUE:
            typeSystemAbortFormatted(code, node,
                                     "opaque types do not have a first-class runtime value representation",
                                     "%s cannot use bare opaque type `%s`; use a pointer like `*%s` instead",
                                     context,
                                     data_type->identifier[0] != '\0' ? data_type->identifier : "opaque",
                                     data_type->identifier[0] != '\0' ? data_type->identifier : "opaque");
            return;
        case AST_DATA_TYPE_KIND_POINTER:
        case AST_DATA_TYPE_KIND_REFERENCE:
            return;
        case AST_DATA_TYPE_KIND_OPTIONAL:
        case AST_DATA_TYPE_KIND_ARRAY:
        case AST_DATA_TYPE_KIND_SLICE:
            typeSystemEnsureNoBareOpaqueInternal(data_type->child, node, code, context, memo);
            return;
        case AST_DATA_TYPE_KIND_FUNCTION: {
            ASTFunctionParameter *parameter = data_type->parameters;
            while(parameter)
            {
                typeSystemEnsureNoBareOpaqueInternal(parameter->data_type, node, code, context, memo);
                parameter = parameter->next;
            }
            typeSystemEnsureNoBareOpaqueInternal(data_type->return_data_type, node, code, context, memo);
            return;
        }
        case AST_DATA_TYPE_KIND_STRUCT: {
            ASTStructMember *member = data_type->members;
            while(member)
            {
                if(member->value == NULL)
                    typeSystemEnsureNoBareOpaqueInternal(member->data_type, node, code, context, memo);
                member = member->next;
            }
            return;
        }
        default:
            return;
    }
}

void typeSystemEnsureNoBareOpaque(ASTDataType *data_type, ASTNode *node, const char *code, const char *context)
{
    TypeSystemVisitedDataTypeEntry *memo = NULL;
    typeSystemEnsureNoBareOpaqueInternal(data_type, node, code, context, &memo);
}

bool isEnumDataType(ASTDataType *data_type)
{
    return data_type != NULL && data_type->kind == AST_DATA_TYPE_KIND_ENUM;
}

bool isArrayDataType(ASTDataType *data_type)
{
    return data_type != NULL && data_type->kind == AST_DATA_TYPE_KIND_ARRAY;
}

bool isSliceDataType(ASTDataType *data_type)
{
    return data_type != NULL && data_type->kind == AST_DATA_TYPE_KIND_SLICE;
}

bool isOptionalDataType(ASTDataType *data_type)
{
    return data_type != NULL && data_type->kind == AST_DATA_TYPE_KIND_OPTIONAL;
}

static size_t moteAlignTo(size_t value, size_t align)
{
    if(align == 0)
        return value;
    size_t remainder = value % align;
    if(remainder == 0)
        return value;
    return value + align - remainder;
}

static size_t moteTypeLayoutAlignment(ASTDataType *data_type);

static size_t moteTypeLayoutSize(ASTDataType *data_type)
{
    if(data_type == NULL)
        return 0;

    switch(data_type->kind)
    {
        case AST_DATA_TYPE_KIND_PRIMARY:
            switch(data_type->primary)
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
        case AST_DATA_TYPE_KIND_OPAQUE:
            typeSystemAbortNoSpan("T1252",
                                  "opaque types do not have a concrete layout",
                                  "use a pointer to the opaque type instead");
        case AST_DATA_TYPE_KIND_POINTER:
        case AST_DATA_TYPE_KIND_REFERENCE:
        case AST_DATA_TYPE_KIND_FUNCTION:
            return sizeof(void*);
        case AST_DATA_TYPE_KIND_OPTIONAL: {
            size_t flag_size = 1;
            size_t child_align = moteTypeLayoutAlignment(data_type->child);
            size_t child_size = moteTypeLayoutSize(data_type->child);
            size_t offset = moteAlignTo(flag_size, child_align);
            size_t max_align = 1 > child_align ? 1 : child_align;
            return moteAlignTo(offset + child_size, max_align);
        }
        case AST_DATA_TYPE_KIND_ENUM:
            return 4;
        case AST_DATA_TYPE_KIND_ARRAY:
            return moteTypeLayoutSize(data_type->child) * (size_t) data_type->array_length;
        case AST_DATA_TYPE_KIND_SLICE: {
            ASTDataType *len_type = newPrimaryDataType(AST_PRIMARY_DATA_TYPE_I64);
            size_t ptr_size = sizeof(void*);
            size_t len_align = moteTypeLayoutAlignment(len_type);
            size_t len_size = moteTypeLayoutSize(len_type);
            size_t offset = moteAlignTo(ptr_size, len_align);
            size_t max_align = sizeof(void*) > len_align ? sizeof(void*) : len_align;
            return moteAlignTo(offset + len_size, max_align);
        }
        case AST_DATA_TYPE_KIND_STRUCT: {
            size_t offset = 0;
            size_t max_align = 1;
            ASTStructMember *member = data_type->members;
            while(member)
            {
                if(member->value == NULL)
                {
                    size_t member_align = moteTypeLayoutAlignment(member->data_type);
                    size_t member_size = moteTypeLayoutSize(member->data_type);
                    offset = moteAlignTo(offset, member_align);
                    offset += member_size;
                    if(member_align > max_align)
                        max_align = member_align;
                }
                member = member->next;
            }
            return moteAlignTo(offset, max_align);
        }
        default:
            return 0;
    }
}

static size_t moteTypeLayoutAlignment(ASTDataType *data_type)
{
    if(data_type == NULL)
        return 1;

    switch(data_type->kind)
    {
        case AST_DATA_TYPE_KIND_PRIMARY:
            switch(data_type->primary)
            {
                case AST_PRIMARY_DATA_TYPE_VOID:
                    return 1;
                case AST_PRIMARY_DATA_TYPE_BOOL:
                case AST_PRIMARY_DATA_TYPE_CHAR:
                case AST_PRIMARY_DATA_TYPE_I8:
                case AST_PRIMARY_DATA_TYPE_U8:
                case AST_PRIMARY_DATA_TYPE_F8:
                    return 1;
                case AST_PRIMARY_DATA_TYPE_I16:
                case AST_PRIMARY_DATA_TYPE_U16:
                case AST_PRIMARY_DATA_TYPE_F16:
                    return 2;
                case AST_PRIMARY_DATA_TYPE_I32:
                case AST_PRIMARY_DATA_TYPE_U32:
                case AST_PRIMARY_DATA_TYPE_F32:
                    return 4;
                case AST_PRIMARY_DATA_TYPE_I64:
                case AST_PRIMARY_DATA_TYPE_U64:
                case AST_PRIMARY_DATA_TYPE_F64:
                case AST_PRIMARY_DATA_TYPE_TYPE:
                    return sizeof(void*) > 8 ? sizeof(void*) : 8;
            }
            return 1;
        case AST_DATA_TYPE_KIND_OPAQUE:
            typeSystemAbortNoSpan("T1253",
                                  "opaque types do not have a concrete alignment",
                                  "use a pointer to the opaque type instead");
        case AST_DATA_TYPE_KIND_POINTER:
        case AST_DATA_TYPE_KIND_REFERENCE:
        case AST_DATA_TYPE_KIND_FUNCTION:
            return sizeof(void*);
        case AST_DATA_TYPE_KIND_OPTIONAL: {
            size_t flag_align = 1;
            size_t child_align = moteTypeLayoutAlignment(data_type->child);
            return flag_align > child_align ? flag_align : child_align;
        }
        case AST_DATA_TYPE_KIND_ENUM:
            return 4;
        case AST_DATA_TYPE_KIND_ARRAY:
            return moteTypeLayoutAlignment(data_type->child);
        case AST_DATA_TYPE_KIND_SLICE: {
            ASTDataType *len_type = newPrimaryDataType(AST_PRIMARY_DATA_TYPE_I64);
            size_t len_align = moteTypeLayoutAlignment(len_type);
            return sizeof(void*) > len_align ? sizeof(void*) : len_align;
        }
        case AST_DATA_TYPE_KIND_STRUCT: {
            size_t max_align = 1;
            ASTStructMember *member = data_type->members;
            while(member)
            {
                if(member->value == NULL)
                {
                    size_t member_align = moteTypeLayoutAlignment(member->data_type);
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

ASTStructMember* findStructMember(ASTDataType *struct_type, const char *identifier)
{
    if(struct_type == NULL || struct_type->kind != AST_DATA_TYPE_KIND_STRUCT)
        return NULL;

    ASTStructMember *member = struct_type->members;
    while(member)
    {
        if(strcmp(member->identifier, identifier) == 0)
            return member;
        member = member->next;
    }
    return NULL;
}

ASTEnumVariant* findEnumVariant(ASTDataType *enum_type, const char *identifier)
{
    if(enum_type == NULL || enum_type->kind != AST_DATA_TYPE_KIND_ENUM)
        return NULL;

    ASTEnumVariant *variant = enum_type->variants;
    while(variant)
    {
        if(strcmp(variant->identifier, identifier) == 0)
            return variant;
        variant = variant->next;
    }
    return NULL;
}

int getIntegerPrimaryWidth(ASTPrimaryDataType primary)
{
    switch(primary)
    {
        case AST_PRIMARY_DATA_TYPE_I8:
        case AST_PRIMARY_DATA_TYPE_U8:
        case AST_PRIMARY_DATA_TYPE_CHAR:
            return 8;
        case AST_PRIMARY_DATA_TYPE_I16:
        case AST_PRIMARY_DATA_TYPE_U16:
            return 16;
        case AST_PRIMARY_DATA_TYPE_I32:
        case AST_PRIMARY_DATA_TYPE_U32:
            return 32;
        case AST_PRIMARY_DATA_TYPE_I64:
        case AST_PRIMARY_DATA_TYPE_U64:
            return 64;
        default:
            diagnosticAbortFormatted("T1998",
                                     makeSourceSpan(NULL, 0, 0, 0, 0),
                                     NULL,
                                     "getIntegerPrimaryWidth: unsupported type %s",
                                     astPrimaryDataTypeToString(primary));
    }
}

int getFloatPrimaryWidth(ASTPrimaryDataType primary)
{
    switch(primary)
    {
        case AST_PRIMARY_DATA_TYPE_F8: return 8;
        case AST_PRIMARY_DATA_TYPE_F16: return 16;
        case AST_PRIMARY_DATA_TYPE_F32: return 32;
        case AST_PRIMARY_DATA_TYPE_F64: return 64;
        default:
            diagnosticAbortFormatted("T1999",
                                     makeSourceSpan(NULL, 0, 0, 0, 0),
                                     NULL,
                                     "getFloatPrimaryWidth: unsupported type %s",
                                     astPrimaryDataTypeToString(primary));
    }
}

bool isSameFunctionSignature(ASTDataType *lhs, ASTDataType *rhs)
{
    if(lhs->is_variadic != rhs->is_variadic)
        return false;

    ASTFunctionParameter *lhs_parameter = lhs->parameters;
    ASTFunctionParameter *rhs_parameter = rhs->parameters;
    while(lhs_parameter && rhs_parameter)
    {
        if(!isSameDataType(lhs_parameter->data_type, rhs_parameter->data_type))
            return false;
        lhs_parameter = lhs_parameter->next;
        rhs_parameter = rhs_parameter->next;
    }

    if(lhs_parameter != NULL || rhs_parameter != NULL)
        return false;

    return isSameDataType(lhs->return_data_type, rhs->return_data_type);
}

typedef struct ASTDataTypeCompareEntry {
    ASTDataType *lhs;
    ASTDataType *rhs;
    struct ASTDataTypeCompareEntry *next;
} ASTDataTypeCompareEntry;

bool isSameDataTypeInternal(ASTDataType *lhs, ASTDataType *rhs, ASTDataTypeCompareEntry **memo);

bool hasComparedDataTypePair(ASTDataTypeCompareEntry *memo, ASTDataType *lhs, ASTDataType *rhs)
{
    while(memo)
    {
        if(memo->lhs == lhs && memo->rhs == rhs)
            return true;
        memo = memo->next;
    }
    return false;
}

void rememberComparedDataTypePair(ASTDataTypeCompareEntry **memo, ASTDataType *lhs, ASTDataType *rhs)
{
    ASTDataTypeCompareEntry *entry = (ASTDataTypeCompareEntry*) malloc(sizeof(ASTDataTypeCompareEntry));
    entry->lhs = lhs;
    entry->rhs = rhs;
    entry->next = *memo;
    *memo = entry;
}

bool isSameFunctionSignatureInternal(ASTDataType *lhs, ASTDataType *rhs, ASTDataTypeCompareEntry **memo)
{
    if(lhs->is_variadic != rhs->is_variadic)
        return false;

    ASTFunctionParameter *lhs_parameter = lhs->parameters;
    ASTFunctionParameter *rhs_parameter = rhs->parameters;
    while(lhs_parameter && rhs_parameter)
    {
        if(!isSameDataTypeInternal(lhs_parameter->data_type, rhs_parameter->data_type, memo))
            return false;
        lhs_parameter = lhs_parameter->next;
        rhs_parameter = rhs_parameter->next;
    }

    if(lhs_parameter != NULL || rhs_parameter != NULL)
        return false;

    return isSameDataTypeInternal(lhs->return_data_type, rhs->return_data_type, memo);
}

bool isSameStructTypeInternal(ASTDataType *lhs, ASTDataType *rhs, ASTDataTypeCompareEntry **memo)
{
    if((lhs->identifier[0] != '\0' || rhs->identifier[0] != '\0') &&
       lhs->kind == AST_DATA_TYPE_KIND_STRUCT && rhs->kind == AST_DATA_TYPE_KIND_STRUCT)
        return strcmp(lhs->identifier, rhs->identifier) == 0;

    if(hasComparedDataTypePair(*memo, lhs, rhs))
        return true;
    rememberComparedDataTypePair(memo, lhs, rhs);

    ASTStructMember *lhs_member = lhs->members;
    ASTStructMember *rhs_member = rhs->members;
    while(lhs_member && rhs_member)
    {
        if(strcmp(lhs_member->identifier, rhs_member->identifier) != 0)
            return false;

        bool lhs_is_method = lhs_member->value != NULL;
        bool rhs_is_method = rhs_member->value != NULL;
        if(lhs_is_method != rhs_is_method)
            return false;
        if(!lhs_is_method && !isSameDataTypeInternal(lhs_member->data_type, rhs_member->data_type, memo))
            return false;

        lhs_member = lhs_member->next;
        rhs_member = rhs_member->next;
    }

    return lhs_member == NULL && rhs_member == NULL;
}

bool isSameStructType(ASTDataType *lhs, ASTDataType *rhs)
{
    ASTDataTypeCompareEntry *memo = NULL;
    return isSameStructTypeInternal(lhs, rhs, &memo);
}

bool isSameEnumTypeInternal(ASTDataType *lhs, ASTDataType *rhs, ASTDataTypeCompareEntry **memo)
{
    if((lhs->identifier[0] != '\0' || rhs->identifier[0] != '\0') &&
       lhs->kind == AST_DATA_TYPE_KIND_ENUM && rhs->kind == AST_DATA_TYPE_KIND_ENUM)
        return strcmp(lhs->identifier, rhs->identifier) == 0;

    if(hasComparedDataTypePair(*memo, lhs, rhs))
        return true;
    rememberComparedDataTypePair(memo, lhs, rhs);

    ASTEnumVariant *lhs_variant = lhs->variants;
    ASTEnumVariant *rhs_variant = rhs->variants;
    while(lhs_variant && rhs_variant)
    {
        if(strcmp(lhs_variant->identifier, rhs_variant->identifier) != 0)
            return false;
        lhs_variant = lhs_variant->next;
        rhs_variant = rhs_variant->next;
    }

    return lhs_variant == NULL && rhs_variant == NULL;
}

bool isSameEnumType(ASTDataType *lhs, ASTDataType *rhs)
{
    ASTDataTypeCompareEntry *memo = NULL;
    return isSameEnumTypeInternal(lhs, rhs, &memo);
}

bool isSameArrayTypeInternal(ASTDataType *lhs, ASTDataType *rhs, ASTDataTypeCompareEntry **memo)
{
    return lhs->array_length == rhs->array_length &&
           isSameDataTypeInternal(lhs->child, rhs->child, memo);
}

bool isSameArrayType(ASTDataType *lhs, ASTDataType *rhs)
{
    ASTDataTypeCompareEntry *memo = NULL;
    return isSameArrayTypeInternal(lhs, rhs, &memo);
}

bool isSameSliceTypeInternal(ASTDataType *lhs, ASTDataType *rhs, ASTDataTypeCompareEntry **memo)
{
    return isSameDataTypeInternal(lhs->child, rhs->child, memo);
}

bool isSameDataTypeInternal(ASTDataType *lhs, ASTDataType *rhs, ASTDataTypeCompareEntry **memo)
{
    if(lhs == NULL || rhs == NULL)
        return lhs == rhs;

    if(lhs == rhs)
        return true;

    if(lhs->kind != rhs->kind)
        return false;

    switch(lhs->kind)
    {
        case AST_DATA_TYPE_KIND_INFER:
            return true;
        case AST_DATA_TYPE_KIND_PRIMARY:
            return lhs->primary == rhs->primary;
        case AST_DATA_TYPE_KIND_POINTER:
        case AST_DATA_TYPE_KIND_REFERENCE:
            if(lhs->mutable != rhs->mutable)
                return false;
            if(hasComparedDataTypePair(*memo, lhs, rhs))
                return true;
            rememberComparedDataTypePair(memo, lhs, rhs);
            return isSameDataTypeInternal(lhs->child, rhs->child, memo);
        case AST_DATA_TYPE_KIND_OPTIONAL:
            if(hasComparedDataTypePair(*memo, lhs, rhs))
                return true;
            rememberComparedDataTypePair(memo, lhs, rhs);
            return isSameDataTypeInternal(lhs->child, rhs->child, memo);
        case AST_DATA_TYPE_KIND_FUNCTION:
            if(hasComparedDataTypePair(*memo, lhs, rhs))
                return true;
            rememberComparedDataTypePair(memo, lhs, rhs);
            return isSameFunctionSignatureInternal(lhs, rhs, memo);
        case AST_DATA_TYPE_KIND_NAMED:
            return strcmp(lhs->identifier, rhs->identifier) == 0;
        case AST_DATA_TYPE_KIND_ARRAY:
            if(hasComparedDataTypePair(*memo, lhs, rhs))
                return true;
            rememberComparedDataTypePair(memo, lhs, rhs);
            return isSameArrayTypeInternal(lhs, rhs, memo);
        case AST_DATA_TYPE_KIND_SLICE:
            if(hasComparedDataTypePair(*memo, lhs, rhs))
                return true;
            rememberComparedDataTypePair(memo, lhs, rhs);
            return isSameSliceTypeInternal(lhs, rhs, memo);
        case AST_DATA_TYPE_KIND_ENUM:
            return isSameEnumTypeInternal(lhs, rhs, memo);
        case AST_DATA_TYPE_KIND_STRUCT:
            return isSameStructTypeInternal(lhs, rhs, memo);
        case AST_DATA_TYPE_KIND_OPAQUE:
            return strcmp(lhs->identifier, rhs->identifier) == 0;
        default:
            return false;
    }
}

bool isSameDataType(ASTDataType *lhs, ASTDataType *rhs)
{
    ASTDataTypeCompareEntry *memo = NULL;
    return isSameDataTypeInternal(lhs, rhs, &memo);
}

static ASTDataType* resolveNamedDataTypeInternal(ASTDataType *data_type, ScopeFrame *scope,
                                                 ASTDataType *self_data_type,
                                                 ResolveDataTypeEntry **memo,
                                                 bool allow_recursive_factory_result)
{
    if(data_type == NULL)
        return NULL;

    ResolveDataTypeEntry *entry = memo == NULL ? NULL : *memo;
    while(entry != NULL)
    {
        if(entry->source == data_type)
            return entry->resolved;
        entry = entry->next;
    }

    switch(data_type->kind)
    {
        case AST_DATA_TYPE_KIND_INFER:
        case AST_DATA_TYPE_KIND_PRIMARY:
        case AST_DATA_TYPE_KIND_ENUM:
        case AST_DATA_TYPE_KIND_OPAQUE:
            return cloneDataType(data_type);
        case AST_DATA_TYPE_KIND_STRUCT: {
            ASTDataType *resolved_struct = newStructDataType(data_type->identifier, NULL);
            ResolveDataTypeEntry *new_entry = (ResolveDataTypeEntry*) malloc(sizeof(ResolveDataTypeEntry));
            new_entry->source = data_type;
            new_entry->resolved = resolved_struct;
            new_entry->next = memo == NULL ? NULL : *memo;
            if(memo != NULL)
                *memo = new_entry;
            resolved_struct->members = resolveStructMembersInternal(data_type->members, scope, resolved_struct, memo, false);
            return resolved_struct;
        }
        case AST_DATA_TYPE_KIND_NAMED: {
            ASTDataType *builtin_type = builtinIdentifierToDataType(data_type->identifier);
            if(builtin_type != NULL)
                return builtin_type;

            if(strcmp(data_type->identifier, "Self") == 0)
            {
                if(self_data_type == NULL)
                    typeSystemAbortNoSpan("T1201",
                                          "`Self` is only allowed inside a struct method",
                                          NULL);
                return self_data_type;
            }

            TypeInfo *type_info = findTypeInfo(scope, data_type->identifier);
            if(type_info == NULL)
            {
                VariableInfo *variable_info = findVariableInfo(scope, data_type->identifier);
                if(variable_info != NULL && variable_info->type_value != NULL)
                    return cloneDataType(variable_info->type_value);

                diagnosticAbortFormatted("T1202",
                                         makeSourceSpan(NULL, 0, 0, 0, 0),
                                         NULL,
                                         "unknown data type `%s`",
                                         data_type->identifier);
            }
            return cloneDataType(type_info->data_type);
        }
        case AST_DATA_TYPE_KIND_POINTER:
        case AST_DATA_TYPE_KIND_REFERENCE:
            return newWrappedDataType(data_type->kind, data_type->mutable,
                                      resolveNamedDataTypeInternal(data_type->child, scope, self_data_type, memo, true));
        case AST_DATA_TYPE_KIND_OPTIONAL:
            return newWrappedDataType(data_type->kind, data_type->mutable,
                                      resolveNamedDataTypeInternal(data_type->child, scope, self_data_type, memo, false));
        case AST_DATA_TYPE_KIND_ARRAY:
            return newArrayDataType(resolveNamedDataTypeInternal(data_type->child, scope, self_data_type, memo, false),
                                    data_type->array_length);
        case AST_DATA_TYPE_KIND_SLICE:
            return newSliceDataType(resolveNamedDataTypeInternal(data_type->child, scope, self_data_type, memo, true));
        case AST_DATA_TYPE_KIND_FUNCTION: {
            ASTFunctionParameter *head = NULL;
            ASTFunctionParameter *tail = NULL;
            ASTFunctionParameter *parameter = data_type->parameters;
            while(parameter)
            {
                ASTFunctionParameter *new_parameter = (ASTFunctionParameter*) malloc(sizeof(ASTFunctionParameter));
                *new_parameter = *parameter;
                new_parameter->next = NULL;
                new_parameter->data_type = resolveNamedDataTypeInternal(parameter->data_type, scope, self_data_type, memo, true);

                if(head == NULL)
                    head = new_parameter;
                else
                    tail->next = new_parameter;
                tail = new_parameter;
                parameter = parameter->next;
            }

            return newFunctionDataType(head, data_type->is_variadic,
                                       resolveNamedDataTypeInternal(data_type->return_data_type, scope, self_data_type, memo, true));
        }
        case AST_DATA_TYPE_KIND_APPLY: {
            if(data_type->callee != NULL && data_type->callee->kind == AST_DATA_TYPE_KIND_NAMED)
            {
                VariableInfo *callee_variable = findVariableInfo(scope, data_type->callee->identifier);
                if(callee_variable != NULL && callee_variable->function_value != NULL)
                {
                    ScopeFrame *active_instantiation = findInstantiatingFunctionScope(scope, callee_variable->function_value);
                    if(active_instantiation != NULL)
                    {
                        if(active_instantiation->instantiating_type_result != NULL &&
                           allow_recursive_factory_result)
                            return active_instantiation->instantiating_type_result;

                        typeSystemAbortNoSpan("T1240",
                                              "recursive generic instantiation is not supported",
                                              "this recursive type use requires an explicit indirection such as `*T`, `&T`, `Function(...)`, or `[]T`");
                    }

                    TypeSystemExprType applied_type = instantiateFunctionCallExprType(
                        callee_variable->function_value,
                        buildTypeLiteralArgumentExprs(data_type->arguments, scope, self_data_type),
                        scope
                        );
                    if(applied_type.kind != TYPE_SYSTEM_EXPR_TYPE_TYPE)
                        typeSystemAbortNoSpan("T1203",
                                              "type application requires a constructor returning `Type`",
                                              NULL);
                    return cloneDataType(applied_type.data_type);
                }
            }

            ASTDataType *resolved_callee = resolveNamedDataTypeInternal(data_type->callee, scope, self_data_type, memo, false);
            ASTTypeArgument *resolved_arguments = NULL;
            ASTTypeArgument *resolved_tail = NULL;
            ASTTypeArgument *argument = data_type->arguments;
            while(argument)
            {
                ASTTypeArgument *new_argument = newASTTypeArgument(resolveNamedDataTypeInternal(argument->data_type, scope, self_data_type, memo, false));
                if(resolved_arguments == NULL)
                    resolved_arguments = new_argument;
                else
                    resolved_tail->next = new_argument;
                resolved_tail = new_argument;
                argument = argument->next;
            }
            return newAppliedDataType(resolved_callee, resolved_arguments);
        }
        default:
            typeSystemAbortNoSpan("ICE0201",
                                  "resolveNamedDataType hit unsupported AST data type kind",
                                  NULL);
    }
}

ASTDataType* resolveNamedDataType(ASTDataType *data_type, ScopeFrame *scope, ASTDataType *self_data_type)
{
    ResolveDataTypeEntry *memo = NULL;
    return resolveNamedDataTypeInternal(data_type, scope, self_data_type, &memo, false);
}

void bindCapturedValuesForInstantiation(ASTFunctionCapture *capture, ScopeFrame *inst_scope, ScopeFrame *outer_scope)
{
    while(capture)
    {
        VariableInfo *outer_variable = findVariableInfo(outer_scope, capture->identifier);
        if(outer_variable == NULL)
            diagnosticAbortFormatted("T1204",
                                     makeSourceSpan(capture->filename,
                                                    capture->line_number, capture->column_number,
                                                    capture->end_line_number, capture->end_column_number),
                                     "unknown capture",
                                     "unknown function capture `%s`",
                                     capture->identifier);

        VariableInfo *inst_variable = declareVariableInfo(inst_scope, capture->identifier);
        inst_variable->mutable = false;
        inst_variable->data_type = cloneDataType(outer_variable->data_type);
        inst_variable->type_value = cloneDataType(outer_variable->type_value);
        inst_variable->function_value = outer_variable->function_value;
        capture = capture->next;
    }
}

void bindCallArgumentsForInstantiation(ASTFunctionParameter *parameter, ASTNode *argument, ScopeFrame *inst_scope, ScopeFrame *outer_scope)
{
    while(parameter && argument)
    {
        VariableInfo *inst_variable = declareVariableInfo(inst_scope, parameter->identifier);
        inst_variable->mutable = false;

        ASTDataType *resolved_parameter_type = resolveNamedDataType(parameter->data_type, inst_scope, NULL);
        inst_variable->data_type = cloneDataType(resolved_parameter_type);

        if(resolved_parameter_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
           resolved_parameter_type->primary == AST_PRIMARY_DATA_TYPE_TYPE)
        {
            TypeSystemExprType argument_type = inferExprType(argument, outer_scope);
            if(argument_type.kind != TYPE_SYSTEM_EXPR_TYPE_TYPE)
                typeSystemAbortNode("T1205", argument,
                                    "expected a `Type` argument",
                                    "this argument does not evaluate to a type");
            inst_variable->type_value = cloneDataType(argument_type.data_type);
        }
        else if(argument->kind == AST_EXPR_FUNCTION)
        {
            inst_variable->function_value = argument;
        }
        else if(argument->kind == AST_EXPR_VARIABLE)
        {
            VariableInfo *outer_variable = findVariableInfo(outer_scope, argument->identifier);
            if(outer_variable != NULL)
            {
                inst_variable->type_value = cloneDataType(outer_variable->type_value);
                inst_variable->function_value = outer_variable->function_value;
                inst_variable->extern_value = outer_variable->extern_value;
            }
        }

        if(resolved_parameter_type->kind == AST_DATA_TYPE_KIND_REFERENCE)
        {
            if(!canBindReferenceArgument(argument, outer_scope, resolved_parameter_type))
                typeSystemAbortNode("T1220", argument,
                                    "function reference argument type mismatch",
                                    "argument cannot bind to the reference parameter");
        }
        else if(resolved_parameter_type->kind != AST_DATA_TYPE_KIND_PRIMARY ||
                resolved_parameter_type->primary != AST_PRIMARY_DATA_TYPE_TYPE)
        {
            if(!canImplicitConvertExprToType(argument, outer_scope, resolved_parameter_type))
                typeSystemAbortNode("T1221", argument,
                                    "function argument type mismatch",
                                    "argument cannot be implicitly converted to the parameter type");
        }

        parameter = parameter->next;
        argument = argument->next;
    }

    if(parameter != NULL || argument != NULL)
        typeSystemAbortNoSpan("T1206",
                              "function call argument count mismatch during instantiation",
                              NULL);
}

ASTNode* findReturnedExpr(ASTNode *function_expr)
{
    if(function_expr == NULL || function_expr->body == NULL)
        return NULL;

    ASTNode *stmt = function_expr->body->lhs;
    while(stmt)
    {
        if(stmt->kind == AST_STATEMENT_RETURN)
            return stmt->lhs;
        stmt = stmt->next;
    }
    return NULL;
}

ASTDataType* instantiateStructTypeExpr(ASTNode *expr, ScopeFrame *inst_scope)
{
    ASTDataType *struct_type = newStructDataType("", NULL);
    inst_scope->instantiating_type_result = struct_type;
    struct_type->members = resolveStructMembers(expr->members, inst_scope, struct_type);

    ASTStructMember *resolved_member = struct_type->members;
    ASTStructMember *original_member = expr->members;
    while(resolved_member != NULL && original_member != NULL)
    {
        if(original_member->data_type != NULL)
            resolved_member->data_type = resolveNamedDataType(original_member->data_type, inst_scope, struct_type);
        resolved_member = resolved_member->next;
        original_member = original_member->next;
    }

    return struct_type;
}

ASTDataType* instantiateTypeExprValue(ASTNode *expr, ScopeFrame *inst_scope)
{
    if(expr == NULL)
        typeSystemAbortNoSpan("T1207",
                              "expected a type-valued return expression",
                              NULL);

    if(expr->kind == AST_EXPR_STRUCT)
        return instantiateStructTypeExpr(expr, inst_scope);
    if(expr->kind == AST_EXPR_ENUM)
        return newEnumDataType("", cloneEnumVariants(expr->variants));
    if(expr->kind == AST_EXPR_TYPE_LITERAL)
        return resolveNamedDataType(expr->data_type, inst_scope, NULL);
    if(expr->kind == AST_EXPR_VARIABLE)
    {
        ASTDataType *builtin_type = builtinIdentifierToDataType(expr->identifier);
        if(builtin_type != NULL)
            return builtin_type;

        VariableInfo *variable_info = findVariableInfo(inst_scope, expr->identifier);
        if(variable_info != NULL && variable_info->type_value != NULL)
            return cloneDataType(variable_info->type_value);

        TypeInfo *type_info = findTypeInfo(inst_scope, expr->identifier);
        if(type_info != NULL)
            return cloneDataType(type_info->data_type);
    }

    if(expr->kind == AST_EXPR_CALL && expr->lhs->kind == AST_EXPR_VARIABLE)
    {
        VariableInfo *callee = findVariableInfo(inst_scope, expr->lhs->identifier);
        if(callee != NULL && callee->function_value != NULL)
        {
            ScopeFrame *nested_scope = newScopeFrame(inst_scope);
            bindCapturedValuesForInstantiation(callee->function_value->captures, nested_scope, inst_scope);
            bindCallArgumentsForInstantiation(callee->function_value->parameters, expr->rhs, nested_scope, inst_scope);
            ASTDataType *result = instantiateTypeExprValue(findReturnedExpr(callee->function_value), nested_scope);
            deleteScopeFrame(nested_scope);
            return result;
        }
    }

    if(expr->kind == AST_EXPR_CALL && expr->lhs->kind == AST_EXPR_MEMBER)
    {
        ASTNode *member_expr = expr->lhs;
        TypeSystemExprType owner_type = inferExprType(member_expr->lhs, inst_scope);
        ASTDataType *struct_type = NULL;
        if(owner_type.kind == TYPE_SYSTEM_EXPR_TYPE_TYPE)
            struct_type = owner_type.data_type;
        else if(owner_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE)
        {
            struct_type = owner_type.data_type;
            if(struct_type != NULL &&
               (struct_type->kind == AST_DATA_TYPE_KIND_POINTER || struct_type->kind == AST_DATA_TYPE_KIND_REFERENCE))
                struct_type = struct_type->child;
        }
        struct_type = resolveNamedDataType(struct_type, inst_scope, NULL);

        if(isStructDataType(struct_type))
        {
            ASTStructMember *member = findStructMember(struct_type, member_expr->identifier);
            if(member != NULL && member->value != NULL && member->value->kind == AST_EXPR_FUNCTION)
            {
                ScopeFrame *nested_scope = newScopeFrame(inst_scope);
                bindCapturedValuesForInstantiation(member->value->captures, nested_scope, inst_scope);
                bindCallArgumentsForInstantiation(member->value->parameters, expr->rhs, nested_scope, inst_scope);
                ASTDataType *result = instantiateTypeExprValue(findReturnedExpr(member->value), nested_scope);
                deleteScopeFrame(nested_scope);
                return result;
            }
        }
    }

    typeSystemAbortNode("T1208", expr,
                        "unsupported type-valued expression",
                        "this expression cannot be evaluated as a type");
}

ASTNode* buildTypeLiteralArgumentExprs(ASTTypeArgument *argument, ScopeFrame *scope, ASTDataType *self_data_type)
{
    ASTNode *head = NULL;
    ASTNode *tail = NULL;

    while(argument)
    {
        ASTNode *type_literal = newASTNode(AST_EXPR_TYPE_LITERAL);
        type_literal->data_type = resolveNamedDataType(argument->data_type, scope, self_data_type);

        if(head == NULL)
            head = type_literal;
        else
            tail->next = type_literal;
        tail = type_literal;
        argument = argument->next;
    }

    return head;
}

ASTNode* resolveFunctionValueExpr(ASTNode *expr, ScopeFrame *scope)
{
    if(expr == NULL)
        return NULL;

    if(expr->kind == AST_EXPR_FUNCTION)
        return expr;

    if(expr->kind == AST_EXPR_VARIABLE)
    {
        VariableInfo *variable_info = findVariableInfo(scope, expr->identifier);
        if(variable_info != NULL)
            return variable_info->function_value;
        return NULL;
    }

    if(expr->kind == AST_EXPR_MEMBER)
    {
        TypeSystemExprType owner_type = inferExprType(expr->lhs, scope);
        ASTDataType *struct_type = NULL;
        if(owner_type.kind == TYPE_SYSTEM_EXPR_TYPE_TYPE)
            struct_type = inferDeclaredTypeFromExpr(expr->lhs, scope);
        else if(owner_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE)
        {
            struct_type = owner_type.data_type;
            if(struct_type != NULL &&
               (struct_type->kind == AST_DATA_TYPE_KIND_POINTER || struct_type->kind == AST_DATA_TYPE_KIND_REFERENCE))
                struct_type = struct_type->child;
        }
        struct_type = resolveNamedDataType(struct_type, scope, NULL);
        if(!isStructDataType(struct_type))
            return NULL;

        ASTStructMember *member = findStructMember(struct_type, expr->identifier);
        if(member != NULL && member->value != NULL && member->value->kind == AST_EXPR_FUNCTION)
            return member->value;
    }

    return NULL;
}

ASTNode* resolveExternValueExpr(ASTNode *expr, ScopeFrame *scope)
{
    if(expr == NULL)
        return NULL;

    if(expr->kind == AST_EXPR_BUILTIN && strcmp(expr->identifier, "extern") == 0)
        return expr;

    if(expr->kind == AST_EXPR_VARIABLE)
    {
        VariableInfo *variable_info = findVariableInfo(scope, expr->identifier);
        if(variable_info != NULL)
            return variable_info->extern_value;
    }

    return NULL;
}

TypeSystemExprType instantiateFunctionCallExprType(ASTNode *function_value, ASTNode *call_arguments, ScopeFrame *outer_scope)
{
    ScopeFrame *inst_scope = newScopeFrame(outer_scope);
    inst_scope->instantiating_function = function_value;
    inst_scope->instantiation_site = call_arguments != NULL ? call_arguments : function_value;
    bindCapturedValuesForInstantiation(function_value->captures, inst_scope, outer_scope);
    bindCallArgumentsForInstantiation(function_value->parameters, call_arguments, inst_scope, outer_scope);

    ASTDataType *resolved_return_type = resolveNamedDataType(function_value->return_data_type, inst_scope, NULL);
    if(resolved_return_type != NULL &&
       resolved_return_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
       resolved_return_type->primary == AST_PRIMARY_DATA_TYPE_TYPE)
    {
        TypeSystemExprType result = newTypeExprType(instantiateTypeExprValue(findReturnedExpr(function_value), inst_scope));
        deleteScopeFrame(inst_scope);
        return result;
    }

    TypeSystemExprType result = newValueExprType(resolved_return_type);
    deleteScopeFrame(inst_scope);
    return result;
}

ASTDataType* instantiateFunctionCallResolvedFunctionType(ASTNode *function_value, ASTNode *call_arguments, ScopeFrame *outer_scope)
{
    ScopeFrame *inst_scope = newScopeFrame(outer_scope);
    bindCapturedValuesForInstantiation(function_value->captures, inst_scope, outer_scope);
    bindCallArgumentsForInstantiation(function_value->parameters, call_arguments, inst_scope, outer_scope);

    ASTFunctionParameter *resolved_head = NULL;
    ASTFunctionParameter *resolved_tail = NULL;
    for(ASTFunctionParameter *parameter = function_value->parameters; parameter != NULL; parameter = parameter->next)
    {
        ASTFunctionParameter *resolved_parameter = (ASTFunctionParameter*) malloc(sizeof(ASTFunctionParameter));
        memset(resolved_parameter, 0, sizeof(ASTFunctionParameter));
        resolved_parameter->filename = parameter->filename;
        resolved_parameter->line_number = parameter->line_number;
        resolved_parameter->column_number = parameter->column_number;
        resolved_parameter->end_line_number = parameter->end_line_number;
        resolved_parameter->end_column_number = parameter->end_column_number;
        strcpy(resolved_parameter->identifier, parameter->identifier);
        resolved_parameter->data_type = resolveNamedDataType(parameter->data_type, inst_scope, NULL);
        if(resolved_head == NULL)
            resolved_head = resolved_parameter;
        else
            resolved_tail->next = resolved_parameter;
        resolved_tail = resolved_parameter;
    }

    ASTDataType *resolved_return_type = resolveNamedDataType(function_value->return_data_type, inst_scope, NULL);
    ASTDataType *result = newFunctionDataType(resolved_head, function_value->is_variadic, resolved_return_type);
    deleteScopeFrame(inst_scope);
    return result;
}

bool canLiteralIntegerFitPrimary(long long int literal_integer, ASTPrimaryDataType target_primary)
{
    switch(target_primary)
    {
        case AST_PRIMARY_DATA_TYPE_I8: return literal_integer >= -128LL && literal_integer <= 127LL;
        case AST_PRIMARY_DATA_TYPE_I16: return literal_integer >= -32768LL && literal_integer <= 32767LL;
        case AST_PRIMARY_DATA_TYPE_I32: return literal_integer >= -2147483648LL && literal_integer <= 2147483647LL;
        case AST_PRIMARY_DATA_TYPE_I64: return true;
        case AST_PRIMARY_DATA_TYPE_U8: return literal_integer >= 0LL && literal_integer <= 255LL;
        case AST_PRIMARY_DATA_TYPE_U16: return literal_integer >= 0LL && literal_integer <= 65535LL;
        case AST_PRIMARY_DATA_TYPE_U32: return literal_integer >= 0LL && literal_integer <= 4294967295LL;
        case AST_PRIMARY_DATA_TYPE_U64: return literal_integer >= 0LL;
        case AST_PRIMARY_DATA_TYPE_CHAR: return literal_integer >= 0LL && literal_integer <= 255LL;
        default: return false;
    }
}

bool isLiteralIntegerZero(ASTNode *source_node)
{
    return source_node != NULL &&
           source_node->kind == AST_EXPR_LITERAL_INTEGER &&
           source_node->literal_integer == 0;
}

bool isZeroComparablePointerOrFunction(TypeSystemExprType value_type, TypeSystemExprType other_type, ASTNode *other_node)
{
    if(value_type.kind != TYPE_SYSTEM_EXPR_TYPE_VALUE || value_type.data_type == NULL)
        return false;

    if(other_type.kind != TYPE_SYSTEM_EXPR_TYPE_LITERAL_INTEGER || !isLiteralIntegerZero(other_node))
        return false;

    return value_type.data_type->kind == AST_DATA_TYPE_KIND_POINTER ||
           value_type.data_type->kind == AST_DATA_TYPE_KIND_FUNCTION;
}

ASTDataType* defaultIntegerDataType()
{
    return newPrimaryDataType(AST_PRIMARY_DATA_TYPE_I32);
}

ASTDataType* defaultFloatDataType()
{
    return newPrimaryDataType(AST_PRIMARY_DATA_TYPE_F64);
}

ASTDataType* getReferenceTargetType(ASTDataType *data_type)
{
    if(data_type->kind == AST_DATA_TYPE_KIND_REFERENCE)
        return data_type->child;
    return data_type;
}

bool isVoidDataType(ASTDataType *data_type)
{
    return data_type != NULL &&
           data_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
           data_type->primary == AST_PRIMARY_DATA_TYPE_VOID;
}

ASTDataType* inferExternBuiltinFunctionType(ASTNode *node, ScopeFrame *scope)
{
    if(node->lhs == NULL || node->lhs->next == NULL || node->lhs->next->next != NULL)
        typeSystemAbortNode("T1209", node,
                            "@extern expects exactly two arguments",
                            "expected `@extern(\"symbol\", Function(...))`");

    ASTNode *symbol_expr = node->lhs;
    ASTNode *type_expr = symbol_expr->next;
    if(symbol_expr->kind != AST_EXPR_LITERAL_STRING)
        typeSystemAbortNode("T1210", symbol_expr,
                            "@extern expects a string literal symbol name",
                            "first argument must be a string literal");

    TypeSystemExprType function_type_expr = inferExprType(type_expr, scope);
    if(function_type_expr.kind != TYPE_SYSTEM_EXPR_TYPE_TYPE)
        typeSystemAbortNode("T1211", type_expr,
                            "@extern expects a function type as its second argument",
                            "second argument must evaluate to a function type");

    ASTDataType *function_type = resolveNamedDataType(function_type_expr.data_type, scope, NULL);
    if(function_type == NULL || function_type->kind != AST_DATA_TYPE_KIND_FUNCTION)
        typeSystemAbortNode("T1212", type_expr,
                            "@extern expects a `Function(...)` type",
                            "second argument is not a function type");

    return function_type;
}

bool canExplicitConvertDataType(TypeSystemExprType source_type, ASTNode *source_node, ASTDataType *target_type)
{
    if(target_type == NULL || isInferDataType(target_type))
        return false;

    if(source_type.kind == TYPE_SYSTEM_EXPR_TYPE_LITERAL_INTEGER)
    {
        if(isOptionalDataType(target_type))
            return canExplicitConvertDataType(source_type, source_node, target_type->child);

        if(target_type->kind == AST_DATA_TYPE_KIND_POINTER)
            return isLiteralIntegerZero(source_node);

        if(target_type->kind == AST_DATA_TYPE_KIND_FUNCTION)
            return isLiteralIntegerZero(source_node);

        if(target_type->kind != AST_DATA_TYPE_KIND_PRIMARY)
            return false;

        if(isIntegerPrimary(target_type->primary) || isFloatPrimary(target_type->primary))
            return true;
        return false;
    }

    if(source_type.kind == TYPE_SYSTEM_EXPR_TYPE_LITERAL_FLOAT)
        return target_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
               (isFloatPrimary(target_type->primary) || isIntegerPrimary(target_type->primary));

    if(source_type.kind == TYPE_SYSTEM_EXPR_TYPE_NULL)
        return isOptionalDataType(target_type);

    if(source_type.kind != TYPE_SYSTEM_EXPR_TYPE_VALUE)
        return false;

    ASTDataType *source_data_type = source_type.data_type;
    if(source_data_type == NULL)
        return false;

    if(isSameDataType(source_data_type, target_type))
        return true;

    if(isOptionalDataType(target_type))
    {
        if(isOptionalDataType(source_data_type))
            return isSameDataType(source_data_type->child, target_type->child);
        return canExplicitConvertDataType(newValueExprType(source_data_type), source_node, target_type->child);
    }

    if(source_data_type->kind == AST_DATA_TYPE_KIND_PRIMARY && target_type->kind == AST_DATA_TYPE_KIND_PRIMARY)
    {
        ASTPrimaryDataType source_primary = source_data_type->primary;
        ASTPrimaryDataType target_primary = target_type->primary;

        if(isVoidPrimary(source_primary) || isVoidPrimary(target_primary) ||
           isBoolPrimary(source_primary) || isBoolPrimary(target_primary) ||
           isTypePrimary(source_primary) || isTypePrimary(target_primary))
            return false;

        if((isIntegerPrimary(source_primary) || isFloatPrimary(source_primary)) &&
           (isIntegerPrimary(target_primary) || isFloatPrimary(target_primary)))
            return true;

        return false;
    }

    if(source_data_type->kind == AST_DATA_TYPE_KIND_POINTER && target_type->kind == AST_DATA_TYPE_KIND_POINTER)
        return true;

    if(source_data_type->kind == AST_DATA_TYPE_KIND_SLICE &&
       target_type->kind == AST_DATA_TYPE_KIND_POINTER &&
       isSameDataType(source_data_type->child, target_type->child))
        return true;

    if(source_data_type->kind == AST_DATA_TYPE_KIND_POINTER &&
       target_type->kind == AST_DATA_TYPE_KIND_FUNCTION &&
       !target_type->is_variadic)
        return true;

    if(source_data_type->kind == AST_DATA_TYPE_KIND_FUNCTION &&
       target_type->kind == AST_DATA_TYPE_KIND_POINTER)
        return true;

    if(source_data_type->kind == AST_DATA_TYPE_KIND_SLICE &&
       target_type->kind == AST_DATA_TYPE_KIND_POINTER)
    {
        if(!isSameDataType(source_data_type->child, target_type->child))
            return false;
        return true;
    }

    if(source_node != NULL &&
       source_node->kind == AST_EXPR_LITERAL_STRING &&
       source_data_type->kind == AST_DATA_TYPE_KIND_ARRAY &&
       source_data_type->child != NULL &&
       source_data_type->child->kind == AST_DATA_TYPE_KIND_PRIMARY &&
       source_data_type->child->primary == AST_PRIMARY_DATA_TYPE_CHAR &&
       target_type->kind == AST_DATA_TYPE_KIND_POINTER &&
       target_type->child != NULL &&
       target_type->child->kind == AST_DATA_TYPE_KIND_PRIMARY &&
       target_type->child->primary == AST_PRIMARY_DATA_TYPE_CHAR)
        return true;

    if(source_data_type->kind == AST_DATA_TYPE_KIND_REFERENCE && target_type->kind == AST_DATA_TYPE_KIND_REFERENCE)
        return canExplicitConvertDataType(newValueExprType(source_data_type->child), source_node, target_type->child);

    if(source_data_type->kind == AST_DATA_TYPE_KIND_POINTER && target_type->kind == AST_DATA_TYPE_KIND_REFERENCE)
        return canExplicitConvertDataType(newValueExprType(source_data_type->child), source_node, target_type->child);

    if(source_data_type->kind == AST_DATA_TYPE_KIND_REFERENCE && target_type->kind == AST_DATA_TYPE_KIND_POINTER)
        return canExplicitConvertDataType(newValueExprType(source_data_type->child), source_node, target_type->child);

    return false;
}

ASTDataType* inferZeroBuiltinValueType(ASTNode *node, ScopeFrame *scope)
{
    if(node->lhs == NULL || node->lhs->next != NULL)
        typeSystemAbortNode("T1213", node,
                            "@zero expects exactly one argument",
                            "expected `@zero(Type)`");

    TypeSystemExprType type_expr = inferExprType(node->lhs, scope);
    if(type_expr.kind != TYPE_SYSTEM_EXPR_TYPE_TYPE)
        typeSystemAbortNode("T1214", node->lhs,
                            "@zero expects a type argument",
                            "argument must evaluate to a type");

    ASTDataType *value_type = resolveNamedDataType(type_expr.data_type, scope, NULL);
    if(value_type == NULL)
        typeSystemAbortNode("T1215", node->lhs,
                            "@zero could not resolve its type argument",
                            "the provided type could not be resolved");

    return value_type;
}

long long int inferTypeBuiltinLayoutValue(ASTNode *node, ScopeFrame *scope, const char *builtin_name,
                                          const char *usage_label, bool want_align)
{
    if(node->lhs == NULL || node->lhs->next != NULL)
        typeSystemAbortFormatted("T1249", node,
                                 usage_label,
                                 "@%s expects exactly one argument",
                                 builtin_name);

    TypeSystemExprType type_expr = inferExprType(node->lhs, scope);
    if(type_expr.kind != TYPE_SYSTEM_EXPR_TYPE_TYPE)
        typeSystemAbortFormatted("T1250", node->lhs,
                                 usage_label,
                                 "@%s expects a type argument",
                                 builtin_name);

    ASTDataType *value_type = resolveNamedDataType(type_expr.data_type, scope, NULL);
    if(value_type == NULL)
        typeSystemAbortFormatted("T1251", node->lhs,
                                 usage_label,
                                 "@%s could not resolve its type argument",
                                 builtin_name);

    if(want_align)
        return (long long int) moteTypeLayoutAlignment(value_type);
    return (long long int) moteTypeLayoutSize(value_type);
}

ASTDataType* inferSizeofBuiltinValueType(ASTNode *node, ScopeFrame *scope)
{
    inferTypeBuiltinLayoutValue(node, scope, "sizeof", "expected `@sizeof(Type)`", false);
    return newPrimaryDataType(AST_PRIMARY_DATA_TYPE_I64);
}

ASTDataType* inferAlignofBuiltinValueType(ASTNode *node, ScopeFrame *scope)
{
    inferTypeBuiltinLayoutValue(node, scope, "alignof", "expected `@alignof(Type)`", true);
    return newPrimaryDataType(AST_PRIMARY_DATA_TYPE_I64);
}

ASTDataType* inferAsBuiltinValueType(ASTNode *node, ScopeFrame *scope)
{
    if(node->lhs == NULL || node->lhs->next == NULL || node->lhs->next->next != NULL)
        typeSystemAbortNode("T1216", node,
                            "@as expects exactly two arguments",
                            "expected `@as(TargetType, value)`");

    ASTNode *target_type_expr = node->lhs;
    ASTNode *value_expr = target_type_expr->next;

    TypeSystemExprType target_type_value = inferExprType(target_type_expr, scope);
    if(target_type_value.kind != TYPE_SYSTEM_EXPR_TYPE_TYPE)
        typeSystemAbortNode("T1217", target_type_expr,
                            "@as expects a type as its first argument",
                            "first argument must evaluate to a type");

    ASTDataType *target_type = resolveNamedDataType(target_type_value.data_type, scope, NULL);
    if(target_type == NULL)
        typeSystemAbortNode("T1218", target_type_expr,
                            "@as could not resolve its target type",
                            "the target type could not be resolved");

    TypeSystemExprType source_type = inferExprType(value_expr, scope);
    if(!canExplicitConvertDataType(source_type, value_expr, target_type))
        typeSystemAbortNode("T1219", value_expr,
                            "invalid explicit conversion with @as",
                            "the source value cannot be explicitly converted to the target type");

    return target_type;
}

ASTDataType* inferLenBuiltinValueType(ASTNode *node, ScopeFrame *scope)
{
    if(node->lhs == NULL || node->lhs->next != NULL)
        typeSystemAbortNode("T1254", node,
                            "@len expects exactly one argument",
                            "expected `@len(slice_value)`");

    TypeSystemExprType operand_type = inferExprType(node->lhs, scope);
    if(operand_type.kind != TYPE_SYSTEM_EXPR_TYPE_VALUE || !isSliceDataType(operand_type.data_type))
        typeSystemAbortNode("T1255", node->lhs,
                            "@len expects a slice value",
                            "argument must have type `[]T`");

    return newPrimaryDataType(AST_PRIMARY_DATA_TYPE_I64);
}

ASTDataType* inferPtrAddBuiltinValueType(ASTNode *node, ScopeFrame *scope)
{
    if(node->lhs == NULL || node->lhs->next == NULL || node->lhs->next->next == NULL || node->lhs->next->next->next != NULL)
        typeSystemAbortNode("T1261", node,
                            "@ptr_add expects exactly three arguments",
                            "expected `@ptr_add(T, ptr, count)`");

    ASTNode *element_type_expr = node->lhs;
    ASTNode *pointer_expr = element_type_expr->next;
    ASTNode *count_expr = pointer_expr->next;

    TypeSystemExprType element_type_value = inferExprType(element_type_expr, scope);
    if(element_type_value.kind != TYPE_SYSTEM_EXPR_TYPE_TYPE)
        typeSystemAbortNode("T1262", element_type_expr,
                            "@ptr_add expects a type as its first argument",
                            "first argument must evaluate to a type");

    ASTDataType *element_type = resolveNamedDataType(element_type_value.data_type, scope, NULL);
    if(element_type == NULL)
        typeSystemAbortNode("T1263", element_type_expr,
                            "@ptr_add could not resolve its element type",
                            "the provided element type could not be resolved");

    TypeSystemExprType pointer_type = inferExprType(pointer_expr, scope);
    if(pointer_type.kind != TYPE_SYSTEM_EXPR_TYPE_VALUE ||
       pointer_type.data_type == NULL ||
       pointer_type.data_type->kind != AST_DATA_TYPE_KIND_POINTER ||
       !isSameDataType(pointer_type.data_type->child, element_type))
        typeSystemAbortNode("T1264", pointer_expr,
                            "@ptr_add expects a pointer to the given element type",
                            "second argument must have type `*T` or `*mut T` matching the first argument");

    TypeSystemExprType count_type = inferExprType(count_expr, scope);
    if(count_type.kind == TYPE_SYSTEM_EXPR_TYPE_TYPE ||
       (count_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE &&
        (count_type.data_type->kind != AST_DATA_TYPE_KIND_PRIMARY ||
         !isIntegerPrimary(count_type.data_type->primary))))
        typeSystemAbortNode("T1265", count_expr,
                            "@ptr_add expects an integer offset",
                            "third argument must be an integer value");

    return cloneDataType(pointer_type.data_type);
}

ASTDataType* inferPtrDiffBuiltinValueType(ASTNode *node, ScopeFrame *scope)
{
    if(node->lhs == NULL || node->lhs->next == NULL || node->lhs->next->next == NULL || node->lhs->next->next->next != NULL)
        typeSystemAbortNode("T1266", node,
                            "@ptr_diff expects exactly three arguments",
                            "expected `@ptr_diff(T, lhs, rhs)`");

    ASTNode *element_type_expr = node->lhs;
    ASTNode *lhs_expr = element_type_expr->next;
    ASTNode *rhs_expr = lhs_expr->next;

    TypeSystemExprType element_type_value = inferExprType(element_type_expr, scope);
    if(element_type_value.kind != TYPE_SYSTEM_EXPR_TYPE_TYPE)
        typeSystemAbortNode("T1267", element_type_expr,
                            "@ptr_diff expects a type as its first argument",
                            "first argument must evaluate to a type");

    ASTDataType *element_type = resolveNamedDataType(element_type_value.data_type, scope, NULL);
    if(element_type == NULL)
        typeSystemAbortNode("T1268", element_type_expr,
                            "@ptr_diff could not resolve its element type",
                            "the provided element type could not be resolved");

    TypeSystemExprType lhs_type = inferExprType(lhs_expr, scope);
    TypeSystemExprType rhs_type = inferExprType(rhs_expr, scope);
    if(lhs_type.kind != TYPE_SYSTEM_EXPR_TYPE_VALUE ||
       rhs_type.kind != TYPE_SYSTEM_EXPR_TYPE_VALUE ||
       lhs_type.data_type == NULL ||
       rhs_type.data_type == NULL ||
       lhs_type.data_type->kind != AST_DATA_TYPE_KIND_POINTER ||
       rhs_type.data_type->kind != AST_DATA_TYPE_KIND_POINTER ||
       !isSameDataType(lhs_type.data_type->child, element_type) ||
       !isSameDataType(rhs_type.data_type->child, element_type))
        typeSystemAbortNode("T1269", lhs_expr,
                            "@ptr_diff expects two pointers to the given element type",
                            "second and third arguments must both have type `*T` or `*mut T` matching the first argument");

    return newPrimaryDataType(AST_PRIMARY_DATA_TYPE_I64);
}

ASTDataType* inferSliceBuiltinValueType(ASTNode *node, ScopeFrame *scope)
{
    if(node->lhs == NULL || node->lhs->next == NULL || node->lhs->next->next == NULL || node->lhs->next->next->next != NULL)
        typeSystemAbortNode("T1256", node,
                            "@slice expects exactly three arguments",
                            "expected `@slice(T, ptr, len)`");

    ASTNode *element_type_expr = node->lhs;
    ASTNode *pointer_expr = element_type_expr->next;
    ASTNode *length_expr = pointer_expr->next;

    TypeSystemExprType element_type_value = inferExprType(element_type_expr, scope);
    if(element_type_value.kind != TYPE_SYSTEM_EXPR_TYPE_TYPE)
        typeSystemAbortNode("T1257", element_type_expr,
                            "@slice expects a type as its first argument",
                            "first argument must evaluate to a type");

    ASTDataType *element_type = resolveNamedDataType(element_type_value.data_type, scope, NULL);
    if(element_type == NULL)
        typeSystemAbortNode("T1258", element_type_expr,
                            "@slice could not resolve its element type",
                            "the provided element type could not be resolved");

    TypeSystemExprType pointer_type = inferExprType(pointer_expr, scope);
    if(pointer_type.kind != TYPE_SYSTEM_EXPR_TYPE_VALUE ||
       pointer_type.data_type == NULL ||
       pointer_type.data_type->kind != AST_DATA_TYPE_KIND_POINTER ||
       !pointer_type.data_type->mutable ||
       !isSameDataType(pointer_type.data_type->child, element_type))
        typeSystemAbortNode("T1259", pointer_expr,
                            "@slice expects a mutable pointer to the given element type",
                            "second argument must have type `*mut T` matching the first argument");

    TypeSystemExprType length_type = inferExprType(length_expr, scope);
    if(length_type.kind == TYPE_SYSTEM_EXPR_TYPE_TYPE ||
       (length_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE &&
        (length_type.data_type->kind != AST_DATA_TYPE_KIND_PRIMARY ||
         !isIntegerPrimary(length_type.data_type->primary))))
        typeSystemAbortNode("T1260", length_expr,
                            "@slice expects an integer length",
                            "third argument must be an integer value");

    return newSliceDataType(element_type);
}

ASTDataType* inferUnwrapBuiltinValueType(ASTNode *node, ScopeFrame *scope)
{
    if(node->lhs == NULL || node->lhs->next != NULL)
        typeSystemAbortNode("T1220", node,
                            "@unwrap expects exactly one argument",
                            "expected `@unwrap(optional_value)`");

    TypeSystemExprType operand_type = inferExprType(node->lhs, scope);
    if(operand_type.kind != TYPE_SYSTEM_EXPR_TYPE_VALUE || !isOptionalDataType(operand_type.data_type))
        typeSystemAbortNode("T1221", node->lhs,
                            "@unwrap expects an optional value",
                            "argument must have type `?T`");

    return cloneDataType(operand_type.data_type->child);
}

bool canImplicitConvertDataType(TypeSystemExprType source_type, ASTNode *source_node, ASTDataType *target_type)
{
    if(target_type == NULL || isInferDataType(target_type))
        return false;

    if(source_type.kind == TYPE_SYSTEM_EXPR_TYPE_NULL)
        return isOptionalDataType(target_type);

    if(source_type.kind == TYPE_SYSTEM_EXPR_TYPE_LITERAL_INTEGER)
    {
        if(isOptionalDataType(target_type))
            return canImplicitConvertDataType(source_type, source_node, target_type->child);

        if(target_type->kind == AST_DATA_TYPE_KIND_POINTER)
            return isLiteralIntegerZero(source_node);

        if(target_type->kind == AST_DATA_TYPE_KIND_FUNCTION)
            return isLiteralIntegerZero(source_node);

        if(target_type->kind != AST_DATA_TYPE_KIND_PRIMARY)
            return false;

        if(isIntegerPrimary(target_type->primary) || isBoolPrimary(target_type->primary))
            return canLiteralIntegerFitPrimary(source_node->literal_integer, target_type->primary);
        if(isFloatPrimary(target_type->primary))
            return true;
        return false;
    }

    if(source_type.kind == TYPE_SYSTEM_EXPR_TYPE_LITERAL_FLOAT)
    {
        if(isOptionalDataType(target_type))
            return canImplicitConvertDataType(source_type, source_node, target_type->child);
        return target_type->kind == AST_DATA_TYPE_KIND_PRIMARY && isFloatPrimary(target_type->primary);
    }

    if(source_type.kind == TYPE_SYSTEM_EXPR_TYPE_TYPE)
    {
        return target_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
               target_type->primary == AST_PRIMARY_DATA_TYPE_TYPE;
    }

    ASTDataType *source_data_type = source_type.data_type;
    if(source_data_type == NULL)
        return false;

    if(isSameDataType(source_data_type, target_type))
        return true;

    if(isOptionalDataType(target_type))
    {
        if(isOptionalDataType(source_data_type))
            return isSameDataType(source_data_type->child, target_type->child);
        return canImplicitConvertDataType(newValueExprType(source_data_type), source_node, target_type->child) ||
               isSameDataType(source_data_type, target_type->child);
    }

    if(source_data_type->kind == AST_DATA_TYPE_KIND_PRIMARY && target_type->kind == AST_DATA_TYPE_KIND_PRIMARY)
    {
        ASTPrimaryDataType source_primary = source_data_type->primary;
        ASTPrimaryDataType target_primary = target_type->primary;

        if(isBoolPrimary(source_primary) || isBoolPrimary(target_primary))
            return false;
        if(isVoidPrimary(source_primary) || isVoidPrimary(target_primary))
            return false;

        if(isCharPrimary(source_primary))
            return isCharPrimary(target_primary) || isIntegerPrimary(target_primary) || isFloatPrimary(target_primary);

        if(isSignedIntegerPrimary(source_primary) && isSignedIntegerPrimary(target_primary))
            return getIntegerPrimaryWidth(source_primary) <= getIntegerPrimaryWidth(target_primary);

        if(isUnsignedIntegerPrimary(source_primary) && isUnsignedIntegerPrimary(target_primary))
            return getIntegerPrimaryWidth(source_primary) <= getIntegerPrimaryWidth(target_primary);

        if(isIntegerPrimary(source_primary) && isFloatPrimary(target_primary))
            return true;

        if(isFloatPrimary(source_primary) && isFloatPrimary(target_primary))
            return getFloatPrimaryWidth(source_primary) <= getFloatPrimaryWidth(target_primary);

        return false;
    }

    if(source_data_type->kind == AST_DATA_TYPE_KIND_POINTER && target_type->kind == AST_DATA_TYPE_KIND_POINTER)
    {
        bool mutable_compatible = source_data_type->mutable == target_type->mutable ||
                                  (source_data_type->mutable && !target_type->mutable);
        if(!mutable_compatible)
            return false;

        if(isSameDataType(source_data_type->child, target_type->child))
            return true;

        if(isVoidDataType(source_data_type->child) || isVoidDataType(target_type->child))
            return true;

        return false;
    }

    if(source_data_type->kind == AST_DATA_TYPE_KIND_FUNCTION && target_type->kind == AST_DATA_TYPE_KIND_FUNCTION)
        return isSameFunctionSignature(source_data_type, target_type);

    if(source_data_type->kind == AST_DATA_TYPE_KIND_SLICE &&
       target_type->kind == AST_DATA_TYPE_KIND_POINTER &&
       isSameDataType(source_data_type->child, target_type->child))
        return true;

    if(source_node != NULL &&
       source_node->kind == AST_EXPR_LITERAL_STRING &&
       source_data_type->kind == AST_DATA_TYPE_KIND_ARRAY &&
       source_data_type->child != NULL &&
       source_data_type->child->kind == AST_DATA_TYPE_KIND_PRIMARY &&
       source_data_type->child->primary == AST_PRIMARY_DATA_TYPE_CHAR &&
       target_type->kind == AST_DATA_TYPE_KIND_POINTER &&
       target_type->child != NULL &&
       target_type->child->kind == AST_DATA_TYPE_KIND_PRIMARY &&
       target_type->child->primary == AST_PRIMARY_DATA_TYPE_CHAR)
        return true;

    if(source_data_type->kind == AST_DATA_TYPE_KIND_REFERENCE && target_type->kind == AST_DATA_TYPE_KIND_REFERENCE)
    {
        if(source_data_type->mutable && !target_type->mutable && isSameDataType(source_data_type->child, target_type->child))
            return true;
        return false;
    }

    return false;
}

bool canImplicitConvertExprToType(ASTNode *expr, ScopeFrame *scope, ASTDataType *target_type)
{
    if(expr == NULL || target_type == NULL || isInferDataType(target_type))
        return false;

    if(isOptionalDataType(target_type) && expr->kind != AST_EXPR_LITERAL_NULL)
        return canImplicitConvertExprToType(expr, scope, target_type->child) ||
               canImplicitConvertDataType(inferExprType(expr, scope), expr, target_type);

    if(expr->kind == AST_EXPR_PARENTHESIS)
        return canImplicitConvertExprToType(expr->lhs, scope, target_type);

    if(target_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
       (isIntegerPrimary(target_type->primary) || isFloatPrimary(target_type->primary)))
    {
        switch(expr->kind)
        {
            case AST_EXPR_UNARY_PLUS:
            case AST_EXPR_UNARY_MINUS:
                if(expr->kind == AST_EXPR_UNARY_MINUS)
                {
                    ResolvedOperatorOverload overload = {0};
                    if(resolveOperatorOverload(AST_OPERATOR_SUB, expr->lhs, NULL, scope, &overload))
                        return isSameDataType(overload.result_type, target_type);
                }
                return canImplicitConvertExprToType(expr->lhs, scope, target_type);
            case AST_EXPR_ADD:
            case AST_EXPR_SUB:
            case AST_EXPR_MUL:
            case AST_EXPR_DIV: {
                ASTOperatorKind operator_kind = AST_OPERATOR_ADD;
                if(expr->kind == AST_EXPR_SUB) operator_kind = AST_OPERATOR_SUB;
                else if(expr->kind == AST_EXPR_MUL) operator_kind = AST_OPERATOR_MUL;
                else if(expr->kind == AST_EXPR_DIV) operator_kind = AST_OPERATOR_DIV;

                ResolvedOperatorOverload overload = {0};
                if(resolveOperatorOverload(operator_kind, expr->lhs, expr->rhs, scope, &overload))
                    return isSameDataType(overload.result_type, target_type);

                return canImplicitConvertExprToType(expr->lhs, scope, target_type) &&
                       canImplicitConvertExprToType(expr->rhs, scope, target_type);
            }
            default:
                break;
        }
    }

    if(expr->kind == AST_EXPR_ARRAY_LITERAL)
    {
        if(target_type->kind != AST_DATA_TYPE_KIND_ARRAY)
            return false;

        long long int length = 0;
        ASTNode *element = expr->lhs;
        while(element)
        {
            if(!canImplicitConvertExprToType(element, scope, target_type->child))
                return false;
            length++;
            element = element->next;
        }

        return length == target_type->array_length;
    }

    if(expr->kind == AST_EXPR_STRUCT_LITERAL)
    {
        TypeSystemExprType type_expr = inferExprType(expr->lhs, scope);
        if(type_expr.kind != TYPE_SYSTEM_EXPR_TYPE_TYPE || !isStructDataType(type_expr.data_type))
            return false;
        if(!isSameDataType(type_expr.data_type, target_type))
            return false;

        ASTStructMember *member = target_type->members;
        while(member)
        {
            if(member->value == NULL)
            {
                ASTStructLiteralField *field = expr->struct_literal_fields;
                while(field && strcmp(field->identifier, member->identifier) != 0)
                    field = field->next;
                if(field == NULL)
                    return false;
                if(!canImplicitConvertExprToType(field->value, scope, member->data_type))
                    return false;
            }
            member = member->next;
        }

        ASTStructLiteralField *field = expr->struct_literal_fields;
        while(field)
        {
            ASTStructMember *declared_member = findStructMember(target_type, field->identifier);
            if(declared_member == NULL || declared_member->value != NULL)
                return false;
            field = field->next;
        }

        return true;
    }

    return canImplicitConvertDataType(inferExprType(expr, scope), expr, target_type);
}

MOTE_NORETURN void typeErrorBinaryOperator(ASTNode *node, const char *operator_name,
                                           TypeSystemExprType lhs_type, TypeSystemExprType rhs_type)
{
    char lhs_buffer[256] = {0};
    char rhs_buffer[256] = {0};
    typeSystemDescribeExprType(lhs_type, lhs_buffer, sizeof(lhs_buffer));
    typeSystemDescribeExprType(rhs_type, rhs_buffer, sizeof(rhs_buffer));

    Diagnostic diagnostic = diagnosticMake(DIAGNOSTIC_SEVERITY_ERROR,
                                           "T1001",
                                           astNodeSourceSpan(node),
                                           "invalid binary operator operands");
    diagnosticSetPrimaryLabel(&diagnostic,
                              "operator %s cannot be applied to %s and %s",
                              operator_name, lhs_buffer, rhs_buffer);
    diagnosticAbort(diagnostic);
}

MOTE_NORETURN void typeErrorUnaryOperator(ASTNode *node, const char *operator_name, TypeSystemExprType operand_type)
{
    char operand_buffer[256] = {0};
    typeSystemDescribeExprType(operand_type, operand_buffer, sizeof(operand_buffer));

    Diagnostic diagnostic = diagnosticMake(DIAGNOSTIC_SEVERITY_ERROR,
                                           "T1002",
                                           astNodeSourceSpan(node),
                                           "invalid unary operator operand");
    diagnosticSetPrimaryLabel(&diagnostic,
                              "operator %s cannot be applied to %s",
                              operator_name, operand_buffer);
    diagnosticAbort(diagnostic);
}

MOTE_NORETURN void typeErrorAssign(ASTNode *node, ASTNode *source_node, TypeSystemExprType source_type, ASTDataType *target_type)
{
    char source_buffer[256] = {0};
    char target_buffer[256] = {0};
    typeSystemDescribeExprType(source_type, source_buffer, sizeof(source_buffer));
    typeSystemDescribeExprType(newValueExprType(target_type), target_buffer, sizeof(target_buffer));

    Diagnostic diagnostic = diagnosticMake(DIAGNOSTIC_SEVERITY_ERROR,
                                           "T1003",
                                           astNodeSourceSpan(source_node),
                                           "cannot implicitly convert assigned value");
    diagnosticSetPrimaryLabel(&diagnostic,
                              "cannot implicitly convert %s to %s",
                              source_buffer, target_buffer);
    if(node->identifier[0] != '\0')
        diagnosticAddNote(&diagnostic, "assignment target: `%s`", node->identifier);
    diagnosticAbort(diagnostic);
}

ASTDataType* normalizeNumericDataType(TypeSystemExprType expr_type)
{
    if(expr_type.kind == TYPE_SYSTEM_EXPR_TYPE_LITERAL_INTEGER)
        return defaultIntegerDataType();
    if(expr_type.kind == TYPE_SYSTEM_EXPR_TYPE_LITERAL_FLOAT)
        return defaultFloatDataType();
    if(expr_type.kind == TYPE_SYSTEM_EXPR_TYPE_TYPE)
    {
        diagnosticAbortSimple("T1004",
                              "type cannot be used as a numeric expression",
                              makeSourceSpan(NULL, 0, 0, 0, 0),
                              NULL);
    }
    if(expr_type.data_type->kind == AST_DATA_TYPE_KIND_PRIMARY && isCharPrimary(expr_type.data_type->primary))
        return defaultIntegerDataType();
    return cloneDataType(expr_type.data_type);
}

TypeSystemExprType getCommonNumericType(ASTNode *node, TypeSystemExprType lhs_type, TypeSystemExprType rhs_type, const char *operator_name)
{
    if(lhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE &&
       rhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE &&
       lhs_type.data_type != NULL &&
       rhs_type.data_type != NULL &&
       lhs_type.data_type->kind == AST_DATA_TYPE_KIND_NAMED &&
       rhs_type.data_type->kind == AST_DATA_TYPE_KIND_NAMED &&
       strcmp(lhs_type.data_type->identifier, rhs_type.data_type->identifier) == 0)
    {
        return newValueExprType(lhs_type.data_type);
    }

    if(lhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_LITERAL_FLOAT &&
       rhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE &&
       rhs_type.data_type != NULL &&
       rhs_type.data_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
       isFloatPrimary(rhs_type.data_type->primary))
        return newValueExprType(rhs_type.data_type);

    if(rhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_LITERAL_FLOAT &&
       lhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE &&
       lhs_type.data_type != NULL &&
       lhs_type.data_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
       isFloatPrimary(lhs_type.data_type->primary))
        return newValueExprType(lhs_type.data_type);

    if(lhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_LITERAL_INTEGER &&
       rhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE &&
       rhs_type.data_type != NULL &&
       rhs_type.data_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
       isIntegerPrimary(rhs_type.data_type->primary) &&
       node->lhs != NULL &&
       canLiteralIntegerFitPrimary(node->lhs->literal_integer, rhs_type.data_type->primary))
        return newValueExprType(rhs_type.data_type);

    if(rhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_LITERAL_INTEGER &&
       lhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE &&
       lhs_type.data_type != NULL &&
       lhs_type.data_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
       isIntegerPrimary(lhs_type.data_type->primary) &&
       node->rhs != NULL &&
       canLiteralIntegerFitPrimary(node->rhs->literal_integer, lhs_type.data_type->primary))
        return newValueExprType(lhs_type.data_type);

    ASTDataType *lhs = normalizeNumericDataType(lhs_type);
    ASTDataType *rhs = normalizeNumericDataType(rhs_type);

    if(lhs->kind != AST_DATA_TYPE_KIND_PRIMARY || rhs->kind != AST_DATA_TYPE_KIND_PRIMARY)
        typeErrorBinaryOperator(node, operator_name, lhs_type, rhs_type);

    if(isBoolPrimary(lhs->primary) || isBoolPrimary(rhs->primary))
        typeErrorBinaryOperator(node, operator_name, lhs_type, rhs_type);
    if(isVoidPrimary(lhs->primary) || isVoidPrimary(rhs->primary))
        typeErrorBinaryOperator(node, operator_name, lhs_type, rhs_type);

    if(isFloatPrimary(lhs->primary) || isFloatPrimary(rhs->primary))
    {
        ASTPrimaryDataType target_primary = AST_PRIMARY_DATA_TYPE_F64;
        if(isFloatPrimary(lhs->primary) && isFloatPrimary(rhs->primary))
            target_primary = getFloatPrimaryWidth(lhs->primary) >= getFloatPrimaryWidth(rhs->primary) ? lhs->primary : rhs->primary;
        else if(isFloatPrimary(lhs->primary))
            target_primary = lhs->primary;
        else if(isFloatPrimary(rhs->primary))
            target_primary = rhs->primary;
        return newValueExprType(newPrimaryDataType(target_primary));
    }

    if(isSignedIntegerPrimary(lhs->primary) && isSignedIntegerPrimary(rhs->primary))
    {
        ASTPrimaryDataType target_primary = getIntegerPrimaryWidth(lhs->primary) >= getIntegerPrimaryWidth(rhs->primary)
                                            ? lhs->primary : rhs->primary;
        return newValueExprType(newPrimaryDataType(target_primary));
    }

    if(isUnsignedIntegerPrimary(lhs->primary) && isUnsignedIntegerPrimary(rhs->primary))
    {
        ASTPrimaryDataType target_primary = getIntegerPrimaryWidth(lhs->primary) >= getIntegerPrimaryWidth(rhs->primary)
                                            ? lhs->primary : rhs->primary;
        return newValueExprType(newPrimaryDataType(target_primary));
    }

    typeErrorBinaryOperator(node, operator_name, lhs_type, rhs_type);
    return newValueExprType(defaultIntegerDataType());
}

bool isAddressableExpr(ASTNode *node)
{
    return node->kind == AST_EXPR_VARIABLE || node->kind == AST_EXPR_DEREF ||
           node->kind == AST_EXPR_MEMBER || node->kind == AST_EXPR_INDEX;
}

bool isMutableAddressableExpr(ASTNode *node, ScopeFrame *scope)
{
    if(node->kind == AST_EXPR_VARIABLE)
    {
        VariableInfo *variable_info = findVariableInfo(scope, node->identifier);
        if(variable_info == NULL)
            return false;
        return variable_info->mutable ||
               (variable_info->data_type != NULL &&
                variable_info->data_type->kind == AST_DATA_TYPE_KIND_REFERENCE &&
                variable_info->data_type->mutable);
    }

    if(node->kind == AST_EXPR_DEREF)
    {
        TypeSystemExprType ptr_type = inferExprType(node->lhs, scope);
        if(ptr_type.kind != TYPE_SYSTEM_EXPR_TYPE_VALUE)
            return false;
        if(ptr_type.data_type->kind == AST_DATA_TYPE_KIND_POINTER || ptr_type.data_type->kind == AST_DATA_TYPE_KIND_REFERENCE)
            return ptr_type.data_type->mutable;
    }

    if(node->kind == AST_EXPR_MEMBER)
    {
        TypeSystemExprType owner_type = inferExprType(node->lhs, scope);
        if(owner_type.kind != TYPE_SYSTEM_EXPR_TYPE_VALUE)
            return false;

        ASTDataType *owner_data_type = owner_type.data_type;
        if(owner_data_type->kind == AST_DATA_TYPE_KIND_POINTER || owner_data_type->kind == AST_DATA_TYPE_KIND_REFERENCE)
            return owner_data_type->mutable;

        return isMutableAddressableExpr(node->lhs, scope);
    }

    if(node->kind == AST_EXPR_INDEX)
    {
        TypeSystemExprType owner_type = inferExprType(node->lhs, scope);
        if(owner_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE && isSliceDataType(owner_type.data_type))
            return true;
        return isMutableAddressableExpr(node->lhs, scope);
    }

    return false;
}

bool canBindReferenceArgument(ASTNode *argument, ScopeFrame *scope, ASTDataType *parameter_type)
{
    if(parameter_type->kind != AST_DATA_TYPE_KIND_REFERENCE)
        return false;

    if(argument->kind == AST_EXPR_ADDRESS_OF || argument->kind == AST_EXPR_ADDRESS_OF_MUT)
    {
        if(parameter_type->mutable && argument->kind != AST_EXPR_ADDRESS_OF_MUT)
            return false;

        TypeSystemExprType argument_type = inferExprType(argument, scope);
        if(argument_type.kind != TYPE_SYSTEM_EXPR_TYPE_VALUE)
            return false;
        if(argument_type.data_type->kind == AST_DATA_TYPE_KIND_POINTER)
            return isSameDataType(argument_type.data_type->child, parameter_type->child);
        return isSameDataType(getReferenceTargetType(argument_type.data_type), parameter_type->child);
    }

    if(!isAddressableExpr(argument))
        return false;

    if(parameter_type->mutable && !isMutableAddressableExpr(argument, scope))
        return false;

    TypeSystemExprType argument_type = inferExprType(argument, scope);
    if(argument_type.kind != TYPE_SYSTEM_EXPR_TYPE_VALUE)
        return false;

    return isSameDataType(getReferenceTargetType(argument_type.data_type), parameter_type->child);
}

int countFunctionParameters(ASTFunctionParameter *parameter)
{
    int count = 0;
    while(parameter)
    {
        count ++;
        parameter = parameter->next;
    }
    return count;
}

bool isVariadicPromotableScalar(ASTDataType *data_type)
{
    if(data_type == NULL)
        return false;

    if(data_type->kind == AST_DATA_TYPE_KIND_PRIMARY)
    {
        switch(data_type->primary)
        {
            case AST_PRIMARY_DATA_TYPE_BOOL:
            case AST_PRIMARY_DATA_TYPE_CHAR:
            case AST_PRIMARY_DATA_TYPE_I8:
            case AST_PRIMARY_DATA_TYPE_I16:
            case AST_PRIMARY_DATA_TYPE_U8:
            case AST_PRIMARY_DATA_TYPE_U16:
            case AST_PRIMARY_DATA_TYPE_F32:
                return true;
            default:
                return false;
        }
    }

    return isEnumDataType(data_type);
}

ASTDataType* variadicPromotedDataType(ASTDataType *data_type)
{
    if(data_type == NULL)
        return NULL;

    if(isEnumDataType(data_type))
        return newPrimaryDataType(AST_PRIMARY_DATA_TYPE_I32);

    if(data_type->kind != AST_DATA_TYPE_KIND_PRIMARY)
        return cloneDataType(data_type);

    switch(data_type->primary)
    {
        case AST_PRIMARY_DATA_TYPE_BOOL:
        case AST_PRIMARY_DATA_TYPE_CHAR:
        case AST_PRIMARY_DATA_TYPE_I8:
        case AST_PRIMARY_DATA_TYPE_I16:
        case AST_PRIMARY_DATA_TYPE_U8:
        case AST_PRIMARY_DATA_TYPE_U16:
            return newPrimaryDataType(AST_PRIMARY_DATA_TYPE_I32);
        case AST_PRIMARY_DATA_TYPE_F32:
            return newPrimaryDataType(AST_PRIMARY_DATA_TYPE_F64);
        default:
            return cloneDataType(data_type);
    }
}

ASTDataType* variadicPromotedExprType(TypeSystemExprType expr_type)
{
    if(expr_type.kind == TYPE_SYSTEM_EXPR_TYPE_LITERAL_INTEGER)
        return defaultIntegerDataType();
    if(expr_type.kind == TYPE_SYSTEM_EXPR_TYPE_LITERAL_FLOAT)
        return defaultFloatDataType();
    if(expr_type.kind != TYPE_SYSTEM_EXPR_TYPE_VALUE)
        return NULL;
    return variadicPromotedDataType(expr_type.data_type);
}

void checkFunctionCallArguments(ASTFunctionParameter *parameter, ASTNode *argument, ScopeFrame *scope, bool is_variadic)
{
    while(parameter && argument)
    {
        TypeSystemExprType argument_type = inferExprType(argument, scope);
        if(parameter->data_type->kind == AST_DATA_TYPE_KIND_REFERENCE)
        {
            if(!canBindReferenceArgument(argument, scope, parameter->data_type))
                typeSystemAbortNode("T1220", argument,
                                    "function reference argument type mismatch",
                                    "argument cannot bind to the reference parameter");
        }
        else if(!canImplicitConvertExprToType(argument, scope, parameter->data_type))
            typeSystemAbortNode("T1221", argument,
                                "function argument type mismatch",
                                "argument cannot be implicitly converted to the parameter type");
        parameter = parameter->next;
        argument = argument->next;
    }

    if(parameter != NULL)
        typeSystemAbortNoSpan("T1222", "function argument count mismatch", "too few arguments were provided");

    if(!is_variadic && argument != NULL)
        typeSystemAbortNoSpan("T1223", "function argument count mismatch", "too many arguments were provided");

    while(argument)
    {
        TypeSystemExprType argument_type = inferExprType(argument, scope);
        if(argument_type.kind == TYPE_SYSTEM_EXPR_TYPE_LITERAL_INTEGER ||
           argument_type.kind == TYPE_SYSTEM_EXPR_TYPE_LITERAL_FLOAT)
        {
            argument = argument->next;
            continue;
        }

        if(argument_type.kind != TYPE_SYSTEM_EXPR_TYPE_VALUE)
        {
            typeSystemAbortNode("T1224", argument,
                                "variadic argument must be a runtime value",
                                "type values cannot be passed to variadic parameters");
        }
        if(argument_type.data_type->kind == AST_DATA_TYPE_KIND_REFERENCE)
        {
            typeSystemAbortNode("T1225", argument,
                                "variadic argument cannot be a reference type",
                                "pass the referenced value instead");
        }
        if(argument_type.data_type->kind == AST_DATA_TYPE_KIND_ARRAY)
        {
            typeSystemAbortNode("T1226", argument,
                                "variadic argument cannot be an array value",
                                "array values are not supported in variadic calls");
        }
        argument = argument->next;
    }
}

bool canBindMethodReceiver(ASTNode *receiver, ScopeFrame *scope, ASTDataType *parameter_type, ASTDataType *owner_type)
{
    TypeSystemExprType receiver_type = inferExprType(receiver, scope);

    if(parameter_type->kind == AST_DATA_TYPE_KIND_REFERENCE)
    {
        if(!isAddressableExpr(receiver))
            return false;
        if(parameter_type->mutable && !isMutableAddressableExpr(receiver, scope))
            return false;
        return receiver_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE &&
               isSameDataType(getReferenceTargetType(receiver_type.data_type), parameter_type->child);
    }

    if(parameter_type->kind == AST_DATA_TYPE_KIND_POINTER)
    {
        if(receiver_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE &&
           receiver_type.data_type->kind == AST_DATA_TYPE_KIND_POINTER &&
           canImplicitConvertDataType(receiver_type, receiver, parameter_type))
            return true;

        return isAddressableExpr(receiver) &&
               (!parameter_type->mutable || isMutableAddressableExpr(receiver, scope)) &&
               receiver_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE &&
               isSameDataType(getReferenceTargetType(receiver_type.data_type), parameter_type->child);
    }

    return receiver_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE &&
           isSameDataType(receiver_type.data_type, parameter_type) &&
           isSameDataType(owner_type, parameter_type);
}

static bool operatorSignatureMatches(ASTNode *function_value,
                                     ASTNode *lhs_expr,
                                     ASTNode *rhs_expr,
                                     ScopeFrame *scope,
                                     ASTDataType **out_result_type)
{
    if(function_value == NULL || function_value->data_type == NULL)
        return false;

    ASTDataType *function_type = function_value->data_type;
    if(function_type->kind != AST_DATA_TYPE_KIND_FUNCTION || function_type->is_variadic)
        return false;

    ASTFunctionParameter *parameter = function_type->parameters;
    if(parameter == NULL)
        return false;
    ASTDataType *lhs_target_type = resolveNamedDataType(parameter->data_type, scope, NULL);
    bool lhs_ok = canImplicitConvertExprToType(lhs_expr, scope, lhs_target_type);
    if(!lhs_ok)
        return false;

    parameter = parameter->next;
    if(rhs_expr == NULL)
    {
        if(parameter != NULL)
            return false;
    }
    else
    {
        ASTDataType *rhs_target_type = parameter != NULL ? resolveNamedDataType(parameter->data_type, scope, NULL) : NULL;
        bool rhs_ok = parameter != NULL && canImplicitConvertExprToType(rhs_expr, scope, rhs_target_type);
        if(parameter == NULL || !rhs_ok)
            return false;
        parameter = parameter->next;
        if(parameter != NULL)
            return false;
    }

    if(out_result_type != NULL)
        *out_result_type = cloneDataType(function_type->return_data_type);
    return true;
}

static bool resolveOperatorOverload(ASTOperatorKind operator_kind,
                                    ASTNode *lhs_expr,
                                    ASTNode *rhs_expr,
                                    ScopeFrame *scope,
                                    ResolvedOperatorOverload *out)
{
    if(out != NULL)
    {
        out->function_value = NULL;
        out->result_type = NULL;
    }

    ScopeFrame *current = scope;
    while(current != NULL)
    {
        for(int i = 0; i < current->variable_count; i++)
        {
            VariableInfo *variable = &(current->variable_infos[i]);
            if(variable->operator_kind != operator_kind || variable->function_value == NULL)
                continue;

            ASTDataType *result_type = NULL;
            if(!operatorSignatureMatches(variable->function_value, lhs_expr, rhs_expr, scope, &result_type))
                continue;

            if(out != NULL)
            {
                out->function_value = variable->function_value;
                out->result_type = result_type;
            }
            return true;
        }
        current = current->parent;
    }

    return false;
}

TypeSystemExprType inferExprType(ASTNode *node, ScopeFrame *scope)
{
    switch(node->kind)
    {
        case AST_EXPR_LITERAL_BOOL:
            return newValueExprType(newPrimaryDataType(AST_PRIMARY_DATA_TYPE_BOOL));
        case AST_EXPR_LITERAL_NULL:
            return newNullExprType();
        case AST_EXPR_LITERAL_CHAR:
            return newValueExprType(newPrimaryDataType(AST_PRIMARY_DATA_TYPE_CHAR));
        case AST_EXPR_LITERAL_STRING:
            return newValueExprType(newArrayDataType(
                newPrimaryDataType(AST_PRIMARY_DATA_TYPE_CHAR),
                strlen(node->literal_string)
            ));
        case AST_EXPR_LITERAL_INTEGER:
            return newLiteralIntegerExprType();
        case AST_EXPR_LITERAL_FLOAT:
            return newLiteralFloatExprType();
        case AST_EXPR_TYPE_LITERAL:
            return newTypeExprType(node->data_type);
        case AST_EXPR_BUILTIN:
            if(strcmp(node->identifier, "extern") == 0)
                return newValueExprType(inferExternBuiltinFunctionType(node, scope));
            if(strcmp(node->identifier, "zero") == 0)
                return newValueExprType(inferZeroBuiltinValueType(node, scope));
            if(strcmp(node->identifier, "len") == 0)
                return newValueExprType(inferLenBuiltinValueType(node, scope));
            if(strcmp(node->identifier, "ptr_add") == 0)
                return newValueExprType(inferPtrAddBuiltinValueType(node, scope));
            if(strcmp(node->identifier, "ptr_diff") == 0)
                return newValueExprType(inferPtrDiffBuiltinValueType(node, scope));
            if(strcmp(node->identifier, "sizeof") == 0)
                return newValueExprType(inferSizeofBuiltinValueType(node, scope));
            if(strcmp(node->identifier, "alignof") == 0)
                return newValueExprType(inferAlignofBuiltinValueType(node, scope));
            if(strcmp(node->identifier, "as") == 0)
                return newValueExprType(inferAsBuiltinValueType(node, scope));
            if(strcmp(node->identifier, "slice") == 0)
                return newValueExprType(inferSliceBuiltinValueType(node, scope));
            if(strcmp(node->identifier, "unwrap") == 0)
                return newValueExprType(inferUnwrapBuiltinValueType(node, scope));
            typeSystemAbortFormatted("T1227", node,
                                     "unknown builtin",
                                     "unknown builtin `@%s`",
                                     node->identifier);
        case AST_EXPR_VARIABLE: {
            if(strcmp(node->identifier, "Self") == 0)
                return newTypeExprType(newNamedDataType("Self"));

            ASTDataType *builtin_type = builtinIdentifierToDataType(node->identifier);
            if(builtin_type != NULL)
                return newTypeExprType(builtin_type);

            VariableInfo *variable_info = findVariableInfo(scope, node->identifier);
            if(variable_info != NULL)
            {
                if(variable_info->type_value != NULL)
                    return newTypeExprType(variable_info->type_value);

                ASTDataType *variable_data_type = variable_info->data_type;
                if(variable_data_type->kind == AST_DATA_TYPE_KIND_REFERENCE)
                    return newValueExprType(variable_data_type->child);
                return newValueExprType(variable_data_type);
            }

            TypeInfo *type_info = findTypeInfo(scope, node->identifier);
            if(type_info != NULL)
                return newTypeExprType(type_info->data_type);

            typeSystemAbortFormatted("T1228", node,
                                     "undeclared variable",
                                     "type inference found undeclared variable `%s`",
                                     node->identifier);
        }
        case AST_EXPR_FUNCTION:
            return newValueExprType(node->data_type);
        case AST_EXPR_ENUM:
            return newTypeExprType(node->data_type);
        case AST_EXPR_STRUCT:
            return newTypeExprType(node->data_type);
        case AST_EXPR_ARRAY_LITERAL: {
            ASTNode *element = node->lhs;
            if(element == NULL)
                typeSystemAbortNode("T1229", node,
                                    "empty array literal requires an explicit array type",
                                    "add an explicit array type annotation");

            TypeSystemExprType first_type = inferExprType(element, scope);
            ASTDataType *element_type = inferDeclaredTypeFromExpr(element, scope);
            long long int length = 0;
            while(element)
            {
                TypeSystemExprType current_type = inferExprType(element, scope);
                if(!canImplicitConvertDataType(current_type, element, element_type))
                    typeSystemAbortNode("T1230", element,
                                        "array literal element type mismatch",
                                        "this element does not match the inferred array element type");
                length ++;
                element = element->next;
            }

            return newValueExprType(newArrayDataType(element_type, length));
        }
        case AST_EXPR_STRUCT_LITERAL: {
            TypeSystemExprType type_expr = inferExprType(node->lhs, scope);
            if(type_expr.kind != TYPE_SYSTEM_EXPR_TYPE_TYPE || !isStructDataType(type_expr.data_type))
                typeSystemAbortNode("T1231", node,
                                    "struct literal requires a struct type",
                                    "the expression before `{ ... }` is not a struct type");

            ASTStructMember *member = type_expr.data_type->members;
            while(member)
            {
                if(member->value == NULL)
                {
                    ASTStructLiteralField *field = node->struct_literal_fields;
                    while(field && strcmp(field->identifier, member->identifier) != 0)
                        field = field->next;
                    if(field == NULL)
                        typeSystemAbortFormatted("T1232", node,
                                                 "missing struct field",
                                                 "missing field `%s` in struct literal",
                                                 member->identifier);

                    if(!canImplicitConvertExprToType(field->value, scope, member->data_type))
                        typeSystemAbortFormatted("T1233", field->value,
                                                 "struct field type mismatch",
                                                 "struct field `%s` has an incompatible value",
                                                 member->identifier);
                }
                member = member->next;
            }

            ASTStructLiteralField *field = node->struct_literal_fields;
            while(field)
            {
                ASTStructMember *declared_member = findStructMember(type_expr.data_type, field->identifier);
                if(declared_member == NULL || declared_member->value != NULL)
                    typeSystemAbortFormatted("T1234", field->value != NULL ? field->value : node,
                                             "unknown struct field",
                                             "unknown struct field `%s`",
                                             field->identifier);
                field = field->next;
            }

            return newValueExprType(type_expr.data_type);
        }
        case AST_EXPR_MEMBER: {
            TypeSystemExprType owner_type = inferExprType(node->lhs, scope);
            ASTDataType *owner_data_type = NULL;

            if(owner_type.kind == TYPE_SYSTEM_EXPR_TYPE_TYPE)
                owner_data_type = inferDeclaredTypeFromExpr(node->lhs, scope);
            else if(owner_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE)
            {
                owner_data_type = owner_type.data_type;
                if(owner_data_type->kind == AST_DATA_TYPE_KIND_POINTER || owner_data_type->kind == AST_DATA_TYPE_KIND_REFERENCE)
                    owner_data_type = owner_data_type->child;
            }

            if(isEnumDataType(owner_data_type))
            {
                if(owner_type.kind != TYPE_SYSTEM_EXPR_TYPE_TYPE)
                    typeSystemAbortNode("T1235", node,
                                        "enum variant access requires the enum type",
                                        "use `EnumType.Variant` syntax");

                if(findEnumVariant(owner_data_type, node->identifier) == NULL)
                    typeSystemAbortFormatted("T1236", node,
                                             "unknown enum variant",
                                             "unknown enum variant `%s`",
                                             node->identifier);

                return newValueExprType(owner_data_type);
            }

            if(isSliceDataType(owner_data_type))
                typeSystemAbortFormatted("T1238", node,
                                         "slice members are not exposed",
                                         "use `@len(slice)` or `@as(*T, slice)` instead of `.%s`",
                                         node->identifier);

            ASTDataType *struct_type = resolveNamedDataType(owner_data_type, scope, NULL);
            if(!isStructDataType(struct_type))
                typeSystemAbortNode("T1237", node,
                                    "member access requires a struct type",
                                    "the receiver is not a struct");

            ASTStructMember *member = findStructMember(struct_type, node->identifier);
            if(member == NULL)
                typeSystemAbortFormatted("T1238", node,
                                         "unknown struct member",
                                         "unknown struct member `%s`",
                                         node->identifier);

            if(owner_type.kind == TYPE_SYSTEM_EXPR_TYPE_TYPE && member->value == NULL)
                typeSystemAbortFormatted("T1239", node,
                                         "instance field accessed on type",
                                         "struct field `%s` cannot be accessed on the type itself",
                                         node->identifier);

            if(member->data_type->kind == AST_DATA_TYPE_KIND_REFERENCE)
                return newValueExprType(member->data_type->child);
            return newValueExprType(member->data_type);
        }
        case AST_EXPR_INDEX: {
            TypeSystemExprType owner_type = inferExprType(node->lhs, scope);
            if(owner_type.kind != TYPE_SYSTEM_EXPR_TYPE_VALUE)
                typeSystemAbortNode("T1240", node,
                                    "indexing requires a value receiver",
                                    "the indexed expression is not a runtime value");

            ASTDataType *owner_data_type = owner_type.data_type;
            if(owner_data_type->kind == AST_DATA_TYPE_KIND_POINTER || owner_data_type->kind == AST_DATA_TYPE_KIND_REFERENCE)
                owner_data_type = owner_data_type->child;

            if(!isArrayDataType(owner_data_type) && !isSliceDataType(owner_data_type))
                typeSystemAbortNode("T1241", node,
                                    "indexing requires an array or slice type",
                                    "the indexed expression is not an array or slice");

            TypeSystemExprType index_type = inferExprType(node->rhs, scope);
            if(index_type.kind == TYPE_SYSTEM_EXPR_TYPE_TYPE ||
               (index_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE &&
                (index_type.data_type->kind != AST_DATA_TYPE_KIND_PRIMARY ||
                 !isIntegerPrimary(index_type.data_type->primary))))
                typeSystemAbortNode("T1242", node->rhs,
                                    "index must be an integer",
                                    "this index expression is not an integer");

            return newValueExprType(owner_data_type->child);
        }
        case AST_EXPR_CALL: {
            if(node->lhs->kind == AST_EXPR_VARIABLE)
            {
                VariableInfo *callee_variable = findVariableInfo(scope, node->lhs->identifier);
                if(callee_variable != NULL && callee_variable->function_value != NULL)
                    return instantiateFunctionCallExprType(callee_variable->function_value, node->rhs, scope);
            }

            if(node->lhs->kind == AST_EXPR_MEMBER)
            {
                ASTNode *member_node = node->lhs;
                TypeSystemExprType owner_type = inferExprType(member_node->lhs, scope);
                ASTDataType *struct_type = NULL;
                if(owner_type.kind == TYPE_SYSTEM_EXPR_TYPE_TYPE)
                    struct_type = owner_type.data_type;
                else if(owner_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE)
                {
                    struct_type = owner_type.data_type;
                    if(struct_type != NULL &&
                       (struct_type->kind == AST_DATA_TYPE_KIND_POINTER || struct_type->kind == AST_DATA_TYPE_KIND_REFERENCE))
                        struct_type = struct_type->child;
                }
                struct_type = resolveNamedDataType(struct_type, scope, NULL);

                if(isStructDataType(struct_type))
                {
                    ASTStructMember *member = findStructMember(struct_type, member_node->identifier);
                    if(member != NULL && member->value != NULL && member->value->kind == AST_EXPR_FUNCTION)
                        return instantiateFunctionCallExprType(member->value, node->rhs, scope);
                }
            }

            if(node->lhs->kind == AST_EXPR_MEMBER)
            {
                ASTNode *member_node = node->lhs;
                TypeSystemExprType owner_type = inferExprType(member_node->lhs, scope);
                ASTDataType *struct_type = NULL;
                bool through_type = owner_type.kind == TYPE_SYSTEM_EXPR_TYPE_TYPE;
                if(through_type)
                    struct_type = inferDeclaredTypeFromExpr(member_node->lhs, scope);
                else if(owner_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE)
                {
                    struct_type = owner_type.data_type;
                    if(struct_type->kind == AST_DATA_TYPE_KIND_POINTER || struct_type->kind == AST_DATA_TYPE_KIND_REFERENCE)
                        struct_type = struct_type->child;
                }
                struct_type = resolveNamedDataType(struct_type, scope, NULL);

                if(isStructDataType(struct_type))
                {
                    ASTStructMember *member = findStructMember(struct_type, member_node->identifier);
                    if(member != NULL && member->data_type != NULL &&
                       member->data_type->kind == AST_DATA_TYPE_KIND_FUNCTION)
                    {
                        ASTFunctionParameter *parameter = member->data_type->parameters;
                        if(!through_type && parameter != NULL &&
                           canBindMethodReceiver(member_node->lhs, scope, parameter->data_type, struct_type))
                        {
                            checkFunctionCallArguments(parameter->next, node->rhs, scope, member->data_type->is_variadic);
                            return newValueExprType(member->data_type->return_data_type);
                        }
                    }
                }
            }

            TypeSystemExprType callee_type = inferExprType(node->lhs, scope);
            if(callee_type.kind != TYPE_SYSTEM_EXPR_TYPE_VALUE || callee_type.data_type->kind != AST_DATA_TYPE_KIND_FUNCTION)
                typeSystemAbortNode("T1243", node,
                                    "called expression is not a function",
                                    "this callee does not have a function type");

            checkFunctionCallArguments(callee_type.data_type->parameters, node->rhs, scope, callee_type.data_type->is_variadic);

            return newValueExprType(callee_type.data_type->return_data_type);
        }
        case AST_EXPR_PARENTHESIS:
            return inferExprType(node->lhs, scope);
        case AST_EXPR_UNARY_PLUS:
        case AST_EXPR_UNARY_MINUS: {
            TypeSystemExprType operand_type = inferExprType(node->lhs, scope);
            if(operand_type.kind == TYPE_SYSTEM_EXPR_TYPE_LITERAL_INTEGER)
                return newLiteralIntegerExprType();
            if(operand_type.kind == TYPE_SYSTEM_EXPR_TYPE_LITERAL_FLOAT)
                return newLiteralFloatExprType();
            if(operand_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE &&
               operand_type.data_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
               (isIntegerPrimary(operand_type.data_type->primary) || isFloatPrimary(operand_type.data_type->primary)))
                return newValueExprType(operand_type.data_type);
            if(node->kind == AST_EXPR_UNARY_MINUS)
            {
                ResolvedOperatorOverload overload = {0};
                if(resolveOperatorOverload(AST_OPERATOR_SUB, node->lhs, NULL, scope, &overload))
                    return newValueExprType(overload.result_type);
            }
            typeErrorUnaryOperator(node, node->kind == AST_EXPR_UNARY_PLUS ? "+" : "-", operand_type);
        } break;
        case AST_EXPR_UNARY_LOGICAL_NOT: {
            TypeSystemExprType operand_type = inferExprType(node->lhs, scope);
            if(operand_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE &&
               operand_type.data_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
               isBoolPrimary(operand_type.data_type->primary))
                return newValueExprType(newPrimaryDataType(AST_PRIMARY_DATA_TYPE_BOOL));
            typeErrorUnaryOperator(node, "!", operand_type);
        } break;
        case AST_EXPR_UNARY_BIT_NOT: {
            TypeSystemExprType operand_type = inferExprType(node->lhs, scope);
            if(operand_type.kind == TYPE_SYSTEM_EXPR_TYPE_LITERAL_INTEGER)
                return newValueExprType(defaultIntegerDataType());
            if(operand_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE &&
               operand_type.data_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
               isIntegerPrimary(operand_type.data_type->primary))
            {
                if(isCharPrimary(operand_type.data_type->primary))
                    return newValueExprType(defaultIntegerDataType());
                return newValueExprType(operand_type.data_type);
            }
            typeErrorUnaryOperator(node, "~", operand_type);
        } break;
        case AST_EXPR_ADDRESS_OF:
        case AST_EXPR_ADDRESS_OF_MUT: {
            TypeSystemExprType operand_type = inferExprType(node->lhs, scope);
            if(operand_type.kind == TYPE_SYSTEM_EXPR_TYPE_TYPE)
            {
                return newTypeExprType(newWrappedDataType(
                    AST_DATA_TYPE_KIND_REFERENCE,
                    node->kind == AST_EXPR_ADDRESS_OF_MUT,
                    cloneDataType(operand_type.data_type)
                ));
            }

            if(!isAddressableExpr(node->lhs))
                typeSystemAbortNode("T1244", node,
                                    "cannot take address of non-addressable expression",
                                    "this expression has no stable address");

            if(node->kind == AST_EXPR_ADDRESS_OF_MUT && !isMutableAddressableExpr(node->lhs, scope))
                typeSystemAbortNode("T1245", node,
                                    "cannot take mutable address of immutable expression",
                                    "the target expression is not mutable");

            if(operand_type.kind != TYPE_SYSTEM_EXPR_TYPE_VALUE)
                typeSystemAbortNode("T1246", node,
                                    "cannot take address of literal",
                                    "literals do not have an addressable storage location");

            bool mutable_pointer = node->kind == AST_EXPR_ADDRESS_OF_MUT || isMutableAddressableExpr(node->lhs, scope);

            return newValueExprType(newWrappedDataType(
                AST_DATA_TYPE_KIND_POINTER,
                mutable_pointer,
                cloneDataType(getReferenceTargetType(operand_type.data_type))
            ));
        } break;
        case AST_EXPR_DEREF: {
            TypeSystemExprType operand_type = inferExprType(node->lhs, scope);
            if(operand_type.kind == TYPE_SYSTEM_EXPR_TYPE_TYPE)
            {
                return newTypeExprType(newWrappedDataType(
                    AST_DATA_TYPE_KIND_POINTER,
                    false,
                    cloneDataType(operand_type.data_type)
                ));
            }

            if(operand_type.kind != TYPE_SYSTEM_EXPR_TYPE_VALUE ||
               (operand_type.data_type->kind != AST_DATA_TYPE_KIND_POINTER &&
                operand_type.data_type->kind != AST_DATA_TYPE_KIND_REFERENCE))
            {
                typeErrorUnaryOperator(node, "*", operand_type);
            }

            return newValueExprType(operand_type.data_type->child);
        } break;
        case AST_EXPR_MUL:
        case AST_EXPR_DIV:
        case AST_EXPR_ADD:
        case AST_EXPR_SUB: {
            TypeSystemExprType lhs_type = inferExprType(node->lhs, scope);
            TypeSystemExprType rhs_type = inferExprType(node->rhs, scope);
            const char *operator_name = "+";
            ASTOperatorKind operator_kind = AST_OPERATOR_ADD;
            if(node->kind == AST_EXPR_MUL) operator_name = "*";
            else if(node->kind == AST_EXPR_DIV) operator_name = "/";
            else if(node->kind == AST_EXPR_SUB) operator_name = "-";
            if(node->kind == AST_EXPR_MUL) operator_kind = AST_OPERATOR_MUL;
            else if(node->kind == AST_EXPR_DIV) operator_kind = AST_OPERATOR_DIV;
            else if(node->kind == AST_EXPR_SUB) operator_kind = AST_OPERATOR_SUB;

            if(!isBuiltinNumericOperandType(lhs_type) || !isBuiltinNumericOperandType(rhs_type))
            {
                ResolvedOperatorOverload overload = {0};
                if(resolveOperatorOverload(operator_kind, node->lhs, node->rhs, scope, &overload))
                    return newValueExprType(overload.result_type);
            }
            return getCommonNumericType(node, lhs_type, rhs_type, operator_name);
        }
        case AST_EXPR_MOD:
        case AST_EXPR_SHIFT_LEFT:
        case AST_EXPR_SHIFT_RIGHT:
        case AST_EXPR_BIT_AND:
        case AST_EXPR_BIT_OR:
        case AST_EXPR_BIT_XOR: {
            TypeSystemExprType common = getCommonNumericType(node,
                inferExprType(node->lhs, scope),
                inferExprType(node->rhs, scope),
                node->kind == AST_EXPR_MOD ? "%" :
                node->kind == AST_EXPR_SHIFT_LEFT ? "<<" :
                node->kind == AST_EXPR_SHIFT_RIGHT ? ">>" :
                node->kind == AST_EXPR_BIT_AND ? "&" :
                node->kind == AST_EXPR_BIT_OR ? "|" : "^");
            if(common.data_type->kind != AST_DATA_TYPE_KIND_PRIMARY || !isIntegerPrimary(common.data_type->primary))
                typeErrorBinaryOperator(node, "%", inferExprType(node->lhs, scope), inferExprType(node->rhs, scope));
            return common;
        }
        case AST_EXPR_LOGICAL_AND:
        case AST_EXPR_LOGICAL_OR: {
            TypeSystemExprType lhs_type = inferExprType(node->lhs, scope);
            TypeSystemExprType rhs_type = inferExprType(node->rhs, scope);
            if(lhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE && rhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE &&
               lhs_type.data_type->kind == AST_DATA_TYPE_KIND_PRIMARY && rhs_type.data_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
               isBoolPrimary(lhs_type.data_type->primary) && isBoolPrimary(rhs_type.data_type->primary))
                return newValueExprType(newPrimaryDataType(AST_PRIMARY_DATA_TYPE_BOOL));
            typeErrorBinaryOperator(node, node->kind == AST_EXPR_LOGICAL_AND ? "&&" : "||", lhs_type, rhs_type);
        } break;
        case AST_EXPR_EQUAL:
        case AST_EXPR_NOT_EQUAL: {
            TypeSystemExprType lhs_type = inferExprType(node->lhs, scope);
            TypeSystemExprType rhs_type = inferExprType(node->rhs, scope);
            if(lhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_NULL && rhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_NULL)
                return newValueExprType(newPrimaryDataType(AST_PRIMARY_DATA_TYPE_BOOL));
            if(lhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_NULL &&
               rhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE &&
               isOptionalDataType(rhs_type.data_type))
                return newValueExprType(newPrimaryDataType(AST_PRIMARY_DATA_TYPE_BOOL));
            if(rhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_NULL &&
               lhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE &&
               isOptionalDataType(lhs_type.data_type))
                return newValueExprType(newPrimaryDataType(AST_PRIMARY_DATA_TYPE_BOOL));
            if(lhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE && rhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE &&
               (isOptionalDataType(lhs_type.data_type) || isOptionalDataType(rhs_type.data_type)))
                typeSystemAbortNode("T1248", node,
                                    "optional values currently only support comparison with `null`",
                                    "compare `?T` values using `== null` or `!= null`");
            if(isZeroComparablePointerOrFunction(lhs_type, rhs_type, node->rhs) ||
               isZeroComparablePointerOrFunction(rhs_type, lhs_type, node->lhs))
                return newValueExprType(newPrimaryDataType(AST_PRIMARY_DATA_TYPE_BOOL));
            if(lhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE && rhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE &&
               isSameDataType(lhs_type.data_type, rhs_type.data_type))
                return newValueExprType(newPrimaryDataType(AST_PRIMARY_DATA_TYPE_BOOL));

            if(node->kind == AST_EXPR_EQUAL || node->kind == AST_EXPR_NOT_EQUAL)
            {
                ResolvedOperatorOverload overload = {0};
                if(resolveOperatorOverload(AST_OPERATOR_EQ, node->lhs, node->rhs, scope, &overload))
                    return newValueExprType(newPrimaryDataType(AST_PRIMARY_DATA_TYPE_BOOL));
            }

            getCommonNumericType(node, lhs_type, rhs_type, node->kind == AST_EXPR_EQUAL ? "==" : "!=");
            return newValueExprType(newPrimaryDataType(AST_PRIMARY_DATA_TYPE_BOOL));
        }
        case AST_EXPR_LESS:
        case AST_EXPR_LESS_EQUAL:
        case AST_EXPR_GREATER:
        case AST_EXPR_GREATER_EQUAL: {
            getCommonNumericType(node,
                inferExprType(node->lhs, scope),
                inferExprType(node->rhs, scope),
                node->kind == AST_EXPR_LESS ? "<" :
                node->kind == AST_EXPR_LESS_EQUAL ? "<=" :
                node->kind == AST_EXPR_GREATER ? ">" : ">=");
            return newValueExprType(newPrimaryDataType(AST_PRIMARY_DATA_TYPE_BOOL));
        }
        default:
            typeSystemAbortFormatted("ICE0202", node,
                                     NULL,
                                     "inferExprType hit unsupported AST node kind %s",
                                     astNodeKindToString(node->kind));
    }
}

ASTDataType* inferDeclaredTypeFromExpr(ASTNode *expr, ScopeFrame *scope)
{
    TypeSystemExprType expr_type = inferExprType(expr, scope);
    if(expr_type.kind == TYPE_SYSTEM_EXPR_TYPE_NULL)
        typeSystemAbortNode("T1247", expr,
                            "cannot infer a type from `null` alone",
                            "add an explicit optional type like `?T`");
    if(expr_type.kind == TYPE_SYSTEM_EXPR_TYPE_LITERAL_INTEGER)
        return defaultIntegerDataType();
    if(expr_type.kind == TYPE_SYSTEM_EXPR_TYPE_LITERAL_FLOAT)
        return defaultFloatDataType();
    if(expr_type.kind == TYPE_SYSTEM_EXPR_TYPE_TYPE)
        return cloneDataType(expr_type.data_type);
    return cloneDataType(expr_type.data_type);
}

static ASTStructMember* resolveStructMembersInternal(ASTStructMember *member, ScopeFrame *scope,
                                                     ASTDataType *self_data_type,
                                                     ResolveDataTypeEntry **memo,
                                                     bool allow_recursive_factory_result)
{
    ASTStructMember *head = NULL;
    ASTStructMember *tail = NULL;

    while(member)
    {
        ASTStructMember *new_member = (ASTStructMember*) malloc(sizeof(ASTStructMember));
        memset(new_member, 0, sizeof(ASTStructMember));
        new_member->filename = member->filename;
        new_member->line_number = member->line_number;
        new_member->column_number = member->column_number;
        strcpy(new_member->identifier, member->identifier);
        new_member->value = member->value;
        if(member->data_type)
            new_member->data_type = resolveNamedDataTypeInternal(member->data_type, scope, self_data_type, memo,
                                                                 allow_recursive_factory_result);

        if(head == NULL)
            head = new_member;
        else
            tail->next = new_member;
        tail = new_member;
        member = member->next;
    }

    return head;
}

ASTStructMember* resolveStructMembers(ASTStructMember *member, ScopeFrame *scope, ASTDataType *self_data_type)
{
    ResolveDataTypeEntry *memo = NULL;
    return resolveStructMembersInternal(member, scope, self_data_type, &memo, false);
}

#endif /* TYPE_SYSTEM_H */
