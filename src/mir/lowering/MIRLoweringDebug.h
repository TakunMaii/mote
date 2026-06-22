#ifndef MIR_LOWERING_DEBUG_H
#define MIR_LOWERING_DEBUG_H

#include "MIRLoweringCore.h"

static ASTFunctionParameter* mirNewDebugParam(ASTDataType *data_type)
{
    ASTFunctionParameter *parameter = (ASTFunctionParameter*) malloc(sizeof(ASTFunctionParameter));
    memset(parameter, 0, sizeof(ASTFunctionParameter));
    parameter->data_type = data_type;
    return parameter;
}

static ASTDataType* mirDebugExternType0Void(void)
{
    return newFunctionDataType(NULL, false, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_VOID));
}

static ASTDataType* mirDebugExternTypeCharPtrVoid(void)
{
    ASTFunctionParameter *parameter = mirNewDebugParam(
        newWrappedDataType(AST_DATA_TYPE_KIND_POINTER, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_CHAR))
    );
    return newFunctionDataType(parameter, false, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_VOID));
}

static ASTDataType* mirDebugExternTypeCharPtrI64Void(void)
{
    ASTFunctionParameter *file_param = mirNewDebugParam(
        newWrappedDataType(AST_DATA_TYPE_KIND_POINTER, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_CHAR))
    );
    ASTFunctionParameter *line_param = mirNewDebugParam(newPrimaryDataType(AST_PRIMARY_DATA_TYPE_I64));
    file_param->next = line_param;
    return newFunctionDataType(file_param, false, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_VOID));
}

static ASTDataType* mirDebugExternTypeI32Void(void)
{
    ASTFunctionParameter *parameter = mirNewDebugParam(newPrimaryDataType(AST_PRIMARY_DATA_TYPE_I32));
    return newFunctionDataType(parameter, false, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_VOID));
}

static ASTDataType* mirDebugExternTypeI64Void(void)
{
    ASTFunctionParameter *parameter = mirNewDebugParam(newPrimaryDataType(AST_PRIMARY_DATA_TYPE_I64));
    return newFunctionDataType(parameter, false, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_VOID));
}

static ASTDataType* mirDebugExternTypeU64Void(void)
{
    ASTFunctionParameter *parameter = mirNewDebugParam(newPrimaryDataType(AST_PRIMARY_DATA_TYPE_U64));
    return newFunctionDataType(parameter, false, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_VOID));
}

static ASTDataType* mirDebugExternTypeF64Void(void)
{
    ASTFunctionParameter *parameter = mirNewDebugParam(newPrimaryDataType(AST_PRIMARY_DATA_TYPE_F64));
    return newFunctionDataType(parameter, false, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_VOID));
}

static ASTDataType* mirDebugExternTypeVoidPtrVoid(void)
{
    ASTFunctionParameter *parameter = mirNewDebugParam(
        newWrappedDataType(AST_DATA_TYPE_KIND_POINTER, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_VOID))
    );
    return newFunctionDataType(parameter, false, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_VOID));
}

static void mirEmitDebugRuntimeCall(MirFunctionState *state, ASTNode *node,
                                    const char *symbol_name,
                                    ASTDataType *function_type,
                                    MirOperandList arguments)
{
    (void) mirEnsureExternFunction(
        state->lowering,
        symbol_name,
        function_type,
        node->filename,
        node->line_number,
        node->column_number
    );
    (void) mirEmitExternCall(state, symbol_name, function_type, arguments,
                             newPrimaryDataType(AST_PRIMARY_DATA_TYPE_VOID),
                             node->filename, node->line_number, node->column_number);
}

static void mirEmitDebugWriteCStrLiteral(MirFunctionState *state, ASTNode *node, const char *text)
{
    ASTDataType *char_ptr_type = newWrappedDataType(AST_DATA_TYPE_KIND_POINTER,
                                                    newPrimaryDataType(AST_PRIMARY_DATA_TYPE_CHAR));
    ASTDataType *global_type = newArrayDataType(newPrimaryDataType(AST_PRIMARY_DATA_TYPE_CHAR), strlen(text) + 1);
    const char *global_name = mirEnsureStringLiteralGlobal(state->lowering, text);
    MirValueId global_addr = mirEmitGlobalAddr(state, global_name, global_type,
                                               node->filename, node->line_number, node->column_number);
    MirValueId ptr_value = mirEmitConvert(state, global_addr, char_ptr_type,
                                          node->filename, node->line_number, node->column_number);
    MirOperandList arguments = newMirOperandList(1);
    arguments.items[0] = ptr_value;
    mirEmitDebugRuntimeCall(state, node, "mote_debug_write_cstr", mirDebugExternTypeCharPtrVoid(), arguments);
}

