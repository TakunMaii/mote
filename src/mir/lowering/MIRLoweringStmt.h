#ifndef MIR_LOWERING_STMT_H
#define MIR_LOWERING_STMT_H

#include "MIRLoweringExpr.h"

static void lowerAssignNode(MirFunctionState *state, MirLowerScope *scope, ASTNode *node)
{
    const char *binding_name = semanticAssignIdentifier(node);

    if(isStructDeclAssign(node))
    {
        TypeInfo *existing_type_info = findTypeInfoInScope(&(scope->type_scope), binding_name) >= 0
            ? &(scope->type_scope.type_infos[findTypeInfoInScope(&(scope->type_scope), binding_name)])
            : NULL;
        if(existing_type_info == NULL || existing_type_info->predeclared)
        {
            ASTDataType *struct_type = declareStructType(node, &(scope->type_scope));
            semanticBindTypeDeclarationValue(node, &(scope->type_scope), struct_type);
        }
        return;
    }

    if(isEnumDeclAssign(node))
    {
        TypeInfo *existing_type_info = findTypeInfoInScope(&(scope->type_scope), binding_name) >= 0
            ? &(scope->type_scope.type_infos[findTypeInfoInScope(&(scope->type_scope), binding_name)])
            : NULL;
        if(existing_type_info == NULL || existing_type_info->predeclared)
        {
            ASTDataType *enum_type = declareEnumType(node, &(scope->type_scope));
            semanticBindTypeDeclarationValue(node, &(scope->type_scope), enum_type);
        }
        return;
    }

    if(node->lhs->kind == AST_EXPR_VARIABLE)
    {
        MirRuntimeBinding *local_binding = findMirRuntimeBindingInScope(scope, binding_name);
        MirRuntimeBinding *existing_binding = findMirRuntimeBinding(scope, binding_name);
        VariableInfo *existing_variable_info = findVariableInfo(&(scope->type_scope), binding_name);
        bool explicit_decl = isExplicitDeclared(node);
        bool is_new_variable = explicit_decl ||
                               existing_binding == NULL ||
                               (existing_variable_info != NULL && existing_variable_info->predeclared);

        if(is_new_variable)
        {
            ASTNode *resolved_function_value = resolveFunctionValueExpr(node->rhs, &(scope->type_scope));
            ASTDataType *declared_type = NULL;
            if(resolved_function_value != NULL &&
               functionHasTypeParameters(resolved_function_value->parameters))
                declared_type = cloneDataType(node->data_type);
            else
                declared_type = resolveNamedDataType(node->data_type, &(scope->type_scope), scope->self_data_type);
            TypeSystemExprType expr_type = {0};
            if(node->rhs->kind == AST_EXPR_ARRAY_LITERAL && node->rhs->lhs == NULL &&
               declared_type->kind == AST_DATA_TYPE_KIND_ARRAY && declared_type->array_length == 0)
                expr_type = newValueExprType(declared_type);
            else
                expr_type = inferExprType(node->rhs, &(scope->type_scope));
            mirDeclareVariableInfo(scope, node, declared_type, expr_type);

            MirRuntimeBinding *binding = local_binding;
            if(binding == NULL)
                binding = declareMirRuntimeBinding(scope, binding_name);
            binding->is_compile_time_constant = node->modifier.is_compile_time_binding;
            binding->declared_data_type = cloneDataType(declared_type);
            binding->function_value = resolved_function_value;
            binding->extern_value = resolveExternValueExpr(node->rhs, &(scope->type_scope));

            if(expr_type.kind == TYPE_SYSTEM_EXPR_TYPE_TYPE ||
               mirIsCompileTimeTypeFactory(declared_type) ||
               (node->rhs->kind == AST_EXPR_BUILTIN &&
                strcmp(node->rhs->identifier, "extern") == 0 &&
                declared_type->is_variadic) ||
               (binding->function_value != NULL &&
                functionHasTypeParameters(binding->function_value->parameters)))
            {
                binding->kind = MIR_RUNTIME_BINDING_COMPTIME_ONLY;
                binding->type_value = expr_type.kind == TYPE_SYSTEM_EXPR_TYPE_TYPE ? cloneDataType(expr_type.data_type) : NULL;
                return;
            }

            if(declared_type->kind == AST_DATA_TYPE_KIND_REFERENCE)
            {
                binding->kind = MIR_RUNTIME_BINDING_ALIAS_ADDRESS;
                binding->local_value = lowerExprAsAddress(state, scope, node->rhs);
                return;
            }

            if(scope->declare_as_globals)
            {
                binding->kind = MIR_RUNTIME_BINDING_GLOBAL_SLOT;
                strcpy(binding->global_name, binding_name);
                mirEnsureGlobal(state->lowering, binding_name, declared_type, false);

                if(state->is_top_level_init &&
                   node->rhs->kind == AST_EXPR_BUILTIN &&
                   strcmp(node->rhs->identifier, "zero") == 0)
                    return;
            }
            else
                binding->kind = MIR_RUNTIME_BINDING_LOCAL_SLOT;

            MirValueId value = lowerExprAsValue(state, scope, node->rhs, declared_type);
            value = mirMaybeConvertValue(state, scope, node->rhs, value, declared_type);

            if(scope->declare_as_globals)
            {
                MirValueId global_addr = mirEmitGlobalAddr(state, binding->global_name, declared_type,
                                                          node->filename, node->line_number, node->column_number);
                mirEmitStore(state, global_addr, value, node->filename, node->line_number, node->column_number);
            }
            else
            {
                binding->local_value = mirEmitAlloca(state, declared_type,
                                                     node->filename, node->line_number, node->column_number);
                mirRecordDebugLocal(mirCurrentFunction(state), binding_name, node->filename,
                                    node->line_number, node->column_number, declared_type, binding->local_value,
                                    state->current_debug_scope_id);
                mirEmitStore(state, binding->local_value, value, node->filename, node->line_number, node->column_number);
            }
            return;
        }

        VariableInfo *variable_info = existing_variable_info;
        ASTDataType *target_type = variable_info == NULL ? node->data_type : variable_info->data_type;
        if(target_type != NULL && target_type->kind == AST_DATA_TYPE_KIND_REFERENCE)
            target_type = target_type->child;

        MirValueId address = mirBindingAddress(state, existing_binding, node);
        MirValueId value = lowerExprAsValue(state, scope, node->rhs, target_type);
        value = mirMaybeConvertValue(state, scope, node->rhs, value, target_type);
        mirEmitStore(state, address, value, node->filename, node->line_number, node->column_number);

        if(local_binding == NULL && variable_info == NULL)
            local_binding = existing_binding;
        return;
    }

    MirValueId address = -1;
    ASTDataType *target_type = cloneDataType(node->data_type);
    if(node->lhs->kind == AST_EXPR_DEREF)
    {
        TypeSystemExprType lhs_type = inferExprType(node->lhs->lhs, &(scope->type_scope));
        target_type = lhs_type.data_type->child;
        address = lowerExprAsValue(state, scope, node->lhs->lhs, NULL);
    }
    else
    {
        address = lowerExprAsAddress(state, scope, node->lhs);
        target_type = mirResolvedExprValueType(node->lhs, &(scope->type_scope));
    }

    MirValueId value = lowerExprAsValue(state, scope, node->rhs, target_type);
    value = mirMaybeConvertValue(state, scope, node->rhs, value, target_type);
    mirEmitStore(state, address, value, node->filename, node->line_number, node->column_number);
}

