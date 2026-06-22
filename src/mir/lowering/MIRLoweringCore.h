#ifndef MIR_LOWERING_CORE_H
#define MIR_LOWERING_CORE_H

#include "MIRLoweringShared.h"

static bool functionHasTypeParameters(ASTFunctionParameter *parameter)
{
    while(parameter)
    {
        if(parameter->data_type != NULL &&
           parameter->data_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
           parameter->data_type->primary == AST_PRIMARY_DATA_TYPE_TYPE)
            return true;
        parameter = parameter->next;
    }
    return false;
}

static ASTOperatorKind mirBinaryExprOperatorKind(ASTNodeKind kind)
{
    switch(kind)
    {
        case AST_EXPR_ADD: return AST_OPERATOR_ADD;
        case AST_EXPR_SUB: return AST_OPERATOR_SUB;
        case AST_EXPR_MUL: return AST_OPERATOR_MUL;
        case AST_EXPR_DIV: return AST_OPERATOR_DIV;
        case AST_EXPR_EQUAL:
        case AST_EXPR_NOT_EQUAL: return AST_OPERATOR_EQ;
        default: return AST_OPERATOR_NONE;
    }
}

static bool functionParameterIsComptimeType(ASTFunctionParameter *parameter)
{
    return parameter != NULL &&
           parameter->data_type != NULL &&
           parameter->data_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
           parameter->data_type->primary == AST_PRIMARY_DATA_TYPE_TYPE;
}

static int countRuntimeCallArguments(ASTFunctionParameter *parameter, ASTNode *argument)
{
    int count = 0;
    while(parameter != NULL && argument != NULL)
    {
        if(!functionParameterIsComptimeType(parameter))
            count++;
        parameter = parameter->next;
        argument = argument->next;
    }
    return count;
}

static MirValueId lowerReferenceArgument(MirFunctionState *state, MirLowerScope *scope, ASTNode *argument_node,
                                         ASTDataType *parameter_type)
{
    if(argument_node->kind == AST_EXPR_ADDRESS_OF ||
       argument_node->kind == AST_EXPR_ADDRESS_OF_MUT)
        return lowerExprAsValue(state, scope, argument_node, parameter_type);
    return lowerExprAsAddress(state, scope, argument_node);
}

static MirOperandList lowerSpecializedCallArguments(MirFunctionState *state, MirLowerScope *scope,
                                                    ASTFunctionParameter *source_parameters,
                                                    ASTFunctionParameter *specialized_parameters,
                                                    ASTNode *call_arguments)
{
    MirOperandList arguments = newMirOperandList(countRuntimeCallArguments(source_parameters, call_arguments));
    int index = 0;

    ASTFunctionParameter *source_parameter = source_parameters;
    ASTFunctionParameter *specialized_parameter = specialized_parameters;
    ASTNode *argument = call_arguments;
    while(source_parameter != NULL && argument != NULL)
    {
        if(!functionParameterIsComptimeType(source_parameter))
        {
            ASTDataType *parameter_type = specialized_parameter == NULL ? NULL : specialized_parameter->data_type;
            if(parameter_type != NULL && parameter_type->kind == AST_DATA_TYPE_KIND_REFERENCE)
                arguments.items[index++] = lowerReferenceArgument(state, scope, argument, parameter_type);
            else
                arguments.items[index++] = lowerExprAsValue(state, scope, argument, parameter_type);
            if(specialized_parameter != NULL)
                specialized_parameter = specialized_parameter->next;
        }

        source_parameter = source_parameter->next;
        argument = argument->next;
    }

    return arguments;
}

static bool mirCurrentBlockTerminated(MirFunctionState *state)
{
    return mirCurrentFunction(state)->blocks[state->current_block].terminator.kind != MIR_TERM_NONE;
}

static MirBlockId mirCreateBlock(MirLowering *lowering, MirFunction *function, const char *hint)
{
    MirBlock *block = mirAppendBlock(function);
    if(hint != NULL)
        snprintf(block->name, sizeof(block->name), "%s_%d", hint, lowering->unique_block_counter++);
    else
        snprintf(block->name, sizeof(block->name), "block_%d", lowering->unique_block_counter++);
    return function->block_count - 1;
}

static void mirSwitchToBlock(MirFunctionState *state, MirBlockId block_id)
{
    state->current_block = block_id;
}

