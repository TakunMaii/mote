#ifndef MIR_LOWERING_EXPR_H
#define MIR_LOWERING_EXPR_H

#include "MIRLoweringFunction.h"

static MirValueId lowerSlicePtrValue(MirFunctionState *state, MirLowerScope *scope, ASTNode *slice_expr);
static MirValueId lowerSliceLenValue(MirFunctionState *state, MirLowerScope *scope, ASTNode *slice_expr);

static MirValueId lowerLogicalShortCircuit(MirFunctionState *state, MirLowerScope *scope, ASTNode *node,
                                           bool is_and)
{
    MirValueId result_slot = mirEmitAlloca(state, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_BOOL),
                                           node->filename, node->line_number, node->column_number);
    MirValueId lhs = lowerExprAsValue(state, scope, node->lhs, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_BOOL));

    MirFunction *function = mirCurrentFunction(state);
    MirBlockId rhs_block = mirCreateBlock(state->lowering, function, is_and ? "logical_and_rhs" : "logical_or_rhs");
    MirBlockId short_block = mirCreateBlock(state->lowering, function, is_and ? "logical_and_short" : "logical_or_short");
    MirBlockId end_block = mirCreateBlock(state->lowering, function, is_and ? "logical_and_end" : "logical_or_end");

    if(is_and)
        mirEmitCondBr(state, lhs, rhs_block, short_block);
    else
        mirEmitCondBr(state, lhs, short_block, rhs_block);

    mirSwitchToBlock(state, short_block);
    MirValueId short_value = mirEmitConstBool(state, !is_and, node->filename, node->line_number, node->column_number);
    mirEmitStore(state, result_slot, short_value, node->filename, node->line_number, node->column_number);
    mirEmitBr(state, end_block);

    mirSwitchToBlock(state, rhs_block);
    MirValueId rhs = lowerExprAsValue(state, scope, node->rhs, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_BOOL));
    mirEmitStore(state, result_slot, rhs, node->filename, node->line_number, node->column_number);
    mirEmitBr(state, end_block);

    mirSwitchToBlock(state, end_block);
    return mirEmitLoad(state, result_slot, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_BOOL),
                       node->filename, node->line_number, node->column_number);
}

static MirValueId lowerExternBuiltinExpr(MirFunctionState *state, MirLowerScope *scope, ASTNode *node,
                                         ASTDataType *expected_type)
{
    ASTDataType *function_type = mirResolvedExprValueType(node, &(scope->type_scope));
    if(function_type->is_variadic)
        mirLoweringAbortNode("M2007", node,
                             "variadic extern values cannot be materialized as first-class closures",
                             "call the extern function directly instead of storing it as a runtime closure");

    const char *wrapper_name = mirEnsureExternFunction(
        state->lowering,
        node->lhs->literal_string,
        function_type,
        node->filename,
        node->line_number,
        node->column_number
    );

    MirValueId function_ref = mirEmitFunctionRef(state, wrapper_name, function_type,
                                                 node->filename, node->line_number, node->column_number);
    return mirMaybeConvertValue(state, scope, node, function_ref, expected_type);
}

static MirValueId lowerTypeLayoutBuiltinExpr(MirFunctionState *state, MirLowerScope *scope, ASTNode *node,
                                             bool want_align)
{
    long long int value = inferTypeBuiltinLayoutValue(node, &(scope->type_scope),
                                                      want_align ? "alignof" : "sizeof",
                                                      want_align ? "expected `@alignof(Type)`" : "expected `@sizeof(Type)`",
                                                      want_align);
    return mirEmitConstInt(state, value, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_I64),
                           node->filename, node->line_number, node->column_number);
}

static MirValueId lowerZeroBuiltinExpr(MirFunctionState *state, MirLowerScope *scope, ASTNode *node,
                                       ASTDataType *expected_type)
{
    ASTDataType *value_type = mirResolvedExprValueType(node, &(scope->type_scope));
    MirValueId zero_value = mirEmitZero(state, value_type,
                                        node->filename, node->line_number, node->column_number);
    return mirMaybeConvertValue(state, scope, node, zero_value, expected_type);
}

static MirValueId lowerDebugBuiltinExpr(MirFunctionState *state, MirLowerScope *scope, ASTNode *node)
{
    MirOperandList begin_args = newMirOperandList(2);
    ASTDataType *char_ptr_type = newWrappedDataType(AST_DATA_TYPE_KIND_POINTER,
                                                    newPrimaryDataType(AST_PRIMARY_DATA_TYPE_CHAR));
    ASTDataType *file_global_type = newArrayDataType(newPrimaryDataType(AST_PRIMARY_DATA_TYPE_CHAR),
                                                     strlen(node->filename) + 1);
    const char *file_global = mirEnsureStringLiteralGlobal(state->lowering, node->filename);
    MirValueId file_addr = mirEmitGlobalAddr(state, file_global, file_global_type,
                                             node->filename, node->line_number, node->column_number);
    begin_args.items[0] = mirEmitConvert(state, file_addr, char_ptr_type,
                                         node->filename, node->line_number, node->column_number);
    begin_args.items[1] = mirEmitConstInt(state, node->line_number + 1, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_I64),
                                          node->filename, node->line_number, node->column_number);
    mirEmitDebugRuntimeCall(state, node, "mote_debug_begin", mirDebugExternTypeCharPtrI64Void(), begin_args);

    bool first = true;
    for(ASTNode *argument = node->lhs; argument != NULL; argument = argument->next)
    {
        if(!first)
            mirEmitDebugRuntimeCall(state, node, "mote_debug_sep", mirDebugExternType0Void(), newMirOperandList(0));
        first = false;

        TypeSystemExprType argument_type = inferExprType(argument, &(scope->type_scope));
        if(argument_type.kind == TYPE_SYSTEM_EXPR_TYPE_TYPE)
        {
            char type_buffer[512] = {0};
            appendASTDataTypeString(argument_type.data_type, type_buffer, sizeof(type_buffer));
            mirEmitDebugWriteCStrLiteral(state, node, "Type(");
            mirEmitDebugWriteCStrLiteral(state, node, type_buffer);
            mirEmitDebugWriteCharLiteral(state, node, ')');
            continue;
        }
        if(argument_type.kind == TYPE_SYSTEM_EXPR_TYPE_NULL)
        {
            mirEmitDebugWriteCStrLiteral(state, node, "null");
            continue;
        }

        ASTDataType *value_type = mirResolvedExprValueType(argument, &(scope->type_scope));
        MirValueId value = lowerExprAsValue(state, scope, argument, value_type);
        mirEmitDebugValue(state, scope, argument, value_type, value, 0);
    }

    mirEmitDebugRuntimeCall(state, node, "mote_debug_end", mirDebugExternType0Void(), newMirOperandList(0));
    return -1;
}

static ASTDataType* mirPanicExternTypePtrLenVoid(void)
{
    ASTFunctionParameter *ptr_param = (ASTFunctionParameter*) malloc(sizeof(ASTFunctionParameter));
    ASTFunctionParameter *len_param = (ASTFunctionParameter*) malloc(sizeof(ASTFunctionParameter));
    if(ptr_param == NULL || len_param == NULL)
        mirLoweringAbortInternal("ICE0302", "mirPanicExternTypePtrLenVoid", "AST function parameter allocation failed");
    memset(ptr_param, 0, sizeof(ASTFunctionParameter));
    memset(len_param, 0, sizeof(ASTFunctionParameter));
    ptr_param->data_type = newWrappedDataType(AST_DATA_TYPE_KIND_POINTER, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_CHAR));
    len_param->data_type = newPrimaryDataType(AST_PRIMARY_DATA_TYPE_I64);
    ptr_param->next = len_param;
    return newFunctionDataType(ptr_param, false, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_VOID));
}

static void lowerPanicBuiltinTerminator(MirFunctionState *state, MirLowerScope *scope, ASTNode *node,
                                        ASTNode *message_expr, const char *symbol_name)
{
    ASTDataType *panic_type = mirPanicExternTypePtrLenVoid();
    const char *panic_name = mirEnsureExternFunction(state->lowering, symbol_name, panic_type,
                                                     node->filename, node->line_number, node->column_number);
    MirOperandList panic_args = newMirOperandList(2);
    panic_args.items[0] = lowerSlicePtrValue(state, scope, message_expr);
    panic_args.items[1] = lowerSliceLenValue(state, scope, message_expr);
    (void) mirEmitExternCall(state, panic_name, panic_type, panic_args,
                             newPrimaryDataType(AST_PRIMARY_DATA_TYPE_VOID),
                             node->filename, node->line_number, node->column_number);
    mirEmitUnreachable(state);
}

