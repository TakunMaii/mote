#ifndef MODULE_REWRITE_H
#define MODULE_REWRITE_H

#include "ModuleImport.h"

static void initRewriteScope(RewriteScope *scope, RewriteScope *parent)
{
    memset(scope, 0, sizeof(RewriteScope));
    scope->parent = parent;
}

static void freeRewriteScopeStorage(RewriteScope *scope)
{
    if(scope == NULL)
        return;
    free(scope->value_bindings);
    free(scope->type_bindings);
    scope->value_bindings = NULL;
    scope->type_bindings = NULL;
    scope->value_count = 0;
    scope->type_count = 0;
    scope->value_capacity = 0;
    scope->type_capacity = 0;
}

static RewriteValueBinding* appendRewriteValueBinding(RewriteScope *scope)
{
    if(scope->value_count >= scope->value_capacity)
        scope->value_bindings = (RewriteValueBinding*) moduleGrowItems(scope->value_bindings, sizeof(RewriteValueBinding),
                                                                       &(scope->value_capacity), scope->value_count + 1,
                                                                       "rewrite value binding allocation failed");
    RewriteValueBinding *binding = &(scope->value_bindings[scope->value_count++]);
    memset(binding, 0, sizeof(RewriteValueBinding));
    return binding;
}

static RewriteTypeBinding* appendRewriteTypeBinding(RewriteScope *scope)
{
    if(scope->type_count >= scope->type_capacity)
        scope->type_bindings = (RewriteTypeBinding*) moduleGrowItems(scope->type_bindings, sizeof(RewriteTypeBinding),
                                                                     &(scope->type_capacity), scope->type_count + 1,
                                                                     "rewrite type binding allocation failed");
    RewriteTypeBinding *binding = &(scope->type_bindings[scope->type_count++]);
    memset(binding, 0, sizeof(RewriteTypeBinding));
    return binding;
}

static RewriteValueBinding* findRewriteValueBindingInScope(RewriteScope *scope, const char *identifier)
{
    for(int i = 0; i < scope->value_count; i++)
    {
        if(strcmp(scope->value_bindings[i].original, identifier) == 0)
            return &(scope->value_bindings[i]);
    }
    return NULL;
}

static RewriteValueBinding* findRewriteValueBinding(RewriteScope *scope, const char *identifier)
{
    RewriteScope *current = scope;
    while(current)
    {
        RewriteValueBinding *binding = findRewriteValueBindingInScope(current, identifier);
        if(binding != NULL)
            return binding;
        current = current->parent;
    }
    return NULL;
}

static RewriteTypeBinding* findRewriteTypeBindingInScope(RewriteScope *scope, const char *identifier)
{
    for(int i = 0; i < scope->type_count; i++)
    {
        if(strcmp(scope->type_bindings[i].original, identifier) == 0)
            return &(scope->type_bindings[i]);
    }
    return NULL;
}

static RewriteTypeBinding* findRewriteTypeBinding(RewriteScope *scope, const char *identifier)
{
    RewriteScope *current = scope;
    while(current)
    {
        RewriteTypeBinding *binding = findRewriteTypeBindingInScope(current, identifier);
        if(binding != NULL)
            return binding;
        current = current->parent;
    }
    return NULL;
}

static void declareRewriteValueBinding(RewriteScope *scope, const char *original, const char *rewritten)
{
    RewriteValueBinding *binding = appendRewriteValueBinding(scope);
    strcpy(binding->original, original);
    strcpy(binding->rewritten, rewritten);
}

static void declareRewriteImportBinding(RewriteScope *scope, const char *alias, ModuleSourceFile *imported_module)
{
    RewriteValueBinding *binding = appendRewriteValueBinding(scope);
    strcpy(binding->original, alias);
    binding->is_import_alias = true;
    binding->imported_module = imported_module;
}

static void declareModuleImportsInRewriteScope(RewriteScope *scope, ModuleSourceFile *module)
{
    if(scope == NULL || module == NULL)
        return;

    for(int i = 0; i < module->import_count; i++)
    {
        RewriteValueBinding *existing = findRewriteValueBindingInScope(scope, module->imports[i].alias);
        if(existing != NULL)
            continue;
        declareRewriteImportBinding(scope, module->imports[i].alias, module->imports[i].module);
    }
}

