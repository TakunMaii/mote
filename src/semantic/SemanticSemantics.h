#ifndef SEMANTIC_SEMANTICS_H
#define SEMANTIC_SEMANTICS_H

#include "SemanticShared.h"

void declareFunctionParameters(ASTFunctionParameter *parameter, ScopeFrame *scope, ASTDataType *self_data_type)
{
    while(parameter)
    {
        if(findVariableInfoInScope(scope, parameter->identifier) >= 0)
        {
            Diagnostic diagnostic = diagnosticMake(DIAGNOSTIC_SEVERITY_ERROR,
                                                   "S1001",
                                                   astFunctionParameterSourceSpan(parameter),
                                                   "duplicate function parameter");
            diagnosticSetPrimaryLabel(&diagnostic, "parameter `%s` is declared more than once",
                                      astUserFacingIdentifier(parameter->identifier));
            diagnosticAbort(diagnostic);
        }

        VariableInfo *variable_info = declareVariableInfo(scope, parameter->identifier);
        variable_info->is_compile_time_constant = false;
        variable_info->data_type = resolveNamedDataType(parameter->data_type, scope, self_data_type);
        if(parameter->data_type != NULL &&
           parameter->data_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
           parameter->data_type->primary == AST_PRIMARY_DATA_TYPE_TYPE)
        {
            variable_info->type_value = newNamedDataType(parameter->identifier);
        }
        parameter = parameter->next;
    }
}

void declareFunctionCaptures(ASTFunctionCapture *capture, ScopeFrame *target_scope, ScopeFrame *source_scope)
{
    while(capture)
    {
        if(findVariableInfoInScope(target_scope, capture->identifier) >= 0)
        {
            diagnosticAbortFormatted("S1002",
                                     makeSourceSpan(capture->filename,
                                                    capture->line_number, capture->column_number,
                                                    capture->end_line_number, capture->end_column_number),
                                     "duplicate capture",
                                     "function capture `%s` is declared more than once",
                                     astUserFacingIdentifier(capture->identifier));
        }

        VariableInfo *outer_variable = findVariableInfo(source_scope, capture->identifier);
        if(outer_variable == NULL)
        {
            diagnosticAbortFormatted("S1003",
                                     makeSourceSpan(capture->filename,
                                                    capture->line_number, capture->column_number,
                                                    capture->end_line_number, capture->end_column_number),
                                     "unknown capture",
                                     "unknown function capture `%s`",
                                     astUserFacingIdentifier(capture->identifier));
        }

        VariableInfo *variable_info = declareVariableInfo(target_scope, capture->identifier);
        variable_info->is_compile_time_constant = false;
        variable_info->data_type = cloneDataType(outer_variable->data_type);
        variable_info->type_value = cloneDataType(outer_variable->type_value);
        variable_info->function_value = outer_variable->function_value;
        variable_info->extern_value = outer_variable->extern_value;
        capture = capture->next;
    }
}

void checkFunctionExprSemantics(ASTNode *node, ScopeFrame *scope, ASTDataType *self_data_type)
{
    if(node->is_variadic)
    {
        diagnosticAbortSimple("S1004",
                              "variadic mote function definitions are not supported yet",
                              astNodeSourceSpan(node),
                              "function definition appears here");
    }

    ScopeFrame *function_scope = newScopeFrame(scope);
    declareFunctionParameters(node->parameters, function_scope, self_data_type);
    declareFunctionCaptures(node->captures, function_scope, scope);
    if(self_data_type != NULL)
    {
        VariableInfo *self_variable = declareVariableInfo(function_scope, "Self");
        self_variable->is_compile_time_constant = true;
        self_variable->data_type = newPrimaryDataType(AST_PRIMARY_DATA_TYPE_TYPE);
        self_variable->type_value = cloneDataType(self_data_type);
    }

    FunctionContext function_context = {0};
    function_context.active = true;
    function_context.return_data_type = node->return_data_type;
    function_context.self_data_type = self_data_type;
    function_context.self_available_as_type_value = self_data_type != NULL;
    function_context.loop_depth = 0;
    function_context.inside_defer = false;

    checkAssignSemanticsInBlock(node->body, function_scope, &function_context);
    deleteScopeFrame(function_scope);
}

void checkStructExprSemantics(ASTNode *node, ScopeFrame *scope, ASTDataType *self_data_type)
{
    ASTStructMember *member = node->members;
    while(member)
    {
        if(member->value)
            checkFunctionExprSemantics(member->value, scope, self_data_type);
        member = member->next;
    }
}

