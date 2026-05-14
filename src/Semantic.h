#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "AST.h"
#include "SymbolTable.h"
#include "TypeSystem.h"
#include <stdbool.h>
#include <string.h>

bool isExplicitDeclared(ASTNode *node)
{
    return node->modifier.mutable || node->data_type != AST_DATA_TYPE_INFER;
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

            int variable_index = findVariableInfo(variable_infos, variable_count, node->identifier);
            if(variable_index < 0)
            {
                strcpy(variable_infos[variable_count].identifier, node->identifier);
                variable_infos[variable_count].mutable = node->modifier.mutable;
                variable_infos[variable_count].data_type = AST_DATA_TYPE_INFER;
                variable_count ++;
            }
            else
            {
                if(!variable_infos[variable_index].mutable)
                {
                    printf("Cannot assign to immutable variable %s\n", node->identifier);
                    exit(1);
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
            TypeSystemDataType expr_type = inferExprType(node->rhs, variable_infos, variable_count);

            int variable_index = findVariableInfo(variable_infos, variable_count, node->identifier);
            if(variable_index < 0)
            {
                ASTDataType declared_type = node->data_type;
                if(declared_type == AST_DATA_TYPE_INFER)
                {
                    declared_type = typeSystemDataTypeToAstDataType(
                        inferDeclaredTypeFromExpr(node->rhs, variable_infos, variable_count)
                    );
                    node->data_type = declared_type;
                }
                else
                {
                    TypeSystemDataType target_type = astDataTypeToTypeSystemDataType(declared_type);
                    if(!canImplicitConvertType(expr_type, node->rhs, target_type))
                    {
                        typeErrorAssign(node, node->rhs, expr_type, target_type);
                    }
                }

                strcpy(variable_infos[variable_count].identifier, node->identifier);
                variable_infos[variable_count].mutable = node->modifier.mutable;
                variable_infos[variable_count].data_type = declared_type;
                variable_count ++;
            }
            else
            {
                TypeSystemDataType target_type = astDataTypeToTypeSystemDataType(variable_infos[variable_index].data_type);
                if(!canImplicitConvertType(expr_type, node->rhs, target_type))
                {
                    typeErrorAssign(node, node->rhs, expr_type, target_type);
                }

                node->data_type = variable_infos[variable_index].data_type;
            }
        }

        node = node->next;
    }
}

#endif /* SEMANTIC_H */