static void declareRewriteTypedValueBinding(RewriteScope *scope, const char *original, const char *rewritten)
{
    declareRewriteValueBinding(scope, original, rewritten);
    scope->value_bindings[scope->value_count - 1].is_type_binding = true;
}

static void declareRewriteTypeBinding(RewriteScope *scope, const char *original, const char *rewritten)
{
    RewriteTypeBinding *binding = appendRewriteTypeBinding(scope);
    strcpy(binding->original, original);
    strcpy(binding->rewritten, rewritten);
}

static bool moduleDataTypeIsPrimaryTypeKeyword(ASTDataType *data_type)
{
    return data_type != NULL &&
           data_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
           data_type->primary == AST_PRIMARY_DATA_TYPE_TYPE;
}

static bool moduleFindTypeAliasBinding(RewriteScope *scope, const char *identifier)
{
    RewriteValueBinding *value_binding = findRewriteValueBinding(scope, identifier);
    return identifier != NULL &&
           (builtinIdentifierToDataType(identifier) != NULL ||
            strcmp(identifier, "Self") == 0 ||
            findRewriteTypeBinding(scope, identifier) != NULL ||
            (value_binding != NULL && value_binding->is_type_binding));
}

static bool rewriteExprLooksLikeTypeValue(ModuleSourceFile *module, RewriteScope *scope, ASTNode *node)
{
    if(node == NULL)
        return false;

    switch(node->kind)
    {
        case AST_EXPR_TYPE_LITERAL:
        case AST_EXPR_STRUCT:
        case AST_EXPR_ENUM:
            return true;
        case AST_EXPR_FUNCTION:
            return node->return_data_type != NULL &&
                   moduleDataTypeIsPrimaryTypeKeyword(node->return_data_type);
        case AST_EXPR_PARENTHESIS:
            return rewriteExprLooksLikeTypeValue(module, scope, node->lhs);
        case AST_EXPR_DEREF:
        case AST_EXPR_ADDRESS_OF:
        case AST_EXPR_ADDRESS_OF_MUT:
            return rewriteExprLooksLikeTypeValue(module, scope, node->lhs);
        case AST_EXPR_VARIABLE:
            return moduleFindTypeAliasBinding(scope, node->identifier);
        case AST_EXPR_MEMBER:
            if(node->lhs != NULL && node->lhs->kind == AST_EXPR_VARIABLE)
            {
                RewriteValueBinding *import_binding = findRewriteValueBinding(scope, node->lhs->identifier);
                if(import_binding != NULL && import_binding->is_import_alias)
                {
                    ModuleTopLevelBinding *binding = moduleFindTopLevelBinding(import_binding->imported_module, node->identifier);
                    return binding != NULL && binding->is_pub && binding->is_type_decl;
                }
            }
            return false;
        default:
            return false;
    }
}

static void rewriteDataType(ModuleSourceFile *module, RewriteScope *scope, ASTDataType *data_type);
static void rewriteExpr(ModuleSourceFile *module, RewriteScope *scope, ASTNode *node);
static void rewriteStatement(ModuleSourceFile *module, RewriteScope *scope, ASTNode *node);

static void rewriteExprList(ModuleSourceFile *module, RewriteScope *scope, ASTNode *node)
{
    while(node)
    {
        rewriteExpr(module, scope, node);
        node = node->next;
    }
}

