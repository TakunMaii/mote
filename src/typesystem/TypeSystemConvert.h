#ifndef TYPE_SYSTEM_CONVERT_H
#define TYPE_SYSTEM_CONVERT_H

#include "TypeSystemResolve.h"

bool canLiteralIntegerFitPrimary(ASTIntegerLiteralValue literal_integer, ASTPrimaryDataType target_primary)
{
    switch(target_primary)
    {
        case AST_PRIMARY_DATA_TYPE_I8: return literal_integer.magnitude <= 127ull;
        case AST_PRIMARY_DATA_TYPE_I16: return literal_integer.magnitude <= 32767ull;
        case AST_PRIMARY_DATA_TYPE_I32: return literal_integer.magnitude <= 2147483647ull;
        case AST_PRIMARY_DATA_TYPE_I64: return literal_integer.magnitude <= 9223372036854775807ull;
        case AST_PRIMARY_DATA_TYPE_U8: return literal_integer.magnitude <= 255ull;
        case AST_PRIMARY_DATA_TYPE_U16: return literal_integer.magnitude <= 65535ull;
        case AST_PRIMARY_DATA_TYPE_U32: return literal_integer.magnitude <= 4294967295ull;
        case AST_PRIMARY_DATA_TYPE_U64: return true;
        case AST_PRIMARY_DATA_TYPE_CHAR: return literal_integer.magnitude <= 255ull;
        default: return false;
    }
}

bool isLiteralIntegerZero(ASTNode *source_node)
{
    return source_node != NULL &&
           source_node->kind == AST_EXPR_LITERAL_INTEGER &&
           astIntegerLiteralIsZero(source_node->literal_integer);
}

bool isUnaryNegativeIntegerLiteral(ASTNode *source_node)
{
    return source_node != NULL &&
           source_node->kind == AST_EXPR_UNARY_MINUS &&
           source_node->lhs != NULL &&
           source_node->lhs->kind == AST_EXPR_LITERAL_INTEGER;
}

