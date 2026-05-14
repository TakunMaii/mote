#ifndef LEXER_H
#define LEXER_H

#include "Token.h"
#include <stdio.h>
#include <stdbool.h>

bool expectStr(const char* content, int length, char *code)
{
    char *contentptr = content;
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

bool isDigit(char c)
{
    return (c >= '0' && c <= '9');
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
            token->next = newToken(TK_MUT, filename, line, column);
            token = token->next;
            code += 3;
            column += 3;
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
        else if(expectStr("/", 1, code)) {
            token->next = newToken(TK_SLASH, filename, line, column);
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
        else if(expectStr(";", 1, code)) {
            token->next = newToken(TK_SEMICOLON, filename, line, column);
            token = token->next;
            code += 1;
            column += 1;
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
        else if(isIdentifier(*code))
        {
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

    return head;
}

#endif /* LEXER_H */

