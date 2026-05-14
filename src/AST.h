#ifndef AST_H
#define AST_H

#include <stdio.h>
#include "Token.h"
#include <stdbool.h>

typedef enum ASTNodeKind {
    AST_START_OF_CODE,
    AST_END_OF_CODE,

    AST_ASSIGN,// assign or decl

    AST_EXPR_LOGICAL_OR,
    AST_EXPR_LOGICAL_AND,
    AST_EXPR_BIT_OR,
    AST_EXPR_BIT_XOR,
    AST_EXPR_BIT_AND,
    AST_EXPR_EQUAL,
    AST_EXPR_NOT_EQUAL,
    AST_EXPR_LESS,
    AST_EXPR_LESS_EQUAL,
    AST_EXPR_GREATER,
    AST_EXPR_GREATER_EQUAL,
    AST_EXPR_SHIFT_LEFT,
    AST_EXPR_SHIFT_RIGHT,
    AST_EXPR_MUL,
    AST_EXPR_DIV,
    AST_EXPR_MOD,
    AST_EXPR_ADD,
    AST_EXPR_SUB,
    AST_EXPR_UNARY_PLUS,
    AST_EXPR_UNARY_MINUS,
    AST_EXPR_UNARY_LOGICAL_NOT,
    AST_EXPR_UNARY_BIT_NOT,
    AST_EXPR_PARENTHESIS,
    AST_EXPR_VARIABLE,
    AST_EXPR_LITERAL_BOOL,
    AST_EXPR_LITERAL_INTEGER,
    AST_EXPR_LITERAL_FLOAT,
} ASTNodeKind;

typedef struct {
    bool mutable;
} ASTAssignModifier;

