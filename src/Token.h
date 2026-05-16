#ifndef TOKEN_H
#define TOKEN_H

#include <stdio.h>
#include <stdlib.h>

typedef enum {
    TK_START_OF_CODE,
    TK_END_OF_CODE,

    TK_MUT,
    TK_PUB,
    TK_FN,
    TK_STRUCT,
    TK_ENUM,
    TK_RETURN,
    TK_TYPE,
    TK_IF,
    TK_ELSE,
    TK_FOR,
    TK_WHILE,
    TK_DO,
    TK_BREAK,
    TK_CONTINUE,
    TK_DEFER,
    TK_VOID,
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
    TK_ELLIPSIS,
    TK_LESS,
    TK_LESS_EQUAL,
    TK_GREATER,
    TK_GREATER_EQUAL,
    TK_LEFT_SHIFT,
    TK_RIGHT_SHIFT,
    TK_LEFT_PARENTHESIS,
    TK_RIGHT_PARENTHESIS,
    TK_LEFT_BRACE,
    TK_RIGHT_BRACE,
    TK_LEFT_BRACKET,
    TK_RIGHT_BRACKET,
    TK_COMMA,
    TK_COLON,
    TK_DOT,
    TK_AT,
    TK_EQUAL,
    TK_SEMICOLON,

    TK_IDENTIFIER,

    TK_LITERAL_CHAR,
    TK_LITERAL_STRING,
    TK_LITERAL_INTEGER,
    TK_LITERAL_FLOAT,
} TokenKind;

#define MAX_IDENTIFIER_LENGTH 128
#define MAX_STRING_LITERAL_LENGTH 4096

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
    char literal_string[MAX_STRING_LITERAL_LENGTH];
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
        case TK_PUB: return "TK_PUB";
        case TK_FN: return "TK_FN";
        case TK_ENUM: return "TK_ENUM";
        case TK_STRUCT: return "TK_STRUCT";
        case TK_RETURN: return "TK_RETURN";
        case TK_TYPE: return "TK_TYPE";
        case TK_IF: return "TK_IF";
        case TK_ELSE: return "TK_ELSE";
        case TK_FOR: return "TK_FOR";
        case TK_WHILE: return "TK_WHILE";
        case TK_DO: return "TK_DO";
        case TK_BREAK: return "TK_BREAK";
        case TK_CONTINUE: return "TK_CONTINUE";
        case TK_DEFER: return "TK_DEFER";
        case TK_VOID: return "TK_VOID";
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
        case TK_ELLIPSIS: return "TK_ELLIPSIS";
        case TK_LESS: return "TK_LESS";
        case TK_LESS_EQUAL: return "TK_LESS_EQUAL";
        case TK_GREATER: return "TK_GREATER";
        case TK_GREATER_EQUAL: return "TK_GREATER_EQUAL";
        case TK_LEFT_SHIFT: return "TK_LEFT_SHIFT";
        case TK_RIGHT_SHIFT: return "TK_RIGHT_SHIFT";
        case TK_LEFT_PARENTHESIS: return "TK_LEFT_PARENTHESIS";
        case TK_RIGHT_PARENTHESIS: return "TK_RIGHT_PARENTHESIS";
        case TK_LEFT_BRACE: return "TK_LEFT_BRACE";
        case TK_RIGHT_BRACE: return "TK_RIGHT_BRACE";
        case TK_LEFT_BRACKET: return "TK_LEFT_BRACKET";
        case TK_RIGHT_BRACKET: return "TK_RIGHT_BRACKET";
        case TK_COMMA: return "TK_COMMA";
        case TK_COLON: return "TK_COLON";
        case TK_DOT: return "TK_DOT";
        case TK_AT: return "TK_AT";
        case TK_EQUAL: return "TK_EQUAL";
        case TK_SEMICOLON: return "TK_SEMICOLON";
        case TK_IDENTIFIER: return "TK_IDENTIFIER";
        case TK_LITERAL_CHAR: return "TK_LITERAL_CHAR";
        case TK_LITERAL_STRING: return "TK_LITERAL_STRING";
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
        case TK_PUB: {printf("TK_PUB\n");}break;
        case TK_FN: {printf("TK_FN\n");}break;
        case TK_ENUM: {printf("TK_ENUM\n");}break;
        case TK_STRUCT: {printf("TK_STRUCT\n");}break;
        case TK_RETURN: {printf("TK_RETURN\n");}break;
        case TK_TYPE: {printf("TK_TYPE\n");}break;
        case TK_IF: {printf("TK_IF\n");}break;
        case TK_ELSE: {printf("TK_ELSE\n");}break;
        case TK_FOR: {printf("TK_FOR\n");}break;
        case TK_WHILE: {printf("TK_WHILE\n");}break;
        case TK_DO: {printf("TK_DO\n");}break;
        case TK_BREAK: {printf("TK_BREAK\n");}break;
        case TK_CONTINUE: {printf("TK_CONTINUE\n");}break;
        case TK_DEFER: {printf("TK_DEFER\n");}break;
        case TK_VOID: {printf("TK_VOID\n");}break;
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
        case TK_ELLIPSIS: {printf("TK_ELLIPSIS\n");}break;
        case TK_LESS: {printf("TK_LESS\n");}break;
        case TK_LESS_EQUAL: {printf("TK_LESS_EQUAL\n");}break;
        case TK_GREATER: {printf("TK_GREATER\n");}break;
        case TK_GREATER_EQUAL: {printf("TK_GREATER_EQUAL\n");}break;
        case TK_LEFT_SHIFT: {printf("TK_LEFT_SHIFT\n");}break;
        case TK_RIGHT_SHIFT: {printf("TK_RIGHT_SHIFT\n");}break;
        case TK_LEFT_PARENTHESIS: {printf("TK_LEFT_PARENTHESIS\n");}break;
        case TK_RIGHT_PARENTHESIS: {printf("TK_RIGHT_PARENTHESIS\n");}break;
        case TK_LEFT_BRACE: {printf("TK_LEFT_BRACE\n");}break;
        case TK_RIGHT_BRACE: {printf("TK_RIGHT_BRACE\n");}break;
        case TK_LEFT_BRACKET: {printf("TK_LEFT_BRACKET\n");}break;
        case TK_RIGHT_BRACKET: {printf("TK_RIGHT_BRACKET\n");}break;
        case TK_COMMA: {printf("TK_COMMA\n");}break;
        case TK_COLON: {printf("TK_COLON\n");}break;
        case TK_DOT: {printf("TK_DOT\n");}break;
        case TK_AT: {printf("TK_AT\n");}break;
        case TK_SEMICOLON: {printf("TK_SEMICOLON\n");}break;
        case TK_IDENTIFIER: {printf("TK_IDENTIFIER: %s\n", token.identifier);}break;
        case TK_LITERAL_CHAR: {
            printf("TK_LITERAL_CHAR: ");
            printEscapedChar(token.literal_char);
            printf("\n");
        }break;
        case TK_LITERAL_STRING: {printf("TK_LITERAL_STRING: \"%s\"\n", token.literal_string);}break;
        case TK_LITERAL_INTEGER: {printf("TK_LITERAL_INTEGER: %lld\n", token.literal_integer);}break;
        case TK_LITERAL_FLOAT: {printf("TK_LITERAL_FLOAT %Lf\n", token.literal_float);}break;
        default:
            printf("In printToken: Unknown Token kind\n");
            exit(1);
    }
}

#endif /* TOKEN_H */

