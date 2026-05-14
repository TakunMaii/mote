#ifndef PARSER_H
#define PARSER_H

#include "AST.h"
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

Token* expectToken(Token* token, TokenKind kind)
{
    if(token->kind != kind)
    {
        printf("Expected %s but got %s at file %s, line %d, column %d\n",
            tokenKindToString(kind), tokenKindToString(token->kind),
            token->filename, token->line_number, token->column_number);
        exit(1);
    }
    return token;
}

ASTAssignModifier parseModifier(Token **token)
{
    ASTAssignModifier modifier = {0};
    if((*token)->kind == TK_MUT)
    {
        modifier.mutable = true;
        (*token) = (*token)->next;
    }
    return modifier;
}

// literal value = literal integer | literal float
ASTNode* parseLiteralValue(Token **token)
{
    ASTNode *node = newASTNode(AST_EXPR_LITERAL_INTEGER);

    if((*token)->kind == TK_LITERAL_INTEGER)
    {
        node->literal_integer = (*token)->literal_integer;
        (*token) = (*token)->next;
    }
    else if((*token)->kind == TK_LITERAL_FLOAT)
    {
        node->kind = AST_EXPR_LITERAL_FLOAT;
        node->literal_float = (*token)->literal_float;
        (*token) = (*token)->next;
    }
    else {
        printf("parseLiteralValue: expected literal integer or float here at file %s, line %d, column %d",
               (*token)->filename, (*token)->line_number, (*token)->column_number);
        exit(1);
    }

    return node;
}

ASTNode* parseExpr(Token **token);

// parenthesis = '(' expr ')'
ASTNode* parseParenthesis(Token **token)
{
    expectToken(*token, TK_LEFT_PARENTHESIS);
    (*token) = (*token)->next;

    ASTNode *node = parseExpr(token);

    expectToken(*token, TK_RIGHT_PARENTHESIS);
    (*token) = (*token)->next;

    return node;
}

// factor = parenthesis | identifier | literal value
ASTNode* parseFactor(Token **token)
{
    if((*token)->kind == TK_LEFT_PARENTHESIS)
    {
        ASTNode *parenthesis_node = newASTNode(AST_EXPR_PARENTHESIS);
        parenthesis_node->lhs = parseParenthesis(token);
        return parenthesis_node;
    }
    else if((*token)->kind == TK_IDENTIFIER)
    {
        ASTNode *node = newASTNode(AST_EXPR_VARIABLE);
        strcpy(node->identifier, (*token)->identifier);
        (*token) = (*token)->next;
        return node;
    }
    else
    {
        return parseLiteralValue(token);
    }
}

// term = factor (('*' | '/') factor)*
ASTNode* parseTerm(Token **token)
{
    ASTNode *node = parseFactor(token);

    while((*token)->kind == TK_STAR || (*token)->kind == TK_SLASH)
    {
        ASTNodeKind kind = (*token)->kind == TK_STAR ? AST_EXPR_MUL : AST_EXPR_DIV;
        ASTNode *new_node = newASTNode(kind);
        new_node->lhs = node;

        (*token) = (*token)->next;
        new_node->rhs = parseFactor(token);

        node = new_node;
    }

    return node;
}

// expr = term (('+' | '-') term)*
ASTNode* parseExpr(Token **token)
{
    ASTNode *node = parseTerm(token);

    while((*token)->kind == TK_PLUS || (*token)->kind == TK_MINUS)
    {
        ASTNodeKind kind = (*token)->kind == TK_PLUS ? AST_EXPR_ADD : AST_EXPR_SUB;
        ASTNode *new_node = newASTNode(kind);
        new_node->lhs = node;

        (*token) = (*token)->next;
        new_node->rhs = parseTerm(token);

        node = new_node;
    }

    return node;
}

ASTNode* parseAssign(Token **token)
{
    ASTNode *node = newASTNode(AST_ASSIGN);

    // modifier
    ASTAssignModifier modifier = parseModifier(token);
    node->modifier = modifier;

    // identifier
    strcpy(node->identifier, expectToken(*token, TK_IDENTIFIER)->identifier);
    (*token) = (*token)->next;

    // =
    expectToken(*token, TK_EQUAL);
    (*token) = (*token)->next;

    // expression
    node->rhs = parseExpr(token);

    // ;
    expectToken(*token, TK_SEMICOLON);
    (*token) = (*token)->next;

    return node;
}

ASTNode* parse(Token *token)
{
    expectToken(token, TK_START_OF_CODE);
    token = token->next;

    ASTNode *node = newASTNode(AST_START_OF_CODE);
    ASTNode *root= node;
    while(token && token->kind != TK_END_OF_CODE)
    {
        node->next = parseAssign(&token);
        node = node->next;
    }

    node->next = newASTNode(AST_END_OF_CODE);
    return root;
}

#endif /* PARSER_H */