bool canNegativeLiteralIntegerFitPrimary(ASTIntegerLiteralValue literal_integer, ASTPrimaryDataType target_primary)
{
    switch(target_primary)
    {
        case AST_PRIMARY_DATA_TYPE_I8: return literal_integer.magnitude <= 128ull;
        case AST_PRIMARY_DATA_TYPE_I16: return literal_integer.magnitude <= 32768ull;
        case AST_PRIMARY_DATA_TYPE_I32: return literal_integer.magnitude <= 2147483648ull;
        case AST_PRIMARY_DATA_TYPE_I64: return literal_integer.magnitude <= 9223372036854775808ull;
        case AST_PRIMARY_DATA_TYPE_F8:
        case AST_PRIMARY_DATA_TYPE_F16:
        case AST_PRIMARY_DATA_TYPE_F32:
        case AST_PRIMARY_DATA_TYPE_F64:
            return true;
        default: return false;
    }
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

static void typeSystemRejectVoidValueExpr(ASTNode *expr, ScopeFrame *scope,
                                          const char *code,
                                          const char *message,
                                          const char *detail)
{
    if(expr == NULL)
        return;
    if(expr->kind == AST_EXPR_ARRAY_LITERAL && expr->lhs == NULL)
        return;

    TypeSystemExprType expr_type = inferExprType(expr, scope);
    if(expr_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE && isVoidDataType(expr_type.data_type))
        typeSystemAbortNode(code, expr, message, detail);
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

    if((source_data_type->kind == AST_DATA_TYPE_KIND_SLICE ||
        source_data_type->kind == AST_DATA_TYPE_KIND_STRING) &&
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

    if((source_data_type->kind == AST_DATA_TYPE_KIND_SLICE ||
        source_data_type->kind == AST_DATA_TYPE_KIND_STRING) &&
       target_type->kind == AST_DATA_TYPE_KIND_POINTER)
    {
        if(!isSameDataType(source_data_type->child, target_type->child))
            return false;
        return true;
    }

    if(source_data_type->kind == AST_DATA_TYPE_KIND_STRING &&
       target_type->kind == AST_DATA_TYPE_KIND_SLICE &&
       target_type->child != NULL &&
       target_type->child->kind == AST_DATA_TYPE_KIND_PRIMARY &&
       target_type->child->primary == AST_PRIMARY_DATA_TYPE_CHAR)
        return true;

    if(source_data_type->kind == AST_DATA_TYPE_KIND_SLICE &&
       source_data_type->child != NULL &&
       source_data_type->child->kind == AST_DATA_TYPE_KIND_PRIMARY &&
       source_data_type->child->primary == AST_PRIMARY_DATA_TYPE_CHAR &&
       target_type->kind == AST_DATA_TYPE_KIND_STRING)
        return true;

    if(source_node != NULL &&
       source_node->kind == AST_EXPR_LITERAL_STRING &&
       source_data_type->kind == AST_DATA_TYPE_KIND_STRING &&
       target_type->kind == AST_DATA_TYPE_KIND_POINTER &&
       target_type->child != NULL &&
       target_type->child->kind == AST_DATA_TYPE_KIND_PRIMARY &&
       target_type->child->primary == AST_PRIMARY_DATA_TYPE_CHAR)
        return true;

    if(source_node != NULL &&
       source_node->kind == AST_EXPR_LITERAL_STRING &&
       source_data_type->kind == AST_DATA_TYPE_KIND_STRING &&
       target_type->kind == AST_DATA_TYPE_KIND_ARRAY &&
       target_type->child != NULL &&
       target_type->child->kind == AST_DATA_TYPE_KIND_PRIMARY &&
       target_type->child->primary == AST_PRIMARY_DATA_TYPE_CHAR)
        return target_type->array_length == (long long int) strlen(source_node->literal_string);

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

ASTDataType* inferDebugBuiltinValueType(ASTNode *node, ScopeFrame *scope)
{
    if(node->lhs == NULL)
        typeSystemAbortNode("T1249", node,
                            "@debug expects at least one argument",
                            "expected `@debug(value, ...)`");

    for(ASTNode *argument = node->lhs; argument != NULL; argument = argument->next)
        (void) inferExprType(argument, scope);

    return newPrimaryDataType(AST_PRIMARY_DATA_TYPE_VOID);
}

ASTDataType* inferPanicBuiltinValueType(ASTNode *node, ScopeFrame *scope)
{
    if(node->lhs == NULL || node->lhs->next != NULL)
        typeSystemAbortNode("T1270", node,
                            "@panic expects exactly one argument",
                            "expected `@panic(message)`");

    TypeSystemExprType message_type = inferExprType(node->lhs, scope);
    if(message_type.kind != TYPE_SYSTEM_EXPR_TYPE_VALUE ||
       message_type.data_type == NULL ||
       !isStringDataType(message_type.data_type))
        typeSystemAbortExpectedDescriptionFoundExpr("T1271", node->lhs,
                                                    "@panic expects a string message",
                                                    "a `string` value",
                                                    message_type);

    return newPrimaryDataType(AST_PRIMARY_DATA_TYPE_VOID);
}

ASTDataType* inferAssertBuiltinValueType(ASTNode *node, ScopeFrame *scope)
{
    if(node->lhs == NULL || node->lhs->next != NULL)
        typeSystemAbortNode("T1272", node,
                            "@assert expects exactly one argument",
                            "expected `@assert(condition)`");

    TypeSystemExprType condition_type = inferExprType(node->lhs, scope);
    if(condition_type.kind != TYPE_SYSTEM_EXPR_TYPE_VALUE ||
       condition_type.data_type == NULL ||
       condition_type.data_type->kind != AST_DATA_TYPE_KIND_PRIMARY ||
       condition_type.data_type->primary != AST_PRIMARY_DATA_TYPE_BOOL)
        typeSystemAbortExpectedDescriptionFoundExpr("T1273", node->lhs,
                                                    "@assert expects a bool condition",
                                                    "a `bool` value",
                                                    condition_type);

    return newPrimaryDataType(AST_PRIMARY_DATA_TYPE_VOID);
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
        typeSystemAbortExpectedDataTypeFoundExpr("T1219", value_expr,
                                                 "invalid explicit conversion with @as",
                                                 target_type,
                                                 source_type);

    return target_type;
}

ASTDataType* inferLenBuiltinValueType(ASTNode *node, ScopeFrame *scope)
{
    if(node->lhs == NULL || node->lhs->next != NULL)
        typeSystemAbortNode("T1254", node,
                            "@len expects exactly one argument",
                            "expected `@len(slice_value)`");

    TypeSystemExprType operand_type = inferExprType(node->lhs, scope);
    if(operand_type.kind != TYPE_SYSTEM_EXPR_TYPE_VALUE ||
       (!isSliceDataType(operand_type.data_type) && !isStringDataType(operand_type.data_type)))
        typeSystemAbortExpectedDescriptionFoundExpr("T1255", node->lhs,
                                                    "@len expects a slice or string value",
                                                    "`[]T` or `string`",
                                                    operand_type);

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
        typeSystemAbortExpectedDataTypeFoundExpr("T1264", pointer_expr,
                                                 "@ptr_add expects a pointer to the given element type",
                                                 newWrappedDataType(AST_DATA_TYPE_KIND_POINTER, cloneDataType(element_type)),
                                                 pointer_type);

    TypeSystemExprType count_type = inferExprType(count_expr, scope);
    if(count_type.kind == TYPE_SYSTEM_EXPR_TYPE_TYPE ||
       (count_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE &&
        (count_type.data_type->kind != AST_DATA_TYPE_KIND_PRIMARY ||
         !isIntegerPrimary(count_type.data_type->primary))))
        typeSystemAbortExpectedDescriptionFoundExpr("T1265", count_expr,
                                                    "@ptr_add expects an integer offset",
                                                    "an integer value",
                                                    count_type);

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
    {
        char expected_buffer[256] = {0};
        char lhs_buffer[256] = {0};
        char rhs_buffer[256] = {0};
        ASTDataType *expected_pointer_type = newWrappedDataType(AST_DATA_TYPE_KIND_POINTER, cloneDataType(element_type));
        typeSystemDescribeDataType(expected_pointer_type, expected_buffer, sizeof(expected_buffer));
        typeSystemDescribeExprType(lhs_type, lhs_buffer, sizeof(lhs_buffer));
        typeSystemDescribeExprType(rhs_type, rhs_buffer, sizeof(rhs_buffer));

        Diagnostic diagnostic = diagnosticMake(DIAGNOSTIC_SEVERITY_ERROR,
                                               "T1269",
                                               astNodeSourceSpan(lhs_expr),
                                               "@ptr_diff expects two pointers to the given element type");
        diagnosticSetPrimaryLabel(&diagnostic,
                                  "expected both operands to be %s, found %s and %s",
                                  expected_buffer, lhs_buffer, rhs_buffer);
        diagnosticAbort(diagnostic);
    }

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
       !isSameDataType(pointer_type.data_type->child, element_type))
        typeSystemAbortExpectedDataTypeFoundExpr("T1259", pointer_expr,
                                                 "@slice expects a pointer to the given element type",
                                                 newWrappedDataType(AST_DATA_TYPE_KIND_POINTER, cloneDataType(element_type)),
                                                 pointer_type);

    TypeSystemExprType length_type = inferExprType(length_expr, scope);
    if(length_type.kind == TYPE_SYSTEM_EXPR_TYPE_TYPE ||
       (length_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE &&
        (length_type.data_type->kind != AST_DATA_TYPE_KIND_PRIMARY ||
         !isIntegerPrimary(length_type.data_type->primary))))
        typeSystemAbortExpectedDescriptionFoundExpr("T1260", length_expr,
                                                    "@slice expects an integer length",
                                                    "an integer value",
                                                    length_type);

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

    if(isUnaryNegativeIntegerLiteral(source_node))
    {
        if(isOptionalDataType(target_type))
            return canImplicitConvertDataType(source_type, source_node->lhs, target_type->child);

        if(target_type->kind != AST_DATA_TYPE_KIND_PRIMARY)
            return false;

        if(isIntegerPrimary(target_type->primary) || isBoolPrimary(target_type->primary))
            return canNegativeLiteralIntegerFitPrimary(source_node->lhs->literal_integer, target_type->primary);
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
        if(isSameDataType(source_data_type->child, target_type->child))
            return true;

        if(isVoidDataType(source_data_type->child) || isVoidDataType(target_type->child))
            return true;

        return false;
    }

    if(source_data_type->kind == AST_DATA_TYPE_KIND_FUNCTION && target_type->kind == AST_DATA_TYPE_KIND_FUNCTION)
        return isSameFunctionSignature(source_data_type, target_type);

    if((source_data_type->kind == AST_DATA_TYPE_KIND_SLICE ||
        source_data_type->kind == AST_DATA_TYPE_KIND_STRING) &&
       target_type->kind == AST_DATA_TYPE_KIND_POINTER &&
       isSameDataType(source_data_type->child, target_type->child))
        return true;

    if(source_node != NULL &&
       source_node->kind == AST_EXPR_LITERAL_STRING &&
       source_data_type->kind == AST_DATA_TYPE_KIND_STRING &&
       target_type->kind == AST_DATA_TYPE_KIND_POINTER &&
       target_type->child != NULL &&
       target_type->child->kind == AST_DATA_TYPE_KIND_PRIMARY &&
       target_type->child->primary == AST_PRIMARY_DATA_TYPE_CHAR)
        return true;

    if(source_node != NULL &&
       source_node->kind == AST_EXPR_LITERAL_STRING &&
       source_data_type->kind == AST_DATA_TYPE_KIND_STRING &&
       target_type->kind == AST_DATA_TYPE_KIND_ARRAY &&
       target_type->child != NULL &&
       target_type->child->kind == AST_DATA_TYPE_KIND_PRIMARY &&
       target_type->child->primary == AST_PRIMARY_DATA_TYPE_CHAR)
        return target_type->array_length == (long long int) strlen(source_node->literal_string);

    if(source_data_type->kind == AST_DATA_TYPE_KIND_REFERENCE && target_type->kind == AST_DATA_TYPE_KIND_REFERENCE)
        return isSameDataType(source_data_type->child, target_type->child);

    return false;
}

bool canImplicitConvertExprToType(ASTNode *expr, ScopeFrame *scope, ASTDataType *target_type)
{
    if(expr == NULL || target_type == NULL || isInferDataType(target_type))
        return false;

    if(expr->kind == AST_EXPR_ARRAY_LITERAL && expr->lhs == NULL)
        return target_type->kind == AST_DATA_TYPE_KIND_ARRAY && target_type->array_length == 0;

    TypeSystemExprType expr_type = inferExprType(expr, scope);
    if(expr_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE && isVoidDataType(expr_type.data_type))
        return isVoidDataType(target_type);

    if(isOptionalDataType(target_type) && expr->kind != AST_EXPR_LITERAL_NULL)
        return canImplicitConvertExprToType(expr, scope, target_type->child) ||
               canImplicitConvertDataType(expr_type, expr, target_type);

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
        ASTDataType *literal_struct_type = type_expr.kind == TYPE_SYSTEM_EXPR_TYPE_TYPE
            ? resolveNamedDataType(type_expr.data_type, scope, NULL)
            : NULL;
        ASTDataType *resolved_target_type = resolveNamedDataType(target_type, scope, NULL);
        if(type_expr.kind != TYPE_SYSTEM_EXPR_TYPE_TYPE || !isStructDataType(literal_struct_type))
            return false;
        if(!isSameDataType(literal_struct_type, resolved_target_type))
            return false;

        ASTStructLiteralField *literal_field = expr->struct_literal_fields;
        bool literal_fields_have_explicit_names = literal_field != NULL && literal_field->has_name;
        ASTStructMember *member = resolved_target_type->members;
        while(member)
        {
            if(member->value == NULL)
            {
                ASTStructLiteralField *field = literal_field;
                if(literal_fields_have_explicit_names)
                {
                    while(field && strcmp(field->identifier, member->identifier) != 0)
                        field = field->next;
                }
                else if(field != NULL)
                {
                    literal_field = literal_field->next;
                }

                if(field == NULL || field->has_name != literal_fields_have_explicit_names)
                    return false;
                if(!canImplicitConvertExprToType(field->value, scope, member->data_type))
                    return false;
            }
            member = member->next;
        }

        ASTStructLiteralField *field = expr->struct_literal_fields;
        while(field)
        {
            if(field->has_name != literal_fields_have_explicit_names)
                return false;
            if(!literal_fields_have_explicit_names)
            {
                field = field->next;
                continue;
            }
            ASTStructMember *declared_member = findStructMember(resolved_target_type, field->identifier);
            if(declared_member == NULL || declared_member->value != NULL)
                return false;
            field = field->next;
        }

        return true;
    }

    return canImplicitConvertDataType(expr_type, expr, target_type);
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
        diagnosticAddNote(&diagnostic, "assignment target: `%s`", astUserFacingIdentifier(node->identifier));
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
        return variable_info != NULL && !variable_info->is_compile_time_constant;
    }

    if(node->kind == AST_EXPR_DEREF)
    {
        TypeSystemExprType ptr_type = inferExprType(node->lhs, scope);
        if(ptr_type.kind != TYPE_SYSTEM_EXPR_TYPE_VALUE)
            return false;
        if(ptr_type.data_type->kind == AST_DATA_TYPE_KIND_POINTER || ptr_type.data_type->kind == AST_DATA_TYPE_KIND_REFERENCE)
            return true;
    }

    if(node->kind == AST_EXPR_MEMBER)
    {
        TypeSystemExprType owner_type = inferExprType(node->lhs, scope);
        if(owner_type.kind != TYPE_SYSTEM_EXPR_TYPE_VALUE)
            return false;

        ASTDataType *owner_data_type = owner_type.data_type;
        if(owner_data_type->kind == AST_DATA_TYPE_KIND_POINTER || owner_data_type->kind == AST_DATA_TYPE_KIND_REFERENCE)
            return true;

        return isMutableAddressableExpr(node->lhs, scope);
    }

    if(node->kind == AST_EXPR_INDEX)
    {
        TypeSystemExprType owner_type = inferExprType(node->lhs, scope);
        if(owner_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE && isStringDataType(owner_type.data_type))
            return false;
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
        TypeSystemExprType argument_type = inferExprType(argument, scope);
        if(argument_type.kind != TYPE_SYSTEM_EXPR_TYPE_VALUE)
            return false;
        if(argument_type.data_type->kind == AST_DATA_TYPE_KIND_POINTER)
            return isSameDataType(argument_type.data_type->child, parameter_type->child);
        return isSameDataType(getReferenceTargetType(argument_type.data_type), parameter_type->child);
    }

    if(!isAddressableExpr(argument))
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

void checkFunctionCallArguments(ASTFunctionParameter *parameter, ASTNode *argument, ScopeFrame *scope, bool is_variadic, ASTNode *call_site)
{
    ASTFunctionParameter *expected_parameters = parameter;
    ASTNode *provided_arguments = argument;

    while(parameter && argument)
    {
        if(parameter->data_type->kind == AST_DATA_TYPE_KIND_REFERENCE)
        {
            if(!canBindReferenceArgument(argument, scope, parameter->data_type))
                typeSystemAbortExpectedDataTypeFoundExpr("T1220", argument,
                                                         "function reference argument type mismatch",
                                                         parameter->data_type,
                                                         inferExprType(argument, scope));
        }
        else if(!canImplicitConvertExprToType(argument, scope, parameter->data_type))
            typeSystemAbortExpectedDataTypeFoundExpr("T1221", argument,
                                                     "function argument type mismatch",
                                                     parameter->data_type,
                                                     inferExprType(argument, scope));
        parameter = parameter->next;
        argument = argument->next;
    }

    if(parameter != NULL)
    {
        Diagnostic diagnostic = diagnosticMake(DIAGNOSTIC_SEVERITY_ERROR,
                                               "T1222",
                                               astNodeSourceSpan(call_site != NULL ? call_site : provided_arguments),
                                               "function argument count mismatch");
        diagnosticSetPrimaryLabel(&diagnostic,
                                  "expected %d argument%s, found %d",
                                  typeSystemCountFunctionParameters(expected_parameters),
                                  typeSystemCountFunctionParameters(expected_parameters) == 1 ? "" : "s",
                                  typeSystemCountCallArguments(provided_arguments));
        diagnosticAddNote(&diagnostic, "too few arguments were provided");
        diagnosticAbort(diagnostic);
    }

    if(!is_variadic && argument != NULL)
    {
        Diagnostic diagnostic = diagnosticMake(DIAGNOSTIC_SEVERITY_ERROR,
                                               "T1223",
                                               astNodeSourceSpan(argument != NULL ? argument : call_site),
                                               "function argument count mismatch");
        diagnosticSetPrimaryLabel(&diagnostic,
                                  "expected %d argument%s, found %d",
                                  typeSystemCountFunctionParameters(expected_parameters),
                                  typeSystemCountFunctionParameters(expected_parameters) == 1 ? "" : "s",
                                  typeSystemCountCallArguments(provided_arguments));
        diagnosticAddNote(&diagnostic, "too many arguments were provided");
        diagnosticAbort(diagnostic);
    }

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


#endif /* TYPE_SYSTEM_CONVERT_H */