static void rewriteNamedDataTypeIdentifier(ModuleSourceFile *module, RewriteScope *scope, ASTDataType *data_type)
{
    (void) module;
    char *dot = strchr(data_type->identifier, '.');
    if(dot != NULL)
    {
        char alias[MAX_IDENTIFIER_LENGTH] = {0};
        char member[MAX_IDENTIFIER_LENGTH] = {0};
        size_t alias_length = (size_t)(dot - data_type->identifier);
        memcpy(alias, data_type->identifier, alias_length);
        alias[alias_length] = '\0';
        strcpy(member, dot + 1);

        if(strchr(member, '.') != NULL)
            moduleSystemError("nested qualified imported type names are not supported yet",
                              NULL, 0, 0);

        RewriteValueBinding *import_binding = findRewriteValueBinding(scope, alias);
        if(import_binding == NULL || !import_binding->is_import_alias)
            moduleSystemError("qualified type name requires an imported module alias",
                              NULL, 0, 0);

        ModuleTopLevelBinding *binding = moduleFindTopLevelBinding(import_binding->imported_module, member);
        if(binding == NULL || !binding->is_pub)
            moduleSystemError("imported module does not export the requested type name",
                              NULL, 0, 0);

        strcpy(data_type->identifier, binding->mangled);
        return;
    }

    RewriteTypeBinding *type_binding = findRewriteTypeBinding(scope, data_type->identifier);
    if(type_binding != NULL)
    {
        strcpy(data_type->identifier, type_binding->rewritten);
        return;
    }

    RewriteValueBinding *value_binding = findRewriteValueBinding(scope, data_type->identifier);
    if(value_binding != NULL)
    {
        if(value_binding->is_import_alias)
            moduleSystemError("a module alias cannot be used as a type without selecting a member",
                              NULL, 0, 0);
        strcpy(data_type->identifier, value_binding->rewritten);
    }
}

static void rewriteStructMembers(ModuleSourceFile *module, RewriteScope *scope, ASTStructMember *member)
{
    while(member)
    {
        rewriteDataType(module, scope, member->data_type);
        if(member->value != NULL)
            rewriteExpr(module, scope, member->value);
        member = member->next;
    }
}

static void rewriteFunctionExpr(ModuleSourceFile *module, RewriteScope *scope, ASTNode *node)
{
    RewriteScope *function_scope = (RewriteScope*) malloc(sizeof(RewriteScope));
    initRewriteScope(function_scope, scope);

    if(findRewriteTypeBinding(scope, "Self") != NULL)
        declareRewriteTypeBinding(function_scope, "Self", "Self");

    ASTFunctionCapture *capture = node->captures;
    while(capture)
    {
        declareRewriteValueBinding(function_scope, capture->identifier, capture->identifier);
        if(findRewriteTypeBinding(scope, capture->identifier) != NULL)
            declareRewriteTypeBinding(function_scope, capture->identifier, capture->identifier);
        capture = capture->next;
    }

    ASTFunctionParameter *parameter = node->parameters;
    while(parameter)
    {
        rewriteDataType(module, function_scope, parameter->data_type);
        declareRewriteValueBinding(function_scope, parameter->identifier, parameter->identifier);
        if(moduleDataTypeIsPrimaryTypeKeyword(parameter->data_type))
            declareRewriteTypeBinding(function_scope, parameter->identifier, parameter->identifier);
        parameter = parameter->next;
    }

    rewriteDataType(module, function_scope, node->return_data_type);
    rewriteStatement(module, function_scope, node->body);
    freeRewriteScopeStorage(function_scope);
    free(function_scope);
}

static void rewriteDataType(ModuleSourceFile *module, RewriteScope *scope, ASTDataType *data_type)
{
    if(data_type == NULL)
        return;

    switch(data_type->kind)
    {
        case AST_DATA_TYPE_KIND_NAMED:
            rewriteNamedDataTypeIdentifier(module, scope, data_type);
            return;
        case AST_DATA_TYPE_KIND_POINTER:
        case AST_DATA_TYPE_KIND_REFERENCE:
        case AST_DATA_TYPE_KIND_OPTIONAL:
        case AST_DATA_TYPE_KIND_SLICE:
            rewriteDataType(module, scope, data_type->child);
            return;
        case AST_DATA_TYPE_KIND_FUNCTION: {
            ASTFunctionParameter *parameter = data_type->parameters;
            while(parameter)
            {
                rewriteDataType(module, scope, parameter->data_type);
                parameter = parameter->next;
            }
            rewriteDataType(module, scope, data_type->return_data_type);
            return;
        }
        case AST_DATA_TYPE_KIND_ARRAY:
            rewriteDataType(module, scope, data_type->child);
            return;
        case AST_DATA_TYPE_KIND_APPLY:
            rewriteDataType(module, scope, data_type->callee);
            for(ASTTypeArgument *argument = data_type->arguments; argument != NULL; argument = argument->next)
                rewriteDataType(module, scope, argument->data_type);
            return;
        case AST_DATA_TYPE_KIND_STRUCT:
            rewriteStructMembers(module, scope, data_type->members);
            return;
        default:
            return;
    }
}