static MirValueId mirCreateInput(MirFunction *function, ASTDataType *data_type, const char *name)
{
    return mirAppendValue(function, data_type, name, true);
}

static int mirAppendDebugScope(MirFunction *function, int parent_scope_id,
                               const char *filename, int line_number, int column_number)
{
    if(function == NULL)
        return -1;
    function->debug_scopes = (MirDebugScope*) realloc(function->debug_scopes,
                                                      sizeof(MirDebugScope) * (size_t) (function->debug_scope_count + 1));
    if(function->debug_scopes == NULL)
        diagnosticAbortInternal("mirAppendDebugScope", "allocation failed");
    MirDebugScope *scope = &(function->debug_scopes[function->debug_scope_count]);
    memset(scope, 0, sizeof(*scope));
    scope->parent_scope_id = parent_scope_id;
    scope->filename = filename;
    scope->line_number = line_number;
    scope->column_number = column_number;
    return function->debug_scope_count++;
}

static void mirRecordDebugLocalEx(MirFunction *function, const char *identifier, const char *filename,
                                  int line_number, int column_number, ASTDataType *data_type,
                                  MirValueId slot_value, bool is_parameter, int argument_index, int debug_scope_id)
{
    if(function == NULL || identifier == NULL || filename == NULL || slot_value < 0)
        return;
    function->debug_locals = (MirDebugLocal*) realloc(function->debug_locals,
                                                      sizeof(MirDebugLocal) * (size_t) (function->debug_local_count + 1));
    if(function->debug_locals == NULL)
        diagnosticAbortInternal("mirRecordDebugLocal", "allocation failed");
    MirDebugLocal *local = &(function->debug_locals[function->debug_local_count++]);
    memset(local, 0, sizeof(*local));
    strcpy(local->identifier, identifier);
    local->filename = filename;
    local->line_number = line_number;
    local->column_number = column_number;
    local->data_type = data_type != NULL ? cloneDataType(data_type) : NULL;
    local->slot_value = slot_value;
    local->is_parameter = is_parameter;
    local->argument_index = argument_index;
    local->debug_scope_id = debug_scope_id;
}

static void mirRecordDebugLocal(MirFunction *function, const char *identifier, const char *filename,
                                int line_number, int column_number, ASTDataType *data_type,
                                MirValueId slot_value, int debug_scope_id)
{
    mirRecordDebugLocalEx(function, identifier, filename, line_number, column_number,
                          data_type, slot_value, false, 0, debug_scope_id);
}

static void mirRecordDebugParameter(MirFunction *function, const char *identifier, const char *filename,
                                    int line_number, int column_number, ASTDataType *data_type,
                                    MirValueId slot_value, int argument_index, int debug_scope_id)
{
    mirRecordDebugLocalEx(function, identifier, filename, line_number, column_number, data_type,
                          slot_value, true, argument_index, debug_scope_id);
}

static MirValueId mirEmitResultInst(MirFunctionState *state, MirInstKind kind, ASTDataType *result_type,
                                    const char *filename, int line_number, int column_number)
{
    MirFunction *function = mirCurrentFunction(state);
    MirBlock *block = &(function->blocks[state->current_block]);
    MirInst *inst = mirAppendInst(block);
    inst->kind = kind;
    inst->result_type = cloneDataType(result_type);
    inst->filename = filename;
    inst->line_number = line_number;
    inst->column_number = column_number;
    inst->debug_scope_id = state->current_debug_scope_id;
    inst->result = mirAppendValue(function, result_type, NULL, false);
    return inst->result;
}

static MirInst* mirGetLastInst(MirFunctionState *state)
{
    MirBlock *block = &(mirCurrentFunction(state)->blocks[state->current_block]);
    return &(block->insts[block->inst_count - 1]);
}

static MirValueId mirEmitConstBool(MirFunctionState *state, bool value, const char *filename, int line, int column)
{
    MirValueId result = mirEmitResultInst(state, MIR_INST_CONST_BOOL,
                                          newPrimaryDataType(AST_PRIMARY_DATA_TYPE_BOOL),
                                          filename, line, column);
    mirGetLastInst(state)->data.const_bool.value = value;
    return result;
}