void checkExprDeclaredVariable(ASTNode *node, ScopeFrame *scope)
{
    if(node == NULL)
        return;

    if(node->kind == AST_EXPR_VARIABLE)
    {
        if(strcmp(node->identifier, "Self") == 0)
            return;
        if(builtinIdentifierToDataType(node->identifier) != NULL)
            return;

        if(findVariableInfo(scope, node->identifier) == NULL && findTypeInfo(scope, node->identifier) == NULL)
        {
            diagnosticAbortFormatted("S1005",
                                     astNodeSourceSpan(node),
                                     "unknown name",
                                     "use of undeclared variable `%s` in expression",
                                     astUserFacingIdentifier(node->identifier));
        }
        return;
    }

    if(node->kind == AST_EXPR_FUNCTION)
    {
        checkFunctionExprSemantics(node, scope, NULL);
        return;
    }

    if(node->kind == AST_EXPR_STRUCT)
    {
        return;
    }

    if(node->kind == AST_EXPR_STRUCT_LITERAL)
    {
        checkExprDeclaredVariable(node->lhs, scope);
        ASTStructLiteralField *field = node->struct_literal_fields;
        while(field)
        {
            checkExprDeclaredVariable(field->value, scope);
            field = field->next;
        }
        return;
    }

    if(node->kind == AST_EXPR_ARRAY_LITERAL)
    {
        ASTNode *element = node->lhs;
        while(element)
        {
            checkExprDeclaredVariable(element, scope);
            element = element->next;
        }
        return;
    }

    if(node->kind == AST_EXPR_LITERAL_STRING)
        return;

    checkExprDeclaredVariable(node->lhs, scope);
    checkExprDeclaredVariable(node->rhs, scope);
}

void checkDeferredAssignmentSemantics(ASTNode *node, ScopeFrame *scope)
{
    if(node == NULL || node->kind != AST_ASSIGN)
        return;

    if(isTypeDeclAssign(node, scope))
    {
        semanticAbortNode("S1006", node,
                          "defer cannot declare a type",
                          "move the type declaration outside `defer`");
    }

    if(node->lhs->kind != AST_EXPR_VARIABLE)
        return;

    if(isExplicitDeclared(node))
    {
        semanticAbortNode("S1007", node,
                          "defer cannot declare a variable",
                          "declare the variable before the `defer` statement");
    }

    if(findVariableInfo(scope, semanticAssignIdentifier(node)) == NULL)
    {
        semanticAbortNode("S1008", node,
                          "defer assignment target must already exist",
                          "assignment target is not declared in an outer scope");
    }
}