static void lowerBlockNode(MirFunctionState *state, MirLowerScope *parent_scope, ASTNode *block_node)
{
    int saved_debug_scope_id = state->current_debug_scope_id;
    bool suppress_debug_scope = state != NULL && state->suppress_next_block_debug_scope;
    if(state != NULL)
        state->suppress_next_block_debug_scope = false;
    if(block_node != NULL && !suppress_debug_scope)
        state->current_debug_scope_id = mirAppendDebugScope(mirCurrentFunction(state),
                                                            saved_debug_scope_id,
                                                            block_node->filename,
                                                            block_node->line_number,
                                                            block_node->column_number);

    MirLowerScope *block_scope = (MirLowerScope*) malloc(sizeof(MirLowerScope));
    initMirLowerScope(block_scope, parent_scope, false);
    block_scope->self_data_type = parent_scope->self_data_type;

    MirCleanupFrame cleanup = {0};
    cleanup.parent = state->cleanup_top;
    state->cleanup_top = &cleanup;

    ASTNode *statement = block_node->lhs;
    while(statement)
    {
        lowerStatement(state, block_scope, statement);
        if(mirCurrentBlockTerminated(state))
            break;
        statement = statement->next;
    }

    if(!mirCurrentBlockTerminated(state))
        emitCleanupRange(state, block_scope, &cleanup, cleanup.parent);

    state->cleanup_top = cleanup.parent;
    state->current_debug_scope_id = saved_debug_scope_id;
}