static MirValueId mirEmitZero(MirFunctionState *state, ASTDataType *data_type,
                              const char *filename, int line, int column)
{
    return mirEmitResultInst(state, MIR_INST_ZERO, data_type, filename, line, column);
}

static MirValueId mirEmitConstChar(MirFunctionState *state, char value, const char *filename, int line, int column)
{
    MirValueId result = mirEmitResultInst(state, MIR_INST_CONST_CHAR,
                                          newPrimaryDataType(AST_PRIMARY_DATA_TYPE_CHAR),
                                          filename, line, column);
    mirGetLastInst(state)->data.const_char.value = value;
    return result;
}

static MirValueId mirEmitConstInt(MirFunctionState *state, unsigned long long value, ASTDataType *data_type,
                                  const char *filename, int line, int column)
{
    MirValueId result = mirEmitResultInst(state, MIR_INST_CONST_INT, data_type, filename, line, column);
    mirGetLastInst(state)->data.const_int.value = value;
    return result;
}

static unsigned long long mirNegateLiteralIntegerOrAbort(ASTNode *node)
{
    if(node == NULL || node->kind != AST_EXPR_LITERAL_INTEGER)
        mirLoweringAbortInternal("M2016", "mirNegateLiteralIntegerOrAbort", "expected literal integer node");

    if(node->literal_integer.magnitude > 9223372036854775808ull)
        mirLoweringAbortNode("M2017", node,
                             "negative integer literal is out of range",
                             "literal magnitude exceeds what `i64` can represent when negated");
    return 0ull - node->literal_integer.magnitude;
}

static MirValueId mirEmitConstFloat(MirFunctionState *state, long double value, ASTDataType *data_type,
                                    const char *filename, int line, int column)
{
    MirValueId result = mirEmitResultInst(state, MIR_INST_CONST_FLOAT, data_type, filename, line, column);
    mirGetLastInst(state)->data.const_float.value = value;
    return result;
}

static MirValueId mirEmitConstString(MirFunctionState *state, const char *value, ASTDataType *data_type,
                                     const char *filename, int line, int column)
{
    MirValueId result = mirEmitResultInst(state, MIR_INST_CONST_STRING, data_type, filename, line, column);
    strcpy(mirGetLastInst(state)->data.const_string.value, value);
    return result;
}

static MirValueId mirEmitConvert(MirFunctionState *state, MirValueId operand, ASTDataType *target_type,
                                 const char *filename, int line, int column)
{
    if(isSameDataType(mirGetValueType(state, operand), target_type))
        return operand;

    MirValueId result = mirEmitResultInst(state, MIR_INST_CONVERT, target_type, filename, line, column);
    MirInst *inst = mirGetLastInst(state);
    inst->data.convert.operand = operand;
    inst->data.convert.target_type = cloneDataType(target_type);
    return result;
}

static MirValueId mirEmitUnary(MirFunctionState *state, MirInstKind kind, MirValueId operand, ASTDataType *result_type,
                               const char *filename, int line, int column)
{
    MirValueId result = mirEmitResultInst(state, kind, result_type, filename, line, column);
    mirGetLastInst(state)->data.unary.operand = operand;
    return result;
}

static MirValueId mirEmitBinary(MirFunctionState *state, MirInstKind kind, MirValueId lhs, MirValueId rhs,
                                ASTDataType *result_type, const char *filename, int line, int column)
{
    MirValueId result = mirEmitResultInst(state, kind, result_type, filename, line, column);
    MirInst *inst = mirGetLastInst(state);
    inst->data.binary.lhs = lhs;
    inst->data.binary.rhs = rhs;
    return result;
}

static MirValueId mirEmitAlloca(MirFunctionState *state, ASTDataType *alloca_type,
                                const char *filename, int line, int column)
{
    ASTDataType *pointer_type = newWrappedDataType(AST_DATA_TYPE_KIND_POINTER, cloneDataType(alloca_type));
    MirValueId result = mirEmitResultInst(state, MIR_INST_ALLOCA, pointer_type, filename, line, column);
    mirGetLastInst(state)->data.alloca_inst.alloca_type = cloneDataType(alloca_type);
    return result;
}

