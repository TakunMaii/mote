#ifndef MIR_LOWERING_RUNTIME_H
#define MIR_LOWERING_RUNTIME_H

#include "MIRLoweringDebug.h"

static MirValueId mirBindingAddress(MirFunctionState *state, MirRuntimeBinding *binding, ASTNode *use_node)
{
    if(binding->kind == MIR_RUNTIME_BINDING_ALIAS_ADDRESS || binding->kind == MIR_RUNTIME_BINDING_LOCAL_SLOT)
        return binding->local_value;
    if(binding->kind == MIR_RUNTIME_BINDING_GLOBAL_SLOT)
        return mirEmitGlobalAddr(state, binding->global_name, binding->declared_data_type,
                                 use_node->filename, use_node->line_number, use_node->column_number);

    mirLoweringAbortNodeFormatted("M2003", use_node,
                                  "runtime address is unavailable here",
                                  "identifier `%s` has no runtime address",
                                  astUserFacingIdentifier(binding->identifier));
}

static MirValueId lowerVariableValue(MirFunctionState *state, MirLowerScope *scope, ASTNode *node, ASTDataType *expected_type)
{
    MirRuntimeBinding *binding = findMirRuntimeBinding(scope, node->identifier);
    if(binding == NULL || binding->kind == MIR_RUNTIME_BINDING_COMPTIME_ONLY)
    {
        VariableInfo *variable_info = findVariableInfo(&(scope->type_scope), node->identifier);
        if(variable_info != NULL && variable_info->function_value != NULL)
            return lowerFunctionExprAsValue(state, scope, variable_info->function_value, node->identifier, scope->self_data_type);
        mirLoweringAbortNodeFormatted("M2004", node,
                                      "this name does not exist as a runtime value",
                                      "variable `%s` is compile-time only or unavailable",
                                      astUserFacingIdentifier(node->identifier));
    }

    ASTDataType *expr_type = binding->declared_data_type != NULL
        ? cloneDataType(binding->declared_data_type)
        : mirResolvedExprValueType(node, &(scope->type_scope));
    MirValueId address = mirBindingAddress(state, binding, node);
    MirValueId value = mirEmitLoad(state, address, expr_type, node->filename, node->line_number, node->column_number);
    return mirMaybeConvertValue(state, scope, node, value, expected_type);
}

static MirValueId lowerExprMaterializedAddress(MirFunctionState *state, MirLowerScope *scope, ASTNode *node)
{
    MirValueId value = lowerExprAsValue(state, scope, node, NULL);
    ASTDataType *value_type = mirGetValueType(state, value);
    MirValueId slot = mirEmitAlloca(state, value_type, node->filename, node->line_number, node->column_number);
    mirEmitStore(state, slot, value, node->filename, node->line_number, node->column_number);
    return slot;
}

static void appendDeferredStatement(MirCleanupFrame *cleanup, ASTNode *statement)
{
    MirDeferredStmt *entry = (MirDeferredStmt*) malloc(sizeof(MirDeferredStmt));
    entry->statement = statement;
    entry->next = cleanup->deferred_head;
    cleanup->deferred_head = entry;
}

static void emitCleanupRange(MirFunctionState *state, MirLowerScope *scope,
                             MirCleanupFrame *from, MirCleanupFrame *stop_exclusive)
{
    MirCleanupFrame *frame = from;
    while(frame != stop_exclusive)
    {
        MirDeferredStmt *deferred = frame->deferred_head;
        while(deferred)
        {
            lowerStatement(state, scope, deferred->statement);
            deferred = deferred->next;
            if(mirCurrentBlockTerminated(state))
                return;
        }
        frame = frame->parent;
    }
}

static MirGlobal* mirEnsureGlobal(MirLowering *lowering, const char *name, ASTDataType *data_type, bool mutable)
{
    for(int i = 0; i < lowering->program->global_count; i++)
    {
        if(strcmp(lowering->program->globals[i].name, name) == 0)
            return &(lowering->program->globals[i]);
    }
    MirGlobal *global = mirAppendGlobal(lowering->program);
    global->kind = MIR_GLOBAL_VAR;
    strcpy(global->name, name);
    global->data_type = cloneDataType(data_type);
    global->is_runtime_storage = true;
    return global;
}

