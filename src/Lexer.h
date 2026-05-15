#ifndef LEXER_H
#define LEXER_H

#include "Token.h"
#include <stdio.h>
#include <stdbool.h>

bool expectStr(const char* content, int length, char *code)
{
    const char *contentptr = content;
    for(int i = 0;i<length;i++)
    {
        if(!(*code)) return false;
        if(*contentptr != *code) return false;

        contentptr++;
        code++;
    }
    return true;
}

bool isIdentifier(char c)
{
    return (c >= 'a' && c <= 'z')||
           (c >= 'A' && c <= 'Z')||
           (c == '_') || (c >= '0' && c <= '9');
}

bool isIdentifierHead(char c)
{
    return (c >= 'a' && c <= 'z')||
           (c >= 'A' && c <= 'Z')||
           (c == '_');
}

bool isDigit(char c)
{
    return (c >= '0' && c <= '9');
}

char parseEscapedChar(char escape_char, const char *filename, int line, int column)
{
    switch(escape_char)
    {
        case '0': return '\0';
        case 'n': return '\n';
        case 'r': return '\r';
        case 't': return '\t';
        case '\\': return '\\';
        case '\'': return '\'';
        default:
            printf("Unknown escaped char %c at file %s, line %d, column %d\n",
                   escape_char, filename, line, column);
            exit(1);
    }
}

void skipLineComment(char **code, int *column)
{
    (*code) += 2;
    (*column) += 2;

    while(**code && **code != '\n')
    {
        (*code) ++;
        (*column) ++;
    }
}

void skipBlockComment(char **code, int *line, int *column, const char *filename)
{
    int start_line = *line;
    int start_column = *column;

    (*code) += 2;
    (*column) += 2;

    while(**code)
    {
        if(expectStr("*/", 2, *code))
        {
            (*code) += 2;
            (*column) += 2;
            return;
        }

        if(**code == '\n')
        {
            (*code) ++;
            (*line) ++;
            *column = 0;
        }
        else
        {
            (*code) ++;
            (*column) ++;
        }
    }

    printf("Unclosed block comment at file %s, line %d, column %d\n",
           filename, start_line, start_column);
    exit(1);
}