static void lowerStatement(MirFunctionState *state, MirLowerScope *scope, ASTNode *node)
{
    if(node == NULL || mirCurrentBlockTerminated(state))
        return;

    switch(node->kind)
    {
        case AST_BLOCK:
            lowerBlockNode(state, scope, node);
            return;
        case AST_ASSIGN:
            lowerAssignNode(state, scope, node);
            return;
        case AST_STATEMENT_EXPR:
            lowerExprAsValue(state, scope, node->lhs, NULL);
            return;
        case AST_STATEMENT_RETURN: {
            ASTDataType *return_type = mirCurrentFunction(state)->return_data_type;
            MirValueId return_value = -1;
            if(node->lhs != NULL)
            {
                return_value = lowerExprAsValue(state, scope, node->lhs, return_type);
                return_value = mirMaybeConvertValue(state, scope, node->lhs, return_value, return_type);
            }
            emitCleanupRange(state, scope, state->cleanup_top, NULL);
            if(node->lhs == NULL)
                mirEmitRetVoidAt(state, node->filename, node->line_number, node->column_number);
            else
                mirEmitRetValueAt(state, return_value, node->filename, node->line_number, node->column_number);
            return;
        }
        case AST_STATEMENT_IF: {
            MirFunction *function = mirCurrentFunction(state);
            MirBlockId then_block = mirCreateBlock(state->lowering, function, "if_then");
            MirBlockId else_block = mirCreateBlock(state->lowering, function, "if_else");
            MirBlockId end_block = -1;
            MirValueId condition = lowerExprAsValue(state, scope, node->lhs, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_BOOL));
            mirEmitCondBrAt(state, condition, then_block, else_block, node->filename, node->line_number, node->column_number);

            mirSwitchToBlock(state, then_block);
            lowerStatement(state, scope, node->rhs);
            if(!mirCurrentBlockTerminated(state))
            {
                if(end_block < 0)
                    end_block = mirCreateBlock(state->lowering, function, "if_end");
                mirEmitBrAt(state, end_block, node->filename, node->line_number, node->column_number);
            }

            mirSwitchToBlock(state, else_block);
            if(node->body != NULL)
                lowerStatement(state, scope, node->body);
            if(!mirCurrentBlockTerminated(state))
            {
                if(end_block < 0)
                    end_block = mirCreateBlock(state->lowering, function, "if_end");
                mirEmitBrAt(state, end_block, node->filename, node->line_number, node->column_number);
            }

            if(end_block >= 0)
                mirSwitchToBlock(state, end_block);
            return;
        }
        case AST_STATEMENT_WHILE: {
            MirFunction *function = mirCurrentFunction(state);
            MirBlockId cond_block = mirCreateBlock(state->lowering, function, "while_cond");
            MirBlockId body_block = mirCreateBlock(state->lowering, function, "while_body");
            MirBlockId end_block = mirCreateBlock(state->lowering, function, "while_end");
            mirEmitBrAt(state, cond_block, node->filename, node->line_number, node->column_number);

            MirLoopContext loop_context = {0};
            loop_context.parent = state->loop_top;
            loop_context.break_block = end_block;
            loop_context.continue_block = cond_block;
            loop_context.cleanup_stop = state->cleanup_top;
            state->loop_top = &loop_context;

            mirSwitchToBlock(state, cond_block);
            MirValueId condition = lowerExprAsValue(state, scope, node->lhs, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_BOOL));
            mirEmitCondBrAt(state, condition, body_block, end_block, node->filename, node->line_number, node->column_number);

            mirSwitchToBlock(state, body_block);
            lowerStatement(state, scope, node->body);
            if(!mirCurrentBlockTerminated(state))
                mirEmitBrAt(state, cond_block, node->filename, node->line_number, node->column_number);

            state->loop_top = loop_context.parent;
            mirSwitchToBlock(state, end_block);
            return;
        }
        case AST_STATEMENT_DO_WHILE: {
            MirFunction *function = mirCurrentFunction(state);
            MirBlockId body_block = mirCreateBlock(state->lowering, function, "do_body");
            MirBlockId cond_block = mirCreateBlock(state->lowering, function, "do_cond");
            MirBlockId end_block = mirCreateBlock(state->lowering, function, "do_end");
            mirEmitBrAt(state, body_block, node->filename, node->line_number, node->column_number);

            MirLoopContext loop_context = {0};
            loop_context.parent = state->loop_top;
            loop_context.break_block = end_block;
            loop_context.continue_block = cond_block;
            loop_context.cleanup_stop = state->cleanup_top;
            state->loop_top = &loop_context;

            mirSwitchToBlock(state, body_block);
            lowerStatement(state, scope, node->body);
            if(!mirCurrentBlockTerminated(state))
                mirEmitBrAt(state, cond_block, node->filename, node->line_number, node->column_number);

            mirSwitchToBlock(state, cond_block);
            MirValueId condition = lowerExprAsValue(state, scope, node->lhs, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_BOOL));
            mirEmitCondBrAt(state, condition, body_block, end_block, node->filename, node->line_number, node->column_number);

            state->loop_top = loop_context.parent;
            mirSwitchToBlock(state, end_block);
            return;
        }
        case AST_STATEMENT_FOR: {
            MirLowerScope *loop_scope = (MirLowerScope*) malloc(sizeof(MirLowerScope));
            initMirLowerScope(loop_scope, scope, false);
            loop_scope->self_data_type = scope->self_data_type;

            if(node->lhs != NULL)
                lowerStatement(state, loop_scope, node->lhs);

            MirFunction *function = mirCurrentFunction(state);
            MirBlockId cond_block = mirCreateBlock(state->lowering, function, "for_cond");
            MirBlockId body_block = mirCreateBlock(state->lowering, function, "for_body");
            MirBlockId post_block = mirCreateBlock(state->lowering, function, "for_post");
            MirBlockId end_block = mirCreateBlock(state->lowering, function, "for_end");
            mirEmitBrAt(state, cond_block, node->filename, node->line_number, node->column_number);

            MirLoopContext loop_context = {0};
            loop_context.parent = state->loop_top;
            loop_context.break_block = end_block;
            loop_context.continue_block = post_block;
            loop_context.cleanup_stop = state->cleanup_top;
            state->loop_top = &loop_context;

            mirSwitchToBlock(state, cond_block);
            if(node->rhs != NULL)
            {
                MirValueId condition = lowerExprAsValue(state, loop_scope, node->rhs, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_BOOL));
                mirEmitCondBrAt(state, condition, body_block, end_block, node->filename, node->line_number, node->column_number);
            }
            else
                mirEmitBrAt(state, body_block, node->filename, node->line_number, node->column_number);

            mirSwitchToBlock(state, body_block);
            lowerStatement(state, loop_scope, node->body);
            if(!mirCurrentBlockTerminated(state))
                mirEmitBrAt(state, post_block, node->filename, node->line_number, node->column_number);

            mirSwitchToBlock(state, post_block);
            if(node->extra != NULL)
                lowerStatement(state, loop_scope, node->extra);
            if(!mirCurrentBlockTerminated(state))
                mirEmitBrAt(state, cond_block, node->filename, node->line_number, node->column_number);

            state->loop_top = loop_context.parent;
            mirSwitchToBlock(state, end_block);
            return;
        }
        case AST_STATEMENT_BREAK:
            if(state->loop_top == NULL)
                mirLoweringAbortNode("M2013", node,
                                     "break used outside loop",
                                     "this `break` has no enclosing loop to exit");
            emitCleanupRange(state, scope, state->cleanup_top, state->loop_top->cleanup_stop);
            mirEmitBr(state, state->loop_top->break_block);
            return;
        case AST_STATEMENT_CONTINUE:
            if(state->loop_top == NULL)
                mirLoweringAbortNode("M2014", node,
                                     "continue used outside loop",
                                     "this `continue` has no enclosing loop to target");
            emitCleanupRange(state, scope, state->cleanup_top, state->loop_top->cleanup_stop);
            mirEmitBr(state, state->loop_top->continue_block);
            return;
        case AST_STATEMENT_DEFER:
            appendDeferredStatement(state->cleanup_top, node->lhs);
            return;
        default:
            mirLoweringAbortNodeFormatted("ICE0305", node,
                                          NULL,
                                          "MIR lowering hit unsupported statement kind %s",
                                          astNodeKindToString(node->kind));
    }
}

