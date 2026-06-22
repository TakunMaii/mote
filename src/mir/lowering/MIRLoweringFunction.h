#ifndef MIR_LOWERING_FUNCTION_H
#define MIR_LOWERING_FUNCTION_H

#include "MIRLoweringRuntime.h"

static MirLowerScope* instantiateFunctionCallScope(MirFunctionState *state, MirLowerScope *outer_scope,
                                                   ASTNode *function_value, ASTNode *call_arguments,
                                                   ASTDataType *self_data_type)
{
    MirLowerScope *inst_scope = (MirLowerScope*) malloc(sizeof(MirLowerScope));
    initMirLowerScope(inst_scope, outer_scope, false);
    inst_scope->self_data_type = self_data_type != NULL ? self_data_type : outer_scope->self_data_type;

    if(self_data_type != NULL)
    {
        VariableInfo *self_variable = declareVariableInfo(&(inst_scope->type_scope), "Self");
        self_variable->is_compile_time_constant = true;
        self_variable->data_type = newPrimaryDataType(AST_PRIMARY_DATA_TYPE_TYPE);
        self_variable->type_value = cloneDataType(self_data_type);
    }

    ASTFunctionCapture *capture = function_value->captures;
    while(capture)
    {
        VariableInfo *outer_variable = findVariableInfo(&(outer_scope->type_scope), capture->identifier);
        if(outer_variable != NULL)
        {
            VariableInfo *inst_variable = declareVariableInfo(&(inst_scope->type_scope), capture->identifier);
            inst_variable->is_compile_time_constant = false;
            inst_variable->data_type = cloneDataType(outer_variable->data_type);
            inst_variable->type_value = cloneDataType(outer_variable->type_value);
            inst_variable->function_value = outer_variable->function_value;

            MirRuntimeBinding *outer_binding = findMirRuntimeBinding(outer_scope, capture->identifier);
            if(outer_binding != NULL)
            {
                MirRuntimeBinding *inst_binding = declareMirRuntimeBinding(inst_scope, capture->identifier);
                *inst_binding = *outer_binding;
            }
        }
        capture = capture->next;
    }

    ASTFunctionParameter *parameter = function_value->parameters;
    ASTNode *argument = call_arguments;
    while(parameter && argument)
    {
        ASTDataType *resolved_parameter_type = resolveNamedDataType(parameter->data_type, &(inst_scope->type_scope), self_data_type);
        VariableInfo *inst_variable = declareVariableInfo(&(inst_scope->type_scope), parameter->identifier);
        inst_variable->is_compile_time_constant = false;
        inst_variable->data_type = cloneDataType(resolved_parameter_type);

        if(resolved_parameter_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
           resolved_parameter_type->primary == AST_PRIMARY_DATA_TYPE_TYPE)
        {
            TypeSystemExprType argument_type = inferExprType(argument, &(outer_scope->type_scope));
            inst_variable->type_value = cloneDataType(argument_type.data_type);
        }
        else
        {
            if(argument->kind == AST_EXPR_FUNCTION)
                inst_variable->function_value = argument;
            else if(argument->kind == AST_EXPR_VARIABLE)
            {
                VariableInfo *outer_variable = findVariableInfo(&(outer_scope->type_scope), argument->identifier);
                if(outer_variable != NULL)
                {
                    inst_variable->type_value = cloneDataType(outer_variable->type_value);
                    inst_variable->function_value = outer_variable->function_value;
                }
            }
        }

        if(resolved_parameter_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
           resolved_parameter_type->primary == AST_PRIMARY_DATA_TYPE_TYPE)
        {
        }
        else if(resolved_parameter_type->kind == AST_DATA_TYPE_KIND_REFERENCE)
        {
            MirRuntimeBinding *binding = declareMirRuntimeBinding(inst_scope, parameter->identifier);
            binding->kind = MIR_RUNTIME_BINDING_ALIAS_ADDRESS;
            binding->is_compile_time_constant = false;
            binding->declared_data_type = cloneDataType(resolved_parameter_type);
            binding->local_value = lowerExprAsAddress(state, outer_scope, argument);
        }
        else
        {
            MirValueId argument_value = lowerExprAsValue(state, outer_scope, argument, resolved_parameter_type);
            MirValueId slot = mirEmitAlloca(state, resolved_parameter_type,
                                            argument->filename, argument->line_number, argument->column_number);
            mirRecordDebugLocal(mirCurrentFunction(state), parameter->identifier, parameter->filename,
                                parameter->line_number, parameter->column_number, resolved_parameter_type, slot,
                                state->current_debug_scope_id);
            mirEmitStore(state, slot, argument_value, argument->filename, argument->line_number, argument->column_number);

            MirRuntimeBinding *binding = declareMirRuntimeBinding(inst_scope, parameter->identifier);
            binding->kind = MIR_RUNTIME_BINDING_LOCAL_SLOT;
            binding->is_compile_time_constant = false;
            binding->declared_data_type = cloneDataType(resolved_parameter_type);
            binding->local_value = slot;
        }

        parameter = parameter->next;
        argument = argument->next;
    }

    if(parameter != NULL || argument != NULL)
        mirLoweringAbortInternal("ICE0302",
                                 "function call argument count mismatch during MIR instantiation",
                                 "type checking should reject mismatched call arity before MIR lowering");

    return inst_scope;
}

