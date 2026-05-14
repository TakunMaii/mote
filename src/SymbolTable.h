#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include "AST.h"
#include <string.h>

typedef struct VariableInfo {
    bool mutable;
    ASTDataType data_type;
    char identifier[MAX_IDENTIFIER_LENGTH];
} VariableInfo;

int findVariableInfo(VariableInfo *variable_infos, int variable_count, const char *identifier)
{
    for(int i = 0;i<variable_count;i++)
    {
        if(strcmp(variable_infos[i].identifier, identifier) == 0)
            return i;
    }
    return -1;
}

#endif /* SYMBOL_TABLE_H */