static MirValueId mirEmitLoad(MirFunctionState *state, MirValueId address, ASTDataType *loaded_type,
                              const char *filename, int line, int column)
{
    MirValueId result = mirEmitResultInst(state, MIR_INST_LOAD, loaded_type, filename, line, column);
    mirGetLastInst(state)->data.load.address = address;
    return result;
}

static void mirEmitStore(MirFunctionState *state, MirValueId address, MirValueId value,
                         const char *filename, int line, int column)
{
    MirBlock *block = &(mirCurrentFunction(state)->blocks[state->current_block]);
    MirInst *inst = mirAppendInst(block);
    inst->kind = MIR_INST_STORE;
    inst->filename = filename;
    inst->line_number = line;
    inst->column_number = column;
    inst->debug_scope_id = state->current_debug_scope_id;
    inst->data.store.address = address;
    inst->data.store.value = value;
}

static MirValueId mirEmitGlobalAddr(MirFunctionState *state, const char *global_name, ASTDataType *global_type,
                                    const char *filename, int line, int column)
{
    MirValueId result = mirEmitResultInst(
        state,
        MIR_INST_GLOBAL_ADDR,
        newWrappedDataType(AST_DATA_TYPE_KIND_POINTER, cloneDataType(global_type)),
        filename, line, column
    );
    strcpy(mirGetLastInst(state)->data.global_addr.global_name, global_name);
    return result;
}

static MirValueId mirEmitFunctionRef(MirFunctionState *state, const char *function_name, ASTDataType *function_type,
                                     const char *filename, int line, int column)
{
    MirValueId result = mirEmitResultInst(state, MIR_INST_FUNCTION_REF, function_type, filename, line, column);
    strcpy(mirGetLastInst(state)->data.function_ref.function_name, function_name);
    return result;
}

static MirValueId mirEmitMakeClosure(MirFunctionState *state, const char *function_name, ASTDataType *function_type,
                                     ASTDataType *environment_type, MirOperandList captures,
                                     const char *filename, int line, int column)
{
    MirValueId result = mirEmitResultInst(state, MIR_INST_MAKE_CLOSURE, function_type, filename, line, column);
    MirInst *inst = mirGetLastInst(state);
    strcpy(inst->data.make_closure.function_name, function_name);
    inst->data.make_closure.environment_type = cloneDataType(environment_type);
    inst->data.make_closure.captures = captures;
    return result;
}

static MirValueId mirEmitFieldPtr(MirFunctionState *state, MirValueId base_address, ASTDataType *field_type,
                                  const char *identifier, int field_index,
                                  const char *filename, int line, int column)
{
    MirValueId result = mirEmitResultInst(
        state,
        MIR_INST_FIELD_PTR,
        newWrappedDataType(AST_DATA_TYPE_KIND_POINTER, cloneDataType(field_type)),
        filename, line, column
    );
    MirInst *inst = mirGetLastInst(state);
    inst->data.field_ptr.base_address = base_address;
    strcpy(inst->data.field_ptr.identifier, identifier);
    inst->data.field_ptr.field_index = field_index;
    return result;
}

static MirValueId mirEmitIndexPtr(MirFunctionState *state, MirValueId base_address, MirValueId index_value,
                                  ASTDataType *element_type, bool base_is_element_pointer,
                                  const char *filename, int line, int column)
{
    MirValueId result = mirEmitResultInst(
        state,
        MIR_INST_INDEX_PTR,
        newWrappedDataType(AST_DATA_TYPE_KIND_POINTER, cloneDataType(element_type)),
        filename, line, column
    );
    MirInst *inst = mirGetLastInst(state);
    inst->data.index_ptr.base_address = base_address;
    inst->data.index_ptr.index_value = index_value;
    inst->data.index_ptr.base_is_element_pointer = base_is_element_pointer;
    return result;
}

static MirValueId mirEmitPtrDiff(MirFunctionState *state, MirValueId lhs, MirValueId rhs,
                                 const char *filename, int line, int column)
{
    MirValueId result = mirEmitResultInst(
        state,
        MIR_INST_PTR_DIFF,
        newPrimaryDataType(AST_PRIMARY_DATA_TYPE_I64),
        filename, line, column
    );
    MirInst *inst = mirGetLastInst(state);
    inst->data.ptr_diff.lhs = lhs;
    inst->data.ptr_diff.rhs = rhs;
    return result;
}

