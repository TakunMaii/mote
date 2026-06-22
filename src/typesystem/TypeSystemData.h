#ifndef TYPE_SYSTEM_DATA_H
#define TYPE_SYSTEM_DATA_H

#include "TypeSystemShared.h"

static ASTDataType* typeSystemResolvePredeclaredVariableType(VariableInfo *variable_info, ScopeFrame *scope)
{
    if(variable_info == NULL || variable_info->data_type == NULL)
        return variable_info != NULL ? variable_info->data_type : NULL;
    if(!isInferDataType(variable_info->data_type))
    {
        if(variable_info->type_value == NULL &&
           variable_info->value_expr != NULL &&
           variable_info->data_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
           variable_info->data_type->primary == AST_PRIMARY_DATA_TYPE_TYPE)
        {
            if(variable_info->resolving)
                typeSystemAbortNode("T1248", variable_info->value_expr,
                                    "cyclic top-level value dependency is not supported",
                                    "break the cycle by adding an explicit type or refactoring the initialization");

            variable_info->resolving = true;
            TypeSystemExprType resolved_expr_type = inferExprType(variable_info->value_expr, scope);
            if(resolved_expr_type.kind == TYPE_SYSTEM_EXPR_TYPE_TYPE)
                variable_info->type_value = cloneDataType(resolved_expr_type.data_type);
            variable_info->resolving = false;
        }
        return variable_info->data_type;
    }
    if(variable_info->value_expr == NULL)
        return variable_info->data_type;
    if(variable_info->resolving)
        typeSystemAbortNode("T1248", variable_info->value_expr,
                            "cyclic top-level value dependency is not supported",
                            "break the cycle by adding an explicit type or refactoring the initialization");

    variable_info->resolving = true;
    TypeSystemExprType resolved_expr_type = inferExprType(variable_info->value_expr, scope);
    ASTDataType *resolved_type = NULL;
    if(resolved_expr_type.kind == TYPE_SYSTEM_EXPR_TYPE_LITERAL_INTEGER ||
       resolved_expr_type.kind == TYPE_SYSTEM_EXPR_TYPE_LITERAL_FLOAT ||
       resolved_expr_type.kind == TYPE_SYSTEM_EXPR_TYPE_NULL)
        resolved_type = inferDeclaredTypeFromExpr(variable_info->value_expr, scope);
    else
        resolved_type = cloneDataType(resolved_expr_type.data_type);

    variable_info->data_type = cloneDataType(resolved_type);
    if(resolved_expr_type.kind == TYPE_SYSTEM_EXPR_TYPE_TYPE)
        variable_info->type_value = cloneDataType(resolved_expr_type.data_type);
    else
        variable_info->type_value = NULL;
    if(variable_info->function_value == NULL)
        variable_info->function_value = resolveFunctionValueExpr(variable_info->value_expr, scope);
    if(variable_info->extern_value == NULL)
        variable_info->extern_value = resolveExternValueExpr(variable_info->value_expr, scope);
    variable_info->resolving = false;
    return variable_info->data_type;
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
    if(strcmp(identifier, "string") == 0) return newStringDataType();
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
        case AST_DATA_TYPE_KIND_STRING:
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

bool isStringDataType(ASTDataType *data_type)
{
    return data_type != NULL && data_type->kind == AST_DATA_TYPE_KIND_STRING;
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
            return sizeof(void*);
        case AST_DATA_TYPE_KIND_FUNCTION:
            return sizeof(void*) * 2;
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
        case AST_DATA_TYPE_KIND_STRING: {
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
            return sizeof(void*);
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
        case AST_DATA_TYPE_KIND_STRING: {
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
        if(!isSameDataTypeInternal(lhs_member->data_type, rhs_member->data_type, memo))
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
        case AST_DATA_TYPE_KIND_STRING:
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


#endif /* TYPE_SYSTEM_DATA_H */