static ASTFunctionParameter* instantiateRuntimeFunctionParameters(ASTFunctionParameter *parameter,
                                                                  ScopeFrame *scope,
                                                                  ASTDataType *self_data_type)
{
    ASTFunctionParameter *head = NULL;
    ASTFunctionParameter *tail = NULL;
    while(parameter)
    {
        ASTDataType *resolved_type = resolveNamedDataType(parameter->data_type, scope, self_data_type);
        if(!(resolved_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
             resolved_type->primary == AST_PRIMARY_DATA_TYPE_TYPE))
        {
            ASTFunctionParameter *new_parameter = (ASTFunctionParameter*) malloc(sizeof(ASTFunctionParameter));
            memset(new_parameter, 0, sizeof(ASTFunctionParameter));
            new_parameter->filename = parameter->filename;
            new_parameter->line_number = parameter->line_number;
            new_parameter->column_number = parameter->column_number;
            new_parameter->end_line_number = parameter->end_line_number;
            new_parameter->end_column_number = parameter->end_column_number;
            strcpy(new_parameter->identifier, parameter->identifier);
            new_parameter->data_type = resolved_type;

            if(head == NULL)
                head = new_parameter;
            else
                tail->next = new_parameter;
            tail = new_parameter;
        }
        parameter = parameter->next;
    }
    return head;
}

static ASTNode* instantiateRuntimeFunctionExpr(ASTNode *function_expr, MirLowerScope *inst_scope)
{
    ASTNode *specialized = (ASTNode*) malloc(sizeof(ASTNode));
    memset(specialized, 0, sizeof(ASTNode));
    *specialized = *function_expr;
    specialized->parameters = instantiateRuntimeFunctionParameters(function_expr->parameters,
                                                                   &(inst_scope->type_scope),
                                                                   inst_scope->self_data_type);
    specialized->return_data_type = resolveNamedDataType(function_expr->return_data_type,
                                                         &(inst_scope->type_scope),
                                                         inst_scope->self_data_type);
    specialized->data_type = newFunctionDataType(cloneFunctionParameters(specialized->parameters),
                                                 function_expr->is_variadic,
                                                 cloneDataType(specialized->return_data_type));
    return specialized;
}

static int mirFindSpecializedFunctionCacheEntry(MirLowering *lowering, ASTNode *source_function,
                                                ASTDataType *self_data_type, ASTDataType *function_type)
{
    for(int i = 0; i < lowering->specialized_function_count; i++)
    {
        MirSpecializedFunctionCacheEntry *entry = &(lowering->specialized_functions[i]);
        if(entry->source_function != source_function)
            continue;
        if(!isSameDataType(entry->self_data_type, self_data_type))
            continue;
        if(!isSameDataType(entry->function_type, function_type))
            continue;
        return i;
    }
    return -1;
}