static void mirEmitDebugWriteCharLiteral(MirFunctionState *state, ASTNode *node, int ch)
{
    MirOperandList arguments = newMirOperandList(1);
    arguments.items[0] = mirEmitConstInt(state, ch, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_I32),
                                         node->filename, node->line_number, node->column_number);
    mirEmitDebugRuntimeCall(state, node, "mote_debug_write_char", mirDebugExternTypeI32Void(), arguments);
}

static MirValueId mirDebugValueAddress(MirFunctionState *state, MirLowerScope *scope, ASTNode *node)
{
    if(isAddressableExpr(node))
        return lowerExprAsAddress(state, scope, node);
    return lowerExprMaterializedAddress(state, scope, node);
}

static void mirEmitDebugTypeAndOpen(MirFunctionState *state, ASTNode *node, ASTDataType *data_type)
{
    char type_buffer[512] = {0};
    appendASTDataTypeString(data_type, type_buffer, sizeof(type_buffer));
    mirEmitDebugWriteCStrLiteral(state, node, type_buffer);
    mirEmitDebugWriteCharLiteral(state, node, '(');
}

static void mirEmitDebugClose(MirFunctionState *state, ASTNode *node)
{
    mirEmitDebugWriteCharLiteral(state, node, ')');
}

static void mirEmitDebugWriteBool(MirFunctionState *state, ASTNode *node, MirValueId value)
{
    MirFunction *function = mirCurrentFunction(state);
    MirBlockId true_block = mirCreateBlock(state->lowering, function, "debug_bool_true");
    MirBlockId false_block = mirCreateBlock(state->lowering, function, "debug_bool_false");
    MirBlockId end_block = mirCreateBlock(state->lowering, function, "debug_bool_end");
    mirEmitCondBr(state, value, true_block, false_block);

    mirSwitchToBlock(state, true_block);
    mirEmitDebugWriteCStrLiteral(state, node, "true");
    mirEmitBr(state, end_block);

    mirSwitchToBlock(state, false_block);
    mirEmitDebugWriteCStrLiteral(state, node, "false");
    mirEmitBr(state, end_block);

    mirSwitchToBlock(state, end_block);
}

static void mirEmitDebugValue(MirFunctionState *state, MirLowerScope *scope, ASTNode *node,
                              ASTDataType *data_type, MirValueId value, int depth);

static bool mirDebugBodyOwnsTypeEnvelope(ASTDataType *resolved_type)
{
    return resolved_type != NULL &&
           (resolved_type->kind == AST_DATA_TYPE_KIND_ENUM ||
            resolved_type->kind == AST_DATA_TYPE_KIND_FUNCTION);
}