static MirValueId lowerPanicBuiltinExpr(MirFunctionState *state, MirLowerScope *scope, ASTNode *node)
{
    lowerPanicBuiltinTerminator(state, scope, node, node->lhs, "mote_panic");
    return -1;
}

static MirValueId lowerAssertBuiltinExpr(MirFunctionState *state, MirLowerScope *scope, ASTNode *node)
{
    MirFunction *function = mirCurrentFunction(state);
    MirBlockId ok_block = mirCreateBlock(state->lowering, function, "assert_ok");
    MirBlockId panic_block = mirCreateBlock(state->lowering, function, "assert_panic");
    MirValueId condition = lowerExprAsValue(state, scope, node->lhs, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_BOOL));
    mirEmitCondBr(state, condition, ok_block, panic_block);

    mirSwitchToBlock(state, panic_block);
    ASTNode *message = newASTNode(AST_EXPR_LITERAL_STRING);
    strcpy(message->literal_string, "@assert failed");
    message->filename = node->filename;
    message->line_number = node->line_number;
    message->column_number = node->column_number;
    message->end_line_number = node->end_line_number;
    message->end_column_number = node->end_column_number;
    lowerPanicBuiltinTerminator(state, scope, node, message, "mote_assert_fail");

    mirSwitchToBlock(state, ok_block);
    return -1;
}

static MirValueId lowerSlicePtrValue(MirFunctionState *state, MirLowerScope *scope, ASTNode *slice_expr)
{
    TypeSystemExprType slice_type = inferExprType(slice_expr, &(scope->type_scope));
    if(slice_type.kind != TYPE_SYSTEM_EXPR_TYPE_VALUE ||
       (!isSliceDataType(slice_type.data_type) && !isStringDataType(slice_type.data_type)))
        mirLoweringAbortNode("M2015", slice_expr,
                             "slice pointer extraction requires a slice-like value",
                             "type checking should reject non-slice operands here");

    MirValueId slice_address = isAddressableExpr(slice_expr)
                               ? lowerExprAsAddress(state, scope, slice_expr)
                               : lowerExprMaterializedAddress(state, scope, slice_expr);
    ASTDataType *ptr_field_type = newWrappedDataType(AST_DATA_TYPE_KIND_POINTER,
                                                     cloneDataType(slice_type.data_type->child));
    MirValueId ptr_field_address = mirEmitFieldPtr(state, slice_address, ptr_field_type, "ptr", 0,
                                                   slice_expr->filename, slice_expr->line_number, slice_expr->column_number);
    return mirEmitLoad(state, ptr_field_address, ptr_field_type,
                       slice_expr->filename, slice_expr->line_number, slice_expr->column_number);
}

static MirValueId lowerSliceLenValue(MirFunctionState *state, MirLowerScope *scope, ASTNode *slice_expr)
{
    TypeSystemExprType slice_type = inferExprType(slice_expr, &(scope->type_scope));
    if(slice_type.kind != TYPE_SYSTEM_EXPR_TYPE_VALUE ||
       (!isSliceDataType(slice_type.data_type) && !isStringDataType(slice_type.data_type)))
        mirLoweringAbortNode("M2016", slice_expr,
                             "slice length extraction requires a slice-like value",
                             "type checking should reject non-slice operands here");

    MirValueId slice_address = isAddressableExpr(slice_expr)
                               ? lowerExprAsAddress(state, scope, slice_expr)
                               : lowerExprMaterializedAddress(state, scope, slice_expr);
    ASTDataType *len_field_type = newPrimaryDataType(AST_PRIMARY_DATA_TYPE_I64);
    MirValueId len_field_address = mirEmitFieldPtr(state, slice_address, len_field_type, "len", 1,
                                                   slice_expr->filename, slice_expr->line_number, slice_expr->column_number);
    return mirEmitLoad(state, len_field_address, len_field_type,
                       slice_expr->filename, slice_expr->line_number, slice_expr->column_number);
}

static MirValueId lowerLenBuiltinExpr(MirFunctionState *state, MirLowerScope *scope, ASTNode *node,
                                      ASTDataType *expected_type)
{
    MirValueId length_value = lowerSliceLenValue(state, scope, node->lhs);
    return mirMaybeConvertValue(state, scope, node, length_value, expected_type);
}

static MirValueId lowerPtrAddBuiltinExpr(MirFunctionState *state, MirLowerScope *scope, ASTNode *node,
                                         ASTDataType *expected_type)
{
    ASTNode *element_type_expr = node->lhs;
    ASTNode *pointer_expr = element_type_expr != NULL ? element_type_expr->next : NULL;
    ASTNode *count_expr = pointer_expr != NULL ? pointer_expr->next : NULL;
    TypeSystemExprType ptr_type = inferExprType(pointer_expr, &(scope->type_scope));
    ASTDataType *result_type = mirResolvedExprValueType(node, &(scope->type_scope));
    MirValueId base_ptr = lowerExprAsValue(state, scope, pointer_expr, ptr_type.data_type);
    MirValueId offset = lowerExprAsValue(state, scope, count_expr, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_I64));
    MirValueId result = mirEmitIndexPtr(state, base_ptr, offset, result_type->child, true,
                                        node->filename, node->line_number, node->column_number);
    return mirMaybeConvertValue(state, scope, node, result, expected_type);
}

static MirValueId lowerPtrDiffBuiltinExpr(MirFunctionState *state, MirLowerScope *scope, ASTNode *node,
                                          ASTDataType *expected_type)
{
    ASTNode *element_type_expr = node->lhs;
    ASTNode *lhs_expr = element_type_expr != NULL ? element_type_expr->next : NULL;
    ASTNode *rhs_expr = lhs_expr != NULL ? lhs_expr->next : NULL;
    ASTDataType *ptr_type = mirResolvedExprValueType(lhs_expr, &(scope->type_scope));
    MirValueId lhs = lowerExprAsValue(state, scope, lhs_expr, ptr_type);
    MirValueId rhs = lowerExprAsValue(state, scope, rhs_expr, ptr_type);
    MirValueId result = mirEmitPtrDiff(state, lhs, rhs, node->filename, node->line_number, node->column_number);
    return mirMaybeConvertValue(state, scope, node, result, expected_type);
}

static MirValueId lowerAsBuiltinExpr(MirFunctionState *state, MirLowerScope *scope, ASTNode *node,
                                     ASTDataType *expected_type)
{
    ASTNode *target_type_expr = node->lhs;
    ASTNode *value_expr = target_type_expr != NULL ? target_type_expr->next : NULL;
    ASTDataType *target_type = mirResolvedExprValueType(node, &(scope->type_scope));
    TypeSystemExprType source_type = value_expr != NULL ? inferExprType(value_expr, &(scope->type_scope))
                                                        : (TypeSystemExprType){0};

    if(value_expr != NULL &&
       value_expr->kind == AST_EXPR_LITERAL_STRING &&
       mirIsCharPointerTarget(target_type))
        return mirMaybeConvertValue(state, scope, node,
                                    lowerStringLiteralAsPointer(state, value_expr, target_type),
                                    expected_type);

    if(value_expr != NULL &&
       isLiteralIntegerZero(value_expr) &&
       (target_type->kind == AST_DATA_TYPE_KIND_POINTER ||
        target_type->kind == AST_DATA_TYPE_KIND_FUNCTION))
    {
        return mirMaybeConvertValue(state, scope, node,
                                    mirEmitZero(state, target_type,
                                                node->filename, node->line_number, node->column_number),
                                    expected_type);
    }

    if(value_expr != NULL &&
       source_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE &&
       source_type.data_type != NULL &&
       (source_type.data_type->kind == AST_DATA_TYPE_KIND_SLICE ||
        source_type.data_type->kind == AST_DATA_TYPE_KIND_STRING) &&
       target_type->kind == AST_DATA_TYPE_KIND_POINTER)
        return mirMaybeConvertValue(state, scope, node,
                                    lowerSlicePtrValue(state, scope, value_expr),
                                    expected_type);

    if(value_expr != NULL &&
       value_expr->kind == AST_EXPR_LITERAL_STRING &&
       source_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE &&
       source_type.data_type != NULL &&
       source_type.data_type->kind == AST_DATA_TYPE_KIND_STRING &&
       target_type->kind == AST_DATA_TYPE_KIND_ARRAY)
        return mirEmitConstString(state, value_expr->literal_string, cloneDataType(target_type),
                                  node->filename, node->line_number, node->column_number);

    if(value_expr != NULL && target_type->kind == AST_DATA_TYPE_KIND_FUNCTION)
    {
        ASTDataType *pointer_target = newWrappedDataType(AST_DATA_TYPE_KIND_POINTER,
                                                         newPrimaryDataType(AST_PRIMARY_DATA_TYPE_VOID));
        if(target_type->is_variadic)
            mirLoweringAbortNode("M2014", node,
                                 "cannot convert a raw function address into a variadic function value",
                                 "variadic function pointers are not supported by this closure bridge");

        MirValueId raw_ptr = lowerExprAsValue(state, scope, value_expr, pointer_target);
        MirOperandList captures = newMirOperandList(1);
        captures.items[0] = raw_ptr;
        ASTDataType *env_type = mirDynamicFunctionEnvType();
        const char *wrapper_name = mirEnsureDynamicFunctionWrapper(state->lowering, target_type);
        return mirMaybeConvertValue(state, scope, node,
                                    mirEmitMakeClosure(state, wrapper_name, target_type, env_type, captures,
                                                       node->filename, node->line_number, node->column_number),
                                    expected_type);
    }

    MirValueId value = lowerExprAsValue(state, scope, value_expr, NULL);
    value = mirEmitConvert(state, value, target_type,
                           node->filename, node->line_number, node->column_number);
    return mirMaybeConvertValue(state, scope, node, value, expected_type);
}