Token* tokenize(char *code, const char* filename)
{
    int line = 0;
    int column = 0;

    Token *token = newToken(TK_START_OF_CODE, filename, line, column);
    Token *head = token;
    while(*code)
    {
        if(*code == '\n') {
            code ++;
            line ++;
            column = 0;
        }
        else if(*code == ' ' || *code == 13) {
            code ++;
            column ++;
        }
        else if(expectStr("mut", 3, code)) {
            if(isIdentifier(code[3]))
                goto tokenize_identifier;
            token->next = newToken(TK_MUT, filename, line, column);
            token = token->next;
            code += 3;
            column += 3;
        }
        else if(expectStr("fn", 2, code)) {
            if(isIdentifier(code[2]))
                goto tokenize_identifier;
            token->next = newToken(TK_FN, filename, line, column);
            token = token->next;
            code += 2;
            column += 2;
        }
        else if(expectStr("struct", 6, code)) {
            if(isIdentifier(code[6]))
                goto tokenize_identifier;
            token->next = newToken(TK_STRUCT, filename, line, column);
            token = token->next;
            code += 6;
            column += 6;
        }
        else if(expectStr("return", 6, code)) {
            if(isIdentifier(code[6]))
                goto tokenize_identifier;
            token->next = newToken(TK_RETURN, filename, line, column);
            token = token->next;
            code += 6;
            column += 6;
        }
        else if(expectStr("void", 4, code)) {
            if(isIdentifier(code[4]))
                goto tokenize_identifier;
            token->next = newToken(TK_VOID, filename, line, column);
            token = token->next;
            code += 4;
            column += 4;
        }
        else if(expectStr("true", 4, code)) {
            if(isIdentifier(code[4]))
                goto tokenize_identifier;
            token->next = newToken(TK_TRUE, filename, line, column);
            token = token->next;
            code += 4;
            column += 4;
        }
        else if(expectStr("false", 5, code)) {
            if(isIdentifier(code[5]))
                goto tokenize_identifier;
            token->next = newToken(TK_FALSE, filename, line, column);
            token = token->next;
            code += 5;
            column += 5;
        }
        else if(expectStr("&&", 2, code)) {
            token->next = newToken(TK_DOUBLE_AMPERSAND, filename, line, column);
            token = token->next;
            code += 2;
            column += 2;
        }
        else if(expectStr("||", 2, code)) {
            token->next = newToken(TK_DOUBLE_PIPE, filename, line, column);
            token = token->next;
            code += 2;
            column += 2;
        }
        else if(expectStr("==", 2, code)) {
            token->next = newToken(TK_DOUBLE_EQUAL, filename, line, column);
            token = token->next;
            code += 2;
            column += 2;
        }
        else if(expectStr("!=", 2, code)) {
            token->next = newToken(TK_EXCLAMATION_EQUAL, filename, line, column);
            token = token->next;
            code += 2;
            column += 2;
        }
        else if(expectStr("<=", 2, code)) {
            token->next = newToken(TK_LESS_EQUAL, filename, line, column);
            token = token->next;
            code += 2;
            column += 2;
        }
        else if(expectStr(">=", 2, code)) {
            token->next = newToken(TK_GREATER_EQUAL, filename, line, column);
            token = token->next;
            code += 2;
            column += 2;
        }
        else if(expectStr("<<", 2, code)) {
            token->next = newToken(TK_LEFT_SHIFT, filename, line, column);
            token = token->next;
            code += 2;
            column += 2;
        }
        else if(expectStr(">>", 2, code)) {
            token->next = newToken(TK_RIGHT_SHIFT, filename, line, column);
            token = token->next;
            code += 2;
            column += 2;
        }
        else if(expectStr("=", 1, code)) {
            token->next = newToken(TK_EQUAL, filename, line, column);
            token = token->next;
            code += 1;
            column += 1;
        }
        else if(expectStr("+", 1, code)) {
            token->next = newToken(TK_PLUS, filename, line, column);
            token = token->next;
            code += 1;
            column += 1;
        }
        else if(expectStr("-", 1, code)) {
            token->next = newToken(TK_MINUS, filename, line, column);
            token = token->next;
            code += 1;
            column += 1;
        }
        else if(expectStr("*", 1, code)) {
            token->next = newToken(TK_STAR, filename, line, column);
            token = token->next;
            code += 1;
            column += 1;
        }
        else if(expectStr("//", 2, code)) {
            skipLineComment(&code, &column);
        }
        else if(expectStr("/*", 2, code)) {
            skipBlockComment(&code, &line, &column, filename);
        }
        else if(expectStr("/", 1, code)) {
            token->next = newToken(TK_SLASH, filename, line, column);
            token = token->next;
            code += 1;
            column += 1;
        }
        else if(expectStr("%", 1, code)) {
            token->next = newToken(TK_PERCENT, filename, line, column);
            token = token->next;
            code += 1;
            column += 1;
        }
        else if(expectStr("&", 1, code)) {
            token->next = newToken(TK_AMPERSAND, filename, line, column);
            token = token->next;
            code += 1;
            column += 1;
        }
        else if(expectStr("|", 1, code)) {
            token->next = newToken(TK_PIPE, filename, line, column);
            token = token->next;
            code += 1;
            column += 1;
        }
        else if(expectStr("^", 1, code)) {
            token->next = newToken(TK_CARET, filename, line, column);
            token = token->next;
            code += 1;
            column += 1;
        }
        else if(expectStr("~", 1, code)) {
            token->next = newToken(TK_TILDE, filename, line, column);
            token = token->next;
            code += 1;
            column += 1;
        }
        else if(expectStr("!", 1, code)) {
            token->next = newToken(TK_EXCLAMATION, filename, line, column);
            token = token->next;
            code += 1;
            column += 1;
        }
        else if(expectStr("<", 1, code)) {
            token->next = newToken(TK_LESS, filename, line, column);
            token = token->next;
            code += 1;
            column += 1;
        }
        else if(expectStr(">", 1, code)) {
            token->next = newToken(TK_GREATER, filename, line, column);
            token = token->next;
            code += 1;
            column += 1;
        }
        else if(expectStr("(", 1, code)) {
            token->next = newToken(TK_LEFT_PARENTHESIS, filename, line, column);
            token = token->next;
            code += 1;
            column += 1;
        }
        else if(expectStr(")", 1, code)) {
            token->next = newToken(TK_RIGHT_PARENTHESIS, filename, line, column);
            token = token->next;
            code += 1;
            column += 1;
        }
        else if(expectStr("{", 1, code)) {
            token->next = newToken(TK_LEFT_BRACE, filename, line, column);
            token = token->next;
            code += 1;
            column += 1;
        }
        else if(expectStr("}", 1, code)) {
            token->next = newToken(TK_RIGHT_BRACE, filename, line, column);
            token = token->next;
            code += 1;
            column += 1;
        }
        else if(expectStr(",", 1, code)) {
            token->next = newToken(TK_COMMA, filename, line, column);
            token = token->next;
            code += 1;
            column += 1;
        }
        else if(expectStr(":", 1, code)) {
            token->next = newToken(TK_COLON, filename, line, column);
            token = token->next;
            code += 1;
            column += 1;
        }
        else if(expectStr(".", 1, code)) {
            token->next = newToken(TK_DOT, filename, line, column);
            token = token->next;
            code += 1;
            column += 1;
        }
        else if(expectStr(";", 1, code)) {
            token->next = newToken(TK_SEMICOLON, filename, line, column);
            token = token->next;
            code += 1;
            column += 1;
        }
        else if(expectStr("'", 1, code)) {
            int literal_line = line;
            int literal_column = column;

            token->next = newToken(TK_LITERAL_CHAR, filename, line, column);
            token = token->next;
            code ++;
            column ++;

            if(*code == '\0' || *code == '\n')
            {
                printf("Unclosed char literal at file %s, line %d, column %d\n",
                       filename, literal_line, literal_column);
                exit(1);
            }

            if(*code == '\\')
            {
                code ++;
                column ++;

                if(*code == '\0' || *code == '\n')
                {
                    printf("Unclosed char literal at file %s, line %d, column %d\n",
                           filename, literal_line, literal_column);
                    exit(1);
                }

                token->literal_char = parseEscapedChar(*code, filename, line, column);
                code ++;
                column ++;
            }
            else
            {
                token->literal_char = *code;
                code ++;
                column ++;
            }

            if(*code != '\'')
            {
                printf("Char literal should contain exactly one char at file %s, line %d, column %d\n",
                       filename, literal_line, literal_column);
                exit(1);
            }

            code ++;
            column ++;
        }
        else if(isDigit(*code)) {
            token->next = newToken(TK_LITERAL_INTEGER, filename, line, column);
            token = token->next;

            bool float_mode = false;
            long long int literal_integer = 0;
            long double literal_float = 0;
            long double float_helper = 0.1;

            do {
                if(*code == '.' && float_mode)
                {
                    printf("Two dots in a number, at file %s, line %d, column %d\n",
                          filename, line, column);
                    exit(1);
                }
                else if(*code == '.' && !float_mode)
                {
                    float_mode = true;
                    literal_float = literal_integer;
                }
                else if(!isDigit(*code))
                {
                    break;
                }
                else
                {
                    int digit = *code - '0';
                    if(!float_mode)
                    {
                        literal_integer = literal_integer * 10 + digit;
                    }
                    else
                    {
                        literal_float += float_helper * digit;
                        float_helper *= 0.1;
                    }
                }
                code ++;
            } while(true);

            if(float_mode)
            {
                token->kind = TK_LITERAL_FLOAT;
                token->literal_float = literal_float;
            }
            else token->literal_integer = literal_integer;
        }
        else if(isIdentifierHead(*code))
        {
tokenize_identifier:
            token->next = newToken(TK_IDENTIFIER, filename, line, column);
            token = token->next;
            int counter = 0;
            while(isIdentifier(*code))
            {
                token->identifier[counter++] = *(code++);
                column ++;
            }
            token->identifier[counter] = 0;
        }
        else
        {
            printf("The Lexer dose't know how to deal with %d, at file %s, line %d, column %d\n", *code,
                  filename, line, column);
            exit(1);
        }
    }

    token->next = newToken(TK_END_OF_CODE, filename, line, column);

    return head;
}

#endif /* LEXER_H */