MirProgram* lowerASTToMIR(ASTNode *root)
{
    if(root == NULL || root->lhs == NULL || root->lhs->kind != AST_BLOCK)
        mirLoweringAbortInternal("ICE0306",
                                 "AST root should contain a top-level block before MIR lowering",
                                 NULL);

    MirProgram *program = newMirProgram();
    MirLowering lowering = {0};
    lowering.program = program;

    MirFunction *init_function = mirAppendFunction(program);
    strcpy(init_function->name, "__mote_init");
    init_function->return_data_type = newPrimaryDataType(AST_PRIMARY_DATA_TYPE_VOID);
    init_function->entry_block = mirCreateBlock(&lowering, init_function, "entry");

    MirFunctionState init_state = {0};
    init_state.lowering = &lowering;
    init_state.function_index = 0;
    init_state.current_block = init_function->entry_block;
    init_state.current_debug_scope_id = mirAppendDebugScope(init_function, -1, root->filename, root->line_number, root->column_number);
    init_state.is_top_level_init = true;

    MirLowerScope *top_scope = (MirLowerScope*) malloc(sizeof(MirLowerScope));
    initMirLowerScope(top_scope, NULL, true);
    mirPredeclareTopLevelBindings(top_scope, root->lhs);
    mirResolveTopLevelTypes(top_scope, root->lhs);
    predeclareTopLevelFunctionTypes(root->lhs, &(top_scope->type_scope), NULL);
    mirPredeclareTopLevelRuntimeBindings(&lowering, top_scope, root->lhs);

    MirCleanupFrame cleanup = {0};
    init_state.cleanup_top = &cleanup;
    init_state.loop_top = NULL;

    ASTNode *statement = root->lhs->lhs;
    while(statement)
    {
        lowerStatement(&init_state, top_scope, statement);
        if(mirCurrentBlockTerminated(&init_state))
            break;
        statement = statement->next;
    }

    if(!mirCurrentBlockTerminated(&init_state))
    {
        emitCleanupRange(&init_state, top_scope, &cleanup, NULL);
        if(!mirCurrentBlockTerminated(&init_state))
            mirEmitRetVoid(&init_state);
    }

    return program;
}

#endif /* MIR_LOWERING_STMT_H */