static int mirCacheSpecializedFunctionDefinition(MirLowering *lowering, ASTNode *source_function,
                                                 ASTDataType *self_data_type, ASTDataType *function_type,
                                                 int function_index)
{
    lowering->specialized_functions = (MirSpecializedFunctionCacheEntry*) realloc(
        lowering->specialized_functions,
        sizeof(MirSpecializedFunctionCacheEntry) * (lowering->specialized_function_count + 1)
    );
    MirSpecializedFunctionCacheEntry *entry =
        &(lowering->specialized_functions[lowering->specialized_function_count++]);
    memset(entry, 0, sizeof(MirSpecializedFunctionCacheEntry));
    entry->source_function = source_function;
    entry->self_data_type = cloneDataType(self_data_type);
    entry->function_type = cloneDataType(function_type);
    entry->function_index = function_index;
    return function_index;
}

static MirValueId lowerRuntimeSpecializedFunctionValue(MirFunctionState *state, MirLowerScope *scope,
                                                       ASTNode *source_function, ASTNode *call_arguments,
                                                       const char *name_hint, ASTDataType *self_data_type,
                                                       ASTFunctionParameter **out_runtime_parameters,
                                                       ASTDataType **out_return_type)
{
    MirLowerScope *inst_scope = instantiateFunctionCallScope(
        state,
        scope,
        source_function,
        call_arguments,
        self_data_type
    );
    ASTNode *specialized_function = instantiateRuntimeFunctionExpr(source_function, inst_scope);

    if(out_runtime_parameters != NULL)
        *out_runtime_parameters = specialized_function->parameters;
    if(out_return_type != NULL)
        *out_return_type = specialized_function->return_data_type;

    if(source_function->captures == NULL)
    {
        int cache_index = mirFindSpecializedFunctionCacheEntry(
            state->lowering,
            source_function,
            self_data_type,
            specialized_function->data_type
        );
        int function_index = -1;
        if(cache_index >= 0)
            function_index = state->lowering->specialized_functions[cache_index].function_index;
        else
        {
            function_index = lowerFunctionExprDefinition(
                state->lowering,
                inst_scope,
                specialized_function,
                name_hint,
                self_data_type
            );
            mirCacheSpecializedFunctionDefinition(
                state->lowering,
                source_function,
                self_data_type,
                specialized_function->data_type,
                function_index
            );
        }

        MirFunction *mir_function = state->lowering->program->functions[function_index];
        return mirEmitFunctionRef(state, mir_function->name, specialized_function->data_type,
                                  source_function->filename, source_function->line_number, source_function->column_number);
    }

    return lowerFunctionExprAsValue(
        state,
        inst_scope,
        specialized_function,
        name_hint,
        self_data_type
    );
}

static MirMaybeValue tryLowerComptimeFunctionCall(MirFunctionState *state, MirLowerScope *scope, ASTNode *call_node)
{
    MirMaybeValue result = {0};
    if(call_node->lhs == NULL || call_node->lhs->kind != AST_EXPR_VARIABLE)
        return result;

    VariableInfo *callee_variable = findVariableInfo(&(scope->type_scope), call_node->lhs->identifier);
    if(callee_variable == NULL || callee_variable->function_value == NULL)
        return result;

    ASTNode *returned_expr = findReturnedExpr(callee_variable->function_value);
    if(returned_expr == NULL)
        return result;

    MirRuntimeBinding *callee_binding = findMirRuntimeBinding(scope, call_node->lhs->identifier);
    bool callee_is_comptime_only = callee_binding != NULL &&
                                   callee_binding->kind == MIR_RUNTIME_BINDING_COMPTIME_ONLY;
    if(!callee_is_comptime_only || returned_expr->kind != AST_EXPR_FUNCTION)
        return result;

    MirLowerScope *inst_scope = instantiateFunctionCallScope(
        state,
        scope,
        callee_variable->function_value,
        call_node->rhs,
        NULL
    );

    result.value = lowerExprAsValue(state, inst_scope, returned_expr, NULL);
    result.valid = true;
    return result;
}