static void mirEmitDebugValueBody(MirFunctionState *state, MirLowerScope *scope, ASTNode *node,
                                  ASTDataType *data_type, MirValueId value, int depth)
{
    ASTDataType *resolved_type = resolveNamedDataType(data_type, &(scope->type_scope), scope->self_data_type);
    if(resolved_type == NULL)
        resolved_type = data_type;

    switch(resolved_type->kind)
    {
        case AST_DATA_TYPE_KIND_PRIMARY:
            switch(resolved_type->primary)
            {
                case AST_PRIMARY_DATA_TYPE_VOID:
                    mirEmitDebugWriteCStrLiteral(state, node, "void");
                    return;
                case AST_PRIMARY_DATA_TYPE_BOOL:
                    mirEmitDebugWriteBool(state, node, value);
                    return;
                case AST_PRIMARY_DATA_TYPE_CHAR: {
                    ASTDataType *i32_type = newPrimaryDataType(AST_PRIMARY_DATA_TYPE_I32);
                    MirValueId ch_i32 = mirEmitConvert(state, value, i32_type,
                                                       node->filename, node->line_number, node->column_number);
                    mirEmitDebugWriteCharLiteral(state, node, '\'');
                    MirOperandList arguments = newMirOperandList(1);
                    arguments.items[0] = ch_i32;
                    mirEmitDebugRuntimeCall(state, node, "mote_debug_write_char", mirDebugExternTypeI32Void(), arguments);
                    mirEmitDebugWriteCharLiteral(state, node, '\'');
                    return;
                }
                case AST_PRIMARY_DATA_TYPE_I8:
                case AST_PRIMARY_DATA_TYPE_I16:
                case AST_PRIMARY_DATA_TYPE_I32:
                case AST_PRIMARY_DATA_TYPE_I64: {
                    MirValueId widened = value;
                    if(resolved_type->primary != AST_PRIMARY_DATA_TYPE_I64)
                        widened = mirEmitConvert(state, value, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_I64),
                                                 node->filename, node->line_number, node->column_number);
                    MirOperandList arguments = newMirOperandList(1);
                    arguments.items[0] = widened;
                    mirEmitDebugRuntimeCall(state, node, "mote_debug_write_i64", mirDebugExternTypeI64Void(), arguments);
                    return;
                }
                case AST_PRIMARY_DATA_TYPE_U8:
                case AST_PRIMARY_DATA_TYPE_U16:
                case AST_PRIMARY_DATA_TYPE_U32:
                case AST_PRIMARY_DATA_TYPE_U64: {
                    MirValueId widened = value;
                    if(resolved_type->primary != AST_PRIMARY_DATA_TYPE_U64)
                        widened = mirEmitConvert(state, value, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_U64),
                                                 node->filename, node->line_number, node->column_number);
                    MirOperandList arguments = newMirOperandList(1);
                    arguments.items[0] = widened;
                    mirEmitDebugRuntimeCall(state, node, "mote_debug_write_u64", mirDebugExternTypeU64Void(), arguments);
                    return;
                }
                case AST_PRIMARY_DATA_TYPE_F32:
                case AST_PRIMARY_DATA_TYPE_F64: {
                    MirValueId widened = value;
                    if(resolved_type->primary != AST_PRIMARY_DATA_TYPE_F64)
                        widened = mirEmitConvert(state, value, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_F64),
                                                 node->filename, node->line_number, node->column_number);
                    MirOperandList arguments = newMirOperandList(1);
                    arguments.items[0] = widened;
                    mirEmitDebugRuntimeCall(state, node, "mote_debug_write_f64", mirDebugExternTypeF64Void(), arguments);
                    return;
                }
                case AST_PRIMARY_DATA_TYPE_TYPE:
                    mirEmitDebugWriteCStrLiteral(state, node, "<type>");
                    return;
                case AST_PRIMARY_DATA_TYPE_F8:
                case AST_PRIMARY_DATA_TYPE_F16:
                    mirLoweringAbortNode("M2017", node,
                                         "@debug does not support this floating-point width in LLVM lowering yet",
                                         "use f32 or f64 for now");
                    return;
            }
            return;
        case AST_DATA_TYPE_KIND_POINTER:
        case AST_DATA_TYPE_KIND_REFERENCE: {
            MirValueId ptr_value = value;
            if(resolved_type->kind == AST_DATA_TYPE_KIND_REFERENCE)
                ptr_value = mirEmitConvert(state, value,
                                           newWrappedDataType(AST_DATA_TYPE_KIND_POINTER,
                                                              cloneDataType(resolved_type->child)),
                                           node->filename, node->line_number, node->column_number);
            MirValueId void_ptr = mirEmitConvert(state, ptr_value,
                                                 newWrappedDataType(AST_DATA_TYPE_KIND_POINTER,
                                                                    newPrimaryDataType(AST_PRIMARY_DATA_TYPE_VOID)),
                                                 node->filename, node->line_number, node->column_number);
            MirOperandList arguments = newMirOperandList(1);
            arguments.items[0] = void_ptr;
            mirEmitDebugRuntimeCall(state, node, "mote_debug_write_ptr", mirDebugExternTypeVoidPtrVoid(), arguments);
            return;
        }
        case AST_DATA_TYPE_KIND_FUNCTION: {
            MirValueId closure_address = mirDebugValueAddress(state, scope, node);
            ASTDataType *void_ptr_type = newWrappedDataType(AST_DATA_TYPE_KIND_POINTER,
                                                            newPrimaryDataType(AST_PRIMARY_DATA_TYPE_VOID));
            mirEmitDebugTypeAndOpen(state, node, data_type);
            mirEmitDebugWriteCStrLiteral(state, node, "code=");
            MirValueId code_ptr = mirEmitFieldPtr(state, closure_address, void_ptr_type, "", 0,
                                                  node->filename, node->line_number, node->column_number);
            MirValueId code_value = mirEmitLoad(state, code_ptr, void_ptr_type,
                                                node->filename, node->line_number, node->column_number);
            MirOperandList code_args = newMirOperandList(1);
            code_args.items[0] = code_value;
            mirEmitDebugRuntimeCall(state, node, "mote_debug_write_ptr", mirDebugExternTypeVoidPtrVoid(), code_args);
            mirEmitDebugWriteCStrLiteral(state, node, ", env=");
            MirValueId env_ptr = mirEmitFieldPtr(state, closure_address, void_ptr_type, "", 1,
                                                 node->filename, node->line_number, node->column_number);
            MirValueId env_value = mirEmitLoad(state, env_ptr, void_ptr_type,
                                               node->filename, node->line_number, node->column_number);
            MirOperandList env_args = newMirOperandList(1);
            env_args.items[0] = env_value;
            mirEmitDebugRuntimeCall(state, node, "mote_debug_write_ptr", mirDebugExternTypeVoidPtrVoid(), env_args);
            mirEmitDebugClose(state, node);
            return;
        }
        case AST_DATA_TYPE_KIND_ENUM: {
            mirEmitDebugTypeAndOpen(state, node, data_type);
            MirFunction *function = mirCurrentFunction(state);
            MirBlockId end_block = mirCreateBlock(state->lowering, function, "debug_enum_end");
            MirBlockId next_block = -1;
            ASTEnumVariant *variant = resolved_type->variants;
            int ordinal = 0;
            while(variant != NULL)
            {
                MirBlockId match_block = mirCreateBlock(state->lowering, function, "debug_enum_match");
                next_block = mirCreateBlock(state->lowering, function, "debug_enum_next");
                MirValueId ordinal_value = mirEmitConstInt(state, ordinal, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_I32),
                                                           node->filename, node->line_number, node->column_number);
                MirValueId is_match = mirEmitBinary(state, MIR_INST_EQ, value, ordinal_value,
                                                    newPrimaryDataType(AST_PRIMARY_DATA_TYPE_BOOL),
                                                    node->filename, node->line_number, node->column_number);
                mirEmitCondBr(state, is_match, match_block, next_block);

                mirSwitchToBlock(state, match_block);
                mirEmitDebugWriteCStrLiteral(state, node, astUserFacingIdentifier(variant->identifier));
                mirEmitBr(state, end_block);

                mirSwitchToBlock(state, next_block);
                variant = variant->next;
                ordinal++;
            }
            mirEmitDebugWriteCStrLiteral(state, node, "<invalid>");
            mirEmitBr(state, end_block);
            mirSwitchToBlock(state, end_block);
            mirEmitDebugClose(state, node);
            return;
        }
        case AST_DATA_TYPE_KIND_ARRAY: {
            MirValueId base_address = mirDebugValueAddress(state, scope, node);
            mirEmitDebugWriteCharLiteral(state, node, '[');
            for(long long int i = 0; i < resolved_type->array_length; i++)
            {
                if(i > 0)
                    mirEmitDebugWriteCStrLiteral(state, node, ", ");
                MirValueId index_value = mirEmitConstInt(state, i, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_I64),
                                                         node->filename, node->line_number, node->column_number);
                MirValueId element_ptr = mirEmitIndexPtr(state, base_address, index_value, resolved_type->child, false,
                                                         node->filename, node->line_number, node->column_number);
                MirValueId element_value = mirEmitLoad(state, element_ptr, resolved_type->child,
                                                       node->filename, node->line_number, node->column_number);
                mirEmitDebugValue(state, scope, node, resolved_type->child, element_value, depth + 1);
            }
            mirEmitDebugWriteCharLiteral(state, node, ']');
            return;
        }
        case AST_DATA_TYPE_KIND_SLICE:
        case AST_DATA_TYPE_KIND_STRING: {
            MirValueId slice_address = mirDebugValueAddress(state, scope, node);
            ASTDataType *ptr_field_type = newWrappedDataType(AST_DATA_TYPE_KIND_POINTER,
                                                             cloneDataType(resolved_type->child));
            MirValueId ptr_field_ptr = mirEmitFieldPtr(state, slice_address, ptr_field_type, "ptr", 0,
                                                       node->filename, node->line_number, node->column_number);
            MirValueId len_field_ptr = mirEmitFieldPtr(state, slice_address, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_I64),
                                                       "len", 1,
                                                       node->filename, node->line_number, node->column_number);
            MirValueId ptr_value = mirEmitLoad(state, ptr_field_ptr, ptr_field_type,
                                               node->filename, node->line_number, node->column_number);
            MirValueId len_value = mirEmitLoad(state, len_field_ptr, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_I64),
                                               node->filename, node->line_number, node->column_number);
            mirEmitDebugWriteCStrLiteral(state, node, "len=");
            MirOperandList len_args = newMirOperandList(1);
            len_args.items[0] = len_value;
            mirEmitDebugRuntimeCall(state, node, "mote_debug_write_i64", mirDebugExternTypeI64Void(), len_args);
            mirEmitDebugWriteCStrLiteral(state, node, ", ptr=");
            MirValueId void_ptr = mirEmitConvert(state, ptr_value,
                                                 newWrappedDataType(AST_DATA_TYPE_KIND_POINTER,
                                                                    newPrimaryDataType(AST_PRIMARY_DATA_TYPE_VOID)),
                                                 node->filename, node->line_number, node->column_number);
            MirOperandList ptr_args = newMirOperandList(1);
            ptr_args.items[0] = void_ptr;
            mirEmitDebugRuntimeCall(state, node, "mote_debug_write_ptr", mirDebugExternTypeVoidPtrVoid(), ptr_args);
            mirEmitDebugWriteCStrLiteral(state, node, ", [");

            MirValueId index_slot = mirEmitAlloca(state, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_I64),
                                                  node->filename, node->line_number, node->column_number);
            mirEmitStore(state, index_slot,
                         mirEmitConstInt(state, 0, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_I64),
                                         node->filename, node->line_number, node->column_number),
                         node->filename, node->line_number, node->column_number);

            MirFunction *function = mirCurrentFunction(state);
            MirBlockId cond_block = mirCreateBlock(state->lowering, function, "debug_slice_cond");
            MirBlockId body_block = mirCreateBlock(state->lowering, function, "debug_slice_body");
            MirBlockId end_block = mirCreateBlock(state->lowering, function, "debug_slice_end");
            mirEmitBr(state, cond_block);

            mirSwitchToBlock(state, cond_block);
            MirValueId index_value = mirEmitLoad(state, index_slot, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_I64),
                                                 node->filename, node->line_number, node->column_number);
            MirValueId cond = mirEmitBinary(state, MIR_INST_LT, index_value, len_value,
                                            newPrimaryDataType(AST_PRIMARY_DATA_TYPE_BOOL),
                                            node->filename, node->line_number, node->column_number);
            mirEmitCondBr(state, cond, body_block, end_block);

            mirSwitchToBlock(state, body_block);
            MirValueId is_first = mirEmitBinary(state, MIR_INST_EQ, index_value,
                                                mirEmitConstInt(state, 0, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_I64),
                                                                node->filename, node->line_number, node->column_number),
                                                newPrimaryDataType(AST_PRIMARY_DATA_TYPE_BOOL),
                                                node->filename, node->line_number, node->column_number);
            MirBlockId first_block = mirCreateBlock(state->lowering, function, "debug_slice_first");
            MirBlockId later_block = mirCreateBlock(state->lowering, function, "debug_slice_later");
            MirBlockId after_sep_block = mirCreateBlock(state->lowering, function, "debug_slice_after_sep");
            mirEmitCondBr(state, is_first, first_block, later_block);

            mirSwitchToBlock(state, first_block);
            mirEmitBr(state, after_sep_block);

            mirSwitchToBlock(state, later_block);
            mirEmitDebugWriteCStrLiteral(state, node, ", ");
            mirEmitBr(state, after_sep_block);

            mirSwitchToBlock(state, after_sep_block);
            MirValueId element_ptr = mirEmitIndexPtr(state, ptr_value, index_value, resolved_type->child, true,
                                                     node->filename, node->line_number, node->column_number);
            MirValueId element_value = mirEmitLoad(state, element_ptr, resolved_type->child,
                                                   node->filename, node->line_number, node->column_number);
            mirEmitDebugValue(state, scope, node, resolved_type->child, element_value, depth + 1);
            MirValueId next_index = mirEmitBinary(state, MIR_INST_ADD, index_value,
                                                  mirEmitConstInt(state, 1, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_I64),
                                                                  node->filename, node->line_number, node->column_number),
                                                  newPrimaryDataType(AST_PRIMARY_DATA_TYPE_I64),
                                                  node->filename, node->line_number, node->column_number);
            mirEmitStore(state, index_slot, next_index, node->filename, node->line_number, node->column_number);
            mirEmitBr(state, cond_block);

            mirSwitchToBlock(state, end_block);
            mirEmitDebugWriteCharLiteral(state, node, ']');
            return;
        }
        case AST_DATA_TYPE_KIND_OPTIONAL: {
            MirValueId optional_address = mirDebugValueAddress(state, scope, node);
            MirValueId has_value_ptr = mirEmitFieldPtr(state, optional_address, mirOptionalBoolType(),
                                                       "has_value", 0,
                                                       node->filename, node->line_number, node->column_number);
            MirValueId has_value = mirEmitLoad(state, has_value_ptr, mirOptionalBoolType(),
                                               node->filename, node->line_number, node->column_number);
            MirFunction *function = mirCurrentFunction(state);
            MirBlockId some_block = mirCreateBlock(state->lowering, function, "debug_optional_some");
            MirBlockId none_block = mirCreateBlock(state->lowering, function, "debug_optional_none");
            MirBlockId end_block = mirCreateBlock(state->lowering, function, "debug_optional_end");
            mirEmitCondBr(state, has_value, some_block, none_block);

            mirSwitchToBlock(state, none_block);
            mirEmitDebugWriteCStrLiteral(state, node, "null");
            mirEmitBr(state, end_block);

            mirSwitchToBlock(state, some_block);
            MirValueId inner_ptr = mirEmitFieldPtr(state, optional_address, resolved_type->child,
                                                   "value", 1,
                                                   node->filename, node->line_number, node->column_number);
            MirValueId inner_value = mirEmitLoad(state, inner_ptr, resolved_type->child,
                                                 node->filename, node->line_number, node->column_number);
            mirEmitDebugValue(state, scope, node, resolved_type->child, inner_value, depth + 1);
            mirEmitBr(state, end_block);

            mirSwitchToBlock(state, end_block);
            return;
        }
        case AST_DATA_TYPE_KIND_STRUCT: {
            mirEmitDebugWriteCharLiteral(state, node, '{');
            ASTStructMember *member = resolved_type->members;
            int field_index = 0;
            bool first = true;
            MirValueId base_address = mirDebugValueAddress(state, scope, node);
            while(member != NULL)
            {
                if(member->value == NULL)
                {
                    if(!first)
                        mirEmitDebugWriteCStrLiteral(state, node, ", ");
                    mirEmitDebugWriteCStrLiteral(state, node, astUserFacingIdentifier(member->identifier));
                    mirEmitDebugWriteCStrLiteral(state, node, ": ");
                    MirValueId field_ptr = mirEmitFieldPtr(state, base_address, member->data_type,
                                                           member->identifier, field_index,
                                                           node->filename, node->line_number, node->column_number);
                    MirValueId field_value = mirEmitLoad(state, field_ptr, member->data_type,
                                                         node->filename, node->line_number, node->column_number);
                    mirEmitDebugValue(state, scope, node, member->data_type, field_value, depth + 1);
                    first = false;
                    field_index++;
                }
                member = member->next;
            }
            mirEmitDebugWriteCharLiteral(state, node, '}');
            return;
        }
        default:
            mirLoweringAbortNodeFormatted("M2018", node,
                                          "@debug lowering is missing this type shape",
                                          "unsupported @debug type kind %d",
                                          resolved_type->kind);
    }
}

static void mirEmitDebugValue(MirFunctionState *state, MirLowerScope *scope, ASTNode *node,
                              ASTDataType *data_type, MirValueId value, int depth)
{
    (void) depth;
    ASTDataType *resolved_type = resolveNamedDataType(data_type, &(scope->type_scope), scope->self_data_type);
    if(resolved_type == NULL)
        resolved_type = data_type;
    if(!mirDebugBodyOwnsTypeEnvelope(resolved_type))
        mirEmitDebugTypeAndOpen(state, node, data_type);
    mirEmitDebugValueBody(state, scope, node, data_type, value, depth);
    if(!mirDebugBodyOwnsTypeEnvelope(resolved_type))
        mirEmitDebugClose(state, node);
}

#endif /* MIR_LOWERING_DEBUG_H */
