#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "Diagnostic.h"
#include "AST.h"
#include "SymbolTable.h"
#include "TypeSystem.h"
#include <stdbool.h>
#include <string.h>

void semanticAbortNode(const char *code, ASTNode *node, const char *message, const char *label)
{
    diagnosticAbortSimple(code, message, astNodeSourceSpan(node), label);
}

void semanticAbortNodeFormatted(const char *code, ASTNode *node, const char *label, const char *format, ...)
{
    Diagnostic diagnostic = diagnosticMake(DIAGNOSTIC_SEVERITY_ERROR, code, astNodeSourceSpan(node), "");
    va_list args;
    va_start(args, format);
    diagnosticVFormat(diagnostic.message, sizeof(diagnostic.message), format, args);
    va_end(args);
    if(label != NULL)
        diagnosticSetPrimaryLabel(&diagnostic, "%s", label);
    diagnosticAbort(diagnostic);
}

void semanticAbortTypeNode(const char *code, ASTNode *node, const char *message, const char *label)
{
    diagnosticAbortSimple(code, message, astNodeSourceSpan(node), label);
}

void semanticAbortTypeFormatted(const char *code, ASTNode *node, const char *label, const char *format, ...)
{
    Diagnostic diagnostic = diagnosticMake(DIAGNOSTIC_SEVERITY_ERROR, code, astNodeSourceSpan(node), "");
    va_list args;
    va_start(args, format);
    diagnosticVFormat(diagnostic.message, sizeof(diagnostic.message), format, args);
    va_end(args);
    if(label != NULL)
        diagnosticSetPrimaryLabel(&diagnostic, "%s", label);
    diagnosticAbort(diagnostic);
}

void semanticAbortTypeDataType(const char *code, ASTNode *node, const char *message, ASTDataType *expected_type)
{
    char type_buffer[256] = {0};
    appendASTDataTypeString(expected_type, type_buffer, sizeof(type_buffer));
    Diagnostic diagnostic = diagnosticMake(DIAGNOSTIC_SEVERITY_ERROR, code, astNodeSourceSpan(node), message);
    diagnosticSetPrimaryLabel(&diagnostic, "expected type %s", type_buffer);
    diagnosticAbort(diagnostic);
}

bool isExplicitDeclared(ASTNode *node)
{
    return node->modifier.mutable || node->modifier.explicit_type;
}

bool isReferenceDataType(ASTDataType *data_type)
{
    return data_type != NULL && data_type->kind == AST_DATA_TYPE_KIND_REFERENCE;
}

bool isPointerOrReferenceDataType(ASTDataType *data_type)
{
    return data_type != NULL &&
           (data_type->kind == AST_DATA_TYPE_KIND_POINTER || data_type->kind == AST_DATA_TYPE_KIND_REFERENCE);
}

bool isStructDeclAssign(ASTNode *node)
{
    return node != NULL &&
           node->kind == AST_ASSIGN &&
           node->lhs != NULL &&
           node->lhs->kind == AST_EXPR_VARIABLE &&
           node->rhs != NULL &&
           node->rhs->kind == AST_EXPR_STRUCT;
}

bool isEnumDeclAssign(ASTNode *node)
{
    return node != NULL &&
           node->kind == AST_ASSIGN &&
           node->lhs != NULL &&
           node->lhs->kind == AST_EXPR_VARIABLE &&
           node->rhs != NULL &&
           node->rhs->kind == AST_EXPR_ENUM;
}

bool exprLooksLikeTypeDeclValue(ASTNode *node, ScopeFrame *scope)
{
    if(node == NULL)
        return false;

    switch(node->kind)
    {
        case AST_EXPR_TYPE_LITERAL:
        case AST_EXPR_STRUCT:
        case AST_EXPR_ENUM:
            return true;
        case AST_EXPR_PARENTHESIS:
        case AST_EXPR_DEREF:
        case AST_EXPR_ADDRESS_OF:
        case AST_EXPR_ADDRESS_OF_MUT:
            return exprLooksLikeTypeDeclValue(node->lhs, scope);
        case AST_EXPR_VARIABLE:
            return strcmp(node->identifier, "Self") == 0 ||
                   strcmp(node->identifier, "opaque") == 0 ||
                   builtinIdentifierToDataType(node->identifier) != NULL ||
                   findTypeInfo(scope, node->identifier) != NULL;
        default:
            return false;
    }
}

bool isTypeDeclAssign(ASTNode *node, ScopeFrame *scope)
{
    if(node == NULL ||
       node->kind != AST_ASSIGN ||
       node->lhs == NULL ||
       node->lhs->kind != AST_EXPR_VARIABLE ||
       node->rhs == NULL)
        return false;

    if(isStructDeclAssign(node) || isEnumDeclAssign(node))
        return true;

    return exprLooksLikeTypeDeclValue(node->rhs, scope);
}

void checkExprDeclaredVariable(ASTNode *node, ScopeFrame *scope);
void checkAssignSemanticsInBlock(ASTNode *block, ScopeFrame *parent_scope, FunctionContext *function_context);
void checkAssignTypesInBlock(ASTNode *block, ScopeFrame *parent_scope, FunctionContext *function_context);
void checkStatementSemantics(ASTNode *node, ScopeFrame *scope, FunctionContext *function_context);
void checkStatementTypes(ASTNode *node, ScopeFrame *scope, FunctionContext *function_context);
void declareResolvedFunctionParameters(ASTFunctionParameter *parameter, ScopeFrame *scope, ASTDataType *self_data_type);
void predeclareTopLevelBindings(ASTNode *block, ScopeFrame *scope);

bool isInsideFunction(FunctionContext *function_context)
{
    return function_context != NULL && function_context->active;
}

FunctionContext* deriveLoopContext(FunctionContext *parent_context, FunctionContext *loop_context)
{
    memset(loop_context, 0, sizeof(FunctionContext));
    if(parent_context != NULL)
        *loop_context = *parent_context;
    loop_context->parent = parent_context;
    loop_context->loop_depth += 1;
    return loop_context;
}

FunctionContext* deriveDeferContext(FunctionContext *parent_context, FunctionContext *defer_context)
{
    memset(defer_context, 0, sizeof(FunctionContext));
    if(parent_context != NULL)
        *defer_context = *parent_context;
    defer_context->parent = parent_context;
    defer_context->inside_defer = true;
    return defer_context;
}