static void bindSpecializedNamedTypes(MirLowerScope *scope, ASTDataType *source_type, ASTDataType *resolved_type)
{
    if(source_type == NULL || resolved_type == NULL)
        return;

    if(source_type->kind == AST_DATA_TYPE_KIND_NAMED)
    {
        ASTDataType *builtin_type = builtinIdentifierToDataType(source_type->identifier);
        bool same_named = resolved_type->kind == AST_DATA_TYPE_KIND_NAMED &&
                          strcmp(source_type->identifier, resolved_type->identifier) == 0;
        if(builtin_type == NULL &&
           strcmp(source_type->identifier, "Self") != 0 &&
           !same_named &&
           findVariableInfo(&(scope->type_scope), source_type->identifier) == NULL &&
           findTypeInfo(&(scope->type_scope), source_type->identifier) == NULL)
        {
            VariableInfo *type_variable = declareVariableInfo(&(scope->type_scope), source_type->identifier);
            type_variable->is_compile_time_constant = true;
            type_variable->data_type = newPrimaryDataType(AST_PRIMARY_DATA_TYPE_TYPE);
            type_variable->type_value = cloneDataType(resolved_type);
        }
        return;
    }

    if(source_type->kind != resolved_type->kind)
        return;

    switch(source_type->kind)
    {
        case AST_DATA_TYPE_KIND_POINTER:
        case AST_DATA_TYPE_KIND_REFERENCE:
        case AST_DATA_TYPE_KIND_ARRAY:
            bindSpecializedNamedTypes(scope, source_type->child, resolved_type->child);
            return;
        case AST_DATA_TYPE_KIND_FUNCTION: {
            ASTFunctionParameter *source_parameter = source_type->parameters;
            ASTFunctionParameter *resolved_parameter = resolved_type->parameters;
            while(source_parameter != NULL && resolved_parameter != NULL)
            {
                bindSpecializedNamedTypes(scope, source_parameter->data_type, resolved_parameter->data_type);
                source_parameter = source_parameter->next;
                resolved_parameter = resolved_parameter->next;
            }
            bindSpecializedNamedTypes(scope, source_type->return_data_type, resolved_type->return_data_type);
            return;
        }
        case AST_DATA_TYPE_KIND_APPLY: {
            bindSpecializedNamedTypes(scope, source_type->callee, resolved_type->callee);
            ASTTypeArgument *source_argument = source_type->arguments;
            ASTTypeArgument *resolved_argument = resolved_type->arguments;
            while(source_argument != NULL && resolved_argument != NULL)
            {
                bindSpecializedNamedTypes(scope, source_argument->data_type, resolved_argument->data_type);
                source_argument = source_argument->next;
                resolved_argument = resolved_argument->next;
            }
            return;
        }
        case AST_DATA_TYPE_KIND_STRUCT: {
            (void) scope;
            return;
        }
        default:
            return;
    }
}

static int lowerFunctionExprDefinition(MirLowering *lowering, MirLowerScope *scope, ASTNode *function_expr,
                                       const char *name_hint, ASTDataType *self_data_type);

static MirValueId lowerFunctionExprAsValue(MirFunctionState *state, MirLowerScope *scope, ASTNode *function_expr,
                                           const char *name_hint, ASTDataType *self_data_type)
{
    if(functionHasTypeParameters(function_expr->parameters))
        mirLoweringAbortNode("M2005", function_expr,
                             "generic function value requires compile-time specialization before runtime lowering",
                             "specialize this generic function before using it as a runtime value");

    int function_index = lowerFunctionExprDefinition(state->lowering, scope, function_expr, name_hint, self_data_type);
    MirFunction *mir_function = state->lowering->program->functions[function_index];

    int runtime_capture_count = 0;
    ASTFunctionCapture *capture = function_expr->captures;
    while(capture)
    {
        VariableInfo *variable_info = findVariableInfo(&(scope->type_scope), capture->identifier);
        MirRuntimeBinding *binding = findMirRuntimeBinding(scope, capture->identifier);
        if(variable_info != NULL && binding != NULL)
            runtime_capture_count++;
        capture = capture->next;
    }

    MirOperandList captures = newMirOperandList(runtime_capture_count);
    int capture_index = 0;
    capture = function_expr->captures;
    while(capture)
    {
        VariableInfo *variable_info = findVariableInfo(&(scope->type_scope), capture->identifier);
        MirRuntimeBinding *binding = findMirRuntimeBinding(scope, capture->identifier);
        if(variable_info != NULL && binding != NULL)
        {
            if(capture->kind == AST_FUNCTION_CAPTURE_VALUE)
            {
                ASTNode fake_var = {0};
                fake_var.kind = AST_EXPR_VARIABLE;
                fake_var.filename = capture->filename;
                fake_var.line_number = capture->line_number;
                fake_var.column_number = capture->column_number;
                strcpy(fake_var.identifier, capture->identifier);
                captures.items[capture_index++] = lowerVariableValue(state, scope, &fake_var, NULL);
            }
            else
                captures.items[capture_index++] = mirBindingAddress(state, binding, function_expr);
        }
        capture = capture->next;
    }

    ASTDataType *function_type = resolveNamedDataType(function_expr->data_type, &(scope->type_scope), self_data_type);
    return mirEmitMakeClosure(state, mir_function->name, function_type, mir_function->closure_env_type, captures,
                              function_expr->filename, function_expr->line_number, function_expr->column_number);
}

