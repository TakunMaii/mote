#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "AST.h"
#include "SymbolTable.h"
#include "TypeSystem.h"
#include <stdbool.h>
#include <string.h>

bool isExplicitDeclared(ASTNode *node)
{
    return node->modifier.mutable || !isInferDataType(node->data_type);
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

void checkExprDeclaredVariable(ASTNode *node, ScopeFrame *scope);
void checkAssignSemanticsInBlock(ASTNode *block, ScopeFrame *parent_scope, FunctionContext *function_context);
void checkAssignTypesInBlock(ASTNode *block, ScopeFrame *parent_scope, FunctionContext *function_context);
void checkStatementSemantics(ASTNode *node, ScopeFrame *scope, FunctionContext *function_context);
void checkStatementTypes(ASTNode *node, ScopeFrame *scope, FunctionContext *function_context);

bool isInsideFunction(FunctionContext *function_context)
{
    return function_context != NULL && function_context->return_data_type != NULL;
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
            printf("Function parameter %s is declared more than once\n", parameter->identifier);
            exit(1);
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
            printf("Function capture %s is declared more than once\n", capture->identifier);
            exit(1);
        }

        VariableInfo *outer_variable = findVariableInfo(source_scope, capture->identifier);
        if(outer_variable == NULL)
        {
            printf("Unknown function capture %s\n", capture->identifier);
            exit(1);
        }

        VariableInfo *variable_info = declareVariableInfo(target_scope, capture->identifier);
        variable_info->mutable = false;
        variable_info->data_type = cloneDataType(outer_variable->data_type);
        variable_info->type_value = cloneDataType(outer_variable->type_value);
        variable_info->function_value = outer_variable->function_value;
        capture = capture->next;
    }
}

void checkFunctionExprSemantics(ASTNode *node, ScopeFrame *scope)
{
    ScopeFrame function_scope = {0};
    initScopeFrame(&function_scope, scope);
    declareFunctionParameters(node->parameters, &function_scope);
    declareFunctionCaptures(node->captures, &function_scope, scope);

    FunctionContext function_context = {0};
    function_context.return_data_type = node->return_data_type;
    function_context.self_available_as_type_value = false;
    function_context.loop_depth = 0;
    function_context.inside_defer = false;

    checkAssignSemanticsInBlock(node->body, &function_scope, &function_context);
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
            printf("Use of undeclared variable %s in expression\n", node->identifier);
            exit(1);
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

    checkExprDeclaredVariable(node->lhs, scope);
    checkExprDeclaredVariable(node->rhs, scope);
}

void checkDeferredAssignmentSemantics(ASTNode *node, ScopeFrame *scope)
{
    if(node == NULL || node->kind != AST_ASSIGN)
        return;

    if(isStructDeclAssign(node) || isEnumDeclAssign(node))
    {
        printf("Semantic error: defer cannot declare a type at file %s, line %d, column %d\n",
               node->filename, node->line_number, node->column_number);
        exit(1);
    }

    if(node->lhs->kind != AST_EXPR_VARIABLE)
        return;

    if(isExplicitDeclared(node))
    {
        printf("Semantic error: defer cannot declare a variable at file %s, line %d, column %d\n",
               node->filename, node->line_number, node->column_number);
        exit(1);
    }

    if(findVariableInfo(scope, node->identifier) == NULL)
    {
        printf("Semantic error: defer assignment target must already exist at file %s, line %d, column %d\n",
               node->filename, node->line_number, node->column_number);
        exit(1);
    }
}