static MirValueId lowerSliceBuiltinExpr(MirFunctionState *state, MirLowerScope *scope, ASTNode *node,
                                        ASTDataType *expected_type)
{
    ASTDataType *slice_type = mirResolvedExprValueType(node, &(scope->type_scope));
    ASTNode *element_type_expr = node->lhs;
    ASTNode *pointer_expr = element_type_expr != NULL ? element_type_expr->next : NULL;
    ASTNode *length_expr = pointer_expr != NULL ? pointer_expr->next : NULL;

    MirFieldValueList fields = newMirFieldValueList(2);
    strcpy(fields.items[0].identifier, "ptr");
    fields.items[0].value = lowerExprAsValue(state, scope, pointer_expr,
                                             newWrappedDataType(AST_DATA_TYPE_KIND_POINTER,
                                                                cloneDataType(slice_type->child)));
    strcpy(fields.items[1].identifier, "len");
    fields.items[1].value = lowerExprAsValue(state, scope, length_expr,
                                             newPrimaryDataType(AST_PRIMARY_DATA_TYPE_I64));

    MirValueId value = mirEmitStructLiteral(state, fields, slice_type,
                                            node->filename, node->line_number, node->column_number);
    return mirMaybeConvertValue(state, scope, node, value, expected_type);
}

static MirValueId lowerUnwrapBuiltinExpr(MirFunctionState *state, MirLowerScope *scope, ASTNode *node,
                                         ASTDataType *expected_type)
{
    ASTNode *operand_expr = node->lhs;
    ASTDataType *optional_type = mirResolvedExprValueType(operand_expr, &(scope->type_scope));
    if(optional_type == NULL || optional_type->kind != AST_DATA_TYPE_KIND_OPTIONAL)
        mirLoweringAbortNode("M2004", node,
                             "@unwrap lowering expected an optional operand",
                             "type checking should reject non-optional unwrap operands");

    MirValueId optional_value = lowerExprAsValue(state, scope, operand_expr, optional_type);
    MirValueId optional_slot = mirEmitAlloca(state, optional_type,
                                             node->filename, node->line_number, node->column_number);
    mirEmitStore(state, optional_slot, optional_value,
                 node->filename, node->line_number, node->column_number);

    MirValueId has_value_ptr = mirEmitFieldPtr(state, optional_slot, mirOptionalBoolType(),
                                               "has_value", 0,
                                               node->filename, node->line_number, node->column_number);
    MirValueId has_value = mirEmitLoad(state, has_value_ptr, mirOptionalBoolType(),
                                       node->filename, node->line_number, node->column_number);

    MirFunction *function = mirCurrentFunction(state);
    MirBlockId ok_block = mirCreateBlock(state->lowering, function, "unwrap_ok");
    MirBlockId panic_block = mirCreateBlock(state->lowering, function, "unwrap_panic");
    MirBlockId end_block = mirCreateBlock(state->lowering, function, "unwrap_end");
    MirValueId result_slot = mirEmitAlloca(state, optional_type->child,
                                           node->filename, node->line_number, node->column_number);

    mirEmitCondBr(state, has_value, ok_block, panic_block);

    mirSwitchToBlock(state, panic_block);
    ASTFunctionParameter *panic_param = NULL;
    ASTDataType *panic_type = newFunctionDataType(panic_param, false, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_VOID));
    const char *panic_name = mirEnsureExternFunction(state->lowering, "mote_unwrap_null_panic", panic_type,
                                                     node->filename, node->line_number, node->column_number);
    MirOperandList panic_args = newMirOperandList(0);
    MirValueId panic_call = mirEmitExternCall(state, panic_name, panic_type, panic_args,
                                              newPrimaryDataType(AST_PRIMARY_DATA_TYPE_VOID),
                                              node->filename, node->line_number, node->column_number);
    (void)panic_call;
    mirEmitUnreachable(state);

    mirSwitchToBlock(state, ok_block);
    MirValueId inner_ptr = mirEmitFieldPtr(state, optional_slot, optional_type->child,
                                           "value", 1,
                                           node->filename, node->line_number, node->column_number);
    MirValueId inner_value = mirEmitLoad(state, inner_ptr, optional_type->child,
                                         node->filename, node->line_number, node->column_number);
    mirEmitStore(state, result_slot, inner_value,
                 node->filename, node->line_number, node->column_number);
    mirEmitBr(state, end_block);

    mirSwitchToBlock(state, end_block);
    MirValueId result = mirEmitLoad(state, result_slot, optional_type->child,
                                    node->filename, node->line_number, node->column_number);
    return mirMaybeConvertValue(state, scope, node, result, expected_type);
}

static ASTDataType* mirInferVariadicArgumentType(ASTNode *argument_node, MirLowerScope *scope)
{
    TypeSystemExprType argument_type = inferExprType(argument_node, &(scope->type_scope));
    ASTDataType *promoted_type = variadicPromotedExprType(argument_type);
    if(promoted_type == NULL)
        mirLoweringAbortNode("M2008", argument_node,
                             "variadic argument must be a runtime value",
                             "compile-time-only expressions cannot be passed through variadic ABI lowering");
    return promoted_type;
}

static MirValueId lowerDirectExternCall(MirFunctionState *state, MirLowerScope *scope, ASTNode *call_node,
                                        ASTNode *extern_node, ASTDataType *function_type,
                                        ASTDataType *expected_type)
{
    const char *symbol_name = mirEnsureExternFunction(
        state->lowering,
        extern_node->lhs->literal_string,
        function_type,
        call_node->filename,
        call_node->line_number,
        call_node->column_number
    );

    MirOperandList arguments = newMirOperandList(countASTNodes(call_node->rhs));
    ASTFunctionParameter *parameter = function_type->parameters;
    ASTNode *argument_node = call_node->rhs;
    int index = 0;
    while(argument_node)
    {
        ASTDataType *parameter_type = NULL;
        if(parameter != NULL)
        {
            parameter_type = parameter->data_type;
            parameter = parameter->next;
        }
        else
            parameter_type = mirInferVariadicArgumentType(argument_node, scope);

        if(parameter_type != NULL && parameter_type->kind == AST_DATA_TYPE_KIND_REFERENCE)
            arguments.items[index++] = lowerReferenceArgument(state, scope, argument_node, parameter_type);
        else
            arguments.items[index++] = lowerExprAsValue(state, scope, argument_node, parameter_type);

        argument_node = argument_node->next;
    }

    ASTDataType *return_type = mirResolvedExprValueType(call_node, &(scope->type_scope));
    MirValueId call_value = mirEmitExternCall(state, symbol_name, function_type, arguments, return_type,
                                              call_node->filename, call_node->line_number, call_node->column_number);
    if(mirIsValueTypeVoid(return_type))
        return call_value;
    return mirMaybeConvertValue(state, scope, call_node, call_value, expected_type);
}

