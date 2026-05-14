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

ASTDataType parseDataType(Token **token)
{
    const char *identifier = expectToken(*token, TK_IDENTIFIER)->identifier;

    ASTDataType data_type = AST_DATA_TYPE_INFER;
    if(strcmp(identifier, "i8") == 0)
        data_type = AST_DATA_TYPE_I8;
    else if(strcmp(identifier, "i16") == 0)
        data_type = AST_DATA_TYPE_I16;
    else if(strcmp(identifier, "i32") == 0)
        data_type = AST_DATA_TYPE_I32;
    else if(strcmp(identifier, "i64") == 0)
        data_type = AST_DATA_TYPE_I64;
    else if(strcmp(identifier, "u8") == 0)
        data_type = AST_DATA_TYPE_U8;
    else if(strcmp(identifier, "u16") == 0)
        data_type = AST_DATA_TYPE_U16;
    else if(strcmp(identifier, "u32") == 0)
        data_type = AST_DATA_TYPE_U32;
    else if(strcmp(identifier, "u64") == 0)
        data_type = AST_DATA_TYPE_U64;
    else if(strcmp(identifier, "f8") == 0)
        data_type = AST_DATA_TYPE_F8;
    else if(strcmp(identifier, "f16") == 0)
        data_type = AST_DATA_TYPE_F16;
    else if(strcmp(identifier, "f32") == 0)
        data_type = AST_DATA_TYPE_F32;
    else if(strcmp(identifier, "f64") == 0)
        data_type = AST_DATA_TYPE_F64;
    else if(strcmp(identifier, "char") == 0)
        data_type = AST_DATA_TYPE_CHAR;
    else if(strcmp(identifier, "bool") == 0)
        data_type = AST_DATA_TYPE_BOOL;
    else
    {
        printf("Unknown data type %s at file %s, line %d, column %d\n",
               identifier, (*token)->filename, (*token)->line_number, (*token)->column_number);
        exit(1);
    }

    (*token) = (*token)->next;
    return data_type;
}

// literal value = true | false | literal char | literal integer | literal float
ASTNode* parseLiteralValue(Token **token)
{
    ASTNode *node = newASTNodeFromToken(AST_EXPR_LITERAL_INTEGER, *token);

    if((*token)->kind == TK_TRUE)
    {
        node->kind = AST_EXPR_LITERAL_BOOL;
        node->literal_bool = true;
        (*token) = (*token)->next;
    }
    else if((*token)->kind == TK_FALSE)
    {
        node->kind = AST_EXPR_LITERAL_BOOL;
        node->literal_bool = false;
        (*token) = (*token)->next;
    }
    else if((*token)->kind == TK_LITERAL_CHAR)
    {
        node->kind = AST_EXPR_LITERAL_CHAR;
        node->literal_char = (*token)->literal_char;
        (*token) = (*token)->next;
    }
    else if((*token)->kind == TK_LITERAL_INTEGER)
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
        printf("parseLiteralValue: expected literal bool, char, integer or float here at file %s, line %d, column %d",
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
        ASTNode *parenthesis_node = newASTNodeFromToken(AST_EXPR_PARENTHESIS, *token);
        parenthesis_node->lhs = parseParenthesis(token);
        return parenthesis_node;
    }
    else if((*token)->kind == TK_IDENTIFIER)
    {
        ASTNode *node = newASTNodeFromToken(AST_EXPR_VARIABLE, *token);
        strcpy(node->identifier, (*token)->identifier);
        (*token) = (*token)->next;
        return node;
    }
    else
    {
        return parseLiteralValue(token);
    }
}