void declareFunctionParameters(ASTFunctionParameter *parameter, ScopeFrame *scope)
{
    while(parameter)
    {
        if(findVariableInfoInScope(scope, parameter->identifier) >= 0)
        {
            Diagnostic diagnostic = diagnosticMake(DIAGNOSTIC_SEVERITY_ERROR,
                                                   "S1001",
                                                   astFunctionParameterSourceSpan(parameter),
                                                   "duplicate function parameter");
            diagnosticSetPrimaryLabel(&diagnostic, "parameter `%s` is declared more than once", parameter->identifier);
            diagnosticAbort(diagnostic);
        }

        VariableInfo *variable_info = declareVariableInfo(scope, parameter->identifier);
        variable_info->mutable = false;
        variable_info->data_type = cloneDataType(parameter->data_type);
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
                                     capture->identifier);
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
                                     capture->identifier);
        }

        VariableInfo *variable_info = declareVariableInfo(target_scope, capture->identifier);
        variable_info->mutable = false;
        variable_info->data_type = cloneDataType(outer_variable->data_type);
        variable_info->type_value = cloneDataType(outer_variable->type_value);
        variable_info->function_value = outer_variable->function_value;
        variable_info->extern_value = outer_variable->extern_value;
        capture = capture->next;
    }
}

void checkFunctionExprSemantics(ASTNode *node, ScopeFrame *scope)
{
    if(node->is_variadic)
    {
        diagnosticAbortSimple("S1004",
                              "variadic mote function definitions are not supported yet",
                              astNodeSourceSpan(node),
                              "function definition appears here");
    }

    ScopeFrame *function_scope = newScopeFrame(scope);
    declareFunctionParameters(node->parameters, function_scope);
    declareFunctionCaptures(node->captures, function_scope, scope);

    FunctionContext function_context = {0};
    function_context.active = true;
    function_context.return_data_type = node->return_data_type;
    function_context.self_available_as_type_value = false;
    function_context.loop_depth = 0;
    function_context.inside_defer = false;

    checkAssignSemanticsInBlock(node->body, function_scope, &function_context);
    deleteScopeFrame(function_scope);
}

void checkStructExprSemantics(ASTNode *node, ScopeFrame *scope)
{
    ASTStructMember *member = node->members;
    while(member)
    {
        if(member->value)
            checkFunctionExprSemantics(member->value, scope);
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
                                     node->identifier);
        }
        return;
    }

    if(node->kind == AST_EXPR_FUNCTION)
    {
        checkFunctionExprSemantics(node, scope);
        return;
    }

    if(node->kind == AST_EXPR_STRUCT)
    {
        checkStructExprSemantics(node, scope);
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

    if(findVariableInfo(scope, node->identifier) == NULL)
    {
        semanticAbortNode("S1008", node,
                          "defer assignment target must already exist",
                          "assignment target is not declared in an outer scope");
    }
}