static ASTDataType* inferComparisonOperandType(ASTNode *lhs, ASTNode *rhs, ScopeFrame *scope)
{
    TypeSystemExprType lhs_type = inferExprType(lhs, scope);
    TypeSystemExprType rhs_type = inferExprType(rhs, scope);

    if(lhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE && rhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE &&
       isSameDataType(lhs_type.data_type, rhs_type.data_type))
        return cloneDataType(lhs_type.data_type);

    if(lhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_NULL &&
       rhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE &&
       rhs_type.data_type != NULL &&
       rhs_type.data_type->kind == AST_DATA_TYPE_KIND_OPTIONAL)
        return cloneDataType(rhs_type.data_type);

    if(rhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_NULL &&
       lhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE &&
       lhs_type.data_type != NULL &&
       lhs_type.data_type->kind == AST_DATA_TYPE_KIND_OPTIONAL)
        return cloneDataType(lhs_type.data_type);

    if(lhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_LITERAL_INTEGER || lhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_LITERAL_FLOAT)
    {
        if(rhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE)
            return cloneDataType(rhs_type.data_type);
    }

    if(rhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_LITERAL_INTEGER || rhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_LITERAL_FLOAT)
    {
        if(lhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE)
            return cloneDataType(lhs_type.data_type);
    }

    if(lhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE)
        return cloneDataType(lhs_type.data_type);
    if(rhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE)
        return cloneDataType(rhs_type.data_type);
    return defaultIntegerDataType();
}

static MirValueId lowerOptionalFieldLoad(MirFunctionState *state, MirValueId optional_slot,
                                         ASTDataType *field_type, const char *field_name, int field_index,
                                         ASTNode *node)
{
    MirValueId field_ptr = mirEmitFieldPtr(state, optional_slot, field_type, field_name, field_index,
                                           node->filename, node->line_number, node->column_number);
    return mirEmitLoad(state, field_ptr, field_type,
                       node->filename, node->line_number, node->column_number);
}

static MirValueId lowerValueEqualityCompare(MirFunctionState *state, MirLowerScope *scope,
                                            ASTNode *node, ASTDataType *operand_type,
                                            MirValueId lhs, MirValueId rhs, bool is_equal)
{
    ASTDataType *resolved_operand_type = resolveNamedDataType(operand_type, &(scope->type_scope), scope->self_data_type);
    if(resolved_operand_type != NULL &&
       resolved_operand_type->kind == AST_DATA_TYPE_KIND_OPTIONAL)
    {
        MirValueId lhs_slot = mirEmitAlloca(state, resolved_operand_type,
                                            node->filename, node->line_number, node->column_number);
        MirValueId rhs_slot = mirEmitAlloca(state, resolved_operand_type,
                                            node->filename, node->line_number, node->column_number);
        mirEmitStore(state, lhs_slot, lhs, node->filename, node->line_number, node->column_number);
        mirEmitStore(state, rhs_slot, rhs, node->filename, node->line_number, node->column_number);

        ASTDataType *bool_type = mirOptionalBoolType();
        MirValueId lhs_has_value = lowerOptionalFieldLoad(state, lhs_slot, bool_type, "has_value", 0, node);
        MirValueId rhs_has_value = lowerOptionalFieldLoad(state, rhs_slot, bool_type, "has_value", 0, node);
        MirValueId same_has_value = mirEmitBinary(state, MIR_INST_EQ, lhs_has_value, rhs_has_value, bool_type,
                                                  node->filename, node->line_number, node->column_number);

        MirValueId lhs_inner = lowerOptionalFieldLoad(state, lhs_slot, resolved_operand_type->child, "value", 1, node);
        MirValueId rhs_inner = lowerOptionalFieldLoad(state, rhs_slot, resolved_operand_type->child, "value", 1, node);
        MirValueId same_inner = lowerValueEqualityCompare(state, scope, node, resolved_operand_type->child,
                                                          lhs_inner, rhs_inner, true);

        MirValueId both_none = mirEmitUnary(state, MIR_INST_NOT, lhs_has_value, bool_type,
                                            node->filename, node->line_number, node->column_number);
        both_none = mirEmitBinary(state, MIR_INST_BIT_AND, both_none,
                                  mirEmitUnary(state, MIR_INST_NOT, rhs_has_value, bool_type,
                                               node->filename, node->line_number, node->column_number),
                                  bool_type,
                                  node->filename, node->line_number, node->column_number);
        MirValueId both_some = mirEmitBinary(state, MIR_INST_BIT_AND, lhs_has_value, rhs_has_value, bool_type,
                                             node->filename, node->line_number, node->column_number);
        MirValueId some_and_equal = mirEmitBinary(state, MIR_INST_BIT_AND, both_some, same_inner, bool_type,
                                                  node->filename, node->line_number, node->column_number);
        MirValueId equal_value = mirEmitBinary(state, MIR_INST_BIT_OR, both_none, some_and_equal, bool_type,
                                               node->filename, node->line_number, node->column_number);
        equal_value = mirEmitBinary(state, MIR_INST_BIT_AND, same_has_value, equal_value, bool_type,
                                    node->filename, node->line_number, node->column_number);
        if(is_equal)
            return equal_value;
        return mirEmitUnary(state, MIR_INST_NOT, equal_value, bool_type,
                            node->filename, node->line_number, node->column_number);
    }

    MirInstKind kind = is_equal ? MIR_INST_EQ : MIR_INST_NE;
    return mirEmitBinary(state, kind, lhs, rhs, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_BOOL),
                         node->filename, node->line_number, node->column_number);
}

