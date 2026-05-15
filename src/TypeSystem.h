#ifndef TYPE_SYSTEM_H
#define TYPE_SYSTEM_H

#include "AST.h"
#include "SymbolTable.h"
#include <stdbool.h>
#include <stdio.h>

typedef enum TypeSystemExprTypeKind {
    TYPE_SYSTEM_EXPR_TYPE_VALUE,
    TYPE_SYSTEM_EXPR_TYPE_TYPE,
    TYPE_SYSTEM_EXPR_TYPE_LITERAL_INTEGER,
    TYPE_SYSTEM_EXPR_TYPE_LITERAL_FLOAT,
} TypeSystemExprTypeKind;

typedef struct TypeSystemExprType {
    TypeSystemExprTypeKind kind;
    ASTDataType *data_type;
} TypeSystemExprType;

TypeSystemExprType inferExprType(ASTNode *node, ScopeFrame *scope);
bool isSameDataType(ASTDataType *lhs, ASTDataType *rhs);

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

TypeSystemExprType newLiteralFloatExprType()
{
    TypeSystemExprType expr_type = {0};
    expr_type.kind = TYPE_SYSTEM_EXPR_TYPE_LITERAL_FLOAT;
    return expr_type;
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

bool isStructDataType(ASTDataType *data_type)
{
    return data_type != NULL && data_type->kind == AST_DATA_TYPE_KIND_STRUCT;
}

bool isEnumDataType(ASTDataType *data_type)
{
    return data_type != NULL && data_type->kind == AST_DATA_TYPE_KIND_ENUM;
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
            printf("getIntegerPrimaryWidth: unsupported type %s\n", astPrimaryDataTypeToString(primary));
            exit(1);
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
            printf("getFloatPrimaryWidth: unsupported type %s\n", astPrimaryDataTypeToString(primary));
            exit(1);
    }
}