static void rewriteExpr(ModuleSourceFile *module, RewriteScope *scope, ASTNode *node)
{
    if(node == NULL)
        return;

    switch(node->kind)
    {
        case AST_EXPR_VARIABLE: {
            RewriteValueBinding *value_binding = findRewriteValueBinding(scope, node->identifier);
            if(value_binding != NULL)
            {
                if(value_binding->is_import_alias)
                    moduleSystemError("module values can only be used through member access",
                                      node->filename, node->line_number, node->column_number);
                if(value_binding->is_type_binding)
                {
                    RewriteTypeBinding *type_binding = findRewriteTypeBinding(scope, node->identifier);
                    if(type_binding != NULL)
                    {
                        strcpy(node->identifier, type_binding->rewritten);
                        return;
                    }
                }
                strcpy(node->identifier, value_binding->rewritten);
                return;
            }

            RewriteTypeBinding *type_binding = findRewriteTypeBinding(scope, node->identifier);
            if(type_binding != NULL)
                strcpy(node->identifier, type_binding->rewritten);
            return;
        }
        case AST_EXPR_BUILTIN:
            if(strcmp(node->identifier, "import") == 0)
                moduleSystemError("module imports must be used through member access or import bindings",
                                  node->filename, node->line_number, node->column_number);
            rewriteExprList(module, scope, node->lhs);
            return;
        case AST_EXPR_MEMBER: {
            if(node->lhs != NULL && node->lhs->kind == AST_EXPR_VARIABLE)
            {
                RewriteValueBinding *import_binding = findRewriteValueBinding(scope, node->lhs->identifier);
                if(import_binding != NULL && import_binding->is_import_alias)
                {
                    ModuleTopLevelBinding *binding = moduleFindTopLevelBinding(import_binding->imported_module, node->identifier);
                    if(binding == NULL || !binding->is_pub)
                        moduleSystemError("imported module does not export the requested member",
                                          node->filename, node->line_number, node->column_number);

                    node->kind = AST_EXPR_VARIABLE;
                    node->lhs = NULL;
                    node->rhs = NULL;
                    node->extra = NULL;
                    node->body = NULL;
                    strcpy(node->identifier, binding->mangled);
                    return;
                }
            }
            else if(node->lhs != NULL && node->lhs->kind == AST_EXPR_BUILTIN &&
                    strcmp(node->lhs->identifier, "import") == 0)
            {
                moduleSystemError("direct member access on @import(...) is not allowed; bind the import to a name first",
                                  node->filename, node->line_number, node->column_number);
            }

            rewriteExpr(module, scope, node->lhs);
            return;
        }
        case AST_EXPR_FUNCTION:
            rewriteFunctionExpr(module, scope, node);
            return;
        case AST_EXPR_STRUCT:
            rewriteStructMembers(module, scope, node->members);
            return;
        case AST_EXPR_STRUCT_LITERAL:
            rewriteExpr(module, scope, node->lhs);
            for(ASTStructLiteralField *field = node->struct_literal_fields; field != NULL; field = field->next)
                rewriteExpr(module, scope, field->value);
            return;
        case AST_EXPR_ARRAY_LITERAL:
            rewriteExprList(module, scope, node->lhs);
            return;
        case AST_EXPR_CALL:
            rewriteExpr(module, scope, node->lhs);
            rewriteExprList(module, scope, node->rhs);
            return;
        case AST_EXPR_LOGICAL_OR:
        case AST_EXPR_LOGICAL_AND:
        case AST_EXPR_BIT_OR:
        case AST_EXPR_BIT_XOR:
        case AST_EXPR_BIT_AND:
        case AST_EXPR_EQUAL:
        case AST_EXPR_NOT_EQUAL:
        case AST_EXPR_LESS:
        case AST_EXPR_LESS_EQUAL:
        case AST_EXPR_GREATER:
        case AST_EXPR_GREATER_EQUAL:
        case AST_EXPR_SHIFT_LEFT:
        case AST_EXPR_SHIFT_RIGHT:
        case AST_EXPR_MUL:
        case AST_EXPR_DIV:
        case AST_EXPR_MOD:
        case AST_EXPR_ADD:
        case AST_EXPR_SUB:
        case AST_EXPR_UNARY_PLUS:
        case AST_EXPR_UNARY_MINUS:
        case AST_EXPR_UNARY_LOGICAL_NOT:
        case AST_EXPR_UNARY_BIT_NOT:
        case AST_EXPR_ADDRESS_OF:
        case AST_EXPR_ADDRESS_OF_MUT:
        case AST_EXPR_DEREF:
        case AST_EXPR_PARENTHESIS:
        case AST_EXPR_INDEX:
        case AST_STATEMENT_EXPR:
        case AST_STATEMENT_RETURN:
            rewriteExpr(module, scope, node->lhs);
            rewriteExpr(module, scope, node->rhs);
            rewriteExpr(module, scope, node->extra);
            return;
        case AST_EXPR_TYPE_LITERAL:
            rewriteDataType(module, scope, node->data_type);
            return;
        default:
            return;
    }
}