static MirValueId lowerCallExpr(MirFunctionState *state, MirLowerScope *scope, ASTNode *node, ASTDataType *expected_type)
{
    MirMaybeValue comptime_call = tryLowerComptimeFunctionCall(state, scope, node);
    if(comptime_call.valid)
        return mirMaybeConvertValue(state, scope, node, comptime_call.value, expected_type);

    ASTNode *direct_generic_function = NULL;
    const char *direct_name_hint = NULL;
    if(tryGetDirectGenericFunctionValue(scope, node->lhs, &direct_generic_function, &direct_name_hint) &&
       functionHasTypeParameters(direct_generic_function->parameters))
    {
        ASTFunctionParameter *specialized_parameters = NULL;
        ASTDataType *specialized_return_type = NULL;
        MirValueId callee = lowerRuntimeSpecializedFunctionValue(
            state,
            scope,
            direct_generic_function,
            node->rhs,
            direct_name_hint,
            NULL,
            &specialized_parameters,
            &specialized_return_type
        );
        MirOperandList arguments = lowerSpecializedCallArguments(
            state,
            scope,
            direct_generic_function->parameters,
            specialized_parameters,
            node->rhs
        );

        MirValueId call_value = mirEmitCall(state, callee, arguments, specialized_return_type,
                                            node->filename, node->line_number, node->column_number);
        if(mirIsValueTypeVoid(specialized_return_type))
            return call_value;
        return mirMaybeConvertValue(state, scope, node, call_value, expected_type);
    }

    if(node->lhs->kind == AST_EXPR_VARIABLE)
    {
        VariableInfo *callee_variable = findVariableInfo(&(scope->type_scope), node->lhs->identifier);
        MirRuntimeBinding *callee_binding = findMirRuntimeBinding(scope, node->lhs->identifier);
        if(callee_variable != NULL &&
           callee_variable->function_value != NULL &&
           callee_binding != NULL &&
           callee_binding->kind == MIR_RUNTIME_BINDING_COMPTIME_ONLY)
        {
            ASTFunctionParameter *specialized_parameters = NULL;
            ASTDataType *specialized_return_type = NULL;
            MirValueId callee = lowerRuntimeSpecializedFunctionValue(
                state,
                scope,
                callee_variable->function_value,
                node->rhs,
                node->lhs->identifier,
                NULL,
                &specialized_parameters,
                &specialized_return_type
            );
            MirOperandList arguments = lowerSpecializedCallArguments(
                state,
                scope,
                callee_variable->function_value->parameters,
                specialized_parameters,
                node->rhs
            );

            MirValueId call_value = mirEmitCall(state, callee, arguments, specialized_return_type,
                                                node->filename, node->line_number, node->column_number);
            if(mirIsValueTypeVoid(specialized_return_type))
                return call_value;
            return mirMaybeConvertValue(state, scope, node, call_value, expected_type);
        }

        if(callee_variable != NULL && callee_variable->extern_value != NULL)
        {
            ASTDataType *function_type = mirResolvedExprValueType(node->lhs, &(scope->type_scope));
            if(function_type != NULL && function_type->is_variadic)
                return lowerDirectExternCall(state, scope, node, callee_variable->extern_value, function_type, expected_type);
        }
    }

    ASTNode *callee_expr = node->lhs;
    MirValueId callee = -1;
    MirOperandList arguments = {0};
    ASTDataType *return_type = mirResolvedExprValueType(node, &(scope->type_scope));

    if(callee_expr->kind == AST_EXPR_MEMBER)
    {
        ASTNode *member_node = callee_expr;
        TypeSystemExprType owner_type = inferExprType(member_node->lhs, &(scope->type_scope));
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
        struct_type = resolveNamedDataType(struct_type, &(scope->type_scope), scope->self_data_type);

        if(isStructDataType(struct_type))
        {
            ASTStructMember *member = findStructMember(struct_type, member_node->identifier);
            if(member != NULL && member->value != NULL && member->data_type != NULL &&
               member->data_type->kind == AST_DATA_TYPE_KIND_FUNCTION)
            {
                callee = lowerMethodFunctionValue(state, scope, member_node, struct_type);
                int receiver_count = 0;
                ASTFunctionParameter *parameter = member->data_type->parameters;
                if(!through_type && parameter != NULL &&
                   canBindMethodReceiver(member_node->lhs, &(scope->type_scope), parameter->data_type, struct_type))
                    receiver_count = 1;

                int arg_count = countASTNodes(node->rhs) + receiver_count;
                arguments = newMirOperandList(arg_count);
                int index = 0;
                if(receiver_count == 1)
                {
                    if(parameter->data_type->kind == AST_DATA_TYPE_KIND_REFERENCE)
                        arguments.items[index++] = lowerReferenceArgument(state, scope, member_node->lhs, parameter->data_type);
                    else if(parameter->data_type->kind == AST_DATA_TYPE_KIND_POINTER)
                    {
                        TypeSystemExprType receiver_type = inferExprType(member_node->lhs, &(scope->type_scope));
                        if(receiver_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE &&
                           receiver_type.data_type->kind == AST_DATA_TYPE_KIND_POINTER &&
                           canImplicitConvertDataType(receiver_type, member_node->lhs, parameter->data_type))
                            arguments.items[index++] = lowerExprAsValue(state, scope, member_node->lhs, parameter->data_type);
                        else
                            arguments.items[index++] = lowerExprAsAddress(state, scope, member_node->lhs);
                    }
                    else
                        arguments.items[index++] = lowerExprAsValue(state, scope, member_node->lhs, parameter->data_type);
                    parameter = parameter->next;
                }

                ASTNode *argument_node = node->rhs;
                while(argument_node)
                {
                    ASTDataType *parameter_type = parameter == NULL ? NULL : parameter->data_type;
                    if(parameter_type != NULL && parameter_type->kind == AST_DATA_TYPE_KIND_REFERENCE)
                        arguments.items[index++] = lowerReferenceArgument(state, scope, argument_node, parameter_type);
                    else
                        arguments.items[index++] = lowerExprAsValue(state, scope, argument_node, parameter_type);
                    if(parameter != NULL)
                        parameter = parameter->next;
                    argument_node = argument_node->next;
                }

                MirValueId call_value = mirEmitCall(state, callee, arguments, return_type,
                                                    node->filename, node->line_number, node->column_number);
                if(mirIsValueTypeVoid(return_type))
                    return call_value;
                return mirMaybeConvertValue(state, scope, node, call_value, expected_type);
            }
        }
    }

    callee = lowerExprAsValue(state, scope, callee_expr, NULL);
    TypeSystemExprType callee_type = inferExprType(callee_expr, &(scope->type_scope));
    ASTFunctionParameter *parameter = callee_type.data_type == NULL ? NULL : callee_type.data_type->parameters;

    arguments = newMirOperandList(countASTNodes(node->rhs));
    ASTNode *argument_node = node->rhs;
    int index = 0;
    while(argument_node)
    {
        ASTDataType *parameter_type = parameter == NULL ? NULL : parameter->data_type;
        if(parameter_type != NULL && parameter_type->kind == AST_DATA_TYPE_KIND_REFERENCE)
            arguments.items[index++] = lowerReferenceArgument(state, scope, argument_node, parameter_type);
        else
            arguments.items[index++] = lowerExprAsValue(state, scope, argument_node, parameter_type);
        if(parameter != NULL)
            parameter = parameter->next;
        argument_node = argument_node->next;
    }

    MirValueId call_value = mirEmitCall(state, callee, arguments, return_type,
                                        node->filename, node->line_number, node->column_number);
    if(mirIsValueTypeVoid(return_type))
        return call_value;
    return mirMaybeConvertValue(state, scope, node, call_value, expected_type);
}

static MirValueId lowerOperatorOverloadCall(MirFunctionState *state,
                                            MirLowerScope *scope,
                                            ASTNode *node,
                                            ASTOperatorKind operator_kind,
                                            ASTNode *lhs_expr,
                                            ASTNode *rhs_expr,
                                            ASTDataType *expected_type)
{
    ResolvedOperatorOverload overload = {0};
    if(!resolveOperatorOverload(operator_kind, lhs_expr, rhs_expr, &(scope->type_scope), &overload))
        return -1;

    ASTDataType *function_type = overload.function_value->data_type;
    ASTFunctionParameter *parameter = function_type->parameters;
    int argument_count = lhs_expr != NULL ? 1 : 0;
    if(rhs_expr != NULL)
        argument_count++;
    MirOperandList arguments = newMirOperandList(argument_count);
    int argument_index = 0;

    if(lhs_expr != NULL && parameter != NULL)
    {
        MirValueId lhs = lowerExprAsValue(state, scope, lhs_expr, parameter->data_type);
        arguments.items[argument_index++] = lhs;
        parameter = parameter->next;
    }

    if(rhs_expr != NULL && parameter != NULL)
    {
        MirValueId rhs = lowerExprAsValue(state, scope, rhs_expr, parameter->data_type);
        arguments.items[argument_index++] = rhs;
    }

    MirValueId callee = lowerFunctionExprAsValue(state, scope, overload.function_value, "operator", NULL);
    MirValueId call_value = mirEmitCall(state, callee, arguments, overload.result_type,
                                        node->filename, node->line_number, node->column_number);
    if(node->kind == AST_EXPR_NOT_EQUAL)
    {
        MirValueId negated = mirEmitUnary(state, MIR_INST_NOT, call_value, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_BOOL),
                                          node->filename, node->line_number, node->column_number);
        return mirMaybeConvertValue(state, scope, node, negated, expected_type);
    }
    if(mirIsValueTypeVoid(overload.result_type))
        return call_value;
    return mirMaybeConvertValue(state, scope, node, call_value, expected_type);
}

