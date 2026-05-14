#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "AST.h"
#include <stdbool.h>
#include <string.h>

typedef struct VariableInfo {
    bool mutable;
    char identifier[MAX_IDENTIFIER_LENGTH];
} VariableInfo;

bool isExplicitDeclared(ASTNode *node)
{
    return node->modifier.mutable || node->data_type != AST_DATA_TYPE_INFER;
}

int findVariableInfo(VariableInfo *variable_infos, int variable_count, const char *identifier)
{
    for(int i = 0;i<variable_count;i++)
    {
        if(strcmp(variable_infos[i].identifier, identifier) == 0)
            return i;
    }
    return -1;
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

void checkAssignMutability(ASTNode *root)
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

#endif /* SEMANTIC_H */
