#ifndef LEXER_H
#define LEXER_H

#include "Diagnostic.h"
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
        case '"': return '"';
        default:
        {
            char escaped_label[64] = {0};
            diagnosticFormat(escaped_label, sizeof(escaped_label), "invalid escape sequence '\\%c'", escape_char);
            diagnosticAbortSimple("L1001",
                                  "unknown escape sequence",
                                  makePointSourceSpan(filename, line, column),
                                  escaped_label);
        }
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

    diagnosticAbortSimple("L1002",
                          "unterminated block comment",
                          makePointSourceSpan(filename, start_line, start_column),
                          "block comment starts here");
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
            setTokenEnd(token, line, column);
        }
        else if(expectStr("pub", 3, code)) {
            if(isIdentifier(code[3]))
                goto tokenize_identifier;
            token->next = newToken(TK_PUB, filename, line, column);
            token = token->next;
            code += 3;
            column += 3;
            setTokenEnd(token, line, column);
        }
        else if(expectStr("fn", 2, code)) {
            if(isIdentifier(code[2]))
                goto tokenize_identifier;
            token->next = newToken(TK_FN, filename, line, column);
            token = token->next;
            code += 2;
            column += 2;
            setTokenEnd(token, line, column);
        }
        else if(expectStr("enum", 4, code)) {
            if(isIdentifier(code[4]))
                goto tokenize_identifier;
            token->next = newToken(TK_ENUM, filename, line, column);
            token = token->next;
            code += 4;
            column += 4;
            setTokenEnd(token, line, column);
        }
        else if(expectStr("struct", 6, code)) {
            if(isIdentifier(code[6]))
                goto tokenize_identifier;
            token->next = newToken(TK_STRUCT, filename, line, column);
            token = token->next;
            code += 6;
            column += 6;
            setTokenEnd(token, line, column);
        }
        else if(expectStr("return", 6, code)) {
            if(isIdentifier(code[6]))
                goto tokenize_identifier;
            token->next = newToken(TK_RETURN, filename, line, column);
            token = token->next;
            code += 6;
            column += 6;
            setTokenEnd(token, line, column);
        }
        else if(expectStr("Type", 4, code)) {
            if(isIdentifier(code[4]))
                goto tokenize_identifier;
            token->next = newToken(TK_TYPE, filename, line, column);
            token = token->next;
            code += 4;
            column += 4;
            setTokenEnd(token, line, column);
        }
        else if(expectStr("if", 2, code)) {
            if(isIdentifier(code[2]))
                goto tokenize_identifier;
            token->next = newToken(TK_IF, filename, line, column);
            token = token->next;
            code += 2;
            column += 2;
            setTokenEnd(token, line, column);
        }
        else if(expectStr("else", 4, code)) {
            if(isIdentifier(code[4]))
                goto tokenize_identifier;
            token->next = newToken(TK_ELSE, filename, line, column);
            token = token->next;
            code += 4;
            column += 4;
            setTokenEnd(token, line, column);
        }
        else if(expectStr("for", 3, code)) {
            if(isIdentifier(code[3]))
                goto tokenize_identifier;
            token->next = newToken(TK_FOR, filename, line, column);
            token = token->next;
            code += 3;
            column += 3;
            setTokenEnd(token, line, column);
        }
        else if(expectStr("while", 5, code)) {
            if(isIdentifier(code[5]))
                goto tokenize_identifier;
            token->next = newToken(TK_WHILE, filename, line, column);
            token = token->next;
            code += 5;
            column += 5;
            setTokenEnd(token, line, column);
        }
        else if(expectStr("do", 2, code)) {
            if(isIdentifier(code[2]))
                goto tokenize_identifier;
            token->next = newToken(TK_DO, filename, line, column);
            token = token->next;
            code += 2;
            column += 2;
            setTokenEnd(token, line, column);
        }
        else if(expectStr("break", 5, code)) {
            if(isIdentifier(code[5]))
                goto tokenize_identifier;
            token->next = newToken(TK_BREAK, filename, line, column);
            token = token->next;
            code += 5;
            column += 5;
            setTokenEnd(token, line, column);
        }
        else if(expectStr("continue", 8, code)) {
            if(isIdentifier(code[8]))
                goto tokenize_identifier;
            token->next = newToken(TK_CONTINUE, filename, line, column);
            token = token->next;
            code += 8;
            column += 8;
            setTokenEnd(token, line, column);
        }
        else if(expectStr("defer", 5, code)) {
            if(isIdentifier(code[5]))
                goto tokenize_identifier;
            token->next = newToken(TK_DEFER, filename, line, column);
            token = token->next;
            code += 5;
            column += 5;
            setTokenEnd(token, line, column);
        }
        else if(expectStr("void", 4, code)) {
            if(isIdentifier(code[4]))
                goto tokenize_identifier;
            token->next = newToken(TK_VOID, filename, line, column);
            token = token->next;
            code += 4;
            column += 4;
            setTokenEnd(token, line, column);
        }
        else if(expectStr("true", 4, code)) {
            if(isIdentifier(code[4]))
                goto tokenize_identifier;
            token->next = newToken(TK_TRUE, filename, line, column);
            token = token->next;
            code += 4;
            column += 4;
            setTokenEnd(token, line, column);
        }
        else if(expectStr("false", 5, code)) {
            if(isIdentifier(code[5]))
                goto tokenize_identifier;
            token->next = newToken(TK_FALSE, filename, line, column);
            token = token->next;
            code += 5;
            column += 5;
            setTokenEnd(token, line, column);
        }
        else if(expectStr("null", 4, code)) {
            if(isIdentifier(code[4]))
                goto tokenize_identifier;
            token->next = newToken(TK_NULL, filename, line, column);
            token = token->next;
            code += 4;
            column += 4;
            setTokenEnd(token, line, column);
        }
        else if(expectStr("&&", 2, code)) {
            token->next = newToken(TK_DOUBLE_AMPERSAND, filename, line, column);
            token = token->next;
            code += 2;
            column += 2;
            setTokenEnd(token, line, column);
        }
        else if(expectStr("||", 2, code)) {
            token->next = newToken(TK_DOUBLE_PIPE, filename, line, column);
            token = token->next;
            code += 2;
            column += 2;
            setTokenEnd(token, line, column);
        }
        else if(expectStr("==", 2, code)) {
            token->next = newToken(TK_DOUBLE_EQUAL, filename, line, column);
            token = token->next;
            code += 2;
            column += 2;
            setTokenEnd(token, line, column);
        }
        else if(expectStr("!=", 2, code)) {
            token->next = newToken(TK_EXCLAMATION_EQUAL, filename, line, column);
            token = token->next;
            code += 2;
            column += 2;
            setTokenEnd(token, line, column);
        }
        else if(expectStr("?", 1, code)) {
            token->next = newToken(TK_QUESTION, filename, line, column);
            token = token->next;
            code += 1;
            column += 1;
            setTokenEnd(token, line, column);
        }
        else if(expectStr("...", 3, code)) {
            token->next = newToken(TK_ELLIPSIS, filename, line, column);
            token = token->next;
            code += 3;
            column += 3;
            setTokenEnd(token, line, column);
        }
        else if(expectStr("<=", 2, code)) {
            token->next = newToken(TK_LESS_EQUAL, filename, line, column);
            token = token->next;
            code += 2;
            column += 2;
            setTokenEnd(token, line, column);
        }
        else if(expectStr(">=", 2, code)) {
            token->next = newToken(TK_GREATER_EQUAL, filename, line, column);
            token = token->next;
            code += 2;
            column += 2;
            setTokenEnd(token, line, column);
        }
        else if(expectStr("<<", 2, code)) {
            token->next = newToken(TK_LEFT_SHIFT, filename, line, column);
            token = token->next;
            code += 2;
            column += 2;
            setTokenEnd(token, line, column);
        }
        else if(expectStr(">>", 2, code)) {
            token->next = newToken(TK_RIGHT_SHIFT, filename, line, column);
            token = token->next;
            code += 2;
            column += 2;
            setTokenEnd(token, line, column);
        }
        else if(expectStr("=", 1, code)) {
            token->next = newToken(TK_EQUAL, filename, line, column);
            token = token->next;
            code += 1;
            column += 1;
            setTokenEnd(token, line, column);
        }
        else if(expectStr("+", 1, code)) {
            token->next = newToken(TK_PLUS, filename, line, column);
            token = token->next;
            code += 1;
            column += 1;
            setTokenEnd(token, line, column);
        }
        else if(expectStr("-", 1, code)) {
            token->next = newToken(TK_MINUS, filename, line, column);
            token = token->next;
            code += 1;
            column += 1;
            setTokenEnd(token, line, column);
        }
        else if(expectStr("*", 1, code)) {
            token->next = newToken(TK_STAR, filename, line, column);
            token = token->next;
            code += 1;
            column += 1;
            setTokenEnd(token, line, column);
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
            setTokenEnd(token, line, column);
        }
        else if(expectStr("%", 1, code)) {
            token->next = newToken(TK_PERCENT, filename, line, column);
            token = token->next;
            code += 1;
            column += 1;
            setTokenEnd(token, line, column);
        }
        else if(expectStr("&", 1, code)) {
            token->next = newToken(TK_AMPERSAND, filename, line, column);
            token = token->next;
            code += 1;
            column += 1;
            setTokenEnd(token, line, column);
        }
        else if(expectStr("|", 1, code)) {
            token->next = newToken(TK_PIPE, filename, line, column);
            token = token->next;
            code += 1;
            column += 1;
            setTokenEnd(token, line, column);
        }
        else if(expectStr("^", 1, code)) {
            token->next = newToken(TK_CARET, filename, line, column);
            token = token->next;
            code += 1;
            column += 1;
            setTokenEnd(token, line, column);
        }
        else if(expectStr("~", 1, code)) {
            token->next = newToken(TK_TILDE, filename, line, column);
            token = token->next;
            code += 1;
            column += 1;
            setTokenEnd(token, line, column);
        }
        else if(expectStr("!", 1, code)) {
            token->next = newToken(TK_EXCLAMATION, filename, line, column);
            token = token->next;
            code += 1;
            column += 1;
            setTokenEnd(token, line, column);
        }
        else if(expectStr("<", 1, code)) {
            token->next = newToken(TK_LESS, filename, line, column);
            token = token->next;
            code += 1;
            column += 1;
            setTokenEnd(token, line, column);
        }
        else if(expectStr(">", 1, code)) {
            token->next = newToken(TK_GREATER, filename, line, column);
            token = token->next;
            code += 1;
            column += 1;
            setTokenEnd(token, line, column);
        }
        else if(expectStr("(", 1, code)) {
            token->next = newToken(TK_LEFT_PARENTHESIS, filename, line, column);
            token = token->next;
            code += 1;
            column += 1;
            setTokenEnd(token, line, column);
        }
        else if(expectStr(")", 1, code)) {
            token->next = newToken(TK_RIGHT_PARENTHESIS, filename, line, column);
            token = token->next;
            code += 1;
            column += 1;
            setTokenEnd(token, line, column);
        }
        else if(expectStr("{", 1, code)) {
            token->next = newToken(TK_LEFT_BRACE, filename, line, column);
            token = token->next;
            code += 1;
            column += 1;
            setTokenEnd(token, line, column);
        }
        else if(expectStr("}", 1, code)) {
            token->next = newToken(TK_RIGHT_BRACE, filename, line, column);
            token = token->next;
            code += 1;
            column += 1;
            setTokenEnd(token, line, column);
        }
        else if(expectStr("[", 1, code)) {
            token->next = newToken(TK_LEFT_BRACKET, filename, line, column);
            token = token->next;
            code += 1;
            column += 1;
            setTokenEnd(token, line, column);
        }
        else if(expectStr("]", 1, code)) {
            token->next = newToken(TK_RIGHT_BRACKET, filename, line, column);
            token = token->next;
            code += 1;
            column += 1;
            setTokenEnd(token, line, column);
        }
        else if(expectStr(",", 1, code)) {
            token->next = newToken(TK_COMMA, filename, line, column);
            token = token->next;
            code += 1;
            column += 1;
            setTokenEnd(token, line, column);
        }
        else if(expectStr(":", 1, code)) {
            token->next = newToken(TK_COLON, filename, line, column);
            token = token->next;
            code += 1;
            column += 1;
            setTokenEnd(token, line, column);
        }
        else if(expectStr(".", 1, code)) {
            token->next = newToken(TK_DOT, filename, line, column);
            token = token->next;
            code += 1;
            column += 1;
            setTokenEnd(token, line, column);
        }
        else if(expectStr("@", 1, code)) {
            token->next = newToken(TK_AT, filename, line, column);
            token = token->next;
            code += 1;
            column += 1;
            setTokenEnd(token, line, column);
        }
        else if(expectStr(";", 1, code)) {
            token->next = newToken(TK_SEMICOLON, filename, line, column);
            token = token->next;
            code += 1;
            column += 1;
            setTokenEnd(token, line, column);
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
                diagnosticAbortSimple("L1003",
                                      "unterminated character literal",
                                      makePointSourceSpan(filename, literal_line, literal_column),
                                      "character literal starts here");
            }

            if(*code == '\\')
            {
                code ++;
                column ++;

                if(*code == '\0' || *code == '\n')
                {
                    diagnosticAbortSimple("L1003",
                                          "unterminated character literal",
                                          makePointSourceSpan(filename, literal_line, literal_column),
                                          "character literal starts here");
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
                diagnosticAbortSimple("L1004",
                                      "character literal must contain exactly one character",
                                      makePointSourceSpan(filename, literal_line, literal_column),
                                      "invalid character literal");
            }

            code ++;
            column ++;
            setTokenEnd(token, line, column);
        }
        else if(expectStr("\"", 1, code)) {
            int literal_line = line;
            int literal_column = column;
            int string_index = 0;

            token->next = newToken(TK_LITERAL_STRING, filename, line, column);
            token = token->next;
            code ++;
            column ++;

            while(*code && *code != '"' && *code != '\n')
            {
                char ch = *code;
                if(ch == '\\')
                {
                    code ++;
                    column ++;
                    if(*code == '\0' || *code == '\n')
                    {
                        diagnosticAbortSimple("L1005",
                                              "unterminated string literal",
                                              makePointSourceSpan(filename, literal_line, literal_column),
                                              "string literal starts here");
                    }
                    ch = parseEscapedChar(*code, filename, line, column);
                }

                if(string_index >= MAX_STRING_LITERAL_LENGTH - 1)
                {
                    diagnosticAbortSimple("L1006",
                                          "string literal is too long",
                                          makePointSourceSpan(filename, literal_line, literal_column),
                                          "string literal starts here");
                }

                token->literal_string[string_index++] = ch;
                code ++;
                column ++;
            }

            if(*code != '"')
            {
                diagnosticAbortSimple("L1005",
                                      "unterminated string literal",
                                      makePointSourceSpan(filename, literal_line, literal_column),
                                      "string literal starts here");
            }

            token->literal_string[string_index] = '\0';
            code ++;
            column ++;
            setTokenEnd(token, line, column);
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
                    diagnosticAbortSimple("L1007",
                                          "invalid number literal",
                                          makePointSourceSpan(filename, line, column),
                                          "second '.' is not allowed here");
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
            setTokenEnd(token, line, column);
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
            setTokenEnd(token, line, column);
        }
        else
        {
            char label[128] = {0};
            if(isprint((unsigned char)*code))
                diagnosticFormat(label, sizeof(label), "unexpected character '%c'", *code);
            else
                diagnosticFormat(label, sizeof(label), "unexpected byte 0x%02X", (unsigned char)*code);
            diagnosticAbortSimple("L1008",
                                  "unexpected character",
                                  makePointSourceSpan(filename, line, column),
                                  label);
        }
    }

    token->next = newToken(TK_END_OF_CODE, filename, line, column);
    setTokenEnd(token->next, line, column);

    return head;
}

#endif /* LEXER_H */