static MirValueId lowerExprAsValue(MirFunctionState *state, MirLowerScope *scope, ASTNode *node, ASTDataType *expected_type)
{
    if(node->kind == AST_EXPR_ARRAY_LITERAL && node->lhs == NULL)
    {
        ASTDataType *array_type = expected_type != NULL ? cloneDataType(expected_type) : mirResolvedExprValueType(node, &(scope->type_scope));
        MirOperandList elements = newMirOperandList(0);
        return mirEmitArrayLiteral(state, elements, array_type,
                                   node->filename, node->line_number, node->column_number);
    }

    if(node->kind == AST_EXPR_STRUCT_LITERAL)
    {
        ASTDataType *struct_type = NULL;
        if(expected_type != NULL && expected_type->kind == AST_DATA_TYPE_KIND_STRUCT)
            struct_type = cloneDataType(expected_type);
        else
        {
            TypeSystemExprType type_expr = inferExprType(node->lhs, &(scope->type_scope));
            struct_type = resolveNamedDataType(type_expr.data_type, &(scope->type_scope), scope->self_data_type);
        }
        int field_count = countStructDataFields(struct_type);
        MirFieldValueList fields = newMirFieldValueList(field_count);
        ASTStructMember *member = struct_type->members;
        int index = 0;
        bool literal_fields_have_explicit_names = node->struct_literal_fields != NULL && node->struct_literal_fields->has_name;
        while(member)
        {
            if(member->value == NULL)
            {
                ASTStructLiteralField *field = node->struct_literal_fields;

                if(field->has_name != literal_fields_have_explicit_names)
                    mirLoweringAbortNode("M2011", node,
                                         "struct literal fields must either all have explicit names or no explicit names",
                                         member->identifier);
                
                if(literal_fields_have_explicit_names) // then find the field with the matching name
                {
                    while(field && strcmp(field->identifier, member->identifier) != 0) field = field->next;
                    strcpy(fields.items[index].identifier, member->identifier);
                    fields.items[index].value = lowerExprAsValue(state, scope, field->value, member->data_type);
                    index++;
                }
                else // otherwise take the next field in order
                {
                    strcpy(fields.items[index].identifier, member->identifier);
                    fields.items[index].value = lowerExprAsValue(state, scope, node->struct_literal_fields->value, member->data_type);
                    node->struct_literal_fields = node->struct_literal_fields->next;
                    index++;
                }
                
            }
            member = member->next;
        }
        MirValueId value = mirEmitStructLiteral(state, fields, struct_type,
                                                node->filename, node->line_number, node->column_number);
        return mirMaybeConvertValue(state, scope, node, value, expected_type);
    }

    TypeSystemExprType expr_type = inferExprType(node, &(scope->type_scope));

    switch(node->kind)
    {
        case AST_EXPR_LITERAL_BOOL:
            return mirMaybeConvertValue(state, scope, node,
                                        mirEmitConstBool(state, node->literal_bool, node->filename, node->line_number, node->column_number),
                                        expected_type);
        case AST_EXPR_LITERAL_NULL:
            if(expected_type == NULL || expected_type->kind != AST_DATA_TYPE_KIND_OPTIONAL)
                mirLoweringAbortNode("M2010", node,
                                     "`null` requires an expected optional type",
                                     "add an explicit optional type like `?T`");
            return mirLowerOptionalNull(state, expected_type,
                                        node->filename, node->line_number, node->column_number);
        case AST_EXPR_LITERAL_CHAR:
            return mirMaybeConvertValue(state, scope, node,
                                        mirEmitConstChar(state, node->literal_char, node->filename, node->line_number, node->column_number),
                                        expected_type);
        case AST_EXPR_LITERAL_INTEGER: {
            ASTDataType *literal_type = expected_type;
            if(literal_type != NULL && literal_type->kind == AST_DATA_TYPE_KIND_OPTIONAL)
                literal_type = literal_type->child;
            if(literal_type == NULL)
                literal_type = defaultIntegerDataType();
            return mirMaybeConvertValue(state, scope, node,
                                        mirEmitConstInt(state, node->literal_integer.magnitude, literal_type,
                                                        node->filename, node->line_number, node->column_number),
                                        expected_type);
        }
        case AST_EXPR_LITERAL_FLOAT: {
            ASTDataType *literal_type = expected_type;
            if(literal_type != NULL && literal_type->kind == AST_DATA_TYPE_KIND_OPTIONAL)
                literal_type = literal_type->child;
            if(literal_type == NULL)
                literal_type = defaultFloatDataType();
            return mirMaybeConvertValue(state, scope, node,
                                        mirEmitConstFloat(state, node->literal_float, literal_type,
                                                          node->filename, node->line_number, node->column_number),
                                        expected_type);
        }
        case AST_EXPR_LITERAL_STRING: {
            if(mirIsCharPointerTarget(expected_type))
                return mirMaybeConvertValue(state, scope, node,
                                            lowerStringLiteralAsPointer(state, node, expected_type),
                                            expected_type);
            if(expr_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE &&
               expr_type.data_type != NULL &&
               expr_type.data_type->kind == AST_DATA_TYPE_KIND_ARRAY)
            {
                return mirEmitConstString(state, node->literal_string, cloneDataType(expr_type.data_type),
                                          node->filename, node->line_number, node->column_number);
            }
            return mirMaybeConvertValue(state, scope, node,
                                        lowerStringLiteralAsString(state, node),
                                        expected_type);
        }
        case AST_EXPR_BUILTIN:
            if(strcmp(node->identifier, "extern") == 0)
                return lowerExternBuiltinExpr(state, scope, node, expected_type);
            if(strcmp(node->identifier, "sizeof") == 0)
                return lowerTypeLayoutBuiltinExpr(state, scope, node, false);
            if(strcmp(node->identifier, "alignof") == 0)
                return lowerTypeLayoutBuiltinExpr(state, scope, node, true);
            if(strcmp(node->identifier, "zero") == 0)
                return lowerZeroBuiltinExpr(state, scope, node, expected_type);
            if(strcmp(node->identifier, "debug") == 0)
                return lowerDebugBuiltinExpr(state, scope, node);
            if(strcmp(node->identifier, "panic") == 0)
                return lowerPanicBuiltinExpr(state, scope, node);
            if(strcmp(node->identifier, "assert") == 0)
                return lowerAssertBuiltinExpr(state, scope, node);
            if(strcmp(node->identifier, "len") == 0)
                return lowerLenBuiltinExpr(state, scope, node, expected_type);
            if(strcmp(node->identifier, "ptr_add") == 0)
                return lowerPtrAddBuiltinExpr(state, scope, node, expected_type);
            if(strcmp(node->identifier, "ptr_diff") == 0)
                return lowerPtrDiffBuiltinExpr(state, scope, node, expected_type);
            if(strcmp(node->identifier, "as") == 0)
                return lowerAsBuiltinExpr(state, scope, node, expected_type);
            if(strcmp(node->identifier, "slice") == 0)
                return lowerSliceBuiltinExpr(state, scope, node, expected_type);
            if(strcmp(node->identifier, "unwrap") == 0)
                return lowerUnwrapBuiltinExpr(state, scope, node, expected_type);
            mirLoweringAbortNodeFormatted("M2009", node,
                                          "builtin lowering is missing",
                                          "unsupported builtin `@%s`",
                                          node->identifier);
        case AST_EXPR_VARIABLE:
            return lowerVariableValue(state, scope, node, expected_type);
        case AST_EXPR_PARENTHESIS:
            return lowerExprAsValue(state, scope, node->lhs, expected_type);
        case AST_EXPR_FUNCTION:
            return lowerFunctionExprAsValue(state, scope, node, NULL, scope->self_data_type);
        case AST_EXPR_ARRAY_LITERAL: {
            ASTDataType *array_type = expected_type != NULL ? cloneDataType(expected_type)
                                                            : mirResolvedExprValueType(node, &(scope->type_scope));
            int count = countASTNodes(node->lhs);
            MirOperandList elements = newMirOperandList(count);
            ASTNode *element = node->lhs;
            int index = 0;
            while(element)
            {
                elements.items[index++] = lowerExprAsValue(state, scope, element, array_type->child);
                element = element->next;
            }
            MirValueId value = mirEmitArrayLiteral(state, elements, array_type,
                                                   node->filename, node->line_number, node->column_number);
            return mirMaybeConvertValue(state, scope, node, value, expected_type);
        }
        case AST_EXPR_MEMBER: {
            TypeSystemExprType owner_type = inferExprType(node->lhs, &(scope->type_scope));
            ASTDataType *owner_data_type = NULL;
            if(owner_type.kind == TYPE_SYSTEM_EXPR_TYPE_TYPE)
                owner_data_type = owner_type.data_type;
            else
            {
                owner_data_type = owner_type.data_type;
                if(owner_data_type->kind == AST_DATA_TYPE_KIND_POINTER || owner_data_type->kind == AST_DATA_TYPE_KIND_REFERENCE)
                    owner_data_type = owner_data_type->child;
            }

            if(isEnumDataType(owner_data_type))
            {
                int ordinal = findEnumVariantOrdinal(owner_data_type, node->identifier);
                return mirEmitEnumLiteral(state, owner_data_type, owner_data_type->identifier, node->identifier, ordinal,
                                          node->filename, node->line_number, node->column_number);
            }

            ASTStructMember *member = findStructMember(owner_data_type, node->identifier);
            if(member != NULL && member->value != NULL)
                return lowerMethodFunctionValue(state, scope, node, owner_data_type);

            MirValueId address = lowerExprAsAddress(state, scope, node);
            ASTDataType *value_type = mirResolvedExprValueType(node, &(scope->type_scope));
            MirValueId value = mirEmitLoad(state, address, value_type,
                                           node->filename, node->line_number, node->column_number);
            return mirMaybeConvertValue(state, scope, node, value, expected_type);
        }
        case AST_EXPR_INDEX: {
            MirValueId address = lowerExprAsAddress(state, scope, node);
            ASTDataType *value_type = mirResolvedExprValueType(node, &(scope->type_scope));
            MirValueId value = mirEmitLoad(state, address, value_type,
                                           node->filename, node->line_number, node->column_number);
            return mirMaybeConvertValue(state, scope, node, value, expected_type);
        }
        case AST_EXPR_CALL:
            return lowerCallExpr(state, scope, node, expected_type);
        case AST_EXPR_UNARY_PLUS:
            if(expected_type != NULL)
            {
                TypeSystemExprType operand_type = inferExprType(node->lhs, &(scope->type_scope));
                if((operand_type.kind == TYPE_SYSTEM_EXPR_TYPE_LITERAL_INTEGER ||
                    operand_type.kind == TYPE_SYSTEM_EXPR_TYPE_LITERAL_FLOAT) &&
                   expected_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
                   (isIntegerPrimary(expected_type->primary) || isFloatPrimary(expected_type->primary)))
                    return lowerExprAsValue(state, scope, node->lhs, expected_type);
            }
            return lowerExprAsValue(state, scope, node->lhs, expected_type);
        case AST_EXPR_UNARY_MINUS: {
            MirValueId overload = lowerOperatorOverloadCall(state, scope, node,
                                                            AST_OPERATOR_SUB,
                                                            node->lhs, NULL, expected_type);
            if(overload >= 0)
                return overload;
            ASTDataType *result_type = mirPreferredExprValueType(node, &(scope->type_scope), expected_type);
            TypeSystemExprType operand_type = inferExprType(node->lhs, &(scope->type_scope));
            if(expected_type != NULL &&
               expected_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
               (isIntegerPrimary(expected_type->primary) || isFloatPrimary(expected_type->primary)) &&
               (operand_type.kind == TYPE_SYSTEM_EXPR_TYPE_LITERAL_INTEGER ||
                operand_type.kind == TYPE_SYSTEM_EXPR_TYPE_LITERAL_FLOAT))
                result_type = expected_type;
            if(node->lhs->kind == AST_EXPR_LITERAL_INTEGER)
                return mirMaybeConvertValue(state, scope, node,
                                            mirEmitConstInt(state, mirNegateLiteralIntegerOrAbort(node->lhs), result_type,
                                                            node->filename, node->line_number, node->column_number),
                                            expected_type);
            if(node->lhs->kind == AST_EXPR_LITERAL_FLOAT)
                return mirMaybeConvertValue(state, scope, node,
                                            mirEmitConstFloat(state, -(node->lhs->literal_float), result_type,
                                                              node->filename, node->line_number, node->column_number),
                                            expected_type);
            MirValueId operand = lowerExprAsValue(state, scope, node->lhs, result_type);
            MirValueId value = mirEmitUnary(state, MIR_INST_NEG, operand, result_type,
                                            node->filename, node->line_number, node->column_number);
            return mirMaybeConvertValue(state, scope, node, value, expected_type);
        }
        case AST_EXPR_UNARY_LOGICAL_NOT: {
            MirValueId operand = lowerExprAsValue(state, scope, node->lhs, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_BOOL));
            MirValueId value = mirEmitUnary(state, MIR_INST_NOT, operand, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_BOOL),
                                            node->filename, node->line_number, node->column_number);
            return mirMaybeConvertValue(state, scope, node, value, expected_type);
        }
        case AST_EXPR_UNARY_BIT_NOT: {
            ASTDataType *result_type = mirResolvedExprValueType(node, &(scope->type_scope));
            MirValueId operand = lowerExprAsValue(state, scope, node->lhs, result_type);
            MirValueId value = mirEmitUnary(state, MIR_INST_BIT_NOT, operand, result_type,
                                            node->filename, node->line_number, node->column_number);
            return mirMaybeConvertValue(state, scope, node, value, expected_type);
        }
        case AST_EXPR_ADDRESS_OF:
        case AST_EXPR_ADDRESS_OF_MUT:
            return lowerExprAsAddress(state, scope, node->lhs);
        case AST_EXPR_DEREF: {
            MirValueId pointer_value = lowerExprAsValue(state, scope, node->lhs, NULL);
            ASTDataType *value_type = mirResolvedExprValueType(node, &(scope->type_scope));
            MirValueId value = mirEmitLoad(state, pointer_value, value_type,
                                           node->filename, node->line_number, node->column_number);
            return mirMaybeConvertValue(state, scope, node, value, expected_type);
        }
        case AST_EXPR_LOGICAL_AND:
            return lowerLogicalShortCircuit(state, scope, node, true);
        case AST_EXPR_LOGICAL_OR:
            return lowerLogicalShortCircuit(state, scope, node, false);
        case AST_EXPR_ADD:
        case AST_EXPR_SUB:
        case AST_EXPR_MUL:
        case AST_EXPR_DIV:
        case AST_EXPR_MOD:
        case AST_EXPR_SHIFT_LEFT:
        case AST_EXPR_SHIFT_RIGHT:
        case AST_EXPR_BIT_AND:
        case AST_EXPR_BIT_OR:
        case AST_EXPR_BIT_XOR: {
            ASTOperatorKind operator_kind = mirBinaryExprOperatorKind(node->kind);
            if(operator_kind != AST_OPERATOR_NONE)
            {
                MirValueId overload = lowerOperatorOverloadCall(state, scope, node,
                                                                operator_kind,
                                                                node->lhs, node->rhs, expected_type);
                if(overload >= 0)
                    return overload;
            }
            ASTDataType *result_type = mirResolvedExprValueType(node, &(scope->type_scope));
            if(result_type != NULL && result_type->kind == AST_DATA_TYPE_KIND_OPTIONAL)
                result_type = result_type->child;
            if(expected_type != NULL &&
               result_type != NULL &&
               result_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
               expected_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
               ((isFloatPrimary(result_type->primary) && isFloatPrimary(expected_type->primary)) ||
                (isIntegerPrimary(result_type->primary) && isIntegerPrimary(expected_type->primary))))
                result_type = cloneDataType(expected_type);
            else if(result_type != NULL &&
                    result_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
                    isIntegerPrimary(result_type->primary))
            {
                TypeSystemExprType lhs_type = inferExprType(node->lhs, &(scope->type_scope));
                TypeSystemExprType rhs_type = inferExprType(node->rhs, &(scope->type_scope));
                if(lhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE &&
                   lhs_type.data_type != NULL &&
                   lhs_type.data_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
                   isIntegerPrimary(lhs_type.data_type->primary))
                    result_type = cloneDataType(lhs_type.data_type);
                else if(rhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE &&
                        rhs_type.data_type != NULL &&
                        rhs_type.data_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
                        isIntegerPrimary(rhs_type.data_type->primary))
                    result_type = cloneDataType(rhs_type.data_type);
            }
            MirValueId lhs = lowerExprAsValue(state, scope, node->lhs, result_type);
            MirValueId rhs = lowerExprAsValue(state, scope, node->rhs, result_type);
            MirInstKind kind = MIR_INST_ADD;
            if(node->kind == AST_EXPR_SUB) kind = MIR_INST_SUB;
            else if(node->kind == AST_EXPR_MUL) kind = MIR_INST_MUL;
            else if(node->kind == AST_EXPR_DIV) kind = MIR_INST_DIV;
            else if(node->kind == AST_EXPR_MOD) kind = MIR_INST_MOD;
            else if(node->kind == AST_EXPR_SHIFT_LEFT) kind = MIR_INST_SHIFT_LEFT;
            else if(node->kind == AST_EXPR_SHIFT_RIGHT) kind = MIR_INST_SHIFT_RIGHT;
            else if(node->kind == AST_EXPR_BIT_AND) kind = MIR_INST_BIT_AND;
            else if(node->kind == AST_EXPR_BIT_OR) kind = MIR_INST_BIT_OR;
            else if(node->kind == AST_EXPR_BIT_XOR) kind = MIR_INST_BIT_XOR;
            MirValueId value = mirEmitBinary(state, kind, lhs, rhs, result_type,
                                             node->filename, node->line_number, node->column_number);
            return mirMaybeConvertValue(state, scope, node, value, expected_type);
        }
        case AST_EXPR_EQUAL:
        case AST_EXPR_NOT_EQUAL:
        case AST_EXPR_LESS:
        case AST_EXPR_LESS_EQUAL:
        case AST_EXPR_GREATER:
        case AST_EXPR_GREATER_EQUAL: {
            ASTOperatorKind operator_kind = mirBinaryExprOperatorKind(node->kind);
            if(operator_kind != AST_OPERATOR_NONE)
            {
                MirValueId overload = lowerOperatorOverloadCall(state, scope, node,
                                                                operator_kind,
                                                                node->lhs, node->rhs, expected_type);
                if(overload >= 0)
                    return overload;
            }
            ASTDataType *operand_type = inferComparisonOperandType(node->lhs, node->rhs, &(scope->type_scope));
            if((node->kind == AST_EXPR_EQUAL || node->kind == AST_EXPR_NOT_EQUAL) &&
               operand_type != NULL &&
               operand_type->kind == AST_DATA_TYPE_KIND_OPTIONAL)
            {
                TypeSystemExprType lhs_type = inferExprType(node->lhs, &(scope->type_scope));
                TypeSystemExprType rhs_type = inferExprType(node->rhs, &(scope->type_scope));
                bool is_equal = node->kind == AST_EXPR_EQUAL;
                if(lhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_NULL || rhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_NULL)
                {
                    MirValueId optional_value = lhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_NULL
                        ? lowerExprAsValue(state, scope, node->rhs, operand_type)
                        : lowerExprAsValue(state, scope, node->lhs, operand_type);

                    MirValueId optional_slot = mirEmitAlloca(state, operand_type,
                                                             node->filename, node->line_number, node->column_number);
                    mirEmitStore(state, optional_slot, optional_value,
                                 node->filename, node->line_number, node->column_number);
                    MirValueId has_value = lowerOptionalFieldLoad(state, optional_slot, mirOptionalBoolType(),
                                                                  "has_value", 0, node);
                    MirValueId result = has_value;
                    if(is_equal)
                        result = mirEmitUnary(state, MIR_INST_NOT, has_value, mirOptionalBoolType(),
                                              node->filename, node->line_number, node->column_number);
                    return mirMaybeConvertValue(state, scope, node, result, expected_type);
                }

                MirValueId lhs = lowerExprAsValue(state, scope, node->lhs, operand_type);
                MirValueId rhs = lowerExprAsValue(state, scope, node->rhs, operand_type);
                MirValueId value = lowerValueEqualityCompare(state, scope, node, operand_type, lhs, rhs, is_equal);
                return mirMaybeConvertValue(state, scope, node, value, expected_type);
            }
            MirValueId lhs = lowerExprAsValue(state, scope, node->lhs, operand_type);
            MirValueId rhs = lowerExprAsValue(state, scope, node->rhs, operand_type);
            MirInstKind kind = MIR_INST_EQ;
            if(node->kind == AST_EXPR_NOT_EQUAL) kind = MIR_INST_NE;
            else if(node->kind == AST_EXPR_LESS) kind = MIR_INST_LT;
            else if(node->kind == AST_EXPR_LESS_EQUAL) kind = MIR_INST_LE;
            else if(node->kind == AST_EXPR_GREATER) kind = MIR_INST_GT;
            else if(node->kind == AST_EXPR_GREATER_EQUAL) kind = MIR_INST_GE;
            MirValueId value = mirEmitBinary(state, kind, lhs, rhs, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_BOOL),
                                             node->filename, node->line_number, node->column_number);
            return mirMaybeConvertValue(state, scope, node, value, expected_type);
        }
        default:
            mirLoweringAbortNodeFormatted("ICE0304", node,
                                          NULL,
                                          "MIR lowering hit unsupported expression kind %s",
                                          astNodeKindToString(node->kind));
    }
}

