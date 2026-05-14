#ifndef TOKEN_H
#define TOKEN_H

#include <stdio.h>
#include <stdlib.h>

typedef enum {
    TK_START_OF_CODE,
    TK_END_OF_CODE,

    TK_MUT,
    TK_TRUE,
    TK_FALSE,

    TK_PLUS,
    TK_MINUS,
    TK_STAR,
    TK_SLASH,
    TK_PERCENT,
    TK_AMPERSAND,
    TK_PIPE,
    TK_CARET,
    TK_TILDE,
    TK_EXCLAMATION,
    TK_DOUBLE_AMPERSAND,
    TK_DOUBLE_PIPE,
    TK_DOUBLE_EQUAL,
    TK_EXCLAMATION_EQUAL,
    TK_LESS,
    TK_LESS_EQUAL,
    TK_GREATER,
    TK_GREATER_EQUAL,
    TK_LEFT_SHIFT,
    TK_RIGHT_SHIFT,
    TK_LEFT_PARENTHESIS,
    TK_RIGHT_PARENTHESIS,
    TK_COLON,
    TK_EQUAL,
    TK_SEMICOLON,

    TK_IDENTIFIER,

    TK_LITERAL_CHAR,
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
    char literal_char;
    long long int literal_integer;
    long double literal_float;
} Token;

void printEscapedChar(char literal_char)
{
    switch(literal_char)
    {
        case '\0': {printf("'\\0'");} break;
        case '\n': {printf("'\\n'");} break;
        case '\r': {printf("'\\r'");} break;
        case '\t': {printf("'\\t'");} break;
        case '\\': {printf("'\\\\'");} break;
        case '\'': {printf("'\\''");} break;
        default: {printf("'%c'", literal_char);} break;
    }
}

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
        case TK_TRUE: return "TK_TRUE";
        case TK_FALSE: return "TK_FALSE";
        case TK_PLUS: return "TK_PLUS";
        case TK_MINUS: return "TK_MINUS";
        case TK_STAR: return "TK_STAR";
        case TK_SLASH: return "TK_SLASH";
        case TK_PERCENT: return "TK_PERCENT";
        case TK_AMPERSAND: return "TK_AMPERSAND";
        case TK_PIPE: return "TK_PIPE";
        case TK_CARET: return "TK_CARET";
        case TK_TILDE: return "TK_TILDE";
        case TK_EXCLAMATION: return "TK_EXCLAMATION";
        case TK_DOUBLE_AMPERSAND: return "TK_DOUBLE_AMPERSAND";
        case TK_DOUBLE_PIPE: return "TK_DOUBLE_PIPE";
        case TK_DOUBLE_EQUAL: return "TK_DOUBLE_EQUAL";
        case TK_EXCLAMATION_EQUAL: return "TK_EXCLAMATION_EQUAL";
        case TK_LESS: return "TK_LESS";
        case TK_LESS_EQUAL: return "TK_LESS_EQUAL";
        case TK_GREATER: return "TK_GREATER";
        case TK_GREATER_EQUAL: return "TK_GREATER_EQUAL";
        case TK_LEFT_SHIFT: return "TK_LEFT_SHIFT";
        case TK_RIGHT_SHIFT: return "TK_RIGHT_SHIFT";
        case TK_LEFT_PARENTHESIS: return "TK_LEFT_PARENTHESIS";
        case TK_RIGHT_PARENTHESIS: return "TK_RIGHT_PARENTHESIS";
        case TK_COLON: return "TK_COLON";
        case TK_EQUAL: return "TK_EQUAL";
        case TK_SEMICOLON: return "TK_SEMICOLON";
        case TK_IDENTIFIER: return "TK_IDENTIFIER";
        case TK_LITERAL_CHAR: return "TK_LITERAL_CHAR";
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
        case TK_TRUE: {printf("TK_TRUE\n");}break;
        case TK_FALSE: {printf("TK_FALSE\n");}break;
        case TK_EQUAL: {printf("TK_EQUAL\n");}break;
        case TK_PLUS: {printf("TK_PLUS\n");}break;
        case TK_MINUS: {printf("TK_MINUS\n");}break;
        case TK_STAR: {printf("TK_STAR\n");}break;
        case TK_SLASH: {printf("TK_SLASH\n");}break;
        case TK_PERCENT: {printf("TK_PERCENT\n");}break;
        case TK_AMPERSAND: {printf("TK_AMPERSAND\n");}break;
        case TK_PIPE: {printf("TK_PIPE\n");}break;
        case TK_CARET: {printf("TK_CARET\n");}break;
        case TK_TILDE: {printf("TK_TILDE\n");}break;
        case TK_EXCLAMATION: {printf("TK_EXCLAMATION\n");}break;
        case TK_DOUBLE_AMPERSAND: {printf("TK_DOUBLE_AMPERSAND\n");}break;
        case TK_DOUBLE_PIPE: {printf("TK_DOUBLE_PIPE\n");}break;
        case TK_DOUBLE_EQUAL: {printf("TK_DOUBLE_EQUAL\n");}break;
        case TK_EXCLAMATION_EQUAL: {printf("TK_EXCLAMATION_EQUAL\n");}break;
        case TK_LESS: {printf("TK_LESS\n");}break;
        case TK_LESS_EQUAL: {printf("TK_LESS_EQUAL\n");}break;
        case TK_GREATER: {printf("TK_GREATER\n");}break;
        case TK_GREATER_EQUAL: {printf("TK_GREATER_EQUAL\n");}break;
        case TK_LEFT_SHIFT: {printf("TK_LEFT_SHIFT\n");}break;
        case TK_RIGHT_SHIFT: {printf("TK_RIGHT_SHIFT\n");}break;
        case TK_LEFT_PARENTHESIS: {printf("TK_LEFT_PARENTHESIS\n");}break;
        case TK_RIGHT_PARENTHESIS: {printf("TK_RIGHT_PARENTHESIS\n");}break;
        case TK_COLON: {printf("TK_COLON\n");}break;
        case TK_SEMICOLON: {printf("TK_SEMICOLON\n");}break;
        case TK_IDENTIFIER: {printf("TK_IDENTIFIER: %s\n", token.identifier);}break;
        case TK_LITERAL_CHAR: {
            printf("TK_LITERAL_CHAR: ");
            printEscapedChar(token.literal_char);
            printf("\n");
        }break;
        case TK_LITERAL_INTEGER: {printf("TK_LITERAL_INTEGER: %lld\n", token.literal_integer);}break;
        case TK_LITERAL_FLOAT: {printf("TK_LITERAL_FLOAT %Lf\n", token.literal_float);}break;
        default:
            printf("In printToken: Unknown Token kind\n");
            exit(1);
    }
}

#endif /* TOKEN_H */

