#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include "Diagnostic.h"
#include "AST.h"
#include <stdlib.h>
#include <string.h>

typedef struct VariableInfo {
    bool is_compile_time_constant;
    bool predeclared;
    bool resolving;
    ASTDataType *data_type;
    ASTDataType *type_value;
    ASTNode *function_value;
    ASTNode *extern_value;
    ASTNode *value_expr;
    ASTOperatorKind operator_kind;
    char identifier[MAX_IDENTIFIER_LENGTH];
} VariableInfo;

typedef struct TypeInfo {
    ASTDataType *data_type;
    bool predeclared;
    char identifier[MAX_IDENTIFIER_LENGTH];
} TypeInfo;

typedef struct FunctionContext {
    struct FunctionContext *parent;
    bool active;
    ASTDataType *return_data_type;
    ASTDataType *self_data_type;
    bool self_available_as_type_value;
    int loop_depth;
    bool inside_defer;
} FunctionContext;

#define SCOPE_MAX_VARIABLE_INFOS 1024
#define SCOPE_MAX_TYPE_INFOS 256

typedef struct ScopeFrame {
    struct ScopeFrame *parent;
    VariableInfo variable_infos[SCOPE_MAX_VARIABLE_INFOS];
    int variable_count;
    TypeInfo type_infos[SCOPE_MAX_TYPE_INFOS];
    int type_count;
    ASTNode *instantiating_function;
    ASTNode *instantiation_site;
    ASTDataType *instantiating_type_result;
} ScopeFrame;

void initScopeFrame(ScopeFrame *scope, ScopeFrame *parent)
{
    memset(scope, 0, sizeof(ScopeFrame));
    scope->parent = parent;
}

ScopeFrame* newScopeFrame(ScopeFrame *parent)
{
    ScopeFrame *scope = (ScopeFrame*) malloc(sizeof(ScopeFrame));
    if(scope == NULL)
        diagnosticAbortInternal("scope allocation failed", NULL);
    initScopeFrame(scope, parent);
    return scope;
}

ScopeFrame* snapshotScopeFrame(ScopeFrame *scope)
{
    if(scope == NULL)
        return NULL;

    ScopeFrame *copy = (ScopeFrame*) malloc(sizeof(ScopeFrame));
    if(copy == NULL)
        diagnosticAbortInternal("scope clone allocation failed", NULL);
    memset(copy, 0, sizeof(ScopeFrame));
    copy->parent = scope->parent;
    copy->instantiating_function = scope->instantiating_function;
    copy->instantiation_site = scope->instantiation_site;
    copy->instantiating_type_result = NULL;

    copy->variable_count = scope->variable_count;
    for(int i = 0; i < scope->variable_count; i++)
    {
        VariableInfo *src = &(scope->variable_infos[i]);
        VariableInfo *dst = &(copy->variable_infos[i]);
        *dst = *src;
        dst->data_type = cloneDataType(src->data_type);
        dst->type_value = cloneDataType(src->type_value);
        dst->value_expr = NULL;
        dst->function_value = src->function_value;
        dst->extern_value = src->extern_value;
    }

    copy->type_count = scope->type_count;
    for(int i = 0; i < scope->type_count; i++)
    {
        TypeInfo *src = &(scope->type_infos[i]);
        TypeInfo *dst = &(copy->type_infos[i]);
        *dst = *src;
        dst->data_type = cloneDataType(src->data_type);
    }

    return copy;
}

void deleteScopeFrame(ScopeFrame *scope)
{
    if(scope != NULL)
        free(scope);
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
    if(scope->variable_count >= SCOPE_MAX_VARIABLE_INFOS)
        diagnosticAbortFormatted("S2001",
                                 makeSourceSpan(NULL, 0, 0, 0, 0),
                                 NULL,
                                 "scope variable capacity exceeded while declaring `%s`",
                                 astUserFacingIdentifier(identifier));

    VariableInfo *variable_info = &(scope->variable_infos[scope->variable_count++]);
    memset(variable_info, 0, sizeof(VariableInfo));
    strcpy(variable_info->identifier, identifier);
    return variable_info;
}

int findTypeInfoInScope(ScopeFrame *scope, const char *identifier)
{
    for(int i = 0;i<scope->type_count;i++)
    {
        if(strcmp(scope->type_infos[i].identifier, identifier) == 0)
            return i;
    }
    return -1;
}

TypeInfo* findTypeInfo(ScopeFrame *scope, const char *identifier)
{
    ScopeFrame *current = scope;
    while(current)
    {
        int index = findTypeInfoInScope(current, identifier);
        if(index >= 0)
            return &(current->type_infos[index]);
        current = current->parent;
    }
    return NULL;
}

TypeInfo* declareTypeInfo(ScopeFrame *scope, const char *identifier)
{
    if(scope->type_count >= SCOPE_MAX_TYPE_INFOS)
        diagnosticAbortFormatted("S2002",
                                 makeSourceSpan(NULL, 0, 0, 0, 0),
                                 NULL,
                                 "scope type capacity exceeded while declaring `%s`",
                                 astUserFacingIdentifier(identifier));

    TypeInfo *type_info = &(scope->type_infos[scope->type_count++]);
    memset(type_info, 0, sizeof(TypeInfo));
    strcpy(type_info->identifier, identifier);
    return type_info;
}

#endif /* SYMBOL_TABLE_H */