static MirValueId lowerExprAsAddress(MirFunctionState *state, MirLowerScope *scope, ASTNode *node)
{
    switch(node->kind)
    {
        case AST_EXPR_VARIABLE: {
            MirRuntimeBinding *binding = findMirRuntimeBinding(scope, node->identifier);
            if(binding == NULL || binding->kind == MIR_RUNTIME_BINDING_COMPTIME_ONLY)
                mirLoweringAbortNodeFormatted("M2010", node,
                                              "this variable does not have addressable runtime storage",
                                              "variable `%s` is not addressable",
                                              astUserFacingIdentifier(node->identifier));
            return mirBindingAddress(state, binding, node);
        }
        case AST_EXPR_DEREF:
            return lowerExprAsValue(state, scope, node->lhs, NULL);
        case AST_EXPR_MEMBER: {
            TypeSystemExprType owner_type = inferExprType(node->lhs, &(scope->type_scope));
            ASTDataType *struct_type = owner_type.data_type;
            MirValueId base_address = -1;

            if(owner_type.kind != TYPE_SYSTEM_EXPR_TYPE_VALUE)
                mirLoweringAbortNode("M2011", node,
                                     "member base is not a runtime value",
                                     "only runtime values can be lowered to field addresses");

            if(struct_type->kind == AST_DATA_TYPE_KIND_POINTER || struct_type->kind == AST_DATA_TYPE_KIND_REFERENCE)
            {
                base_address = lowerExprAsValue(state, scope, node->lhs, NULL);
                struct_type = struct_type->child;
            }
            else if(isAddressableExpr(node->lhs))
                base_address = lowerExprAsAddress(state, scope, node->lhs);
            else
                base_address = lowerExprMaterializedAddress(state, scope, node->lhs);

            struct_type = resolveNamedDataType(struct_type, &(scope->type_scope), scope->self_data_type);

            if(isSliceDataType(struct_type) || isStringDataType(struct_type))
            {
                ASTDataType *field_type = NULL;
                if(strcmp(node->identifier, "ptr") == 0)
                    field_type = newWrappedDataType(AST_DATA_TYPE_KIND_POINTER, cloneDataType(struct_type->child));
                else if(strcmp(node->identifier, "len") == 0)
                    field_type = newPrimaryDataType(AST_PRIMARY_DATA_TYPE_I64);
                else
                    mirLoweringAbortNodeFormatted("M2012", node,
                                                  "this member is not a stored field",
                                                  "member `%s` is not an addressable slice field",
                                                  astUserFacingIdentifier(node->identifier));

                return mirEmitFieldPtr(state, base_address, field_type, node->identifier,
                                       findStructDataFieldIndex(struct_type, node->identifier),
                                       node->filename, node->line_number, node->column_number);
            }

            ASTStructMember *member = findStructMember(struct_type, node->identifier);
            if(member == NULL || member->value != NULL)
                mirLoweringAbortNodeFormatted("M2012", node,
                                              "this member is not a stored field",
                                              "member `%s` is not an addressable field",
                                              astUserFacingIdentifier(node->identifier));
            return mirEmitFieldPtr(state, base_address, member->data_type, member->identifier,
                                   findStructDataFieldIndex(struct_type, member->identifier),
                                   node->filename, node->line_number, node->column_number);
        }
        case AST_EXPR_INDEX: {
            TypeSystemExprType owner_type = inferExprType(node->lhs, &(scope->type_scope));
            ASTDataType *array_type = owner_type.data_type;
            MirValueId base_address = -1;
            bool base_is_element_pointer = false;
            if(array_type->kind == AST_DATA_TYPE_KIND_POINTER || array_type->kind == AST_DATA_TYPE_KIND_REFERENCE)
            {
                base_address = lowerExprAsValue(state, scope, node->lhs, NULL);
                array_type = array_type->child;
            }
            else if(isSliceDataType(array_type) || isStringDataType(array_type))
            {
                MirValueId slice_address = isAddressableExpr(node->lhs)
                                           ? lowerExprAsAddress(state, scope, node->lhs)
                                           : lowerExprMaterializedAddress(state, scope, node->lhs);
                ASTDataType *ptr_field_type = newWrappedDataType(AST_DATA_TYPE_KIND_POINTER,
                                                                 cloneDataType(array_type->child));
                MirValueId ptr_field_address = mirEmitFieldPtr(state, slice_address, ptr_field_type, "ptr", 0,
                                                               node->filename, node->line_number, node->column_number);
                base_address = mirEmitLoad(state, ptr_field_address, ptr_field_type,
                                           node->filename, node->line_number, node->column_number);
                base_is_element_pointer = true;
            }
            else if(isAddressableExpr(node->lhs))
                base_address = lowerExprAsAddress(state, scope, node->lhs);
            else
                base_address = lowerExprMaterializedAddress(state, scope, node->lhs);

            ASTDataType *index_type = defaultIntegerDataType();
            TypeSystemExprType index_expr_type = inferExprType(node->rhs, &(scope->type_scope));
            ASTDataType *index_expected_type = index_expr_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE
                                               ? index_expr_type.data_type
                                               : index_type;
            MirValueId index_value = lowerExprAsValue(state, scope, node->rhs, index_expected_type);
            index_value = mirMaybeConvertValue(state, scope, node->rhs, index_value, index_type);
            return mirEmitIndexPtr(state, base_address, index_value, array_type->child, base_is_element_pointer,
                                   node->filename, node->line_number, node->column_number);
        }
        case AST_EXPR_PARENTHESIS:
            return lowerExprAsAddress(state, scope, node->lhs);
        default:
            return lowerExprMaterializedAddress(state, scope, node);
    }
}

#endif /* MIR_LOWERING_EXPR_H */