static const char* mirEnsureStringLiteralGlobal(MirLowering *lowering, const char *value)
{
    char name[MIR_MAX_NAME_LENGTH];
    snprintf(name, sizeof(name), "__mote_str_%d", lowering->unique_global_counter++);

    ASTDataType *string_type = newArrayDataType(
        newPrimaryDataType(AST_PRIMARY_DATA_TYPE_CHAR),
        strlen(value) + 1
    );
    MirGlobal *global = mirEnsureGlobal(lowering, name, string_type, false);
    global->has_const_string_initializer = true;
    strcpy(global->const_string_initializer, value);
    return global->name;
}

static void mirDeclareVariableInfo(MirLowerScope *scope, ASTNode *assign_node, ASTDataType *declared_type,
                                   TypeSystemExprType expr_type)
{
    const char *binding_name = assign_node->lhs != NULL &&
                               assign_node->lhs->kind == AST_EXPR_VARIABLE &&
                               assign_node->lhs->identifier[0] != '\0'
        ? assign_node->lhs->identifier
        : assign_node->identifier;
    int existing_index = findVariableInfoInScope(&(scope->type_scope), binding_name);
    VariableInfo *variable_info = existing_index >= 0
        ? &(scope->type_scope.variable_infos[existing_index])
        : declareVariableInfo(&(scope->type_scope), binding_name);
    variable_info->is_compile_time_constant = assign_node->modifier.is_compile_time_binding;
    variable_info->predeclared = false;
    variable_info->operator_kind = assign_node->operator_kind;
    variable_info->data_type = cloneDataType(declared_type);
    if(expr_type.kind == TYPE_SYSTEM_EXPR_TYPE_TYPE)
        variable_info->type_value = cloneDataType(expr_type.data_type);
    else
        variable_info->type_value = NULL;
    variable_info->function_value = resolveFunctionValueExpr(assign_node->rhs, &(scope->type_scope));
    variable_info->extern_value = resolveExternValueExpr(assign_node->rhs, &(scope->type_scope));
}

static void mirPredeclareTopLevelBindings(MirLowerScope *scope, ASTNode *block_node)
{
    if(scope == NULL || block_node == NULL)
        return;

    for(ASTNode *statement = block_node->lhs; statement != NULL; statement = statement->next)
    {
        if(statement->kind != AST_ASSIGN ||
           statement->lhs == NULL ||
           statement->lhs->kind != AST_EXPR_VARIABLE)
            continue;

        const char *binding_name = statement->lhs->identifier;
        if(statement->rhs != NULL && statement->rhs->kind == AST_EXPR_STRUCT)
        {
            if(findTypeInfoInScope(&(scope->type_scope), binding_name) < 0)
            {
                TypeInfo *type_info = declareTypeInfo(&(scope->type_scope), binding_name);
                type_info->data_type = newStructDataType(binding_name, NULL);
                type_info->predeclared = true;
            }
        }
        else if(statement->rhs != NULL && statement->rhs->kind == AST_EXPR_ENUM)
        {
            if(findTypeInfoInScope(&(scope->type_scope), binding_name) < 0)
            {
                TypeInfo *type_info = declareTypeInfo(&(scope->type_scope), binding_name);
                type_info->data_type = newEnumDataType(binding_name, NULL);
                type_info->predeclared = true;
            }
        }

        if(findVariableInfoInScope(&(scope->type_scope), binding_name) < 0)
        {
            VariableInfo *variable_info = declareVariableInfo(&(scope->type_scope), binding_name);
            variable_info->is_compile_time_constant = statement->modifier.is_compile_time_binding;
            variable_info->predeclared = true;
            if(isTypeDeclAssign(statement, &(scope->type_scope)))
                variable_info->data_type = newPrimaryDataType(AST_PRIMARY_DATA_TYPE_TYPE);
            else if(statement->data_type != NULL && isExplicitDeclared(statement))
                variable_info->data_type = cloneDataType(statement->data_type);
            else
                variable_info->data_type = newInferDataType();
            variable_info->operator_kind = statement->operator_kind;
            variable_info->value_expr = statement->rhs;
            if(statement->rhs != NULL && statement->rhs->kind == AST_EXPR_FUNCTION)
                variable_info->function_value = statement->rhs;
            if(statement->rhs != NULL &&
               statement->rhs->kind == AST_EXPR_BUILTIN &&
               strcmp(statement->rhs->identifier, "extern") == 0)
                variable_info->extern_value = statement->rhs;
        }
    }
}