bool isSameFunctionSignature(ASTDataType *lhs, ASTDataType *rhs)
{
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

bool isSameStructType(ASTDataType *lhs, ASTDataType *rhs)
{
    if(lhs->identifier[0] != '\0' || rhs->identifier[0] != '\0')
        return strcmp(lhs->identifier, rhs->identifier) == 0;

    ASTStructMember *lhs_member = lhs->members;
    ASTStructMember *rhs_member = rhs->members;
    while(lhs_member && rhs_member)
    {
        if(strcmp(lhs_member->identifier, rhs_member->identifier) != 0)
            return false;
        if(!isSameDataType(lhs_member->data_type, rhs_member->data_type))
            return false;
        lhs_member = lhs_member->next;
        rhs_member = rhs_member->next;
    }

    return lhs_member == NULL && rhs_member == NULL;
}

bool isSameEnumType(ASTDataType *lhs, ASTDataType *rhs)
{
    if(lhs->identifier[0] != '\0' || rhs->identifier[0] != '\0')
        return strcmp(lhs->identifier, rhs->identifier) == 0;

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

bool isSameDataType(ASTDataType *lhs, ASTDataType *rhs)
{
    if(lhs == NULL || rhs == NULL)
        return lhs == rhs;

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
            return lhs->mutable == rhs->mutable && isSameDataType(lhs->child, rhs->child);
        case AST_DATA_TYPE_KIND_FUNCTION:
            return isSameFunctionSignature(lhs, rhs);
        case AST_DATA_TYPE_KIND_NAMED:
            return strcmp(lhs->identifier, rhs->identifier) == 0;
        case AST_DATA_TYPE_KIND_ENUM:
            return isSameEnumType(lhs, rhs);
        case AST_DATA_TYPE_KIND_STRUCT:
            return isSameStructType(lhs, rhs);
        default:
            return false;
    }
}

ASTDataType* resolveNamedDataType(ASTDataType *data_type, ScopeFrame *scope, ASTDataType *self_data_type)
{
    if(data_type == NULL)
        return NULL;

    switch(data_type->kind)
    {
        case AST_DATA_TYPE_KIND_INFER:
        case AST_DATA_TYPE_KIND_PRIMARY:
        case AST_DATA_TYPE_KIND_ENUM:
        case AST_DATA_TYPE_KIND_STRUCT:
            return cloneDataType(data_type);
        case AST_DATA_TYPE_KIND_NAMED: {
            if(strcmp(data_type->identifier, "Self") == 0)
            {
                if(self_data_type == NULL)
                {
                    printf("Type error: Self is only allowed inside a struct method\n");
                    exit(1);
                }
                return cloneDataType(self_data_type);
            }

            TypeInfo *type_info = findTypeInfo(scope, data_type->identifier);
            if(type_info == NULL)
            {
                printf("Unknown data type %s\n", data_type->identifier);
                exit(1);
            }
            return cloneDataType(type_info->data_type);
        }
        case AST_DATA_TYPE_KIND_POINTER:
        case AST_DATA_TYPE_KIND_REFERENCE:
            return newWrappedDataType(data_type->kind, data_type->mutable,
                                      resolveNamedDataType(data_type->child, scope, self_data_type));
        case AST_DATA_TYPE_KIND_FUNCTION: {
            ASTFunctionParameter *head = NULL;
            ASTFunctionParameter *tail = NULL;
            ASTFunctionParameter *parameter = data_type->parameters;
            while(parameter)
            {
                ASTFunctionParameter *new_parameter = (ASTFunctionParameter*) malloc(sizeof(ASTFunctionParameter));
                *new_parameter = *parameter;
                new_parameter->next = NULL;
                new_parameter->data_type = resolveNamedDataType(parameter->data_type, scope, self_data_type);

                if(head == NULL)
                    head = new_parameter;
                else
                    tail->next = new_parameter;
                tail = new_parameter;
                parameter = parameter->next;
            }

            return newFunctionDataType(head, resolveNamedDataType(data_type->return_data_type, scope, self_data_type));
        }
        default:
            printf("resolveNamedDataType: unsupported AST data type kind\n");
            exit(1);
    }
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

bool canImplicitConvertDataType(TypeSystemExprType source_type, ASTNode *source_node, ASTDataType *target_type)
{
    if(target_type == NULL || isInferDataType(target_type))
        return false;

    if(source_type.kind == TYPE_SYSTEM_EXPR_TYPE_LITERAL_INTEGER)
    {
        if(target_type->kind != AST_DATA_TYPE_KIND_PRIMARY)
            return false;

        if(isIntegerPrimary(target_type->primary))
            return canLiteralIntegerFitPrimary(source_node->literal_integer, target_type->primary);
        if(isFloatPrimary(target_type->primary))
            return true;
        return false;
    }

    if(source_type.kind == TYPE_SYSTEM_EXPR_TYPE_LITERAL_FLOAT)
        return target_type->kind == AST_DATA_TYPE_KIND_PRIMARY && isFloatPrimary(target_type->primary);

    if(source_type.kind == TYPE_SYSTEM_EXPR_TYPE_TYPE)
        return false;

    ASTDataType *source_data_type = source_type.data_type;
    if(isSameDataType(source_data_type, target_type))
        return true;

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
        if(source_data_type->mutable && !target_type->mutable && isSameDataType(source_data_type->child, target_type->child))
            return true;
        return false;
    }

    if(source_data_type->kind == AST_DATA_TYPE_KIND_REFERENCE && target_type->kind == AST_DATA_TYPE_KIND_REFERENCE)
    {
        if(source_data_type->mutable && !target_type->mutable && isSameDataType(source_data_type->child, target_type->child))
            return true;
        return false;
    }

    return false;
}

void typeErrorBinaryOperator(ASTNode *node, const char *operator_name,
                             TypeSystemExprType lhs_type, TypeSystemExprType rhs_type)
{
    printf("Type error: operator %s cannot be applied to ", operator_name);
    if(lhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_LITERAL_INTEGER)
        printf("literal integer");
    else if(lhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_LITERAL_FLOAT)
        printf("literal float");
    else if(lhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_TYPE)
        printf("type");
    else
        printASTDataType(lhs_type.data_type);
    printf(" and ");
    if(rhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_LITERAL_INTEGER)
        printf("literal integer");
    else if(rhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_LITERAL_FLOAT)
        printf("literal float");
    else if(rhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_TYPE)
        printf("type");
    else
        printASTDataType(rhs_type.data_type);
    printf(" at file %s, line %d, column %d\n",
           node->filename, node->line_number, node->column_number);
    exit(1);
}

void typeErrorUnaryOperator(ASTNode *node, const char *operator_name, TypeSystemExprType operand_type)
{
    printf("Type error: operator %s cannot be applied to ", operator_name);
    if(operand_type.kind == TYPE_SYSTEM_EXPR_TYPE_LITERAL_INTEGER)
        printf("literal integer");
    else if(operand_type.kind == TYPE_SYSTEM_EXPR_TYPE_LITERAL_FLOAT)
        printf("literal float");
    else if(operand_type.kind == TYPE_SYSTEM_EXPR_TYPE_TYPE)
        printf("type");
    else
        printASTDataType(operand_type.data_type);
    printf(" at file %s, line %d, column %d\n",
           node->filename, node->line_number, node->column_number);
    exit(1);
}

void typeErrorAssign(ASTNode *node, ASTNode *source_node, TypeSystemExprType source_type, ASTDataType *target_type)
{
    printf("Type error: cannot implicitly convert ");
    if(source_type.kind == TYPE_SYSTEM_EXPR_TYPE_LITERAL_INTEGER)
        printf("literal integer");
    else if(source_type.kind == TYPE_SYSTEM_EXPR_TYPE_LITERAL_FLOAT)
        printf("literal float");
    else if(source_type.kind == TYPE_SYSTEM_EXPR_TYPE_TYPE)
        printf("type");
    else
        printASTDataType(source_type.data_type);
    printf(" to ");
    printASTDataType(target_type);
    printf(" for variable %s at file %s, line %d, column %d\n",
           node->identifier, source_node->filename, source_node->line_number, source_node->column_number);
    exit(1);
}

ASTDataType* normalizeNumericDataType(TypeSystemExprType expr_type)
{
    if(expr_type.kind == TYPE_SYSTEM_EXPR_TYPE_LITERAL_INTEGER)
        return defaultIntegerDataType();
    if(expr_type.kind == TYPE_SYSTEM_EXPR_TYPE_LITERAL_FLOAT)
        return defaultFloatDataType();
    if(expr_type.kind == TYPE_SYSTEM_EXPR_TYPE_TYPE)
    {
        printf("Type error: type cannot be used as a numeric expression\n");
        exit(1);
    }
    if(expr_type.data_type->kind == AST_DATA_TYPE_KIND_PRIMARY && isCharPrimary(expr_type.data_type->primary))
        return defaultIntegerDataType();
    return cloneDataType(expr_type.data_type);
}

TypeSystemExprType getCommonNumericType(ASTNode *node, TypeSystemExprType lhs_type, TypeSystemExprType rhs_type, const char *operator_name)
{
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
    return node->kind == AST_EXPR_VARIABLE || node->kind == AST_EXPR_DEREF || node->kind == AST_EXPR_MEMBER;
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

    return false;
}

bool canBindReferenceArgument(ASTNode *argument, ScopeFrame *scope, ASTDataType *parameter_type)
{
    if(parameter_type->kind != AST_DATA_TYPE_KIND_REFERENCE)
        return false;

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

void checkFunctionCallArguments(ASTFunctionParameter *parameter, ASTNode *argument, ScopeFrame *scope)
{
    while(parameter && argument)
    {
        TypeSystemExprType argument_type = inferExprType(argument, scope);
        if(parameter->data_type->kind == AST_DATA_TYPE_KIND_REFERENCE)
        {
            if(!canBindReferenceArgument(argument, scope, parameter->data_type))
            {
                printf("Type error: function reference argument type mismatch at file %s, line %d, column %d\n",
                       argument->filename, argument->line_number, argument->column_number);
                exit(1);
            }
        }
        else if(!canImplicitConvertDataType(argument_type, argument, parameter->data_type))
        {
            printf("Type error: function argument type mismatch at file %s, line %d, column %d\n",
                   argument->filename, argument->line_number, argument->column_number);
            exit(1);
        }
        parameter = parameter->next;
        argument = argument->next;
    }

    if(parameter != NULL || argument != NULL)
    {
        printf("Type error: function argument count mismatch\n");
        exit(1);
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

TypeSystemExprType inferExprType(ASTNode *node, ScopeFrame *scope)
{
    switch(node->kind)
    {
        case AST_EXPR_LITERAL_BOOL:
            return newValueExprType(newPrimaryDataType(AST_PRIMARY_DATA_TYPE_BOOL));
        case AST_EXPR_LITERAL_CHAR:
            return newValueExprType(newPrimaryDataType(AST_PRIMARY_DATA_TYPE_CHAR));
        case AST_EXPR_LITERAL_INTEGER:
            return newLiteralIntegerExprType();
        case AST_EXPR_LITERAL_FLOAT:
            return newLiteralFloatExprType();
        case AST_EXPR_VARIABLE: {
            VariableInfo *variable_info = findVariableInfo(scope, node->identifier);
            if(variable_info != NULL)
            {
                ASTDataType *variable_data_type = variable_info->data_type;
                if(variable_data_type->kind == AST_DATA_TYPE_KIND_REFERENCE)
                    return newValueExprType(variable_data_type->child);
                return newValueExprType(variable_data_type);
            }

            TypeInfo *type_info = findTypeInfo(scope, node->identifier);
            if(type_info != NULL)
                return newTypeExprType(type_info->data_type);

            printf("Type inference: undeclared variable %s at file %s, line %d, column %d\n",
                   node->identifier, node->filename, node->line_number, node->column_number);
            exit(1);
        }
        case AST_EXPR_FUNCTION:
            return newValueExprType(node->data_type);
        case AST_EXPR_ENUM:
            return newTypeExprType(node->data_type);
        case AST_EXPR_STRUCT:
            return newTypeExprType(node->data_type);
        case AST_EXPR_STRUCT_LITERAL: {
            TypeSystemExprType type_expr = inferExprType(node->lhs, scope);
            if(type_expr.kind != TYPE_SYSTEM_EXPR_TYPE_TYPE || !isStructDataType(type_expr.data_type))
            {
                printf("Type error: struct literal requires a struct type at file %s, line %d, column %d\n",
                       node->filename, node->line_number, node->column_number);
                exit(1);
            }

            ASTStructMember *member = type_expr.data_type->members;
            while(member)
            {
                if(member->value == NULL)
                {
                    ASTStructLiteralField *field = node->struct_literal_fields;
                    while(field && strcmp(field->identifier, member->identifier) != 0)
                        field = field->next;
                    if(field == NULL)
                    {
                        printf("Type error: missing field %s in struct literal at file %s, line %d, column %d\n",
                               member->identifier, node->filename, node->line_number, node->column_number);
                        exit(1);
                    }

                    TypeSystemExprType field_type = inferExprType(field->value, scope);
                    if(!canImplicitConvertDataType(field_type, field->value, member->data_type))
                    {
                        printf("Type error: struct field %s type mismatch at file %s, line %d, column %d\n",
                               member->identifier, field->value->filename, field->value->line_number, field->value->column_number);
                        exit(1);
                    }
                }
                member = member->next;
            }

            ASTStructLiteralField *field = node->struct_literal_fields;
            while(field)
            {
                ASTStructMember *declared_member = findStructMember(type_expr.data_type, field->identifier);
                if(declared_member == NULL || declared_member->value != NULL)
                {
                    printf("Type error: unknown struct field %s at file %s, line %d, column %d\n",
                           field->identifier, field->filename, field->line_number, field->column_number);
                    exit(1);
                }
                field = field->next;
            }

            return newValueExprType(type_expr.data_type);
        }
        case AST_EXPR_MEMBER: {
            TypeSystemExprType owner_type = inferExprType(node->lhs, scope);
            ASTDataType *owner_data_type = NULL;

            if(owner_type.kind == TYPE_SYSTEM_EXPR_TYPE_TYPE)
                owner_data_type = owner_type.data_type;
            else if(owner_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE)
            {
                owner_data_type = owner_type.data_type;
                if(owner_data_type->kind == AST_DATA_TYPE_KIND_POINTER || owner_data_type->kind == AST_DATA_TYPE_KIND_REFERENCE)
                    owner_data_type = owner_data_type->child;
            }

            if(isEnumDataType(owner_data_type))
            {
                if(owner_type.kind != TYPE_SYSTEM_EXPR_TYPE_TYPE)
                {
                    printf("Type error: enum variant access requires the enum type at file %s, line %d, column %d\n",
                           node->filename, node->line_number, node->column_number);
                    exit(1);
                }

                if(findEnumVariant(owner_data_type, node->identifier) == NULL)
                {
                    printf("Type error: unknown enum variant %s at file %s, line %d, column %d\n",
                           node->identifier, node->filename, node->line_number, node->column_number);
                    exit(1);
                }

                return newValueExprType(owner_data_type);
            }

            ASTDataType *struct_type = owner_data_type;
            if(!isStructDataType(struct_type))
            {
                printf("Type error: member access requires a struct type at file %s, line %d, column %d\n",
                       node->filename, node->line_number, node->column_number);
                exit(1);
            }

            ASTStructMember *member = findStructMember(struct_type, node->identifier);
            if(member == NULL)
            {
                printf("Type error: unknown struct member %s at file %s, line %d, column %d\n",
                       node->identifier, node->filename, node->line_number, node->column_number);
                exit(1);
            }

            if(owner_type.kind == TYPE_SYSTEM_EXPR_TYPE_TYPE && member->value == NULL)
            {
                printf("Type error: struct field %s cannot be accessed on the type itself at file %s, line %d, column %d\n",
                       node->identifier, node->filename, node->line_number, node->column_number);
                exit(1);
            }

            if(member->data_type->kind == AST_DATA_TYPE_KIND_REFERENCE)
                return newValueExprType(member->data_type->child);
            return newValueExprType(member->data_type);
        }
        case AST_EXPR_CALL: {
            if(node->lhs->kind == AST_EXPR_MEMBER)
            {
                ASTNode *member_node = node->lhs;
                TypeSystemExprType owner_type = inferExprType(member_node->lhs, scope);
                ASTDataType *struct_type = NULL;
                bool through_type = owner_type.kind == TYPE_SYSTEM_EXPR_TYPE_TYPE;
                if(through_type)
                    struct_type = owner_type.data_type;
                else if(owner_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE)
                {
                    struct_type = owner_type.data_type;
                    if(struct_type->kind == AST_DATA_TYPE_KIND_POINTER || struct_type->kind == AST_DATA_TYPE_KIND_REFERENCE)
                        struct_type = struct_type->child;
                }

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
                            checkFunctionCallArguments(parameter->next, node->rhs, scope);
                            return newValueExprType(member->data_type->return_data_type);
                        }
                    }
                }
            }

            TypeSystemExprType callee_type = inferExprType(node->lhs, scope);
            if(callee_type.kind != TYPE_SYSTEM_EXPR_TYPE_VALUE || callee_type.data_type->kind != AST_DATA_TYPE_KIND_FUNCTION)
            {
                printf("Type error: called expression is not a function at file %s, line %d, column %d\n",
                       node->filename, node->line_number, node->column_number);
                exit(1);
            }

            checkFunctionCallArguments(callee_type.data_type->parameters, node->rhs, scope);

            return newValueExprType(callee_type.data_type->return_data_type);
        }
        case AST_EXPR_PARENTHESIS:
            return inferExprType(node->lhs, scope);
        case AST_EXPR_UNARY_PLUS:
        case AST_EXPR_UNARY_MINUS: {
            TypeSystemExprType operand_type = inferExprType(node->lhs, scope);
            if(operand_type.kind == TYPE_SYSTEM_EXPR_TYPE_LITERAL_INTEGER)
                return newValueExprType(defaultIntegerDataType());
            if(operand_type.kind == TYPE_SYSTEM_EXPR_TYPE_LITERAL_FLOAT)
                return newValueExprType(defaultFloatDataType());
            if(operand_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE &&
               operand_type.data_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
               (isIntegerPrimary(operand_type.data_type->primary) || isFloatPrimary(operand_type.data_type->primary)))
                return newValueExprType(operand_type.data_type);
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
            if(!isAddressableExpr(node->lhs))
            {
                printf("Type error: cannot take address of non-addressable expression at file %s, line %d, column %d\n",
                       node->filename, node->line_number, node->column_number);
                exit(1);
            }

            if(node->kind == AST_EXPR_ADDRESS_OF_MUT && !isMutableAddressableExpr(node->lhs, scope))
            {
                printf("Type error: cannot take mutable address of immutable expression at file %s, line %d, column %d\n",
                       node->filename, node->line_number, node->column_number);
                exit(1);
            }

            TypeSystemExprType operand_type = inferExprType(node->lhs, scope);
            if(operand_type.kind != TYPE_SYSTEM_EXPR_TYPE_VALUE)
            {
                printf("Type error: cannot take address of literal at file %s, line %d, column %d\n",
                       node->filename, node->line_number, node->column_number);
                exit(1);
            }

            bool mutable_pointer = node->kind == AST_EXPR_ADDRESS_OF_MUT || isMutableAddressableExpr(node->lhs, scope);

            return newValueExprType(newWrappedDataType(
                AST_DATA_TYPE_KIND_POINTER,
                mutable_pointer,
                cloneDataType(getReferenceTargetType(operand_type.data_type))
            ));
        } break;
        case AST_EXPR_DEREF: {
            TypeSystemExprType operand_type = inferExprType(node->lhs, scope);
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
            if(node->kind == AST_EXPR_MUL) operator_name = "*";
            else if(node->kind == AST_EXPR_DIV) operator_name = "/";
            else if(node->kind == AST_EXPR_SUB) operator_name = "-";
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
            if(lhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE && rhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE &&
               isSameDataType(lhs_type.data_type, rhs_type.data_type))
                return newValueExprType(newPrimaryDataType(AST_PRIMARY_DATA_TYPE_BOOL));

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
            printf("inferExprType: unsupported AST node kind %s\n", astNodeKindToString(node->kind));
            exit(1);
    }
}

ASTDataType* inferDeclaredTypeFromExpr(ASTNode *expr, ScopeFrame *scope)
{
    TypeSystemExprType expr_type = inferExprType(expr, scope);
    if(expr_type.kind == TYPE_SYSTEM_EXPR_TYPE_LITERAL_INTEGER)
        return defaultIntegerDataType();
    if(expr_type.kind == TYPE_SYSTEM_EXPR_TYPE_LITERAL_FLOAT)
        return defaultFloatDataType();
    return cloneDataType(expr_type.data_type);
}

#endif /* TYPE_SYSTEM_H */