// unary = ('+' | '-' | '!' | '~') unary | factor
ASTNode* parseUnary(Token **token)
{
    ASTNodeKind kind;

    if((*token)->kind == TK_PLUS)
        kind = AST_EXPR_UNARY_PLUS;
    else if((*token)->kind == TK_MINUS)
        kind = AST_EXPR_UNARY_MINUS;
    else if((*token)->kind == TK_EXCLAMATION)
        kind = AST_EXPR_UNARY_LOGICAL_NOT;
    else if((*token)->kind == TK_TILDE)
        kind = AST_EXPR_UNARY_BIT_NOT;
    else
        return parseFactor(token);

    ASTNode *node = newASTNodeFromToken(kind, *token);
    (*token) = (*token)->next;
    node->lhs = parseUnary(token);
    return node;
}

// multiplicative = unary (('*' | '/' | '%') unary)*
ASTNode* parseMultiplicative(Token **token)
{
    ASTNode *node = parseUnary(token);

    while((*token)->kind == TK_STAR || (*token)->kind == TK_SLASH || (*token)->kind == TK_PERCENT)
    {
        ASTNodeKind kind = AST_EXPR_MUL;
        if((*token)->kind == TK_SLASH)
            kind = AST_EXPR_DIV;
        else if((*token)->kind == TK_PERCENT)
            kind = AST_EXPR_MOD;
        ASTNode *new_node = newASTNodeFromToken(kind, *token);
        new_node->lhs = node;

        (*token) = (*token)->next;
        new_node->rhs = parseUnary(token);

        node = new_node;
    }

    return node;
}

// additive = multiplicative (('+' | '-') multiplicative)*
ASTNode* parseAdditive(Token **token)
{
    ASTNode *node = parseMultiplicative(token);

    while((*token)->kind == TK_PLUS || (*token)->kind == TK_MINUS)
    {
        ASTNodeKind kind = (*token)->kind == TK_PLUS ? AST_EXPR_ADD : AST_EXPR_SUB;
        ASTNode *new_node = newASTNodeFromToken(kind, *token);
        new_node->lhs = node;

        (*token) = (*token)->next;
        new_node->rhs = parseMultiplicative(token);

        node = new_node;
    }

    return node;
}

// shift = additive (('<<' | '>>') additive)*
ASTNode* parseShift(Token **token)
{
    ASTNode *node = parseAdditive(token);

    while((*token)->kind == TK_LEFT_SHIFT || (*token)->kind == TK_RIGHT_SHIFT)
    {
        ASTNodeKind kind = (*token)->kind == TK_LEFT_SHIFT ? AST_EXPR_SHIFT_LEFT : AST_EXPR_SHIFT_RIGHT;
        ASTNode *new_node = newASTNodeFromToken(kind, *token);
        new_node->lhs = node;

        (*token) = (*token)->next;
        new_node->rhs = parseAdditive(token);

        node = new_node;
    }

    return node;
}

// relational = shift (('<' | '<=' | '>' | '>=') shift)*
ASTNode* parseRelational(Token **token)
{
    ASTNode *node = parseShift(token);

    while((*token)->kind == TK_LESS || (*token)->kind == TK_LESS_EQUAL ||
          (*token)->kind == TK_GREATER || (*token)->kind == TK_GREATER_EQUAL)
    {
        ASTNodeKind kind = AST_EXPR_LESS;
        if((*token)->kind == TK_LESS_EQUAL)
            kind = AST_EXPR_LESS_EQUAL;
        else if((*token)->kind == TK_GREATER)
            kind = AST_EXPR_GREATER;
        else if((*token)->kind == TK_GREATER_EQUAL)
            kind = AST_EXPR_GREATER_EQUAL;
        ASTNode *new_node = newASTNodeFromToken(kind, *token);
        new_node->lhs = node;

        (*token) = (*token)->next;
        new_node->rhs = parseShift(token);

        node = new_node;
    }

    return node;
}