static int lowerFunctionExprDefinition(MirLowering *lowering, MirLowerScope *scope, ASTNode *function_expr,
                                       const char *name_hint, ASTDataType *self_data_type)
{
    MirFunction *function = mirAppendFunction(lowering->program);
    int function_index = lowering->program->function_count - 1;
    if(name_hint != NULL)
        snprintf(function->name, sizeof(function->name), "%s_%d", name_hint, lowering->unique_function_counter++);
    else
        snprintf(function->name, sizeof(function->name), "lambda_%d", lowering->unique_function_counter++);
    function->source_function = function_expr;
        function->return_data_type = function_expr->return_data_type != NULL
            ? resolveNamedDataType(function_expr->return_data_type, &(scope->type_scope), self_data_type)
            : newPrimaryDataType(AST_PRIMARY_DATA_TYPE_VOID);
    function->closure_env_input = -1;
    function->entry_block = mirCreateBlock(lowering, function, "entry");

    MirLowerScope *function_scope = (MirLowerScope*) malloc(sizeof(MirLowerScope));
    initMirLowerScope(function_scope, scope, false);
    function_scope->self_data_type = self_data_type;

    if(self_data_type != NULL)
    {
        VariableInfo *self_variable = declareVariableInfo(&(function_scope->type_scope), "Self");
        self_variable->is_compile_time_constant = true;
        self_variable->data_type = newPrimaryDataType(AST_PRIMARY_DATA_TYPE_TYPE);
        self_variable->type_value = cloneDataType(self_data_type);
    }

    ASTFunctionCapture *capture = function_expr->captures;
    while(capture)
    {
        VariableInfo *outer_variable = findVariableInfo(&(scope->type_scope), capture->identifier);
        if(outer_variable != NULL)
        {
            VariableInfo *capture_variable = declareVariableInfo(&(function_scope->type_scope), capture->identifier);
            capture_variable->is_compile_time_constant = false;
            capture_variable->data_type = cloneDataType(outer_variable->data_type);
            capture_variable->type_value = cloneDataType(outer_variable->type_value);
            capture_variable->function_value = outer_variable->function_value;
        }
        capture = capture->next;
    }

    MirFunctionState state = {0};
    state.lowering = lowering;
    state.function_index = function_index;
    state.current_block = function->entry_block;
    state.current_debug_scope_id = mirAppendDebugScope(function,
                                                       -1,
                                                       function_expr->filename,
                                                       function_expr->line_number,
                                                       function_expr->column_number);

    capture = function_expr->captures;
    while(capture)
    {
        VariableInfo *outer_variable = findVariableInfo(&(scope->type_scope), capture->identifier);
        MirRuntimeBinding *outer_binding = findMirRuntimeBinding(scope, capture->identifier);
        if(outer_variable != NULL && outer_binding != NULL)
        {
            function->captures = (MirCaptureDesc*) realloc(
                function->captures,
                sizeof(MirCaptureDesc) * (function->capture_count + 1)
            );
            MirCaptureDesc *desc = &(function->captures[function->capture_count++]);
            memset(desc, 0, sizeof(MirCaptureDesc));
            strcpy(desc->identifier, capture->identifier);
            desc->source_data_type = cloneDataType(outer_variable->data_type);
            desc->by_reference = capture->kind != AST_FUNCTION_CAPTURE_VALUE;
            desc->runtime_data_type = desc->by_reference
                ? newWrappedDataType(AST_DATA_TYPE_KIND_POINTER,
                                     cloneDataType(getReferenceTargetType(outer_variable->data_type)))
                : cloneDataType(getReferenceTargetType(outer_variable->data_type));
            desc->input_value = -1;
        }
        capture = capture->next;
    }

    if(function->capture_count > 0)
    {
        function->closure_env_type = mirBuildClosureEnvType(function->captures, function->capture_count);
        function->closure_env_input = mirCreateInput(
            function,
            mirClosureEnvPointerType(function->closure_env_type),
            "__env"
        );

        for(int i = 0; i < function->capture_count; i++)
        {
            MirCaptureDesc *desc = &(function->captures[i]);
            MirRuntimeBinding *binding = declareMirRuntimeBinding(function_scope, desc->identifier);
            binding->is_compile_time_constant = false;
            binding->declared_data_type = cloneDataType(desc->source_data_type);
            binding->kind = MIR_RUNTIME_BINDING_ALIAS_ADDRESS;

            MirValueId field_address = mirEmitFieldPtr(
                &state,
                function->closure_env_input,
                desc->runtime_data_type,
                desc->identifier,
                i,
                function_expr->filename,
                function_expr->line_number,
                function_expr->column_number
            );

            if(desc->by_reference)
            {
                binding->local_value = mirEmitLoad(
                    &state,
                    field_address,
                    desc->runtime_data_type,
                    function_expr->filename,
                    function_expr->line_number,
                    function_expr->column_number
                );
            }
            else
                binding->local_value = field_address;
        }
    }

    ASTFunctionParameter *parameter = function_expr->parameters;
    while(parameter)
    {
        ASTDataType *resolved_type = resolveNamedDataType(parameter->data_type, &(function_scope->type_scope), self_data_type);
        VariableInfo *variable_info = declareVariableInfo(&(function_scope->type_scope), parameter->identifier);
        variable_info->is_compile_time_constant = false;
        variable_info->data_type = cloneDataType(resolved_type);
        if(resolved_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
           resolved_type->primary == AST_PRIMARY_DATA_TYPE_TYPE)
        {
            variable_info->type_value = newNamedDataType(parameter->identifier);
            parameter = parameter->next;
            continue;
        }

        function->parameters = (MirParamDesc*) realloc(
            function->parameters,
            sizeof(MirParamDesc) * (function->parameter_count + 1)
        );
        MirParamDesc *desc = &(function->parameters[function->parameter_count++]);
        memset(desc, 0, sizeof(MirParamDesc));
        strcpy(desc->identifier, parameter->identifier);
        desc->source_data_type = cloneDataType(resolved_type);
        desc->runtime_data_type = mirRuntimeParameterType(resolved_type);
        desc->by_reference = resolved_type->kind == AST_DATA_TYPE_KIND_REFERENCE;
        desc->input_value = mirCreateInput(function, desc->runtime_data_type, desc->identifier);

        MirRuntimeBinding *binding = declareMirRuntimeBinding(function_scope, desc->identifier);
        binding->is_compile_time_constant = false;
        binding->declared_data_type = cloneDataType(resolved_type);
        if(desc->by_reference)
        {
            binding->kind = MIR_RUNTIME_BINDING_ALIAS_ADDRESS;
            binding->local_value = desc->input_value;
        }
        else
        {
            binding->kind = MIR_RUNTIME_BINDING_LOCAL_SLOT;
            binding->local_value = mirEmitAlloca(&state, resolved_type,
                                                 parameter->filename, parameter->line_number, parameter->column_number);
            mirRecordDebugParameter(function, parameter->identifier, parameter->filename,
                                    parameter->line_number, parameter->column_number, resolved_type,
                                    binding->local_value, function->parameter_count, state.current_debug_scope_id);
            mirEmitStore(&state, binding->local_value, desc->input_value,
                         parameter->filename, parameter->line_number, parameter->column_number);
        }

        parameter = parameter->next;
    }

    MirCleanupFrame function_cleanup = {0};
    state.cleanup_top = &function_cleanup;
    state.loop_top = NULL;
    state.suppress_next_block_debug_scope = true;

    lowerStatement(&state, function_scope, function_expr->body);

    if(!mirCurrentBlockTerminated(&state))
    {
        MirFunction *lowered_function = mirCurrentFunction(&state);
        if(mirIsValueTypeVoid(lowered_function->return_data_type))
            mirEmitRetVoid(&state);
        else
            mirLoweringAbortInternal("ICE0303",
                                     "lowered function may fall through without return",
                                     lowered_function->name);
    }

    return function_index;
}