void checkAssignSemanticsNode(ASTNode *node, ScopeFrame *scope)
{
    if(isStructDeclAssign(node) || isEnumDeclAssign(node))
    {
        if(findTypeInfoInScope(scope, node->identifier) >= 0)
        {
            printf("Type %s has already been declared in this scope\n", node->identifier);
            exit(1);
        }

        TypeInfo *type_info = declareTypeInfo(scope, node->identifier);
        type_info->data_type = cloneDataType(node->rhs->data_type);
        strcpy(type_info->data_type->identifier, node->identifier);
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
            printf("Semantic error: member assignment cannot use mut declaration syntax at file %s, line %d, column %d\n",
                   node->filename, node->line_number, node->column_number);
            exit(1);
        }

        checkExprDeclaredVariable(node->lhs->lhs, scope);
        return;
    }

    if(node->lhs->kind == AST_EXPR_INDEX)
    {
        if(node->modifier.mutable)
        {
            printf("Semantic error: index assignment cannot use mut declaration syntax at file %s, line %d, column %d\n",
                   node->filename, node->line_number, node->column_number);
            exit(1);
        }

        checkExprDeclaredVariable(node->lhs->lhs, scope);
        checkExprDeclaredVariable(node->lhs->rhs, scope);
        return;
    }

    if(node->lhs->kind != AST_EXPR_VARIABLE)
    {
        printf("Semantic error: only variable, deref, member, or index can be assigned at file %s, line %d, column %d\n",
               node->filename, node->line_number, node->column_number);
        exit(1);
    }

    VariableInfo *local_variable_info = NULL;
    int local_index = findVariableInfoInScope(scope, node->identifier);
    if(local_index >= 0)
        local_variable_info = &(scope->variable_infos[local_index]);

    if(isExplicitDeclared(node))
    {
        if(local_variable_info != NULL)
        {
            printf("Variable %s has already been declared and cannot be declared again\n", node->identifier);
            exit(1);
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
            printf("Cannot assign to immutable variable %s\n", node->identifier);
            exit(1);
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
            printf("Return statement is only allowed inside a function at file %s, line %d, column %d\n",
                   node->filename, node->line_number, node->column_number);
            exit(1);
        }

        if(function_context->inside_defer)
        {
            printf("Return statement is not allowed inside defer at file %s, line %d, column %d\n",
                   node->filename, node->line_number, node->column_number);
            exit(1);
        }

        checkExprDeclaredVariable(node->lhs, scope);
        return;
    }

    if(node->kind == AST_STATEMENT_BREAK || node->kind == AST_STATEMENT_CONTINUE)
    {
        if(function_context == NULL || function_context->loop_depth <= 0)
        {
            printf("%s statement is only allowed inside a loop at file %s, line %d, column %d\n",
                   node->kind == AST_STATEMENT_BREAK ? "Break" : "Continue",
                   node->filename, node->line_number, node->column_number);
            exit(1);
        }

        if(function_context->inside_defer)
        {
            printf("%s statement is not allowed inside defer at file %s, line %d, column %d\n",
                   node->kind == AST_STATEMENT_BREAK ? "Break" : "Continue",
                   node->filename, node->line_number, node->column_number);
            exit(1);
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
        ScopeFrame loop_scope = {0};
        initScopeFrame(&loop_scope, scope);

        checkStatementSemantics(node->lhs, &loop_scope, function_context);
        checkExprDeclaredVariable(node->rhs, &loop_scope);
        checkStatementSemantics(node->extra, &loop_scope, function_context);

        FunctionContext loop_context = {0};
        checkStatementSemantics(node->body, &loop_scope, deriveLoopContext(function_context, &loop_context));
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

    printf("Semantic error: unsupported statement kind %s\n", astNodeKindToString(node->kind));
    exit(1);
}

void checkAssignSemanticsInBlock(ASTNode *block, ScopeFrame *parent_scope, FunctionContext *function_context)
{
    ScopeFrame current_scope = {0};
    initScopeFrame(&current_scope, parent_scope);

    ASTNode *node = block->lhs;
    while(node)
    {
        checkStatementSemantics(node, &current_scope, function_context);
        node = node->next;
    }
}

void checkAssignSemantics(ASTNode *root)
{
    if(root == NULL || root->lhs == NULL || root->lhs->kind != AST_BLOCK)
    {
        printf("Semantic error: root should contain a top-level block\n");
        exit(1);
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
            if(!isMutableAddressableExpr(argument, scope))
            {
                printf("Type error: mutable reference argument requires a mutable expression at file %s, line %d, column %d\n",
                       argument->filename, argument->line_number, argument->column_number);
                exit(1);
            }
        }

        parameter = parameter->next;
        argument = argument->next;
    }
}