// equality = relational (('==' | '!=') relational)*
ASTNode* parseEquality(Token **token)
{
    ASTNode *node = parseRelational(token);

    while((*token)->kind == TK_DOUBLE_EQUAL || (*token)->kind == TK_EXCLAMATION_EQUAL)
    {
        ASTNodeKind kind = (*token)->kind == TK_DOUBLE_EQUAL ? AST_EXPR_EQUAL : AST_EXPR_NOT_EQUAL;
        ASTNode *new_node = newASTNodeFromToken(kind, *token);
        new_node->lhs = node;

        (*token) = (*token)->next;
        new_node->rhs = parseRelational(token);

        node = new_node;
    }

    return node;
}

// bit and = equality ('&' equality)*
ASTNode* parseBitAnd(Token **token)
{
    ASTNode *node = parseEquality(token);

    while((*token)->kind == TK_AMPERSAND)
    {
        ASTNode *new_node = newASTNodeFromToken(AST_EXPR_BIT_AND, *token);
        new_node->lhs = node;

        (*token) = (*token)->next;
        new_node->rhs = parseEquality(token);

        node = new_node;
    }

    return node;
}

// bit xor = bit and ('^' bit and)*
ASTNode* parseBitXor(Token **token)
{
    ASTNode *node = parseBitAnd(token);

    while((*token)->kind == TK_CARET)
    {
        ASTNode *new_node = newASTNodeFromToken(AST_EXPR_BIT_XOR, *token);
        new_node->lhs = node;

        (*token) = (*token)->next;
        new_node->rhs = parseBitAnd(token);

        node = new_node;
    }

    return node;
}

// bit or = bit xor ('|' bit xor)*
ASTNode* parseBitOr(Token **token)
{
    ASTNode *node = parseBitXor(token);

    while((*token)->kind == TK_PIPE)
    {
        ASTNode *new_node = newASTNodeFromToken(AST_EXPR_BIT_OR, *token);
        new_node->lhs = node;

        (*token) = (*token)->next;
        new_node->rhs = parseBitXor(token);

        node = new_node;
    }

    return node;
}

// logical and = bit or ('&&' bit or)*
ASTNode* parseLogicalAnd(Token **token)
{
    ASTNode *node = parseBitOr(token);

    while((*token)->kind == TK_DOUBLE_AMPERSAND)
    {
        ASTNode *new_node = newASTNodeFromToken(AST_EXPR_LOGICAL_AND, *token);
        new_node->lhs = node;

        (*token) = (*token)->next;
        new_node->rhs = parseBitOr(token);

        node = new_node;
    }

    return node;
}

// expr = logical and ('||' logical and)*
ASTNode* parseExpr(Token **token)
{
    ASTNode *node = parseLogicalAnd(token);

    while((*token)->kind == TK_DOUBLE_PIPE)
    {
        ASTNode *new_node = newASTNodeFromToken(AST_EXPR_LOGICAL_OR, *token);
        new_node->lhs = node;

        (*token) = (*token)->next;
        new_node->rhs = parseLogicalAnd(token);

        node = new_node;
    }

    return node;
}

ASTNode* parseAssign(Token **token)
{
    ASTNode *node = newASTNodeFromToken(AST_ASSIGN, *token);

    // modifier
    ASTAssignModifier modifier = parseModifier(token);
    node->modifier = modifier;

    // identifier
    strcpy(node->identifier, expectToken(*token, TK_IDENTIFIER)->identifier);
    (*token) = (*token)->next;

    // optional type declaration
    node->data_type = AST_DATA_TYPE_INFER;
    if((*token)->kind == TK_COLON)
    {
        (*token) = (*token)->next;
        node->data_type = parseDataType(token);
    }

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

    ASTNode *node = newASTNodeFromToken(AST_START_OF_CODE, token);
    ASTNode *root= node;
    while(token && token->kind != TK_END_OF_CODE)
    {
        node->next = parseAssign(&token);
        node = node->next;
    }

    node->next = newASTNodeFromToken(AST_END_OF_CODE, token);
    return root;
}

#endif /* PARSER_H */