static void mirResolveTopLevelTypes(MirLowerScope *scope, ASTNode *block_node)
{
    if(scope == NULL || block_node == NULL)
        return;

    for(ASTNode *statement = block_node->lhs; statement != NULL; statement = statement->next)
    {
        if(statement->kind != AST_ASSIGN ||
           statement->lhs == NULL ||
           statement->lhs->kind != AST_EXPR_VARIABLE)
            continue;

        const char *binding_name = statement->lhs->identifier;
        if(statement->rhs != NULL && statement->rhs->kind == AST_EXPR_STRUCT)
        {
            TypeInfo *type_info = findTypeInfo(&(scope->type_scope), binding_name);
            if(type_info != NULL && type_info->predeclared)
            {
                ASTDataType *struct_type = declareStructType(statement, &(scope->type_scope));
                semanticBindTypeDeclarationValue(statement, &(scope->type_scope), struct_type);
            }
            continue;
        }

        if(statement->rhs != NULL && statement->rhs->kind == AST_EXPR_ENUM)
        {
            TypeInfo *type_info = findTypeInfo(&(scope->type_scope), binding_name);
            if(type_info != NULL && type_info->predeclared)
            {
                ASTDataType *enum_type = declareEnumType(statement, &(scope->type_scope));
                semanticBindTypeDeclarationValue(statement, &(scope->type_scope), enum_type);
            }
            continue;
        }
    }
}

static void mirPredeclareTopLevelRuntimeBindings(MirLowering *lowering, MirLowerScope *scope, ASTNode *block_node)
{
    if(lowering == NULL || scope == NULL || block_node == NULL)
        return;

    for(ASTNode *statement = block_node->lhs; statement != NULL; statement = statement->next)
    {
        if(statement->kind != AST_ASSIGN ||
           statement->lhs == NULL ||
           statement->lhs->kind != AST_EXPR_VARIABLE)
            continue;

        const char *binding_name = statement->lhs->identifier;
        if(findMirRuntimeBindingInScope(scope, binding_name) != NULL)
            continue;

        ASTNode *resolved_function_value = resolveFunctionValueExpr(statement->rhs, &(scope->type_scope));
        ASTNode *extern_value = resolveExternValueExpr(statement->rhs, &(scope->type_scope));
        ASTDataType *declared_type = resolveNamedDataType(statement->data_type, &(scope->type_scope), scope->self_data_type);
        TypeSystemExprType expr_type = inferExprType(statement->rhs, &(scope->type_scope));

        if(resolved_function_value != NULL)
            continue;

        if(expr_type.kind == TYPE_SYSTEM_EXPR_TYPE_TYPE ||
           mirIsCompileTimeTypeFactory(declared_type) ||
           (statement->rhs->kind == AST_EXPR_BUILTIN &&
            strcmp(statement->rhs->identifier, "extern") == 0 &&
            declared_type->is_variadic))
        {
            MirRuntimeBinding *binding = declareMirRuntimeBinding(scope, binding_name);
            binding->is_compile_time_constant = true;
            binding->declared_data_type = cloneDataType(declared_type);
            binding->type_value = expr_type.kind == TYPE_SYSTEM_EXPR_TYPE_TYPE ? cloneDataType(expr_type.data_type) : NULL;
            binding->function_value = resolved_function_value;
            binding->extern_value = extern_value;
            binding->kind = MIR_RUNTIME_BINDING_COMPTIME_ONLY;
            continue;
        }

        if(declared_type->kind == AST_DATA_TYPE_KIND_REFERENCE)
            continue;

        MirRuntimeBinding *binding = declareMirRuntimeBinding(scope, binding_name);
        binding->is_compile_time_constant = statement->modifier.is_compile_time_binding;
        binding->declared_data_type = cloneDataType(declared_type);
        binding->function_value = resolved_function_value;
        binding->extern_value = extern_value;
        binding->kind = MIR_RUNTIME_BINDING_GLOBAL_SLOT;
        strcpy(binding->global_name, binding_name);
        mirEnsureGlobal(lowering, binding_name, declared_type, false);
    }
}

#endif /* MIR_LOWERING_RUNTIME_H */
