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

void checkExprDeclaredVariable(ASTNode *node, ScopeFrame *scope);
void checkAssignSemanticsInBlock(ASTNode *block, ScopeFrame *parent_scope, FunctionContext *function_context);
void checkAssignTypesInBlock(ASTNode *block, ScopeFrame *parent_scope, FunctionContext *function_context);

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
        parameter = parameter->next;
    }
}

void checkFunctionExprSemantics(ASTNode *node, ScopeFrame *scope)
{
    ScopeFrame function_scope = {0};
    initScopeFrame(&function_scope, scope);
    declareFunctionParameters(node->parameters, &function_scope);

    FunctionContext function_context = {0};
    function_context.return_data_type = node->return_data_type;

    checkAssignSemanticsInBlock(node->body, &function_scope, &function_context);
}

void checkExprDeclaredVariable(ASTNode *node, ScopeFrame *scope)
{
    if(node == NULL)
        return;

    if(node->kind == AST_EXPR_VARIABLE)
    {
        if(findVariableInfo(scope, node->identifier) == NULL)
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

    checkExprDeclaredVariable(node->lhs, scope);
    checkExprDeclaredVariable(node->rhs, scope);
}

void checkAssignSemanticsInBlock(ASTNode *block, ScopeFrame *parent_scope, FunctionContext *function_context)
{
    ScopeFrame current_scope = {0};
    initScopeFrame(&current_scope, parent_scope);

    ASTNode *node = block->lhs;
    while(node)
    {
        if(node->kind == AST_BLOCK)
        {
            checkAssignSemanticsInBlock(node, &current_scope, function_context);
            node = node->next;
            continue;
        }

        if(node->kind == AST_STATEMENT_RETURN)
        {
            if(function_context == NULL)
            {
                printf("Return statement is only allowed inside a function at file %s, line %d, column %d\n",
                       node->filename, node->line_number, node->column_number);
                exit(1);
            }

            checkExprDeclaredVariable(node->lhs, &current_scope);
            node = node->next;
            continue;
        }

        if(node->kind == AST_STATEMENT_EXPR)
        {
            checkExprDeclaredVariable(node->lhs, &current_scope);
            node = node->next;
            continue;
        }

        if(node->kind == AST_ASSIGN)
        {
            checkExprDeclaredVariable(node->rhs, &current_scope);

            if(node->lhs->kind == AST_EXPR_DEREF)
            {
                checkExprDeclaredVariable(node->lhs->lhs, &current_scope);
                node = node->next;
                continue;
            }

            if(node->lhs->kind != AST_EXPR_VARIABLE)
            {
                printf("Semantic error: only variable or deref can be assigned at file %s, line %d, column %d\n",
                       node->filename, node->line_number, node->column_number);
                exit(1);
            }

            VariableInfo *local_variable_info = NULL;
            int local_index = findVariableInfoInScope(&current_scope, node->identifier);
            if(local_index >= 0)
                local_variable_info = &(current_scope.variable_infos[local_index]);

            if(isExplicitDeclared(node))
            {
                if(local_variable_info != NULL)
                {
                    printf("Variable %s has already been declared and cannot be declared again\n", node->identifier);
                    exit(1);
                }

                VariableInfo *new_variable_info = declareVariableInfo(&current_scope, node->identifier);
                new_variable_info->mutable = node->modifier.mutable;
                new_variable_info->data_type = newInferDataType();
            }
            else
            {
                VariableInfo *resolved_variable_info = local_variable_info;
                if(resolved_variable_info == NULL)
                    resolved_variable_info = findVariableInfo(current_scope.parent, node->identifier);

                if(resolved_variable_info == NULL)
                {
                    VariableInfo *new_variable_info = declareVariableInfo(&current_scope, node->identifier);
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

void checkFunctionExprTypes(ASTNode *node, ScopeFrame *scope)
{
    ScopeFrame function_scope = {0};
    initScopeFrame(&function_scope, scope);
    declareFunctionParameters(node->parameters, &function_scope);

    FunctionContext function_context = {0};
    function_context.return_data_type = node->return_data_type;
    checkAssignTypesInBlock(node->body, &function_scope, &function_context);
}

void checkAssignTypesInBlock(ASTNode *block, ScopeFrame *parent_scope, FunctionContext *function_context)
{
    ScopeFrame current_scope = {0};
    initScopeFrame(&current_scope, parent_scope);

    ASTNode *node = block->lhs;
    while(node)
    {
        if(node->kind == AST_BLOCK)
        {
            checkAssignTypesInBlock(node, &current_scope, function_context);
            node = node->next;
            continue;
        }

        if(node->kind == AST_STATEMENT_RETURN)
        {
            if(function_context == NULL)
            {
                printf("Return statement is only allowed inside a function at file %s, line %d, column %d\n",
                       node->filename, node->line_number, node->column_number);
                exit(1);
            }

            if(node->lhs)
            {
                if(node->lhs->kind == AST_EXPR_CALL)
                {
                    TypeSystemExprType callee_type = inferExprType(node->lhs->lhs, &current_scope);
                    if(callee_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE && callee_type.data_type->kind == AST_DATA_TYPE_KIND_FUNCTION)
                        checkFunctionCallArgumentSemantics(node->lhs, callee_type.data_type, &current_scope);
                }
                else if(node->lhs->kind == AST_EXPR_FUNCTION)
                {
                    checkFunctionExprTypes(node->lhs, &current_scope);
                }
                else
                {
                    inferExprType(node->lhs, &current_scope);
                }
            }

            checkFunctionReturnStatement(node, &current_scope, function_context);
            node = node->next;
            continue;
        }

        if(node->kind == AST_STATEMENT_EXPR)
        {
            if(node->lhs->kind == AST_EXPR_CALL)
            {
                TypeSystemExprType callee_type = inferExprType(node->lhs->lhs, &current_scope);
                if(callee_type.kind != TYPE_SYSTEM_EXPR_TYPE_VALUE || callee_type.data_type->kind != AST_DATA_TYPE_KIND_FUNCTION)
                {
                    printf("Type error: called expression is not a function at file %s, line %d, column %d\n",
                           node->lhs->filename, node->lhs->line_number, node->lhs->column_number);
                    exit(1);
                }
                checkFunctionCallArgumentSemantics(node->lhs, callee_type.data_type, &current_scope);
                inferExprType(node->lhs, &current_scope);
            }
            else if(node->lhs->kind == AST_EXPR_FUNCTION)
            {
                checkFunctionExprTypes(node->lhs, &current_scope);
            }
            else
            {
                inferExprType(node->lhs, &current_scope);
            }
            node = node->next;
            continue;
        }

        if(node->kind == AST_ASSIGN)
        {
            if(node->lhs->kind == AST_EXPR_DEREF)
            {
                TypeSystemExprType expr_type = inferExprType(node->rhs, &current_scope);
                TypeSystemExprType lhs_type = inferExprType(node->lhs->lhs, &current_scope);
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
                node = node->next;
                continue;
            }

            VariableInfo *local_variable_info = NULL;
            int local_index = findVariableInfoInScope(&current_scope, node->identifier);
            if(local_index >= 0)
                local_variable_info = &(current_scope.variable_infos[local_index]);

            if(node->rhs->kind == AST_EXPR_FUNCTION)
                checkFunctionExprTypes(node->rhs, &current_scope);
            if(node->rhs->kind == AST_EXPR_CALL)
            {
                TypeSystemExprType callee_type = inferExprType(node->rhs->lhs, &current_scope);
                if(callee_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE && callee_type.data_type->kind == AST_DATA_TYPE_KIND_FUNCTION)
                    checkFunctionCallArgumentSemantics(node->rhs, callee_type.data_type, &current_scope);
            }

            if(isExplicitDeclared(node))
            {
                TypeSystemExprType expr_type = inferExprType(node->rhs, &current_scope);
                if(local_variable_info != NULL)
                {
                    printf("Variable %s has already been declared and cannot be declared again\n", node->identifier);
                    exit(1);
                }

                ASTDataType *declared_type = node->data_type;
                if(isInferDataType(declared_type))
                {
                    declared_type = inferDeclaredTypeFromExpr(node->rhs, &current_scope);
                    node->data_type = declared_type;
                }
                else if(isReferenceDataType(declared_type))
                {
                    if(node->rhs->kind != AST_EXPR_VARIABLE && node->rhs->kind != AST_EXPR_DEREF)
                    {
                        printf("Type error: reference initialization requires an addressable expression at file %s, line %d, column %d\n",
                               node->rhs->filename, node->rhs->line_number, node->rhs->column_number);
                        exit(1);
                    }

                    if(declared_type->mutable && !isMutableAddressableExpr(node->rhs, &current_scope))
                    {
                        printf("Type error: mutable reference requires a mutable expression at file %s, line %d, column %d\n",
                               node->rhs->filename, node->rhs->line_number, node->rhs->column_number);
                        exit(1);
                    }

                    TypeSystemExprType rhs_value_type = inferExprType(node->rhs, &current_scope);
                    ASTDataType *rhs_target_type = getReferenceTargetType(rhs_value_type.data_type);
                    if(!isSameDataType(rhs_target_type, declared_type->child))
                        typeErrorAssign(node, node->rhs, rhs_value_type, declared_type);
                }
                else
                {
                    if(!canImplicitConvertDataType(expr_type, node->rhs, declared_type))
                        typeErrorAssign(node, node->rhs, expr_type, declared_type);
                }

                VariableInfo *new_variable_info = declareVariableInfo(&current_scope, node->identifier);
                new_variable_info->mutable = node->modifier.mutable;
                new_variable_info->data_type = cloneDataType(node->data_type);
            }
            else
            {
                VariableInfo *resolved_variable_info = local_variable_info;
                if(resolved_variable_info == NULL)
                    resolved_variable_info = findVariableInfo(current_scope.parent, node->identifier);

                if(resolved_variable_info == NULL)
                {
                    TypeSystemExprType expr_type = inferExprType(node->rhs, &current_scope);
                    ASTDataType *declared_type = inferDeclaredTypeFromExpr(node->rhs, &current_scope);
                    node->data_type = declared_type;

                    VariableInfo *new_variable_info = declareVariableInfo(&current_scope, node->identifier);
                    new_variable_info->mutable = false;
                    new_variable_info->data_type = cloneDataType(declared_type);

                    if(!canImplicitConvertDataType(expr_type, node->rhs, declared_type))
                        typeErrorAssign(node, node->rhs, expr_type, declared_type);
                }
                else
                {
                    TypeSystemExprType expr_type = inferExprType(node->rhs, &current_scope);
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