static MirValueId mirEmitArrayLiteral(MirFunctionState *state, MirOperandList elements, ASTDataType *array_type,
                                      const char *filename, int line, int column)
{
    MirValueId result = mirEmitResultInst(state, MIR_INST_ARRAY_LITERAL, array_type, filename, line, column);
    mirGetLastInst(state)->data.array_literal.elements = elements;
    return result;
}

static MirValueId mirEmitStructLiteral(MirFunctionState *state, MirFieldValueList fields, ASTDataType *struct_type,
                                       const char *filename, int line, int column)
{
    MirValueId result = mirEmitResultInst(state, MIR_INST_STRUCT_LITERAL, struct_type, filename, line, column);
    mirGetLastInst(state)->data.struct_literal.fields = fields;
    return result;
}

static MirValueId mirEmitOptionalSome(MirFunctionState *state, MirValueId value, ASTDataType *optional_type,
                                      const char *filename, int line, int column)
{
    MirFieldValueList fields = newMirFieldValueList(2);
    strcpy(fields.items[0].identifier, "has_value");
    fields.items[0].value = mirEmitConstBool(state, true, filename, line, column);
    strcpy(fields.items[1].identifier, "value");
    fields.items[1].value = value;
    return mirEmitStructLiteral(state, fields, optional_type, filename, line, column);
}

static MirValueId mirLowerOptionalNull(MirFunctionState *state, ASTDataType *optional_type,
                                       const char *filename, int line, int column)
{
    return mirEmitZero(state, optional_type, filename, line, column);
}

static MirValueId mirEmitEnumLiteral(MirFunctionState *state, ASTDataType *enum_type, const char *enum_name,
                                     const char *variant_name, int ordinal,
                                     const char *filename, int line, int column)
{
    MirValueId result = mirEmitResultInst(state, MIR_INST_ENUM_LITERAL, enum_type, filename, line, column);
    MirInst *inst = mirGetLastInst(state);
    if(enum_name != NULL)
        strcpy(inst->data.enum_literal.enum_name, enum_name);
    strcpy(inst->data.enum_literal.variant_name, variant_name);
    inst->data.enum_literal.ordinal = ordinal;
    return result;
}

static MirValueId mirEmitCall(MirFunctionState *state, MirValueId callee, MirOperandList arguments, ASTDataType *return_type,
                              const char *filename, int line, int column)
{
    MirValueId result = -1;
    if(!mirIsValueTypeVoid(return_type))
        result = mirEmitResultInst(state, MIR_INST_CALL, return_type, filename, line, column);
    else
    {
        MirBlock *block = &(mirCurrentFunction(state)->blocks[state->current_block]);
        MirInst *inst = mirAppendInst(block);
        inst->kind = MIR_INST_CALL;
        inst->result = -1;
        inst->result_type = cloneDataType(return_type);
        inst->filename = filename;
        inst->line_number = line;
        inst->column_number = column;
        inst->debug_scope_id = state->current_debug_scope_id;
    }

    MirInst *inst = mirGetLastInst(state);
    inst->data.call.callee = callee;
    inst->data.call.arguments = arguments;
    return result;
}

static MirValueId mirEmitExternCall(MirFunctionState *state, const char *symbol_name, ASTDataType *function_type,
                                    MirOperandList arguments, ASTDataType *return_type,
                                    const char *filename, int line, int column)
{
    MirValueId result = -1;
    if(!mirIsValueTypeVoid(return_type))
        result = mirEmitResultInst(state, MIR_INST_EXTERN_CALL, return_type, filename, line, column);
    else
    {
        MirBlock *block = &(mirCurrentFunction(state)->blocks[state->current_block]);
        MirInst *inst = mirAppendInst(block);
        inst->kind = MIR_INST_EXTERN_CALL;
        inst->result = -1;
        inst->result_type = cloneDataType(return_type);
        inst->filename = filename;
        inst->line_number = line;
        inst->column_number = column;
        inst->debug_scope_id = state->current_debug_scope_id;
    }

    MirInst *inst = mirGetLastInst(state);
    strcpy(inst->data.extern_call.symbol_name, symbol_name);
    inst->data.extern_call.function_type = cloneDataType(function_type);
    inst->data.extern_call.arguments = arguments;
    return result;
}