typedef struct ASTNode {
    struct ASTNode *next;
    ASTNodeKind kind;

    struct ASTNode *lhs;// parenthesis, binary expr use this as left hand side
    struct ASTNode *rhs;// assign or decl use this as expr
    
    // literal value
    bool literal_bool;
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

const char* astNodeKindToString(ASTNodeKind kind)
{
    switch(kind)
    {
        case AST_START_OF_CODE: return "AST_START_OF_CODE";
        case AST_END_OF_CODE: return "AST_END_OF_CODE";
        case AST_ASSIGN: return "AST_ASSIGN";
        case AST_EXPR_LOGICAL_OR: return "AST_EXPR_LOGICAL_OR";
        case AST_EXPR_LOGICAL_AND: return "AST_EXPR_LOGICAL_AND";
        case AST_EXPR_BIT_OR: return "AST_EXPR_BIT_OR";
        case AST_EXPR_BIT_XOR: return "AST_EXPR_BIT_XOR";
        case AST_EXPR_BIT_AND: return "AST_EXPR_BIT_AND";
        case AST_EXPR_EQUAL: return "AST_EXPR_EQUAL";
        case AST_EXPR_NOT_EQUAL: return "AST_EXPR_NOT_EQUAL";
        case AST_EXPR_LESS: return "AST_EXPR_LESS";
        case AST_EXPR_LESS_EQUAL: return "AST_EXPR_LESS_EQUAL";
        case AST_EXPR_GREATER: return "AST_EXPR_GREATER";
        case AST_EXPR_GREATER_EQUAL: return "AST_EXPR_GREATER_EQUAL";
        case AST_EXPR_SHIFT_LEFT: return "AST_EXPR_SHIFT_LEFT";
        case AST_EXPR_SHIFT_RIGHT: return "AST_EXPR_SHIFT_RIGHT";
        case AST_EXPR_MUL: return "AST_EXPR_MUL";
        case AST_EXPR_DIV: return "AST_EXPR_DIV";
        case AST_EXPR_MOD: return "AST_EXPR_MOD";
        case AST_EXPR_ADD: return "AST_EXPR_ADD";
        case AST_EXPR_SUB: return "AST_EXPR_SUB";
        case AST_EXPR_UNARY_PLUS: return "AST_EXPR_UNARY_PLUS";
        case AST_EXPR_UNARY_MINUS: return "AST_EXPR_UNARY_MINUS";
        case AST_EXPR_UNARY_LOGICAL_NOT: return "AST_EXPR_UNARY_LOGICAL_NOT";
        case AST_EXPR_UNARY_BIT_NOT: return "AST_EXPR_UNARY_BIT_NOT";
        case AST_EXPR_PARENTHESIS: return "AST_EXPR_PARENTHESIS";
        case AST_EXPR_VARIABLE: return "AST_EXPR_VARIABLE";
        case AST_EXPR_LITERAL_BOOL: return "AST_EXPR_LITERAL_BOOL";
        case AST_EXPR_LITERAL_INTEGER: return "AST_EXPR_LITERAL_INTEGER";
        case AST_EXPR_LITERAL_FLOAT: return "AST_EXPR_LITERAL_FLOAT";
        default:
            printf("astNodeKindToString: unknown AST node kind\n");
            exit(1);
    }
}

const char* modifierToString(ASTAssignModifier modifier)
{
    if(modifier.mutable)
        return "mutable";
    else
        return "immutable";
}

void printASTNode(ASTNode node)
{
    switch(node.kind)
    {
        case AST_ASSIGN: {
            printf("AST_ASSIGN: modifier(%s) idenifier(%s) = ",
                modifierToString(node.modifier), node.identifier);
            printASTNode(*(node.rhs));
            printf("\n");
        } break;
        case AST_EXPR_ADD: {
            printf("AST_EXPR_ADD(");
            printASTNode(*(node.lhs));
            printf(" + ");
            printASTNode(*(node.rhs));
            printf(")");
        } break;
        case AST_EXPR_LOGICAL_OR: {
            printf("AST_EXPR_LOGICAL_OR(");
            printASTNode(*(node.lhs));
            printf(" || ");
            printASTNode(*(node.rhs));
            printf(")");
        } break;
        case AST_EXPR_LOGICAL_AND: {
            printf("AST_EXPR_LOGICAL_AND(");
            printASTNode(*(node.lhs));
            printf(" && ");
            printASTNode(*(node.rhs));
            printf(")");
        } break;
        case AST_EXPR_BIT_OR: {
            printf("AST_EXPR_BIT_OR(");
            printASTNode(*(node.lhs));
            printf(" | ");
            printASTNode(*(node.rhs));
            printf(")");
        } break;
        case AST_EXPR_BIT_XOR: {
            printf("AST_EXPR_BIT_XOR(");
            printASTNode(*(node.lhs));
            printf(" ^ ");
            printASTNode(*(node.rhs));
            printf(")");
        } break;
        case AST_EXPR_BIT_AND: {
            printf("AST_EXPR_BIT_AND(");
            printASTNode(*(node.lhs));
            printf(" & ");
            printASTNode(*(node.rhs));
            printf(")");
        } break;
        case AST_EXPR_EQUAL: {
            printf("AST_EXPR_EQUAL(");
            printASTNode(*(node.lhs));
            printf(" == ");
            printASTNode(*(node.rhs));
            printf(")");
        } break;
        case AST_EXPR_NOT_EQUAL: {
            printf("AST_EXPR_NOT_EQUAL(");
            printASTNode(*(node.lhs));
            printf(" != ");
            printASTNode(*(node.rhs));
            printf(")");
        } break;
        case AST_EXPR_LESS: {
            printf("AST_EXPR_LESS(");
            printASTNode(*(node.lhs));
            printf(" < ");
            printASTNode(*(node.rhs));
            printf(")");
        } break;
        case AST_EXPR_LESS_EQUAL: {
            printf("AST_EXPR_LESS_EQUAL(");
            printASTNode(*(node.lhs));
            printf(" <= ");
            printASTNode(*(node.rhs));
            printf(")");
        } break;
        case AST_EXPR_GREATER: {
            printf("AST_EXPR_GREATER(");
            printASTNode(*(node.lhs));
            printf(" > ");
            printASTNode(*(node.rhs));
            printf(")");
        } break;
        case AST_EXPR_GREATER_EQUAL: {
            printf("AST_EXPR_GREATER_EQUAL(");
            printASTNode(*(node.lhs));
            printf(" >= ");
            printASTNode(*(node.rhs));
            printf(")");
        } break;
        case AST_EXPR_SHIFT_LEFT: {
            printf("AST_EXPR_SHIFT_LEFT(");
            printASTNode(*(node.lhs));
            printf(" << ");
            printASTNode(*(node.rhs));
            printf(")");
        } break;
        case AST_EXPR_SHIFT_RIGHT: {
            printf("AST_EXPR_SHIFT_RIGHT(");
            printASTNode(*(node.lhs));
            printf(" >> ");
            printASTNode(*(node.rhs));
            printf(")");
        } break;
        case AST_EXPR_SUB: {
            printf("AST_EXPR_SUB(");
            printASTNode(*(node.lhs));
            printf(" - ");
            printASTNode(*(node.rhs));
            printf(")");
        } break;
        case AST_EXPR_MUL: {
            printf("AST_EXPR_MUL(");
            printASTNode(*(node.lhs));
            printf(" * ");
            printASTNode(*(node.rhs));
            printf(")");
        } break;
        case AST_EXPR_DIV: {
            printf("AST_EXPR_DIV(");
            printASTNode(*(node.lhs));
            printf(" / ");
            printASTNode(*(node.rhs));
            printf(")");
        } break;
        case AST_EXPR_MOD: {
            printf("AST_EXPR_MOD(");
            printASTNode(*(node.lhs));
            printf(" %% ");
            printASTNode(*(node.rhs));
            printf(")");
        } break;
        case AST_EXPR_UNARY_PLUS: {
            printf("AST_EXPR_UNARY_PLUS(+");
            printASTNode(*(node.lhs));
            printf(")");
        } break;
        case AST_EXPR_UNARY_MINUS: {
            printf("AST_EXPR_UNARY_MINUS(-");
            printASTNode(*(node.lhs));
            printf(")");
        } break;
        case AST_EXPR_UNARY_LOGICAL_NOT: {
            printf("AST_EXPR_UNARY_LOGICAL_NOT(!");
            printASTNode(*(node.lhs));
            printf(")");
        } break;
        case AST_EXPR_UNARY_BIT_NOT: {
            printf("AST_EXPR_UNARY_BIT_NOT(~");
            printASTNode(*(node.lhs));
            printf(")");
        } break;
        case AST_EXPR_PARENTHESIS: {
            printf("AST_EXPR_PARENTHESIS(");
            printASTNode(*(node.lhs));
            printf(")");
        } break;
        case AST_EXPR_VARIABLE: {
            printf("AST_EXPR_VARIABLE(%s)", node.identifier);
        } break;
        case AST_EXPR_LITERAL_BOOL: {
            printf("AST_EXPR_LITERAL_BOOL(%s)", node.literal_bool ? "true" : "false");
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