static MirValueId lowerMethodFunctionValue(MirFunctionState *state, MirLowerScope *scope, ASTNode *member_node,
                                           ASTDataType *struct_type)
{
    ASTStructMember *member = findStructMember(struct_type, member_node->identifier);
    if(member == NULL || member->value == NULL)
        mirLoweringAbortNodeFormatted("M2006", member_node,
                                      "this member does not resolve to a method",
                                      "unknown method `%s`",
                                      member_node->identifier);

    char hint[MIR_MAX_NAME_LENGTH] = {0};
    if(struct_type->identifier[0] != '\0')
        snprintf(hint, sizeof(hint), "%s_%s", struct_type->identifier, member_node->identifier);
    else
        snprintf(hint, sizeof(hint), "method_%s", member_node->identifier);

    MirLowerScope method_scope_storage = {0};
    MirLowerScope *method_scope = scope;
    if(member_node->lhs != NULL &&
       member_node->lhs->kind == AST_EXPR_CALL &&
       member_node->lhs->lhs != NULL &&
       member_node->lhs->lhs->kind == AST_EXPR_VARIABLE)
    {
        VariableInfo *callee_variable = findVariableInfo(&(scope->type_scope), member_node->lhs->lhs->identifier);
        if(callee_variable != NULL && callee_variable->function_value != NULL)
        {
            method_scope = instantiateFunctionCallScope(
                state,
                scope,
                callee_variable->function_value,
                member_node->lhs->rhs,
                struct_type
            );
        }
    }
    else if(member->lexical_type_scope != NULL)
    {
        initMirLowerScope(&method_scope_storage, scope, false);
        method_scope = &method_scope_storage;
    }

    if(member->lexical_type_scope != NULL)
        mirBindLexicalTypeScope(method_scope, member->lexical_type_scope);

    if(member->value->data_type != NULL && member->data_type != NULL)
        bindSpecializedNamedTypes(method_scope, member->value->data_type, member->data_type);

    return lowerFunctionExprAsValue(state, method_scope, member->value, hint, struct_type);
}

