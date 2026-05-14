#ifndef TOKEN_H
#define TOKEN_H

#include <stdio.h>

typedef enum {
    TK_START_OF_CODE,
    TK_END_OF_CODE,

    TK_MUT,

    TK_PLUS,
    TK_MINUS,
    TK_STAR,
    TK_SLASH,
    TK_LEFT_PARENTHESIS,
    TK_RIGHT_PARENTHESIS,
    TK_EQUAL,
    TK_SEMICOLON,

    TK_IDENTIFIER,

    TK_LITERAL_INTEGER,
    TK_LITERAL_FLOAT,
} TokenKind;

#define MAX_IDENTIFIER_LENGTH 128

typedef struct Token {
    struct Token *next;
    TokenKind kind;

    // location info
    const char* filename;
    int line_number;
    int column_number;

    // identifier
    char identifier[MAX_IDENTIFIER_LENGTH];

    // literal values
    long long int literal_integer;
    long double literal_float;
} Token;

Token* newToken(TokenKind kind, const char* filename, int line, int column)
{
    Token *token = (Token*)malloc(sizeof(Token));
    token->kind = kind;
    token->filename = filename;
    token->line_number = line;
    token->column_number = column;
    token->next = NULL;

    return token;
}

const char* tokenKindToString(TokenKind kind)
{
    switch(kind)
    {
        case TK_START_OF_CODE: return "TK_START_OF_CODE";
        case TK_END_OF_CODE: return "TK_END_OF_CODE";
        case TK_MUT: return "TK_MUT";
        case TK_PLUS: return "TK_PLUS";
        case TK_MINUS: return "TK_MINUS";
        case TK_STAR: return "TK_STAR";
        case TK_SLASH: return "TK_SLASH";
        case TK_LEFT_PARENTHESIS: return "TK_LEFT_PARENTHESIS";
        case TK_RIGHT_PARENTHESIS: return "TK_RIGHT_PARENTHESIS";
        case TK_EQUAL: return "TK_EQUAL";
        case TK_SEMICOLON: return "TK_SEMICOLON";
        case TK_IDENTIFIER: return "TK_IDENTIFIER";
        case TK_LITERAL_INTEGER: return "TK_LITERAL_INTEGER";
        case TK_LITERAL_FLOAT: return "TK_LITERAL_FLOAT";
        default:
            printf("In tokenKindToString: Unknown Token kind\n");
            exit(1);
    }
}

void printToken(Token token)
{
    switch(token.kind)
    {
        case TK_START_OF_CODE: {printf("TK_START_OF_CODE\n");}break;
        case TK_END_OF_CODE: {printf("TK_END_OF_CODE\n");}break;
        case TK_MUT: {printf("TK_MUT\n");}break;
        case TK_EQUAL: {printf("TK_EQUAL\n");}break;
        case TK_PLUS: {printf("TK_PLUS\n");}break;
        case TK_MINUS: {printf("TK_MINUS\n");}break;
        case TK_STAR: {printf("TK_STAR\n");}break;
        case TK_SLASH: {printf("TK_SLASH\n");}break;
        case TK_LEFT_PARENTHESIS: {printf("TK_LEFT_PARENTHESIS\n");}break;
        case TK_RIGHT_PARENTHESIS: {printf("TK_RIGHT_PARENTHESIS\n");}break;
        case TK_SEMICOLON: {printf("TK_SEMICOLON\n");}break;
        case TK_IDENTIFIER: {printf("TK_IDENTIFIER: %s\n", token.identifier);}break;
        case TK_LITERAL_INTEGER: {printf("TK_LITERAL_INTEGER: %lld\n", token.literal_integer);}break;
        case TK_LITERAL_FLOAT: {printf("TK_LITERAL_FLOAT %Lf\n", token.literal_float);}break;
        default:
            printf("In printToken: Unknown Token kind\n");
            exit(1);
    }
}

#endif /* TOKEN_H */

