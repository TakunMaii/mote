#ifndef AST_H
#define AST_H

#include <stdio.h>
#include "Token.h"
#include <stdbool.h>

typedef enum ASTNodeKind {
    AST_START_OF_CODE,
    AST_END_OF_CODE,

    AST_ASSIGN,// assign or decl

    AST_EXPR_LITERAL_INTEGER,
    AST_EXPR_LITERAL_FLOAT,
} ASTNodeKind;

typedef struct {
    bool mutable;
} ASTAssignModifier;

typedef struct ASTNode {
    struct ASTNode *next;
    ASTNodeKind kind;

    struct ASTNode *lhs;
    struct ASTNode *rhs;// assign or decl use this as expr
    
    // literal value
    long long int literal_integer;
    long double literal_float;

    // assign or decl
    ASTAssignModifier modifier;
    char identifier[MAX_IDENTIFIER_LENGTH];

} ASTNode;

ASTNode* newASTNode(ASTNodeKind kind)
{
    ASTNode *node = (ASTNode*) malloc(sizeof(ASTNode));
    node->kind = kind;
    node->next = NULL;
    return node;
}

void printASTNode(ASTNode node)
{
    switch(node.kind)
    {
        case AST_ASSIGN: {
            printf("AST_ASSIGN: idenifier(%s) = ", node.identifier);
            printASTNode(*(node.rhs));
            printf("\n");
        } break;
        case AST_EXPR_LITERAL_INTEGER: {
            printf("AST_EXPR_LITERAL_INTEGER(%lld)", node.literal_integer);
        } break;
        case AST_EXPR_LITERAL_FLOAT: {
            printf("AST_EXPR_LITERAL_FLOAT(%Lf)", node.literal_float);
        } break;
        case AST_START_OF_CODE: {
            printf("AST_START_OF_CODE\n");
        } break;
        case AST_END_OF_CODE: {
            printf("AST_END_OF_CODE\n");
        } break;
        default:
            printf("printASTNode: unknown AST node kind\n");
            exit(1);
    }
}

#endif /* AST_H */