static bool tryGetDirectGenericFunctionValue(MirLowerScope *scope, ASTNode *expr,
                                             ASTNode **out_function_value,
                                             const char **out_name_hint)
{
    if(out_function_value != NULL)
        *out_function_value = NULL;
    if(out_name_hint != NULL)
        *out_name_hint = NULL;

    if(expr == NULL)
        return false;

    if(expr->kind == AST_EXPR_VARIABLE)
    {
        VariableInfo *variable_info = findVariableInfo(&(scope->type_scope), expr->identifier);
        if(variable_info != NULL && variable_info->function_value != NULL)
        {
            if(out_function_value != NULL)
                *out_function_value = variable_info->function_value;
            if(out_name_hint != NULL)
                *out_name_hint = expr->identifier;
            return true;
        }
        return false;
    }

    if(expr->kind == AST_EXPR_MEMBER)
    {
        TypeSystemExprType owner_type = inferExprType(expr->lhs, &(scope->type_scope));
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
        struct_type = resolveNamedDataType(struct_type, &(scope->type_scope), scope->self_data_type);

        if(!isStructDataType(struct_type))
            return false;

        ASTStructMember *member = findStructMember(struct_type, expr->identifier);
        if(member == NULL || member->value == NULL || member->value->kind != AST_EXPR_FUNCTION)
            return false;

        if(out_function_value != NULL)
            *out_function_value = member->value;
        if(out_name_hint != NULL)
            *out_name_hint = expr->identifier;
        return true;
    }

    return false;
}

#endif /* MIR_LOWERING_FUNCTION_H */
