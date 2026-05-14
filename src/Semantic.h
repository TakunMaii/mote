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

bool isReferenceVariable(VariableInfo *variable_infos, int variable_count, const char *identifier)
{
    int variable_index = findVariableInfo(variable_infos, variable_count, identifier);
    if(variable_index < 0)
        return false;
    return isReferenceDataType(variable_infos[variable_index].data_type);
}

void checkExprDeclaredVariable(ASTNode *node, VariableInfo *variable_infos, int variable_count)
{
    if(node == NULL)
        return;

    if(node->kind == AST_EXPR_VARIABLE)
    {
        if(findVariableInfo(variable_infos, variable_count, node->identifier) < 0)
        {
            printf("Use of undeclared variable %s in expression\n", node->identifier);
            exit(1);
        }
        return;
    }

    checkExprDeclaredVariable(node->lhs, variable_infos, variable_count);
    checkExprDeclaredVariable(node->rhs, variable_infos, variable_count);
}

void checkAssignSemantics(ASTNode *root)
{
    VariableInfo variable_infos[1024] = {0};
    int variable_count = 0;

    ASTNode *node = root;
    while(node)
    {
        if(node->kind == AST_ASSIGN)
        {
            checkExprDeclaredVariable(node->rhs, variable_infos, variable_count);

            if(node->lhs->kind == AST_EXPR_DEREF)
            {
                checkExprDeclaredVariable(node->lhs->lhs, variable_infos, variable_count);
                node = node->next;
                continue;
            }

            int variable_index = findVariableInfo(variable_infos, variable_count, node->identifier);
            if(variable_index < 0)
            {
                strcpy(variable_infos[variable_count].identifier, node->identifier);
                variable_infos[variable_count].mutable = node->modifier.mutable;
                variable_infos[variable_count].data_type = newInferDataType();
                variable_count ++;
            }
            else
            {
                if(node->lhs->kind != AST_EXPR_VARIABLE)
                {
                    printf("Semantic error: only variable or deref can be assigned at file %s, line %d, column %d\n",
                           node->filename, node->line_number, node->column_number);
                    exit(1);
                }

                if(!variable_infos[variable_index].mutable)
                {
                    if(!isReferenceDataType(variable_infos[variable_index].data_type))
                    {
                        printf("Cannot assign to immutable variable %s\n", node->identifier);
                        exit(1);
                    }
                }

                if(isExplicitDeclared(node))
                {
                    printf("Variable %s has already been declared and cannot be declared again\n", node->identifier);
                    exit(1);
                }
            }
        }

        node = node->next;
    }
}

void checkAssignTypes(ASTNode *root)
{
    VariableInfo variable_infos[1024] = {0};
    int variable_count = 0;

    ASTNode *node = root;
    while(node)
    {
        if(node->kind == AST_ASSIGN)
        {
            if(node->lhs->kind == AST_EXPR_DEREF)
            {
                TypeSystemExprType expr_type = inferExprType(node->rhs, variable_infos, variable_count);
                TypeSystemExprType lhs_type = inferExprType(node->lhs->lhs, variable_infos, variable_count);
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
                {
                    typeErrorAssign(node, node->rhs, expr_type, lhs_type.data_type->child);
                }
                node->data_type = cloneDataType(lhs_type.data_type->child);
                node = node->next;
                continue;
            }

            TypeSystemExprType expr_type = inferExprType(node->rhs, variable_infos, variable_count);

            int variable_index = findVariableInfo(variable_infos, variable_count, node->identifier);
            if(variable_index < 0)
            {
                ASTDataType *declared_type = node->data_type;
                if(isInferDataType(declared_type))
                {
                    declared_type = inferDeclaredTypeFromExpr(node->rhs, variable_infos, variable_count);
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

                    bool requires_mutable = declared_type->mutable;
                    if(requires_mutable && !isMutableAddressableExpr(node->rhs, variable_infos, variable_count))
                    {
                        printf("Type error: mutable reference requires a mutable expression at file %s, line %d, column %d\n",
                               node->rhs->filename, node->rhs->line_number, node->rhs->column_number);
                        exit(1);
                    }

                    TypeSystemExprType rhs_value_type = inferExprType(node->rhs, variable_infos, variable_count);
                    ASTDataType *rhs_target_type = getReferenceTargetType(rhs_value_type.data_type);
                    if(!isSameDataType(rhs_target_type, declared_type->child))
                    {
                        typeErrorAssign(node, node->rhs, rhs_value_type, declared_type);
                    }
                }
                else
                {
                    if(!canImplicitConvertDataType(expr_type, node->rhs, declared_type))
                    {
                        typeErrorAssign(node, node->rhs, expr_type, declared_type);
                    }
                }

                strcpy(variable_infos[variable_count].identifier, node->identifier);
                variable_infos[variable_count].mutable = node->modifier.mutable;
                variable_infos[variable_count].data_type = cloneDataType(declared_type);
                variable_count ++;
            }
            else
            {
                ASTDataType *target_type = variable_infos[variable_index].data_type;
                bool assign_through_reference = isReferenceDataType(target_type);

                if(assign_through_reference)
                    target_type = target_type->child;

                if(!canImplicitConvertDataType(expr_type, node->rhs, target_type))
                {
                    typeErrorAssign(node, node->rhs, expr_type, target_type);
                }

                if(assign_through_reference)
                    node->data_type = cloneDataType(target_type);
                else
                    node->data_type = cloneDataType(variable_infos[variable_index].data_type);
            }
        }

        node = node->next;
    }
}

#endif /* SEMANTIC_H */