ASTDataType* declareStructType(ASTNode *node, ScopeFrame *scope)
{
    ASTDataType *struct_type = newStructDataType(node->identifier, NULL);
    TypeInfo *type_info = declareTypeInfo(scope, node->identifier);
    type_info->data_type = struct_type;

    ASTStructMember *resolved_head = NULL;
    ASTStructMember *resolved_tail = NULL;
    ASTStructMember *member = node->rhs->members;
    while(member)
    {
        if(findStructMember(struct_type, member->identifier) != NULL)
        {
            printf("Type error: duplicate struct member %s in %s\n", member->identifier, node->identifier);
            exit(1);
        }

        ASTStructMember *resolved_member = (ASTStructMember*) malloc(sizeof(ASTStructMember));
        memset(resolved_member, 0, sizeof(ASTStructMember));
        resolved_member->filename = member->filename;
        resolved_member->line_number = member->line_number;
        resolved_member->column_number = member->column_number;
        strcpy(resolved_member->identifier, member->identifier);
        resolved_member->value = member->value;
        resolved_member->data_type = resolveNamedDataType(member->data_type, scope, struct_type);
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
    ASTDataType *enum_type = newEnumDataType(node->identifier, NULL);
    TypeInfo *type_info = declareTypeInfo(scope, node->identifier);
    type_info->data_type = enum_type;

    ASTEnumVariant *resolved_head = NULL;
    ASTEnumVariant *resolved_tail = NULL;
    ASTEnumVariant *variant = node->rhs->variants;
    while(variant)
    {
        if(findEnumVariant(enum_type, variant->identifier) != NULL)
        {
            printf("Type error: duplicate enum variant %s in %s\n", variant->identifier, node->identifier);
            exit(1);
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
            printf("Type error: void function should not return a value at file %s, line %d, column %d\n",
                   node->filename, node->line_number, node->column_number);
            exit(1);
        }
        return;
    }

    if(node->lhs == NULL)
    {
        printf("Type error: non-void function must return a value at file %s, line %d, column %d\n",
               node->filename, node->line_number, node->column_number);
        exit(1);
    }

    TypeSystemExprType return_type = inferExprType(node->lhs, scope);
    if(!canImplicitConvertDataType(return_type, node->lhs, expected_type))
    {
        printf("Type error: return type mismatch at file %s, line %d, column %d\n",
               node->lhs->filename, node->lhs->line_number, node->lhs->column_number);
        exit(1);
    }
}

ASTFunctionParameter* resolveFunctionTypeParameters(ASTFunctionParameter *parameter, ScopeFrame *outer_scope,
                                                    ASTDataType *self_data_type)
{
    ASTFunctionParameter *head = NULL;
    ASTFunctionParameter *tail = NULL;

    while(parameter)
    {
        ASTFunctionParameter *resolved_parameter = (ASTFunctionParameter*) malloc(sizeof(ASTFunctionParameter));
        *resolved_parameter = *parameter;
        resolved_parameter->next = NULL;
        resolved_parameter->data_type = resolveNamedDataType(parameter->data_type, outer_scope, self_data_type);

        if(head == NULL)
            head = resolved_parameter;
        else
            tail->next = resolved_parameter;
        tail = resolved_parameter;
        parameter = parameter->next;
    }

    return head;
}

ASTDataType* resolveFunctionExprDataType(ASTNode *node, ScopeFrame *outer_scope, ASTDataType *self_data_type)
{
    ASTFunctionParameter *resolved_parameters = resolveFunctionTypeParameters(node->parameters, outer_scope, self_data_type);

    ScopeFrame signature_scope = {0};
    initScopeFrame(&signature_scope, outer_scope);
    ASTFunctionParameter *parameter = resolved_parameters;
    while(parameter)
    {
        VariableInfo *variable_info = declareVariableInfo(&signature_scope, parameter->identifier);
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

    ASTDataType *resolved_return_type = resolveNamedDataType(node->return_data_type, &signature_scope, self_data_type);
    return newFunctionDataType(resolved_parameters, resolved_return_type);
}

void declareResolvedFunctionParameters(ASTFunctionParameter *parameter, ScopeFrame *scope, ASTDataType *self_data_type)
{
    while(parameter)
    {
        if(findVariableInfoInScope(scope, parameter->identifier) >= 0)
        {
            printf("Function parameter %s is declared more than once\n", parameter->identifier);
            exit(1);
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

    ScopeFrame function_scope = {0};
    initScopeFrame(&function_scope, scope);
    declareResolvedFunctionParameters(node->data_type->parameters, &function_scope, self_data_type);
    declareFunctionCaptures(node->captures, &function_scope, scope);
    if(self_data_type != NULL)
    {
        VariableInfo *self_variable = declareVariableInfo(&function_scope, "Self");
        self_variable->mutable = false;
        self_variable->data_type = newPrimaryDataType(AST_PRIMARY_DATA_TYPE_TYPE);
        self_variable->type_value = cloneDataType(self_data_type);
    }

    FunctionContext function_context = {0};
    function_context.return_data_type = node->data_type->return_data_type;
    function_context.self_data_type = self_data_type;
    function_context.self_available_as_type_value = self_data_type != NULL;
    function_context.loop_depth = 0;
    function_context.inside_defer = false;
    checkAssignTypesInBlock(node->body, &function_scope, &function_context);
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
        printf("Type error: condition must be bool at file %s, line %d, column %d\n",
               condition->filename, condition->line_number, condition->column_number);
        exit(1);
    }
}

void checkAssignTypesNode(ASTNode *node, ScopeFrame *scope, FunctionContext *function_context)
{
    if(isStructDeclAssign(node))
    {
        if(findTypeInfoInScope(scope, node->identifier) >= 0)
        {
            printf("Type %s has already been declared in this scope\n", node->identifier);
            exit(1);
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
        if(findTypeInfoInScope(scope, node->identifier) >= 0)
        {
            printf("Type %s has already been declared in this scope\n", node->identifier);
            exit(1);
        }

        declareEnumType(node, scope);
        return;
    }

    if(node->lhs->kind == AST_EXPR_DEREF)
    {
        TypeSystemExprType expr_type = inferExprType(node->rhs, scope);
        TypeSystemExprType lhs_type = inferExprType(node->lhs->lhs, scope);
        if(lhs_type.kind != TYPE_SYSTEM_EXPR_TYPE_VALUE || !isPointerOrReferenceDataType(lhs_type.data_type))
        {
            printf("Type error: deref assignment requires a pointer or reference at file %s, line %d, column %d\n",
                   node->lhs->filename, node->lhs->line_number, node->lhs->column_number);
            exit(1);
        }
        if(!lhs_type.data_type->mutable)
        {
            printf("Type error: cannot assign through immutable pointer or reference at file %s, line %d, column %d\n",
                   node->lhs->filename, node->lhs->line_number, node->lhs->column_number);
            exit(1);
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
            printf("Type error: member assignment requires a value receiver at file %s, line %d, column %d\n",
                   node->lhs->filename, node->lhs->line_number, node->lhs->column_number);
            exit(1);
        }

        if(struct_type->kind == AST_DATA_TYPE_KIND_POINTER || struct_type->kind == AST_DATA_TYPE_KIND_REFERENCE)
            struct_type = struct_type->child;
        if(!isStructDataType(struct_type))
        {
            printf("Type error: member assignment requires a struct receiver at file %s, line %d, column %d\n",
                   node->lhs->filename, node->lhs->line_number, node->lhs->column_number);
            exit(1);
        }

        ASTStructMember *member = findStructMember(struct_type, node->lhs->identifier);
        if(member == NULL || member->value != NULL)
        {
            printf("Type error: cannot assign to member %s at file %s, line %d, column %d\n",
                   node->lhs->identifier, node->lhs->filename, node->lhs->line_number, node->lhs->column_number);
            exit(1);
        }

        if(!isMutableAddressableExpr(node->lhs, scope))
        {
            printf("Type error: cannot assign through immutable member access at file %s, line %d, column %d\n",
                   node->lhs->filename, node->lhs->line_number, node->lhs->column_number);
            exit(1);
        }

        ASTDataType *target_type = member->data_type;
        if(target_type->kind == AST_DATA_TYPE_KIND_REFERENCE)
            target_type = target_type->child;

        if(!canImplicitConvertDataType(expr_type, node->rhs, target_type))
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
            printf("Type error: index assignment requires a value receiver at file %s, line %d, column %d\n",
                   node->lhs->filename, node->lhs->line_number, node->lhs->column_number);
            exit(1);
        }

        ASTDataType *array_type = owner_type.data_type;
        if(array_type->kind == AST_DATA_TYPE_KIND_POINTER || array_type->kind == AST_DATA_TYPE_KIND_REFERENCE)
            array_type = array_type->child;
        if(!isArrayDataType(array_type))
        {
            printf("Type error: index assignment requires an array receiver at file %s, line %d, column %d\n",
                   node->lhs->filename, node->lhs->line_number, node->lhs->column_number);
            exit(1);
        }

        if(!isMutableAddressableExpr(node->lhs, scope))
        {
            printf("Type error: cannot assign through immutable index access at file %s, line %d, column %d\n",
                   node->lhs->filename, node->lhs->line_number, node->lhs->column_number);
            exit(1);
        }

        if(!canImplicitConvertDataType(expr_type, node->rhs, array_type->child))
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
        TypeSystemExprType callee_type = inferExprType(node->rhs->lhs, scope);
        if(callee_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE && callee_type.data_type->kind == AST_DATA_TYPE_KIND_FUNCTION)
            checkFunctionCallArgumentSemantics(node->rhs, callee_type.data_type, scope);
    }

    if(isExplicitDeclared(node))
    {
        TypeSystemExprType expr_type = {0};
        if(local_variable_info != NULL)
        {
            printf("Variable %s has already been declared and cannot be declared again\n", node->identifier);
            exit(1);
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

        if(node->rhs->kind == AST_EXPR_ARRAY_LITERAL && node->rhs->lhs == NULL)
        {
            if(declared_type->kind != AST_DATA_TYPE_KIND_ARRAY || declared_type->array_length != 0)
            {
                printf("Type error: empty array literal requires an explicit zero-length array type at file %s, line %d, column %d\n",
                       node->rhs->filename, node->rhs->line_number, node->rhs->column_number);
                exit(1);
            }
            expr_type = newValueExprType(declared_type);
        }
        else
            expr_type = inferExprType(node->rhs, scope);

        if(isReferenceDataType(declared_type))
        {
            if(node->rhs->kind != AST_EXPR_VARIABLE && node->rhs->kind != AST_EXPR_DEREF)
            {
                printf("Type error: reference initialization requires an addressable expression at file %s, line %d, column %d\n",
                       node->rhs->filename, node->rhs->line_number, node->rhs->column_number);
                exit(1);
            }

            if(declared_type->mutable && !isMutableAddressableExpr(node->rhs, scope))
            {
                printf("Type error: mutable reference requires a mutable expression at file %s, line %d, column %d\n",
                       node->rhs->filename, node->rhs->line_number, node->rhs->column_number);
                exit(1);
            }

            TypeSystemExprType rhs_value_type = inferExprType(node->rhs, scope);
            ASTDataType *rhs_target_type = getReferenceTargetType(rhs_value_type.data_type);
            if(!isSameDataType(rhs_target_type, declared_type->child))
                typeErrorAssign(node, node->rhs, rhs_value_type, declared_type);
        }
        else
        {
            if(!canImplicitConvertDataType(expr_type, node->rhs, declared_type))
                typeErrorAssign(node, node->rhs, expr_type, declared_type);
        }

        VariableInfo *new_variable_info = declareVariableInfo(scope, node->identifier);
        new_variable_info->mutable = node->modifier.mutable;
        new_variable_info->data_type = cloneDataType(node->data_type);
        if(expr_type.kind == TYPE_SYSTEM_EXPR_TYPE_TYPE)
            new_variable_info->type_value = cloneDataType(expr_type.data_type);
        if(node->rhs->kind == AST_EXPR_FUNCTION)
            new_variable_info->function_value = node->rhs;
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
            if(node->rhs->kind == AST_EXPR_FUNCTION)
                new_variable_info->function_value = node->rhs;

            if(!canImplicitConvertDataType(expr_type, node->rhs, declared_type))
                typeErrorAssign(node, node->rhs, expr_type, declared_type);
        }
        else
        {
            TypeSystemExprType expr_type = inferExprType(node->rhs, scope);
            ASTDataType *target_type = resolved_variable_info->data_type;
            bool assign_through_reference = isReferenceDataType(target_type);

            if(assign_through_reference)
                target_type = target_type->child;

            if(!canImplicitConvertDataType(expr_type, node->rhs, target_type))
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
            printf("Return statement is only allowed inside a function at file %s, line %d, column %d\n",
                   node->filename, node->line_number, node->column_number);
            exit(1);
        }

        if(function_context->inside_defer)
        {
            printf("Return statement is not allowed inside defer at file %s, line %d, column %d\n",
                   node->filename, node->line_number, node->column_number);
            exit(1);
        }

        if(node->lhs)
        {
            if(node->lhs->kind == AST_EXPR_CALL)
            {
                TypeSystemExprType callee_type = inferExprType(node->lhs->lhs, scope);
                if(callee_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE && callee_type.data_type->kind == AST_DATA_TYPE_KIND_FUNCTION)
                    checkFunctionCallArgumentSemantics(node->lhs, callee_type.data_type, scope);
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
            printf("%s statement is only allowed inside a loop at file %s, line %d, column %d\n",
                   node->kind == AST_STATEMENT_BREAK ? "Break" : "Continue",
                   node->filename, node->line_number, node->column_number);
            exit(1);
        }

        if(function_context->inside_defer)
        {
            printf("%s statement is not allowed inside defer at file %s, line %d, column %d\n",
                   node->kind == AST_STATEMENT_BREAK ? "Break" : "Continue",
                   node->filename, node->line_number, node->column_number);
            exit(1);
        }
        return;
    }

    if(node->kind == AST_STATEMENT_EXPR)
    {
        if(node->lhs->kind == AST_EXPR_CALL)
        {
            TypeSystemExprType callee_type = inferExprType(node->lhs->lhs, scope);
            if(callee_type.kind != TYPE_SYSTEM_EXPR_TYPE_VALUE || callee_type.data_type->kind != AST_DATA_TYPE_KIND_FUNCTION)
            {
                printf("Type error: called expression is not a function at file %s, line %d, column %d\n",
                       node->lhs->filename, node->lhs->line_number, node->lhs->column_number);
                exit(1);
            }
            checkFunctionCallArgumentSemantics(node->lhs, callee_type.data_type, scope);
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
        ScopeFrame loop_scope = {0};
        initScopeFrame(&loop_scope, scope);

        checkStatementTypes(node->lhs, &loop_scope, function_context);
        if(node->rhs)
            checkConditionType(node->rhs, &loop_scope);
        checkStatementTypes(node->extra, &loop_scope, function_context);

        FunctionContext loop_context = {0};
        checkStatementTypes(node->body, &loop_scope, deriveLoopContext(function_context, &loop_context));
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

    printf("Type error: unsupported statement kind %s\n", astNodeKindToString(node->kind));
    exit(1);
}

void checkAssignTypesInBlock(ASTNode *block, ScopeFrame *parent_scope, FunctionContext *function_context)
{
    ScopeFrame current_scope = {0};
    initScopeFrame(&current_scope, parent_scope);

    ASTNode *node = block->lhs;
    while(node)
    {
        checkStatementTypes(node, &current_scope, function_context);
        node = node->next;
    }
}

void checkAssignTypes(ASTNode *root)
{
    if(root == NULL || root->lhs == NULL || root->lhs->kind != AST_BLOCK)
    {
        printf("Type error: root should contain a top-level block\n");
        exit(1);
    }

    checkAssignTypesInBlock(root->lhs, NULL, NULL);
}

#endif /* SEMANTIC_H */