static void mirEmitBr(MirFunctionState *state, MirBlockId target)
{
    MirTerminator *term = &(mirCurrentFunction(state)->blocks[state->current_block].terminator);
    term->kind = MIR_TERM_BR;
    term->filename = NULL;
    term->line_number = -1;
    term->column_number = -1;
    term->debug_scope_id = state->current_debug_scope_id;
    term->data.br.target = target;
}

static void mirEmitBrAt(MirFunctionState *state, MirBlockId target, const char *filename, int line, int column)
{
    MirTerminator *term = &(mirCurrentFunction(state)->blocks[state->current_block].terminator);
    term->kind = MIR_TERM_BR;
    term->filename = filename;
    term->line_number = line;
    term->column_number = column;
    term->debug_scope_id = state->current_debug_scope_id;
    term->data.br.target = target;
}

static void mirEmitCondBr(MirFunctionState *state, MirValueId condition, MirBlockId then_block, MirBlockId else_block)
{
    MirTerminator *term = &(mirCurrentFunction(state)->blocks[state->current_block].terminator);
    term->kind = MIR_TERM_COND_BR;
    term->filename = NULL;
    term->line_number = -1;
    term->column_number = -1;
    term->debug_scope_id = state->current_debug_scope_id;
    term->data.cond_br.condition = condition;
    term->data.cond_br.then_block = then_block;
    term->data.cond_br.else_block = else_block;
}

static void mirEmitCondBrAt(MirFunctionState *state, MirValueId condition, MirBlockId then_block, MirBlockId else_block,
                            const char *filename, int line, int column)
{
    MirTerminator *term = &(mirCurrentFunction(state)->blocks[state->current_block].terminator);
    term->kind = MIR_TERM_COND_BR;
    term->filename = filename;
    term->line_number = line;
    term->column_number = column;
    term->debug_scope_id = state->current_debug_scope_id;
    term->data.cond_br.condition = condition;
    term->data.cond_br.then_block = then_block;
    term->data.cond_br.else_block = else_block;
}

static void mirEmitRetVoid(MirFunctionState *state)
{
    MirTerminator *term = &(mirCurrentFunction(state)->blocks[state->current_block].terminator);
    term->kind = MIR_TERM_RET;
    term->filename = NULL;
    term->line_number = -1;
    term->column_number = -1;
    term->debug_scope_id = state->current_debug_scope_id;
    term->data.ret.has_value = false;
}

static void mirEmitRetVoidAt(MirFunctionState *state, const char *filename, int line, int column)
{
    MirTerminator *term = &(mirCurrentFunction(state)->blocks[state->current_block].terminator);
    term->kind = MIR_TERM_RET;
    term->filename = filename;
    term->line_number = line;
    term->column_number = column;
    term->debug_scope_id = state->current_debug_scope_id;
    term->data.ret.has_value = false;
}

static void mirEmitRetValueAt(MirFunctionState *state, MirValueId value, const char *filename, int line, int column)
{
    MirTerminator *term = &(mirCurrentFunction(state)->blocks[state->current_block].terminator);
    term->kind = MIR_TERM_RET;
    term->filename = filename;
    term->line_number = line;
    term->column_number = column;
    term->debug_scope_id = state->current_debug_scope_id;
    term->data.ret.has_value = true;
    term->data.ret.value = value;
}

static void mirEmitUnreachable(MirFunctionState *state)
{
    MirTerminator *term = &(mirCurrentFunction(state)->blocks[state->current_block].terminator);
    term->kind = MIR_TERM_UNREACHABLE;
    term->filename = NULL;
    term->line_number = -1;
    term->column_number = -1;
    term->debug_scope_id = state->current_debug_scope_id;
}

static bool mirIsCharPointerTarget(ASTDataType *data_type)
{
    if(data_type == NULL)
        return false;
    if(data_type->kind == AST_DATA_TYPE_KIND_OPTIONAL)
        data_type = data_type->child;
    return data_type != NULL &&
           data_type->kind == AST_DATA_TYPE_KIND_POINTER &&
           data_type->child != NULL &&
           data_type->child->kind == AST_DATA_TYPE_KIND_PRIMARY &&
           data_type->child->primary == AST_PRIMARY_DATA_TYPE_CHAR;
}