void checkAssignSemanticsNode(ASTNode *node, ScopeFrame *scope)
{
    if(isTypeDeclAssign(node, scope))
    {
        TypeInfo *existing_type_info = findTypeInfo(scope, node->identifier);
        if(existing_type_info != NULL)
        {
            bool is_placeholder_struct = existing_type_info->predeclared &&
                                         node->rhs != NULL &&
                                         node->rhs->kind == AST_EXPR_STRUCT;
            bool is_placeholder_enum = existing_type_info->predeclared &&
                                       node->rhs != NULL &&
                                       node->rhs->kind == AST_EXPR_ENUM;
            if(!is_placeholder_struct && !is_placeholder_enum)
            {
                semanticAbortNodeFormatted("S1009", node,
                                           "duplicate type declaration",
                                           "type `%s` has already been declared in this scope",
                                           node->identifier);
            }
        }

        TypeInfo *type_info = existing_type_info;
        if(type_info == NULL)
            type_info = declareTypeInfo(scope, node->identifier);
        TypeSystemExprType expr_type = inferExprType(node->rhs, scope);
        type_info->data_type = cloneDataType(expr_type.data_type);
        type_info->predeclared = false;
        node->data_type = cloneDataType(type_info->data_type);
        if(node->rhs->kind == AST_EXPR_STRUCT)
            checkStructExprSemantics(node->rhs, scope);
        return;
    }

    checkExprDeclaredVariable(node->rhs, scope);

    if(node->lhs->kind == AST_EXPR_DEREF)
    {
        checkExprDeclaredVariable(node->lhs->lhs, scope);
        return;
    }

    if(node->lhs->kind == AST_EXPR_MEMBER)
    {
        if(node->modifier.mutable)
        {
            semanticAbortNode("S1010", node,
                              "member assignment cannot use `mut` declaration syntax",
                              "remove `mut` from this assignment");
        }

        checkExprDeclaredVariable(node->lhs->lhs, scope);
        return;
    }

    if(node->lhs->kind == AST_EXPR_INDEX)
    {
        if(node->modifier.mutable)
        {
            semanticAbortNode("S1011", node,
                              "index assignment cannot use `mut` declaration syntax",
                              "remove `mut` from this assignment");
        }

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
    int local_index = findVariableInfoInScope(scope, node->identifier);
    if(local_index >= 0)
        local_variable_info = &(scope->variable_infos[local_index]);

    if(isExplicitDeclared(node))
    {
        if(local_variable_info != NULL)
        {
            semanticAbortNodeFormatted("S1013", node,
                                       "duplicate variable declaration",
                                       "variable `%s` has already been declared and cannot be declared again",
                                       node->identifier);
        }

        VariableInfo *new_variable_info = declareVariableInfo(scope, node->identifier);
        new_variable_info->mutable = node->modifier.mutable;
        new_variable_info->data_type = newInferDataType();
    }
    else
    {
        VariableInfo *resolved_variable_info = local_variable_info;
        if(resolved_variable_info == NULL)
            resolved_variable_info = findVariableInfo(scope->parent, node->identifier);

        if(resolved_variable_info == NULL)
        {
            VariableInfo *new_variable_info = declareVariableInfo(scope, node->identifier);
            new_variable_info->mutable = false;
            new_variable_info->data_type = newInferDataType();
        }
        else if(!resolved_variable_info->mutable && !isReferenceDataType(resolved_variable_info->data_type))
        {
            semanticAbortNodeFormatted("S1014", node,
                                       "immutable assignment target",
                                       "cannot assign to immutable variable `%s`",
                                       node->identifier);
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
    if(parent_scope == NULL)
        predeclareTopLevelBindings(block, current_scope);

    ASTNode *node = block->lhs;
    while(node)
    {
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

void checkFunctionCallArgumentSemantics(ASTNode *call_node, ASTDataType *function_type, ScopeFrame *scope)
{
    ASTFunctionParameter *parameter = function_type->parameters;
    ASTNode *argument = call_node->rhs;

    if(call_node->lhs->kind == AST_EXPR_MEMBER)
    {
        ASTNode *member_node = call_node->lhs;
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

        if(!through_type && isStructDataType(struct_type) && parameter != NULL &&
           canBindMethodReceiver(member_node->lhs, scope, parameter->data_type, struct_type))
        {
            parameter = parameter->next;
        }
    }

    while(parameter && argument)
    {
        if(parameter->data_type->kind == AST_DATA_TYPE_KIND_REFERENCE && parameter->data_type->mutable)
        {
            if(argument->kind == AST_EXPR_ADDRESS_OF_MUT)
            {
                parameter = parameter->next;
                argument = argument->next;
                continue;
            }

            if(!isMutableAddressableExpr(argument, scope))
            {
                semanticAbortTypeNode("T1101", argument,
                                      "mutable reference argument requires a mutable expression",
                                      "this argument is not mutable");
            }
        }

        parameter = parameter->next;
        argument = argument->next;
    }
}

ASTDataType* resolveCallSemanticFunctionType(ASTNode *call_node, ScopeFrame *scope)
{
    if(call_node == NULL || call_node->kind != AST_EXPR_CALL)
        return NULL;

    if(call_node->lhs != NULL && call_node->lhs->kind == AST_EXPR_VARIABLE)
    {
        VariableInfo *callee_variable = findVariableInfo(scope, call_node->lhs->identifier);
        if(callee_variable != NULL && callee_variable->function_value != NULL)
            return instantiateFunctionCallResolvedFunctionType(callee_variable->function_value, call_node->rhs, scope);
    }

    TypeSystemExprType callee_type = inferExprType(call_node->lhs, scope);
    if(callee_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE &&
       callee_type.data_type != NULL &&
       callee_type.data_type->kind == AST_DATA_TYPE_KIND_FUNCTION)
        return callee_type.data_type;
    return NULL;
}

void checkSpecializedCallArguments(ASTNode *call_node, ScopeFrame *scope)
{
    if(call_node == NULL || call_node->kind != AST_EXPR_CALL)
        return;

    if(call_node->lhs == NULL || call_node->lhs->kind != AST_EXPR_VARIABLE)
        return;

    VariableInfo *callee_variable = findVariableInfo(scope, call_node->lhs->identifier);
    if(callee_variable == NULL || callee_variable->function_value == NULL)
        return;

    ASTDataType *specialized_type = instantiateFunctionCallResolvedFunctionType(
        callee_variable->function_value,
        call_node->rhs,
        scope
    );
    if(specialized_type != NULL && specialized_type->kind == AST_DATA_TYPE_KIND_FUNCTION)
        checkFunctionCallArguments(specialized_type->parameters, call_node->rhs, scope, specialized_type->is_variadic);
}

ASTDataType* declareStructType(ASTNode *node, ScopeFrame *scope)
{
    TypeInfo *type_info = findTypeInfo(scope, node->identifier);
    ASTDataType *struct_type = NULL;
    if(type_info != NULL)
    {
        struct_type = type_info->data_type;
        if(!type_info->predeclared || struct_type == NULL || struct_type->kind != AST_DATA_TYPE_KIND_STRUCT)
        {
            semanticAbortTypeFormatted("T1108", node,
                                       "duplicate type declaration",
                                       "type `%s` has already been declared in this scope",
                                       node->identifier);
        }
    }
    else
    {
        struct_type = newStructDataType(node->identifier, NULL);
        type_info = declareTypeInfo(scope, node->identifier);
        type_info->data_type = struct_type;
    }
    type_info->predeclared = false;

    ASTStructMember *resolved_head = NULL;
    ASTStructMember *resolved_tail = NULL;
    ASTStructMember *member = node->rhs->members;
    while(member)
    {
        if(findStructMember(struct_type, member->identifier) != NULL)
        {
            semanticAbortTypeFormatted("T1102", node,
                                       "duplicate struct member",
                                       "duplicate struct member `%s` in `%s`",
                                       member->identifier, node->identifier);
        }

        ASTStructMember *resolved_member = (ASTStructMember*) malloc(sizeof(ASTStructMember));
        memset(resolved_member, 0, sizeof(ASTStructMember));
        resolved_member->filename = member->filename;
        resolved_member->line_number = member->line_number;
        resolved_member->column_number = member->column_number;
        strcpy(resolved_member->identifier, member->identifier);
        resolved_member->value = member->value;
        resolved_member->data_type = resolveNamedDataType(member->data_type, scope, struct_type);
        typeSystemEnsureNoBareOpaque(resolved_member->data_type, node, "T1132", "struct field");
        member->data_type = cloneDataType(resolved_member->data_type);

        if(resolved_head == NULL)
            resolved_head = resolved_member;
        else
            resolved_tail->next = resolved_member;
        resolved_tail = resolved_member;
        struct_type->members = resolved_head;
        member = member->next;
    }

    node->data_type = cloneDataType(struct_type);
    node->rhs->data_type = cloneDataType(struct_type);
    return struct_type;
}

ASTDataType* declareEnumType(ASTNode *node, ScopeFrame *scope)
{
    TypeInfo *type_info = findTypeInfo(scope, node->identifier);
    ASTDataType *enum_type = NULL;
    if(type_info != NULL)
    {
        enum_type = type_info->data_type;
        if(!type_info->predeclared || enum_type == NULL || enum_type->kind != AST_DATA_TYPE_KIND_ENUM)
        {
            semanticAbortTypeFormatted("T1109", node,
                                       "duplicate type declaration",
                                       "type `%s` has already been declared in this scope",
                                       node->identifier);
        }
    }
    else
    {
        enum_type = newEnumDataType(node->identifier, NULL);
        type_info = declareTypeInfo(scope, node->identifier);
        type_info->data_type = enum_type;
    }
    type_info->predeclared = false;

    ASTEnumVariant *resolved_head = NULL;
    ASTEnumVariant *resolved_tail = NULL;
    ASTEnumVariant *variant = node->rhs->variants;
    while(variant)
    {
        if(findEnumVariant(enum_type, variant->identifier) != NULL)
        {
            semanticAbortTypeFormatted("T1103", node,
                                       "duplicate enum variant",
                                       "duplicate enum variant `%s` in `%s`",
                                       variant->identifier, node->identifier);
        }

        ASTEnumVariant *resolved_variant = (ASTEnumVariant*) malloc(sizeof(ASTEnumVariant));
        *resolved_variant = *variant;
        resolved_variant->next = NULL;

        if(resolved_head == NULL)
            resolved_head = resolved_variant;
        else
            resolved_tail->next = resolved_variant;
        resolved_tail = resolved_variant;
        enum_type->variants = resolved_head;
        variant = variant->next;
    }

    node->data_type = cloneDataType(enum_type);
    node->rhs->data_type = cloneDataType(enum_type);
    return enum_type;
}

void checkFunctionReturnStatement(ASTNode *node, ScopeFrame *scope, FunctionContext *function_context)
{
    ASTDataType *expected_type = function_context->return_data_type;

    if(expected_type->kind == AST_DATA_TYPE_KIND_PRIMARY && expected_type->primary == AST_PRIMARY_DATA_TYPE_VOID)
    {
        if(node->lhs != NULL)
        {
            semanticAbortTypeNode("T1104", node,
                                  "void function should not return a value",
                                  "remove the returned expression");
        }
        return;
    }

    if(node->lhs == NULL)
    {
        semanticAbortTypeNode("T1105", node,
                              "non-void function must return a value",
                              "this function requires a return value");
    }

    if(!canImplicitConvertExprToType(node->lhs, scope, expected_type))
    {
        semanticAbortTypeNode("T1106", node->lhs,
                              "return type mismatch",
                              "returned expression does not match the function return type");
    }
}

ASTFunctionParameter* resolveFunctionTypeParameters(ASTFunctionParameter *parameter, ScopeFrame *outer_scope,
                                                    ASTDataType *self_data_type)
{
    ASTFunctionParameter *head = NULL;
    ASTFunctionParameter *tail = NULL;
    ScopeFrame *signature_scope = newScopeFrame(outer_scope);

    while(parameter)
    {
        ASTFunctionParameter *resolved_parameter = (ASTFunctionParameter*) malloc(sizeof(ASTFunctionParameter));
        *resolved_parameter = *parameter;
        resolved_parameter->next = NULL;
        resolved_parameter->data_type = resolveNamedDataType(parameter->data_type, signature_scope, self_data_type);
        typeSystemEnsureNoBareOpaque(resolved_parameter->data_type, NULL, "T1133", "function parameter");

        if(head == NULL)
            head = resolved_parameter;
        else
            tail->next = resolved_parameter;
        tail = resolved_parameter;

        VariableInfo *variable_info = declareVariableInfo(signature_scope, resolved_parameter->identifier);
        variable_info->mutable = false;
        variable_info->data_type = cloneDataType(resolved_parameter->data_type);
        if(resolved_parameter->data_type != NULL &&
           resolved_parameter->data_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
           resolved_parameter->data_type->primary == AST_PRIMARY_DATA_TYPE_TYPE)
        {
            variable_info->type_value = newNamedDataType(resolved_parameter->identifier);
        }

        parameter = parameter->next;
    }

    deleteScopeFrame(signature_scope);
    return head;
}

void inferFunctionReturnTypesInStatement(ASTNode *node, ScopeFrame *scope,
                                         ASTDataType **inferred_type,
                                         bool *saw_value_return,
                                         bool *saw_void_return)
{
    if(node == NULL)
        return;

    switch(node->kind)
    {
        case AST_BLOCK: {
            ASTNode *statement = node->lhs;
            while(statement)
            {
                inferFunctionReturnTypesInStatement(statement, scope, inferred_type, saw_value_return, saw_void_return);
                statement = statement->next;
            }
            return;
        }
        case AST_STATEMENT_RETURN:
            if(node->lhs == NULL)
            {
                if(*saw_value_return)
                    semanticAbortTypeNode("T1131", node,
                                          "conflicting inferred return types",
                                          "this `return;` conflicts with earlier non-void returns");
                *saw_void_return = true;
                return;
            }

            if(*saw_void_return)
                semanticAbortTypeNode("T1131", node->lhs,
                                      "conflicting inferred return types",
                                      "this return expression conflicts with earlier `return;`");

            ASTDataType *current_type = inferDeclaredTypeFromExpr(node->lhs, scope);
            if(*inferred_type == NULL)
            {
                *inferred_type = cloneDataType(current_type);
            }
            else if(!isSameDataType(*inferred_type, current_type))
            {
                char expected_buffer[256] = {0};
                char actual_buffer[256] = {0};
                appendASTDataTypeString(*inferred_type, expected_buffer, sizeof(expected_buffer));
                appendASTDataTypeString(current_type, actual_buffer, sizeof(actual_buffer));
                semanticAbortTypeFormatted("T1130", node->lhs,
                                           "conflicting inferred return types",
                                           "this return has type %s, but earlier returns imply %s",
                                           actual_buffer, expected_buffer);
            }

            *saw_value_return = true;
            return;
        case AST_STATEMENT_IF:
            inferFunctionReturnTypesInStatement(node->rhs, scope, inferred_type, saw_value_return, saw_void_return);
            inferFunctionReturnTypesInStatement(node->body, scope, inferred_type, saw_value_return, saw_void_return);
            return;
        case AST_STATEMENT_WHILE:
        case AST_STATEMENT_DO_WHILE:
            inferFunctionReturnTypesInStatement(node->body, scope, inferred_type, saw_value_return, saw_void_return);
            return;
        case AST_STATEMENT_FOR:
            inferFunctionReturnTypesInStatement(node->lhs, scope, inferred_type, saw_value_return, saw_void_return);
            inferFunctionReturnTypesInStatement(node->extra, scope, inferred_type, saw_value_return, saw_void_return);
            inferFunctionReturnTypesInStatement(node->body, scope, inferred_type, saw_value_return, saw_void_return);
            return;
        case AST_STATEMENT_DEFER:
            inferFunctionReturnTypesInStatement(node->lhs, scope, inferred_type, saw_value_return, saw_void_return);
            return;
        default:
            return;
    }
}

ASTDataType* inferFunctionExprReturnType(ASTNode *node, ScopeFrame *outer_scope,
                                         ASTDataType *self_data_type,
                                         ASTFunctionParameter *resolved_parameters)
{
    ScopeFrame *function_scope = newScopeFrame(outer_scope);
    declareResolvedFunctionParameters(resolved_parameters, function_scope, self_data_type);
    declareFunctionCaptures(node->captures, function_scope, outer_scope);
    if(self_data_type != NULL)
    {
        VariableInfo *self_variable = declareVariableInfo(function_scope, "Self");
        self_variable->mutable = false;
        self_variable->data_type = newPrimaryDataType(AST_PRIMARY_DATA_TYPE_TYPE);
        self_variable->type_value = cloneDataType(self_data_type);
    }

    ASTDataType *inferred_type = NULL;
    bool saw_value_return = false;
    bool saw_void_return = false;
    inferFunctionReturnTypesInStatement(node->body, function_scope, &inferred_type, &saw_value_return, &saw_void_return);
    deleteScopeFrame(function_scope);

    if(inferred_type == NULL)
        return newPrimaryDataType(AST_PRIMARY_DATA_TYPE_VOID);
    return inferred_type;
}

ASTDataType* resolveFunctionExprDataType(ASTNode *node, ScopeFrame *outer_scope, ASTDataType *self_data_type)
{
    ASTFunctionParameter *resolved_parameters = resolveFunctionTypeParameters(node->parameters, outer_scope, self_data_type);

    ScopeFrame *signature_scope = newScopeFrame(outer_scope);
    ASTFunctionParameter *parameter = resolved_parameters;
    while(parameter)
    {
        VariableInfo *variable_info = declareVariableInfo(signature_scope, parameter->identifier);
        variable_info->mutable = false;
        variable_info->data_type = cloneDataType(parameter->data_type);
        if(parameter->data_type != NULL &&
           parameter->data_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
           parameter->data_type->primary == AST_PRIMARY_DATA_TYPE_TYPE)
        {
            variable_info->type_value = newNamedDataType(parameter->identifier);
        }
        parameter = parameter->next;
    }

    ASTDataType *resolved_return_type = NULL;
    if(node->return_data_type != NULL)
        resolved_return_type = resolveNamedDataType(node->return_data_type, signature_scope, self_data_type);
    else
        resolved_return_type = inferFunctionExprReturnType(node, outer_scope, self_data_type, resolved_parameters);
    typeSystemEnsureNoBareOpaque(resolved_return_type, node, "T1134", "function return type");
    deleteScopeFrame(signature_scope);
    return newFunctionDataType(resolved_parameters, node->is_variadic, resolved_return_type);
}

void declareResolvedFunctionParameters(ASTFunctionParameter *parameter, ScopeFrame *scope, ASTDataType *self_data_type)
{
    while(parameter)
    {
        if(findVariableInfoInScope(scope, parameter->identifier) >= 0)
        {
            Diagnostic diagnostic = diagnosticMake(DIAGNOSTIC_SEVERITY_ERROR,
                                                   "T1129",
                                                   astFunctionParameterSourceSpan(parameter),
                                                   "duplicate function parameter");
            diagnosticSetPrimaryLabel(&diagnostic,
                                      "parameter `%s` is declared more than once",
                                      parameter->identifier);
            diagnosticAbort(diagnostic);
        }

        VariableInfo *variable_info = declareVariableInfo(scope, parameter->identifier);
        variable_info->mutable = false;
        variable_info->data_type = cloneDataType(parameter->data_type);
        if(variable_info->data_type != NULL &&
           variable_info->data_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
           variable_info->data_type->primary == AST_PRIMARY_DATA_TYPE_TYPE)
        {
            variable_info->type_value = newNamedDataType(parameter->identifier);
        }
        parameter = parameter->next;
    }
}

void checkFunctionExprTypes(ASTNode *node, ScopeFrame *scope, ASTDataType *self_data_type)
{
    node->data_type = resolveFunctionExprDataType(node, scope, self_data_type);
    node->return_data_type = cloneDataType(node->data_type->return_data_type);

    ScopeFrame *function_scope = newScopeFrame(scope);
    declareResolvedFunctionParameters(node->data_type->parameters, function_scope, self_data_type);
    declareFunctionCaptures(node->captures, function_scope, scope);
    if(self_data_type != NULL)
    {
        VariableInfo *self_variable = declareVariableInfo(function_scope, "Self");
        self_variable->mutable = false;
        self_variable->data_type = newPrimaryDataType(AST_PRIMARY_DATA_TYPE_TYPE);
        self_variable->type_value = cloneDataType(self_data_type);
    }

    FunctionContext function_context = {0};
    function_context.active = true;
    function_context.return_data_type = node->data_type->return_data_type;
    function_context.self_data_type = self_data_type;
    function_context.self_available_as_type_value = self_data_type != NULL;
    function_context.loop_depth = 0;
    function_context.inside_defer = false;
    checkAssignTypesInBlock(node->body, function_scope, &function_context);
    deleteScopeFrame(function_scope);
}

bool isBoolConditionType(TypeSystemExprType expr_type)
{
    return expr_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE &&
           expr_type.data_type != NULL &&
           expr_type.data_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
           expr_type.data_type->primary == AST_PRIMARY_DATA_TYPE_BOOL;
}

void checkConditionType(ASTNode *condition, ScopeFrame *scope)
{
    if(condition == NULL)
        return;

    TypeSystemExprType expr_type = inferExprType(condition, scope);
    if(!isBoolConditionType(expr_type))
    {
        semanticAbortTypeNode("T1107", condition,
                              "condition must be bool",
                              "this condition does not evaluate to `bool`");
    }
}

void checkAssignTypesNode(ASTNode *node, ScopeFrame *scope, FunctionContext *function_context)
{
    if(isTypeDeclAssign(node, scope))
    {
        if(isStructDeclAssign(node))
        {
            TypeInfo *existing_type_info = findTypeInfo(scope, node->identifier);
            if(existing_type_info != NULL &&
               (!existing_type_info->predeclared ||
                existing_type_info->data_type == NULL ||
                existing_type_info->data_type->kind != AST_DATA_TYPE_KIND_STRUCT))
            {
                semanticAbortTypeFormatted("T1108", node,
                                           "duplicate type declaration",
                                           "type `%s` has already been declared in this scope",
                                           node->identifier);
            }

            ASTDataType *struct_type = declareStructType(node, scope);
            ASTStructMember *member = node->rhs->members;
            while(member)
            {
                if(member->value)
                    checkFunctionExprTypes(member->value, scope, struct_type);
                member = member->next;
            }

            return;
        }

        if(isEnumDeclAssign(node))
        {
            TypeInfo *existing_type_info = findTypeInfo(scope, node->identifier);
            if(existing_type_info != NULL &&
               (!existing_type_info->predeclared ||
                existing_type_info->data_type == NULL ||
                existing_type_info->data_type->kind != AST_DATA_TYPE_KIND_ENUM))
            {
                semanticAbortTypeFormatted("T1109", node,
                                           "duplicate type declaration",
                                           "type `%s` has already been declared in this scope",
                                           node->identifier);
            }

            declareEnumType(node, scope);
            return;
        }

        TypeInfo *existing_type_info = findTypeInfo(scope, node->identifier);
        if(existing_type_info != NULL && !existing_type_info->predeclared)
        {
            semanticAbortTypeFormatted("T1109", node,
                                       "duplicate type declaration",
                                       "type `%s` has already been declared in this scope",
                                       node->identifier);
        }

        TypeSystemExprType expr_type = inferExprType(node->rhs, scope);
        TypeInfo *type_info = declareTypeInfo(scope, node->identifier);
        type_info->data_type = resolveNamedDataType(expr_type.data_type, scope,
                                                    function_context == NULL ? NULL : function_context->self_data_type);
        if(type_info->data_type != NULL &&
           type_info->data_type->kind == AST_DATA_TYPE_KIND_OPAQUE &&
           type_info->data_type->identifier[0] == '\0')
            strcpy(type_info->data_type->identifier, node->identifier);
        node->data_type = cloneDataType(type_info->data_type);
        return;
    }

    if(node->lhs->kind == AST_EXPR_DEREF)
    {
        TypeSystemExprType expr_type = inferExprType(node->rhs, scope);
        TypeSystemExprType lhs_type = inferExprType(node->lhs->lhs, scope);
        if(lhs_type.kind != TYPE_SYSTEM_EXPR_TYPE_VALUE || !isPointerOrReferenceDataType(lhs_type.data_type))
        {
            semanticAbortTypeNode("T1110", node->lhs,
                                  "deref assignment requires a pointer or reference",
                                  "left-hand side does not dereference a pointer/reference");
        }
        if(!lhs_type.data_type->mutable)
        {
            semanticAbortTypeNode("T1111", node->lhs,
                                  "cannot assign through immutable pointer or reference",
                                  "the dereferenced target is immutable");
        }
        if(!canImplicitConvertDataType(expr_type, node->rhs, lhs_type.data_type->child))
            typeErrorAssign(node, node->rhs, expr_type, lhs_type.data_type->child);

        node->data_type = cloneDataType(lhs_type.data_type->child);
        return;
    }

    if(node->lhs->kind == AST_EXPR_MEMBER)
    {
        TypeSystemExprType expr_type = inferExprType(node->rhs, scope);
        TypeSystemExprType owner_type = inferExprType(node->lhs->lhs, scope);
        ASTDataType *struct_type = owner_type.data_type;
        if(owner_type.kind != TYPE_SYSTEM_EXPR_TYPE_VALUE)
        {
            semanticAbortTypeNode("T1112", node->lhs,
                                  "member assignment requires a value receiver",
                                  "the receiver is not a runtime value");
        }

        if(struct_type->kind == AST_DATA_TYPE_KIND_POINTER || struct_type->kind == AST_DATA_TYPE_KIND_REFERENCE)
            struct_type = struct_type->child;
        if(!isStructDataType(struct_type))
        {
            semanticAbortTypeNode("T1113", node->lhs,
                                  "member assignment requires a struct receiver",
                                  "the receiver is not a struct");
        }

        ASTStructMember *member = findStructMember(struct_type, node->lhs->identifier);
        if(member == NULL || member->value != NULL)
        {
            semanticAbortTypeFormatted("T1114", node->lhs,
                                       "invalid member assignment target",
                                       "cannot assign to member `%s`",
                                       node->lhs->identifier);
        }

        if(!isMutableAddressableExpr(node->lhs, scope))
        {
            semanticAbortTypeNode("T1115", node->lhs,
                                  "cannot assign through immutable member access",
                                  "the member receiver is immutable");
        }

        ASTDataType *target_type = member->data_type;
        if(target_type->kind == AST_DATA_TYPE_KIND_REFERENCE)
            target_type = target_type->child;

        if(!canImplicitConvertExprToType(node->rhs, scope, target_type))
            typeErrorAssign(node, node->rhs, expr_type, target_type);

        node->data_type = cloneDataType(target_type);
        return;
    }

    if(node->lhs->kind == AST_EXPR_INDEX)
    {
        TypeSystemExprType expr_type = inferExprType(node->rhs, scope);
        TypeSystemExprType owner_type = inferExprType(node->lhs->lhs, scope);
        if(owner_type.kind != TYPE_SYSTEM_EXPR_TYPE_VALUE)
        {
            semanticAbortTypeNode("T1116", node->lhs,
                                  "index assignment requires a value receiver",
                                  "the indexed expression is not a runtime value");
        }

        ASTDataType *array_type = owner_type.data_type;
        if(array_type->kind == AST_DATA_TYPE_KIND_POINTER || array_type->kind == AST_DATA_TYPE_KIND_REFERENCE)
            array_type = array_type->child;
        if(!isArrayDataType(array_type) && !isSliceDataType(array_type))
        {
            semanticAbortTypeNode("T1117", node->lhs,
                                  "index assignment requires an array or slice receiver",
                                  "the indexed expression is not an array or slice");
        }

        if(!isMutableAddressableExpr(node->lhs, scope))
        {
            semanticAbortTypeNode("T1118", node->lhs,
                                  "cannot assign through immutable index access",
                                  "the indexed receiver is immutable");
        }

        if(!canImplicitConvertExprToType(node->rhs, scope, array_type->child))
            typeErrorAssign(node, node->rhs, expr_type, array_type->child);

        node->data_type = cloneDataType(array_type->child);
        return;
    }

    VariableInfo *local_variable_info = NULL;
    int local_index = findVariableInfoInScope(scope, node->identifier);
    if(local_index >= 0)
        local_variable_info = &(scope->variable_infos[local_index]);

    if(node->rhs->kind == AST_EXPR_FUNCTION)
        checkFunctionExprTypes(node->rhs, scope, function_context == NULL ? NULL : function_context->self_data_type);
    if(node->rhs->kind == AST_EXPR_CALL)
    {
        checkSpecializedCallArguments(node->rhs, scope);
        ASTDataType *resolved_call_type = resolveCallSemanticFunctionType(node->rhs, scope);
        if(resolved_call_type != NULL && resolved_call_type->kind == AST_DATA_TYPE_KIND_FUNCTION)
            checkFunctionCallArgumentSemantics(node->rhs, resolved_call_type, scope);
    }

    if(isExplicitDeclared(node))
    {
        TypeSystemExprType expr_type = {0};
        if(local_variable_info != NULL)
        {
            semanticAbortTypeFormatted("T1119", node,
                                       "duplicate variable declaration",
                                       "variable `%s` has already been declared and cannot be declared again",
                                       node->identifier);
        }

        ASTDataType *declared_type = node->data_type;
        if(isInferDataType(declared_type))
        {
            declared_type = inferDeclaredTypeFromExpr(node->rhs, scope);
            node->data_type = declared_type;
        }
        else
            declared_type = node->data_type = resolveNamedDataType(declared_type, scope,
                                                                   function_context == NULL ? NULL : function_context->self_data_type);
        typeSystemEnsureNoBareOpaque(declared_type, node, "T1135", "variable declaration");

        if(node->rhs->kind == AST_EXPR_ARRAY_LITERAL && node->rhs->lhs == NULL)
        {
            if(declared_type->kind != AST_DATA_TYPE_KIND_ARRAY || declared_type->array_length != 0)
            {
                semanticAbortTypeNode("T1120", node->rhs,
                                      "empty array literal requires an explicit zero-length array type",
                                      "declare the target as `Array(T, 0)`");
            }
            expr_type = newValueExprType(declared_type);
        }
        else
            expr_type = inferExprType(node->rhs, scope);

        if(isReferenceDataType(declared_type))
        {
            if(node->rhs->kind != AST_EXPR_VARIABLE && node->rhs->kind != AST_EXPR_DEREF)
            {
                semanticAbortTypeNode("T1121", node->rhs,
                                      "reference initialization requires an addressable expression",
                                      "this expression cannot be bound by reference");
            }

            if(declared_type->mutable && !isMutableAddressableExpr(node->rhs, scope))
            {
                semanticAbortTypeNode("T1122", node->rhs,
                                      "mutable reference requires a mutable expression",
                                      "this expression is not mutable");
            }

            TypeSystemExprType rhs_value_type = inferExprType(node->rhs, scope);
            ASTDataType *rhs_target_type = getReferenceTargetType(rhs_value_type.data_type);
            if(!isSameDataType(rhs_target_type, declared_type->child))
                typeErrorAssign(node, node->rhs, rhs_value_type, declared_type);
        }
        else
        {
            if(!canImplicitConvertExprToType(node->rhs, scope, declared_type))
                typeErrorAssign(node, node->rhs, expr_type, declared_type);
        }

        VariableInfo *new_variable_info = declareVariableInfo(scope, node->identifier);
        new_variable_info->mutable = node->modifier.mutable;
        new_variable_info->data_type = cloneDataType(node->data_type);
        if(expr_type.kind == TYPE_SYSTEM_EXPR_TYPE_TYPE)
            new_variable_info->type_value = cloneDataType(expr_type.data_type);
        new_variable_info->function_value = resolveFunctionValueExpr(node->rhs, scope);
        new_variable_info->extern_value = resolveExternValueExpr(node->rhs, scope);
    }
    else
    {
        VariableInfo *resolved_variable_info = local_variable_info;
        if(resolved_variable_info == NULL)
            resolved_variable_info = findVariableInfo(scope->parent, node->identifier);

        if(resolved_variable_info == NULL)
        {
            TypeSystemExprType expr_type = inferExprType(node->rhs, scope);
            ASTDataType *declared_type = inferDeclaredTypeFromExpr(node->rhs, scope);
            node->data_type = declared_type;

            VariableInfo *new_variable_info = declareVariableInfo(scope, node->identifier);
            new_variable_info->mutable = false;
            new_variable_info->data_type = cloneDataType(declared_type);
            if(expr_type.kind == TYPE_SYSTEM_EXPR_TYPE_TYPE)
                new_variable_info->type_value = cloneDataType(expr_type.data_type);
            new_variable_info->function_value = resolveFunctionValueExpr(node->rhs, scope);
            new_variable_info->extern_value = resolveExternValueExpr(node->rhs, scope);

            if(!canImplicitConvertExprToType(node->rhs, scope, declared_type))
                typeErrorAssign(node, node->rhs, expr_type, declared_type);
        }
        else
        {
            TypeSystemExprType expr_type = inferExprType(node->rhs, scope);
            ASTDataType *target_type = resolved_variable_info->data_type;
            bool assign_through_reference = isReferenceDataType(target_type);

            if(assign_through_reference)
                target_type = target_type->child;

            if(!canImplicitConvertExprToType(node->rhs, scope, target_type))
                typeErrorAssign(node, node->rhs, expr_type, target_type);

            if(assign_through_reference)
                node->data_type = cloneDataType(target_type);
            else
                node->data_type = cloneDataType(resolved_variable_info->data_type);
        }
    }
}

void checkStatementTypes(ASTNode *node, ScopeFrame *scope, FunctionContext *function_context)
{
    if(node == NULL)
        return;

    if(node->kind == AST_BLOCK)
    {
        checkAssignTypesInBlock(node, scope, function_context);
        return;
    }

    if(node->kind == AST_STATEMENT_RETURN)
    {
        if(!isInsideFunction(function_context))
        {
            semanticAbortTypeNode("T1123", node,
                                  "return statement is only allowed inside a function",
                                  "remove this `return` or move it into a function body");
        }

        if(function_context->inside_defer)
        {
            semanticAbortTypeNode("T1124", node,
                                  "return statement is not allowed inside defer",
                                  "`defer` bodies cannot exit the surrounding function");
        }

        if(node->lhs)
        {
            if(node->lhs->kind == AST_EXPR_CALL)
            {
                checkSpecializedCallArguments(node->lhs, scope);
                ASTDataType *resolved_call_type = resolveCallSemanticFunctionType(node->lhs, scope);
                if(resolved_call_type != NULL && resolved_call_type->kind == AST_DATA_TYPE_KIND_FUNCTION)
                    checkFunctionCallArgumentSemantics(node->lhs, resolved_call_type, scope);
            }
            else if(node->lhs->kind == AST_EXPR_FUNCTION)
            {
                checkFunctionExprTypes(node->lhs, scope, function_context == NULL ? NULL : function_context->self_data_type);
            }
            else
            {
                inferExprType(node->lhs, scope);
            }
        }

        checkFunctionReturnStatement(node, scope, function_context);
        return;
    }

    if(node->kind == AST_STATEMENT_BREAK || node->kind == AST_STATEMENT_CONTINUE)
    {
        if(function_context == NULL || function_context->loop_depth <= 0)
        {
            semanticAbortTypeFormatted("T1125", node,
                                       "loop control used outside loop",
                                       "%s statement is only allowed inside a loop",
                                       node->kind == AST_STATEMENT_BREAK ? "break" : "continue");
        }

        if(function_context->inside_defer)
        {
            semanticAbortTypeFormatted("T1126", node,
                                       "loop control used inside defer",
                                       "%s statement is not allowed inside defer",
                                       node->kind == AST_STATEMENT_BREAK ? "break" : "continue");
        }
        return;
    }

    if(node->kind == AST_STATEMENT_EXPR)
    {
        if(node->lhs->kind == AST_EXPR_ARRAY_LITERAL && node->lhs->lhs == NULL)
        {
            semanticAbortTypeNode("T1127", node->lhs,
                                  "empty array literal requires an explicit array type",
                                  "add an explicit array type annotation");
        }
        else if(node->lhs->kind == AST_EXPR_CALL)
        {
            checkSpecializedCallArguments(node->lhs, scope);
            ASTDataType *resolved_call_type = resolveCallSemanticFunctionType(node->lhs, scope);
            if(resolved_call_type == NULL || resolved_call_type->kind != AST_DATA_TYPE_KIND_FUNCTION)
            {
                semanticAbortTypeNode("T1128", node->lhs,
                                      "called expression is not a function",
                                      "this expression does not have a function type");
            }
            checkFunctionCallArgumentSemantics(node->lhs, resolved_call_type, scope);
            inferExprType(node->lhs, scope);
        }
        else if(node->lhs->kind == AST_EXPR_FUNCTION)
        {
            checkFunctionExprTypes(node->lhs, scope, function_context == NULL ? NULL : function_context->self_data_type);
        }
        else
        {
            inferExprType(node->lhs, scope);
        }
        return;
    }

    if(node->kind == AST_STATEMENT_IF)
    {
        checkConditionType(node->lhs, scope);
        checkStatementTypes(node->rhs, scope, function_context);
        checkStatementTypes(node->body, scope, function_context);
        return;
    }

    if(node->kind == AST_STATEMENT_WHILE)
    {
        checkConditionType(node->lhs, scope);
        FunctionContext loop_context = {0};
        checkStatementTypes(node->body, scope, deriveLoopContext(function_context, &loop_context));
        return;
    }

    if(node->kind == AST_STATEMENT_DO_WHILE)
    {
        FunctionContext loop_context = {0};
        checkStatementTypes(node->body, scope, deriveLoopContext(function_context, &loop_context));
        checkConditionType(node->lhs, scope);
        return;
    }

    if(node->kind == AST_STATEMENT_FOR)
    {
        ScopeFrame *loop_scope = newScopeFrame(scope);

        checkStatementTypes(node->lhs, loop_scope, function_context);
        if(node->rhs)
            checkConditionType(node->rhs, loop_scope);
        checkStatementTypes(node->extra, loop_scope, function_context);

        FunctionContext loop_context = {0};
        checkStatementTypes(node->body, loop_scope, deriveLoopContext(function_context, &loop_context));
        deleteScopeFrame(loop_scope);
        return;
    }

    if(node->kind == AST_STATEMENT_DEFER)
    {
        FunctionContext defer_context = {0};
        checkStatementTypes(node->lhs, scope, deriveDeferContext(function_context, &defer_context));
        return;
    }

    if(node->kind == AST_ASSIGN)
    {
        checkAssignTypesNode(node, scope, function_context);
        return;
    }

    semanticAbortTypeFormatted("ICE0103", node,
                               NULL,
                               "type checking hit unsupported statement kind %s",
                               astNodeKindToString(node->kind));
}

void checkAssignTypesInBlock(ASTNode *block, ScopeFrame *parent_scope, FunctionContext *function_context)
{
    ScopeFrame *current_scope = newScopeFrame(parent_scope);
    if(parent_scope == NULL)
        predeclareTopLevelBindings(block, current_scope);

    ASTNode *node = block->lhs;
    while(node)
    {
        checkStatementTypes(node, current_scope, function_context);
        node = node->next;
    }
    deleteScopeFrame(current_scope);
}

void checkAssignTypes(ASTNode *root)
{
    if(root == NULL || root->lhs == NULL || root->lhs->kind != AST_BLOCK)
    {
        diagnosticAbortSimple("ICE0104",
                              "type-check root should contain a top-level block",
                              makeSourceSpan(NULL, 0, 0, 0, 0),
                              NULL);
    }

    checkAssignTypesInBlock(root->lhs, NULL, NULL);
}

void predeclareTopLevelBindings(ASTNode *block, ScopeFrame *scope)
{
    if(block == NULL || scope == NULL)
        return;

    ASTNode *node = block->lhs;
    while(node)
    {
        if(node->kind == AST_ASSIGN && node->lhs != NULL && node->lhs->kind == AST_EXPR_VARIABLE)
        {
            if(node->rhs != NULL && node->rhs->kind == AST_EXPR_STRUCT)
            {
                if(findTypeInfoInScope(scope, node->identifier) < 0)
                {
                    TypeInfo *type_info = declareTypeInfo(scope, node->identifier);
                    type_info->data_type = newStructDataType(node->identifier, NULL);
                    type_info->predeclared = true;
                }
            }
            else if(node->rhs != NULL && node->rhs->kind == AST_EXPR_ENUM)
            {
                if(findTypeInfoInScope(scope, node->identifier) < 0)
                {
                    TypeInfo *type_info = declareTypeInfo(scope, node->identifier);
                    type_info->data_type = newEnumDataType(node->identifier, NULL);
                    type_info->predeclared = true;
                }
            }
        }
        node = node->next;
    }
}

#endif /* SEMANTIC_H */
