#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include "AST.h"
#include <string.h>

typedef struct VariableInfo {
    bool mutable;
    ASTDataType *data_type;
    char identifier[MAX_IDENTIFIER_LENGTH];
} VariableInfo;

typedef struct FunctionContext {
    struct FunctionContext *parent;
    ASTDataType *return_data_type;
} FunctionContext;

typedef struct ScopeFrame {
    struct ScopeFrame *parent;
    VariableInfo variable_infos[1024];
    int variable_count;
} ScopeFrame;

void initScopeFrame(ScopeFrame *scope, ScopeFrame *parent)
{
    memset(scope, 0, sizeof(ScopeFrame));
    scope->parent = parent;
}

int findVariableInfoInScope(ScopeFrame *scope, const char *identifier)
{
    for(int i = 0;i<scope->variable_count;i++)
    {
        if(strcmp(scope->variable_infos[i].identifier, identifier) == 0)
            return i;
    }
    return -1;
}

VariableInfo* findVariableInfo(ScopeFrame *scope, const char *identifier)
{
    ScopeFrame *current = scope;
    while(current)
    {
        int index = findVariableInfoInScope(current, identifier);
        if(index >= 0)
            return &(current->variable_infos[index]);
        current = current->parent;
    }
    return NULL;
}

VariableInfo* declareVariableInfo(ScopeFrame *scope, const char *identifier)
{
    VariableInfo *variable_info = &(scope->variable_infos[scope->variable_count++]);
    memset(variable_info, 0, sizeof(VariableInfo));
    strcpy(variable_info->identifier, identifier);
    return variable_info;
}

#endif /* SYMBOL_TABLE_H */