static MirValueId lowerStringLiteralAsPointer(MirFunctionState *state, ASTNode *node, ASTDataType *target_type)
{
    const char *global_name = mirEnsureStringLiteralGlobal(state->lowering, node->literal_string);
    ASTDataType *global_type = newArrayDataType(
        newPrimaryDataType(AST_PRIMARY_DATA_TYPE_CHAR),
        strlen(node->literal_string) + 1
    );
    MirValueId global_addr = mirEmitGlobalAddr(state, global_name, global_type,
                                               node->filename, node->line_number, node->column_number);
    return mirEmitConvert(state, global_addr, target_type,
                          node->filename, node->line_number, node->column_number);
}

static MirValueId lowerSlicePtrFromValue(MirFunctionState *state, ASTNode *node, MirValueId slice_value)
{
    ASTDataType *slice_type = mirGetValueType(state, slice_value);
    if(slice_type == NULL ||
       (!isSliceDataType(slice_type) && !isStringDataType(slice_type)))
        mirLoweringAbortNode("M2015", node,
                             "slice pointer extraction requires a slice-like value",
                             "type checking should reject non-slice operands here");

    MirValueId slice_slot = mirEmitAlloca(state, slice_type,
                                          node->filename, node->line_number, node->column_number);
    mirEmitStore(state, slice_slot, slice_value,
                 node->filename, node->line_number, node->column_number);

    ASTDataType *ptr_field_type = newWrappedDataType(AST_DATA_TYPE_KIND_POINTER,
                                                     cloneDataType(slice_type->child));
    MirValueId ptr_field_address = mirEmitFieldPtr(state, slice_slot, ptr_field_type, "ptr", 0,
                                                   node->filename, node->line_number, node->column_number);
    return mirEmitLoad(state, ptr_field_address, ptr_field_type,
                       node->filename, node->line_number, node->column_number);
}

static MirValueId lowerStringLiteralAsString(MirFunctionState *state, ASTNode *node)
{
    ASTDataType *char_ptr_type = newWrappedDataType(AST_DATA_TYPE_KIND_POINTER,
                                                    newPrimaryDataType(AST_PRIMARY_DATA_TYPE_CHAR));
    MirFieldValueList fields = newMirFieldValueList(2);
    strcpy(fields.items[0].identifier, "ptr");
    fields.items[0].value = lowerStringLiteralAsPointer(state, node, char_ptr_type);
    strcpy(fields.items[1].identifier, "len");
    fields.items[1].value = mirEmitConstInt(state, (long long int) strlen(node->literal_string),
                                            newPrimaryDataType(AST_PRIMARY_DATA_TYPE_I64),
                                            node->filename, node->line_number, node->column_number);
    return mirEmitStructLiteral(state, fields, newStringDataType(),
                                node->filename, node->line_number, node->column_number);
}

static MirValueId mirMaybeConvertValue(MirFunctionState *state, MirLowerScope *scope, ASTNode *node,
                                       MirValueId value, ASTDataType *target_type)
{
    if(target_type == NULL)
        return value;
    if(node != NULL && node->kind == AST_EXPR_LITERAL_NULL && target_type->kind == AST_DATA_TYPE_KIND_OPTIONAL)
        return mirLowerOptionalNull(state, target_type, node->filename, node->line_number, node->column_number);
    ASTDataType *value_type = mirGetValueType(state, value);
    if(isSameDataType(value_type, target_type))
        return value;
    if(target_type->kind == AST_DATA_TYPE_KIND_OPTIONAL && isSameDataType(value_type, target_type->child))
        return mirEmitOptionalSome(state, value, target_type, node->filename, node->line_number, node->column_number);
    if(scope != NULL &&
       value_type != NULL &&
       (value_type->kind == AST_DATA_TYPE_KIND_SLICE || value_type->kind == AST_DATA_TYPE_KIND_STRING) &&
       target_type->kind == AST_DATA_TYPE_KIND_POINTER)
    {
        if(node == NULL)
            return value;
        return lowerSlicePtrFromValue(state, node, value);
    }
    TypeSystemExprType source_type = newValueExprType(value_type);
    if(canImplicitConvertDataType(source_type, node, target_type))
        return mirEmitConvert(state, value, target_type, node->filename, node->line_number, node->column_number);
    return value;
}

#endif /* MIR_LOWERING_CORE_H */