void checkAssignSemanticsNode(ASTNode *node, ScopeFrame *scope)
{
    bool is_top_level_scope = scope != NULL && scope->parent == NULL;

    if(node->operator_kind != AST_OPERATOR_NONE)
    {
        if(node->lhs == NULL || node->lhs->kind != AST_EXPR_VARIABLE)
        {
            semanticAbortNode("S1006", node,
                              "@operator can only annotate named function declarations",
                              "apply @operator to a `name: fn(...) ...` declaration");
        }

        if(node->rhs == NULL || node->rhs->kind != AST_EXPR_FUNCTION)
        {
            semanticAbortNode("S1007", node,
                              "@operator requires a function value",
                              "annotate a function declaration like `name: fn(...) ...`");
        }
    }

    if(isTypeDeclAssign(node, scope))
    {
        const char *binding_name = semanticAssignIdentifier(node);
        TypeInfo *existing_type_info = findTypeInfo(scope, binding_name);
        if(existing_type_info != NULL)
        {
            bool is_placeholder_struct = existing_type_info->predeclared &&
                                         node->rhs != NULL &&
                                         node->rhs->kind == AST_EXPR_STRUCT;
            bool is_placeholder_enum = existing_type_info->predeclared &&
                                       node->rhs != NULL &&
                                       node->rhs->kind == AST_EXPR_ENUM;
            bool is_resolved_top_level_type = is_top_level_scope &&
                                              !existing_type_info->predeclared &&
                                              existing_type_info->data_type != NULL;
            if(!is_placeholder_struct && !is_placeholder_enum && !is_resolved_top_level_type)
            {
                semanticAbortNodeFormatted("S1009", node,
                                           "duplicate type declaration",
                                           "type `%s` has already been declared in this scope",
                                           astUserFacingIdentifier(binding_name));
            }
        }

        TypeInfo *type_info = existing_type_info;
        if(type_info == NULL)
            type_info = declareTypeInfo(scope, binding_name);
        if(!(is_top_level_scope && type_info->data_type != NULL && !type_info->predeclared))
        {
            TypeSystemExprType expr_type = inferExprType(node->rhs, scope);
            type_info->data_type = cloneDataType(expr_type.data_type);
            type_info->predeclared = false;
        }
        node->data_type = cloneDataType(type_info->data_type);
        if(node->rhs->kind == AST_EXPR_STRUCT)
            checkStructExprSemantics(node->rhs, scope, node->data_type);
        return;
    }

    checkExprDeclaredVariable(node->rhs, scope);

    if(node->operator_kind != AST_OPERATOR_NONE)
    {
        ASTNode *function_expr = node->rhs;
        if(semanticFunctionHasTypeParameters(function_expr->parameters))
        {
            semanticAbortNode("S1008", node,
                              "@operator does not support generic functions yet",
                              "remove type parameters from this operator function");
        }
    }

    if(node->lhs->kind == AST_EXPR_DEREF)
    {
        checkExprDeclaredVariable(node->lhs->lhs, scope);
        return;
    }

    if(node->lhs->kind == AST_EXPR_MEMBER)
    {
        checkExprDeclaredVariable(node->lhs->lhs, scope);
        return;
    }

    if(node->lhs->kind == AST_EXPR_INDEX)
    {
        checkExprDeclaredVariable(node->lhs->lhs, scope);
        checkExprDeclaredVariable(node->lhs->rhs, scope);
        return;
    }

    if(node->lhs->kind != AST_EXPR_VARIABLE)
    {
        semanticAbortNode("S1012", node,
                          "invalid assignment target",
                          "only variables, dereferences, member access, and indexing can be assigned");
    }

    VariableInfo *local_variable_info = NULL;
    const char *binding_name = semanticAssignIdentifier(node);
    int local_index = findVariableInfoInScope(scope, binding_name);
    if(local_index >= 0)
        local_variable_info = &(scope->variable_infos[local_index]);

    if(isExplicitDeclared(node))
    {
        if(local_variable_info != NULL && (!is_top_level_scope || !local_variable_info->predeclared))
        {
            semanticAbortNodeFormatted("S1013", node,
                                       "duplicate variable declaration",
                                       "variable `%s` has already been declared and cannot be declared again",
                                       astUserFacingIdentifier(binding_name));
        }
        if(local_variable_info == NULL)
        {
            VariableInfo *new_variable_info = declareVariableInfo(scope, binding_name);
            new_variable_info->is_compile_time_constant = node->modifier.is_compile_time_binding;
            new_variable_info->data_type = newInferDataType();
            new_variable_info->operator_kind = node->operator_kind;
        }
        else
        {
            local_variable_info->is_compile_time_constant = node->modifier.is_compile_time_binding;
            local_variable_info->operator_kind = node->operator_kind;
            local_variable_info->predeclared = false;
        }
    }
    else
    {
        VariableInfo *resolved_variable_info = local_variable_info;
        if(resolved_variable_info == NULL)
            resolved_variable_info = findVariableInfo(scope->parent, binding_name);

        if(resolved_variable_info == NULL || (is_top_level_scope && resolved_variable_info->predeclared))
        {
            semanticAbortNodeFormatted("S1014", node,
                                       "assignment target must already exist",
                                       "use `:=`, `: T =`, `::`, or `: T :` to declare `%s`",
                                       astUserFacingIdentifier(binding_name));
        }
        else if(resolved_variable_info->is_compile_time_constant)
        {
            semanticAbortNodeFormatted("S1014", node,
                                       "compile-time constant cannot be assigned",
                                       "cannot assign to compile-time constant `%s`",
                                       astUserFacingIdentifier(binding_name));
        }
    }
}