static bool rewriteWouldDeclareNewVariable(RewriteScope *scope, ASTNode *node)
{
    if(node == NULL || node->kind != AST_ASSIGN || node->lhs == NULL || node->lhs->kind == AST_EXPR_VARIABLE)
        return false;

    if(node->modifier.is_runtime_binding || node->modifier.is_compile_time_binding)
        return findRewriteValueBindingInScope(scope, node->identifier) == NULL;

    return findRewriteValueBinding(scope, node->identifier) == NULL;
}

static void rewriteStatementList(ModuleSourceFile *module, RewriteScope *scope, ASTNode *statement)
{
    ASTNode *predeclare = statement;
    while(predeclare)
    {
        if(predeclare->kind == AST_ASSIGN &&
           predeclare->lhs != NULL &&
           predeclare->lhs->kind == AST_EXPR_VARIABLE)
        {
            ModuleTopLevelBinding *top_level_binding = moduleFindTopLevelBinding(module, predeclare->identifier);
            const char *rewritten = predeclare->identifier;
            if(top_level_binding != NULL && scope->parent == NULL)
                rewritten = top_level_binding->mangled;

            if(moduleIsStructDeclAssign(predeclare) ||
               moduleIsEnumDeclAssign(predeclare) ||
               rewriteExprLooksLikeTypeValue(module, scope, predeclare->rhs))
            {
                if(findRewriteTypeBindingInScope(scope, predeclare->identifier) == NULL)
                    declareRewriteTypeBinding(scope, predeclare->identifier, rewritten);
            }

            if(scope->parent == NULL &&
               !moduleIsImportDecl(predeclare) &&
               findRewriteValueBindingInScope(scope, predeclare->identifier) == NULL)
            {
                declareRewriteValueBinding(scope, predeclare->identifier, rewritten);
            }
        }
        predeclare = predeclare->next;
    }

    while(statement)
    {
        rewriteStatement(module, scope, statement);
        statement = statement->next;
    }
}

static void rewriteStatement(ModuleSourceFile *module, RewriteScope *scope, ASTNode *node)
{
    if(node == NULL)
        return;

    switch(node->kind)
    {
        case AST_BLOCK: {
            RewriteScope *block_scope = (RewriteScope*) malloc(sizeof(RewriteScope));
            initRewriteScope(block_scope, scope);
            rewriteStatementList(module, block_scope, node->lhs);
            freeRewriteScopeStorage(block_scope);
            free(block_scope);
            return;
        }
        case AST_ASSIGN: {
            if(moduleIsImportDecl(node))
            {
                RewriteValueBinding *existing = findRewriteValueBindingInScope(scope, node->identifier);
                if(existing != NULL && existing->is_import_alias)
                    return;

                for(int i = 0; i < module->import_count; i++)
                {
                    if(strcmp(module->imports[i].alias, node->identifier) == 0)
                    {
                        declareRewriteImportBinding(scope, node->identifier, module->imports[i].module);
                        return;
                    }
                }
                moduleSystemError("internal import binding resolution failed",
                                  node->filename, node->line_number, node->column_number);
            }

            ModuleTopLevelBinding *top_level_binding = moduleFindTopLevelBinding(module, node->identifier);
            bool is_top_level_binding = top_level_binding != NULL && scope->parent == NULL;

            if(moduleIsStructDeclAssign(node) || moduleIsEnumDeclAssign(node) ||
               rewriteExprLooksLikeTypeValue(module, scope, node->rhs))
            {
                char original_identifier[MAX_IDENTIFIER_LENGTH] = {0};
                strcpy(original_identifier, node->identifier);
                if(is_top_level_binding)
                {
                    strcpy(node->identifier, top_level_binding->mangled);
                    strcpy(node->lhs->identifier, top_level_binding->mangled);
                }

                const char *binding_name = is_top_level_binding ? top_level_binding->original : original_identifier;
                if(findRewriteTypeBindingInScope(scope, binding_name) == NULL)
                    declareRewriteTypeBinding(scope, binding_name, node->lhs->identifier);
                rewriteExpr(module, scope, node->rhs);
                return;
            }

            rewriteDataType(module, scope, node->data_type);
            rewriteExpr(module, scope, node->rhs);

            if(node->lhs->kind == AST_EXPR_VARIABLE)
            {
                RewriteValueBinding *existing = findRewriteValueBinding(scope, node->identifier);
                if(existing != NULL && !rewriteWouldDeclareNewVariable(scope, node))
                {
                    if(existing->is_import_alias)
                        moduleSystemError("cannot assign to an imported module alias",
                                          node->filename, node->line_number, node->column_number);
                    strcpy(node->identifier, existing->rewritten);
                    strcpy(node->lhs->identifier, existing->rewritten);
                    return;
                }

                if(is_top_level_binding)
                {
                    strcpy(node->identifier, top_level_binding->mangled);
                    strcpy(node->lhs->identifier, top_level_binding->mangled);
                    if(rewriteExprLooksLikeTypeValue(module, scope, node->rhs))
                        declareRewriteTypedValueBinding(scope, top_level_binding->original, top_level_binding->mangled);
                    else
                        declareRewriteValueBinding(scope, top_level_binding->original, top_level_binding->mangled);
                    if(rewriteExprLooksLikeTypeValue(module, scope, node->rhs))
                        declareRewriteTypeBinding(scope, top_level_binding->original, top_level_binding->mangled);
                }
                else if(rewriteWouldDeclareNewVariable(scope, node))
                {
                    if(rewriteExprLooksLikeTypeValue(module, scope, node->rhs))
                    {
                        declareRewriteTypedValueBinding(scope, node->identifier, node->identifier);
                        declareRewriteTypeBinding(scope, node->identifier, node->identifier);
                    }
                    else
                        declareRewriteValueBinding(scope, node->identifier, node->identifier);
                }
                return;
            }

            rewriteExpr(module, scope, node->lhs);
            return;
        }
        case AST_STATEMENT_RETURN:
            rewriteExpr(module, scope, node->lhs);
            return;
        case AST_STATEMENT_EXPR:
            rewriteExpr(module, scope, node->lhs);
            return;
        case AST_STATEMENT_IF:
            rewriteExpr(module, scope, node->lhs);
            rewriteStatement(module, scope, node->rhs);
            rewriteStatement(module, scope, node->body);
            return;
        case AST_STATEMENT_WHILE:
        case AST_STATEMENT_DO_WHILE:
            rewriteExpr(module, scope, node->lhs);
            rewriteStatement(module, scope, node->body);
            return;
        case AST_STATEMENT_FOR: {
            RewriteScope *for_scope = (RewriteScope*) malloc(sizeof(RewriteScope));
            initRewriteScope(for_scope, scope);
            rewriteStatement(module, for_scope, node->lhs);
            rewriteExpr(module, for_scope, node->rhs);
            rewriteStatement(module, for_scope, node->body);
            rewriteStatement(module, for_scope, node->extra);
            freeRewriteScopeStorage(for_scope);
            free(for_scope);
            return;
        }
        case AST_STATEMENT_DEFER:
            rewriteStatement(module, scope, node->lhs);
            return;
        default:
            return;
    }
}

static void moduleRewrite(ModuleSourceFile *module)
{
    if(module->rewritten)
        return;

    RewriteScope *top_scope = (RewriteScope*) malloc(sizeof(RewriteScope));
    initRewriteScope(top_scope, NULL);
    declareModuleImportsInRewriteScope(top_scope, module);
    rewriteStatementList(module, top_scope, moduleStatements(module->ast_root));
    freeRewriteScopeStorage(top_scope);
    free(top_scope);
    module->rewritten = true;
}

#endif /* MODULE_REWRITE_H */