void checkStatementSemantics(ASTNode *node, ScopeFrame *scope, FunctionContext *function_context)
{
    if(node == NULL)
        return;

    if(node->kind == AST_BLOCK)
    {
        checkAssignSemanticsInBlock(node, scope, function_context);
        return;
    }

    if(node->kind == AST_STATEMENT_RETURN)
    {
        if(!isInsideFunction(function_context))
        {
            semanticAbortNode("S1015", node,
                              "return statement is only allowed inside a function",
                              "remove this `return` or move it into a function body");
        }

        if(function_context->inside_defer)
        {
            semanticAbortNode("S1016", node,
                              "return statement is not allowed inside defer",
                              "`defer` bodies cannot exit the surrounding function");
        }

        if(node->lhs != NULL && node->lhs->kind == AST_EXPR_STRUCT)
        {
            ASTDataType *struct_type = instantiateTypeExprValue(node->lhs, scope);
            ASTStructMember *member = node->lhs->members;
            while(member != NULL)
            {
                if(member->value != NULL)
                    checkFunctionExprSemantics(member->value, scope, struct_type);
                member = member->next;
            }
        }

        checkExprDeclaredVariable(node->lhs, scope);
        return;
    }

    if(node->kind == AST_STATEMENT_BREAK || node->kind == AST_STATEMENT_CONTINUE)
    {
        if(function_context == NULL || function_context->loop_depth <= 0)
        {
            semanticAbortNodeFormatted("S1017", node,
                                       "loop control used outside loop",
                                       "%s statement is only allowed inside a loop",
                                       node->kind == AST_STATEMENT_BREAK ? "break" : "continue");
        }

        if(function_context->inside_defer)
        {
            semanticAbortNodeFormatted("S1018", node,
                                       "loop control used inside defer",
                                       "%s statement is not allowed inside defer",
                                       node->kind == AST_STATEMENT_BREAK ? "break" : "continue");
        }
        return;
    }

    if(node->kind == AST_STATEMENT_EXPR)
    {
        checkExprDeclaredVariable(node->lhs, scope);
        return;
    }

    if(node->kind == AST_STATEMENT_IF)
    {
        checkExprDeclaredVariable(node->lhs, scope);
        checkStatementSemantics(node->rhs, scope, function_context);
        checkStatementSemantics(node->body, scope, function_context);
        return;
    }

    if(node->kind == AST_STATEMENT_WHILE)
    {
        checkExprDeclaredVariable(node->lhs, scope);
        FunctionContext loop_context = {0};
        checkStatementSemantics(node->body, scope, deriveLoopContext(function_context, &loop_context));
        return;
    }

    if(node->kind == AST_STATEMENT_DO_WHILE)
    {
        FunctionContext loop_context = {0};
        checkStatementSemantics(node->body, scope, deriveLoopContext(function_context, &loop_context));
        checkExprDeclaredVariable(node->lhs, scope);
        return;
    }

    if(node->kind == AST_STATEMENT_FOR)
    {
        ScopeFrame *loop_scope = newScopeFrame(scope);

        checkStatementSemantics(node->lhs, loop_scope, function_context);
        checkExprDeclaredVariable(node->rhs, loop_scope);
        checkStatementSemantics(node->extra, loop_scope, function_context);

        FunctionContext loop_context = {0};
        checkStatementSemantics(node->body, loop_scope, deriveLoopContext(function_context, &loop_context));
        deleteScopeFrame(loop_scope);
        return;
    }

    if(node->kind == AST_STATEMENT_DEFER)
    {
        checkDeferredAssignmentSemantics(node->lhs, scope);
        FunctionContext defer_context = {0};
        checkStatementSemantics(node->lhs, scope, deriveDeferContext(function_context, &defer_context));
        return;
    }

    if(node->kind == AST_ASSIGN)
    {
        if(function_context != NULL && function_context->inside_defer)
            checkDeferredAssignmentSemantics(node, scope);
        checkAssignSemanticsNode(node, scope);
        return;
    }

    semanticAbortNodeFormatted("ICE0101", node,
                               NULL,
                               "semantic analysis hit unsupported statement kind %s",
                               astNodeKindToString(node->kind));
}

void checkAssignSemanticsInBlock(ASTNode *block, ScopeFrame *parent_scope, FunctionContext *function_context)
{
    ScopeFrame *current_scope = newScopeFrame(parent_scope);
    current_scope->instantiation_site = parent_scope != NULL ? parent_scope->instantiation_site : block;
    if(parent_scope == NULL)
        predeclareTopLevelBindings(block, current_scope);

    ASTNode *node = block->lhs;
    while(node)
    {
        current_scope->instantiation_site = node;
        checkStatementSemantics(node, current_scope, function_context);
        node = node->next;
    }
    deleteScopeFrame(current_scope);
}

void checkAssignSemantics(ASTNode *root)
{
    if(root == NULL || root->lhs == NULL || root->lhs->kind != AST_BLOCK)
    {
        diagnosticAbortSimple("ICE0102",
                              "semantic root should contain a top-level block",
                              makeSourceSpan(NULL, 0, 0, 0, 0),
                              NULL);
    }

    checkAssignSemanticsInBlock(root->lhs, NULL, NULL);
}

#endif /* SEMANTIC_SEMANTICS_H */
