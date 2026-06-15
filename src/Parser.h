#ifndef PARSER_H
#define PARSER_H

#include "Diagnostic.h"
#include "AST.h"
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

Token* expectToken(Token* token, TokenKind kind)
{
    if(token->kind != kind)
    {
        Diagnostic diagnostic = diagnosticMake(DIAGNOSTIC_SEVERITY_ERROR,
                                               "P1001",
                                               tokenSourceSpan(token),
                                               "unexpected token");
        diagnosticSetPrimaryLabel(&diagnostic,
                                  "expected %s, found %s",
                                  tokenKindToDiagnosticString(kind),
                                  tokenKindToDiagnosticString(token->kind));
        diagnosticAbort(diagnostic);
    }
    return token;
}

ASTDataType* parseTypeExpr(Token **token);
ASTDataType* parseDataType(Token **token);
ASTTypeArgument* parseTypeArgumentList(Token **token);
ASTFunctionCapture* parseFunctionCaptures(Token **token);
bool parseFunctionParameterList(Token **token, bool for_type_syntax, ASTFunctionParameter **out_head, bool *out_is_variadic);
ASTNode* parseBuiltinExpr(Token **token);

static void parsePackageDirective(Token **token, ASTNode *root)
{
    Token *at_token = expectToken(*token, TK_AT);
    Token *builtin_token = at_token->next;
    if(builtin_token == NULL ||
       builtin_token->kind != TK_IDENTIFIER ||
       strcmp(builtin_token->identifier, "package") != 0)
    {
        diagnosticAbortSimple("P1020",
                              "missing leading package declaration",
                              tokenSourceSpan(at_token),
                              "each source file must start with @package(\"name\");");
    }

    *token = builtin_token->next;
    expectToken(*token, TK_LEFT_PARENTHESIS);
    *token = (*token)->next;

    Token *name_token = expectToken(*token, TK_LITERAL_STRING);
    if(name_token->literal_string[0] == '\0')
    {
        diagnosticAbortSimple("P1021",
                              "package name cannot be empty",
                              tokenSourceSpan(name_token),
                              "use a non-empty package name");
    }
    strcpy(root->package_name, name_token->literal_string);
    root->filename = at_token->filename;
    root->line_number = at_token->line_number;
    root->column_number = at_token->column_number;
    root->end_line_number = name_token->end_line_number;
    root->end_column_number = name_token->end_column_number;
    *token = (*token)->next;

    expectToken(*token, TK_RIGHT_PARENTHESIS);
    *token = (*token)->next;

    // expectToken(*token, TK_SEMICOLON);
    // *token = (*token)->next;

    // if the next token is a semicolon, consume it, but don't require it,
    // to allow for more flexible formatting of the package directive
    if((*token)->kind == TK_SEMICOLON)
        *token = (*token)->next;
}

static void parseAnonymousImportIdentifier(ASTNode *import_expr, char *buffer, size_t buffer_size)
{
    if(import_expr == NULL ||
       import_expr->lhs == NULL ||
       import_expr->lhs->next != NULL ||
       import_expr->lhs->kind != AST_EXPR_LITERAL_STRING)
    {
        diagnosticAbortSimple("P1024",
                              "anonymous import requires exactly one string literal path",
                              astNodeSourceSpan(import_expr),
                              "use `@import(\"path\")` or bind the import to a name explicitly");
    }

    const char *import_path = import_expr->lhs->literal_string;
    if(import_path[0] == '\0')
    {
        diagnosticAbortSimple("P1025",
                              "import path cannot be empty",
                              astNodeSourceSpan(import_expr),
                              "use a non-empty package path");
    }

    const char *alias_start = import_path;
    const char *collection_separator = strchr(import_path, ':');
    if(collection_separator != NULL)
        alias_start = collection_separator + 1;

    const char *path_separator = strrchr(alias_start, '/');
    if(path_separator != NULL)
        alias_start = path_separator + 1;

    if(alias_start[0] == '\0')
    {
        diagnosticAbortSimple("P1026",
                              "anonymous import path does not contain a usable module name",
                              astNodeSourceSpan(import_expr),
                              "use an explicit alias such as `name :: @import(\"path\")` instead");
    }

    if(!isIdentifierHead(alias_start[0]))
    {
        diagnosticAbortSimple("P1027",
                              "anonymous import name must be a valid identifier",
                              astNodeSourceSpan(import_expr),
                              "use a valid identifier segment or bind the import explicitly");
    }

    for(const char *cursor = alias_start + 1; *cursor != '\0'; cursor++)
    {
        if(!isIdentifier(*cursor))
        {
            diagnosticAbortSimple("P1027",
                                  "anonymous import name must be a valid identifier",
                                  astNodeSourceSpan(import_expr),
                                  "use a valid identifier segment or bind the import explicitly");
        }
    }

    size_t alias_length = strlen(alias_start);
    if(alias_length >= buffer_size)
    {
        diagnosticAbortSimple("P1028",
                              "anonymous import name is too long",
                              astNodeSourceSpan(import_expr),
                              "use a shorter import path segment or an explicit alias");
    }

    memcpy(buffer, alias_start, alias_length + 1);
}

static ASTNode* parseAnonymousImport(Token **token)
{
    Token *at_token = expectToken(*token, TK_AT);
    ASTNode *import_expr = parseBuiltinExpr(token);

    if(strcmp(import_expr->identifier, "import") != 0)
    {
        diagnosticAbortSimple("P1029",
                              "expected @import builtin",
                              tokenSourceSpan(at_token),
                              "only `@import(...)` can be used as an anonymous import");
    }

    char alias[MAX_IDENTIFIER_LENGTH] = {0};
    parseAnonymousImportIdentifier(import_expr, alias, sizeof(alias));

    ASTNode *node = newASTNodeFromToken(AST_ASSIGN, at_token);

    node->modifier.is_compile_time_binding = true;
    node->modifier.is_runtime_binding = false;
    node->modifier.explicit_type = false;

    ASTNode *lhs = newASTNodeFromToken(AST_EXPR_VARIABLE, at_token);
    strcpy(lhs->identifier, alias);
    strcpy(node->identifier, alias);
    node->lhs = lhs;
    node->rhs = import_expr;
    node->data_type = newInferDataType();
    return node;
}

void parseQualifiedIdentifier(Token **token, char *buffer)
{
    Token *identifier_token = expectToken(*token, TK_IDENTIFIER);
    strcpy(buffer, identifier_token->identifier);
    (*token) = (*token)->next;

    while((*token)->kind == TK_DOT)
    {
        (*token) = (*token)->next;
        Token *member_token = expectToken(*token, TK_IDENTIFIER);
        if(strlen(buffer) + 1 + strlen(member_token->identifier) >= MAX_IDENTIFIER_LENGTH)
        {
            diagnosticAbortSimple("P1002",
                                  "qualified identifier is too long",
                                  tokenSourceSpan(member_token),
                                  "identifier exceeds the compiler limit");
        }
        strcat(buffer, ".");
        strcat(buffer, member_token->identifier);
        (*token) = (*token)->next;
    }
}

ASTAssignModifier parseModifier(Token **token)
{
    ASTAssignModifier modifier = {0};
    return modifier;
}

ASTOperatorKind parseOperatorKind(Token **token)
{
    if((*token)->kind == TK_PLUS)
    {
        (*token) = (*token)->next;
        return AST_OPERATOR_ADD;
    }
    if((*token)->kind == TK_MINUS)
    {
        (*token) = (*token)->next;
        return AST_OPERATOR_SUB;
    }
    if((*token)->kind == TK_STAR)
    {
        (*token) = (*token)->next;
        return AST_OPERATOR_MUL;
    }
    if((*token)->kind == TK_SLASH)
    {
        (*token) = (*token)->next;
        return AST_OPERATOR_DIV;
    }
    if((*token)->kind == TK_DOUBLE_EQUAL)
    {
        (*token) = (*token)->next;
        return AST_OPERATOR_EQ;
    }

    Diagnostic diagnostic = diagnosticMake(DIAGNOSTIC_SEVERITY_ERROR,
                                           "P1006",
                                           tokenSourceSpan(*token),
                                           "unsupported operator annotation");
    diagnosticSetPrimaryLabel(&diagnostic,
                              "expected one of `+`, `-`, `*`, `/`, `==`");
    diagnosticAbort(diagnostic);
}

ASTOperatorKind parseOperatorAnnotation(Token **token)
{
    if((*token)->kind != TK_AT)
        return AST_OPERATOR_NONE;

    Token *at_token = *token;
    if(at_token->next == NULL ||
       at_token->next->kind != TK_IDENTIFIER ||
       strcmp(at_token->next->identifier, "operator") != 0)
        return AST_OPERATOR_NONE;

    (*token) = at_token->next->next;
    expectToken(*token, TK_LEFT_PARENTHESIS);
    (*token) = (*token)->next;
    ASTOperatorKind kind = parseOperatorKind(token);
    expectToken(*token, TK_RIGHT_PARENTHESIS);
    (*token) = (*token)->next;
    return kind;
}

ASTDataType* parsePrimaryDataType(Token **token)
{
    if((*token)->kind == TK_LEFT_BRACKET && (*token)->next != NULL && (*token)->next->kind == TK_RIGHT_BRACKET)
    {
        Token *start_token = *token;
        (*token) = (*token)->next->next;
        ASTDataType *data_type = newSliceDataType(parseDataType(token));
        setASTDataTypeSourceSpanFromToken(data_type, start_token);
        setASTDataTypeEndFromToken(data_type, *token == NULL ? start_token : (*token)->next != NULL ? (*token)->next : start_token);
        return data_type;
    }

    if((*token)->kind == TK_TYPE)
    {
        Token *type_token = *token;
        (*token) = (*token)->next;
        ASTDataType *data_type = newPrimaryDataType(AST_PRIMARY_DATA_TYPE_TYPE);
        setASTDataTypeSourceSpanFromToken(data_type, type_token);
        return data_type;
    }

    if((*token)->kind == TK_VOID)
    {
        Token *void_token = *token;
        (*token) = (*token)->next;
        ASTDataType *data_type = newPrimaryDataType(AST_PRIMARY_DATA_TYPE_VOID);
        setASTDataTypeSourceSpanFromToken(data_type, void_token);
        return data_type;
    }

    if((*token)->kind == TK_OPAQUE)
    {
        Token *opaque_token = *token;
        (*token) = (*token)->next;
        ASTDataType *data_type = newOpaqueDataType("");
        setASTDataTypeSourceSpanFromToken(data_type, opaque_token);
        return data_type;
    }

    Token *start_token = *token;
    char identifier[MAX_IDENTIFIER_LENGTH] = {0};
    parseQualifiedIdentifier(token, identifier);

    ASTDataType *primary = NULL;
    if(strcmp(identifier, "Function") == 0)
    {
        expectToken(*token, TK_LEFT_PARENTHESIS);
        (*token) = (*token)->next;
        expectToken(*token, TK_LEFT_BRACKET);
        (*token) = (*token)->next;

        ASTFunctionParameter *parameters = NULL;
        bool is_variadic = false;
        parseFunctionParameterList(token, true, &parameters, &is_variadic);

        expectToken(*token, TK_RIGHT_BRACKET);
        (*token) = (*token)->next;
        expectToken(*token, TK_COMMA);
        (*token) = (*token)->next;
        ASTDataType *return_data_type = parseDataType(token);
        Token *end_token = expectToken(*token, TK_RIGHT_PARENTHESIS);
        (*token) = (*token)->next;
        ASTDataType *data_type = newFunctionDataType(parameters, is_variadic, return_data_type);
        setASTDataTypeSourceSpanFromToken(data_type, start_token);
        setASTDataTypeEndFromToken(data_type, end_token);
        return data_type;
    }
    else if(strcmp(identifier, "Array") == 0)
    {
        expectToken(*token, TK_LEFT_PARENTHESIS);
        (*token) = (*token)->next;
        ASTDataType *element_type = parseDataType(token);
        expectToken(*token, TK_COMMA);
        (*token) = (*token)->next;
        Token *length_token = expectToken(*token, TK_LITERAL_INTEGER);
        if(length_token->literal_integer > (unsigned long long) LLONG_MAX)
            diagnosticAbortSimple("P1023",
                                  "array length literal is too large",
                                  tokenSourceSpan(length_token),
                                  "array length must fit within `i64`");
        long long int array_length = (long long int) length_token->literal_integer;
        (*token) = (*token)->next;
        Token *end_token = expectToken(*token, TK_RIGHT_PARENTHESIS);
        (*token) = (*token)->next;
        ASTDataType *data_type = newArrayDataType(element_type, array_length);
        setASTDataTypeSourceSpanFromToken(data_type, start_token);
        setASTDataTypeEndFromToken(data_type, end_token);
        return data_type;
    }
    else if(strcmp(identifier, "i8") == 0)
        primary = newPrimaryDataType(AST_PRIMARY_DATA_TYPE_I8);
    else if(strcmp(identifier, "i16") == 0)
        primary = newPrimaryDataType(AST_PRIMARY_DATA_TYPE_I16);
    else if(strcmp(identifier, "i32") == 0)
        primary = newPrimaryDataType(AST_PRIMARY_DATA_TYPE_I32);
    else if(strcmp(identifier, "i64") == 0)
        primary = newPrimaryDataType(AST_PRIMARY_DATA_TYPE_I64);
    else if(strcmp(identifier, "u8") == 0)
        primary = newPrimaryDataType(AST_PRIMARY_DATA_TYPE_U8);
    else if(strcmp(identifier, "u16") == 0)
        primary = newPrimaryDataType(AST_PRIMARY_DATA_TYPE_U16);
    else if(strcmp(identifier, "u32") == 0)
        primary = newPrimaryDataType(AST_PRIMARY_DATA_TYPE_U32);
    else if(strcmp(identifier, "u64") == 0)
        primary = newPrimaryDataType(AST_PRIMARY_DATA_TYPE_U64);
    else if(strcmp(identifier, "f8") == 0)
        primary = newPrimaryDataType(AST_PRIMARY_DATA_TYPE_F8);
    else if(strcmp(identifier, "f16") == 0)
        primary = newPrimaryDataType(AST_PRIMARY_DATA_TYPE_F16);
    else if(strcmp(identifier, "f32") == 0)
        primary = newPrimaryDataType(AST_PRIMARY_DATA_TYPE_F32);
    else if(strcmp(identifier, "f64") == 0)
        primary = newPrimaryDataType(AST_PRIMARY_DATA_TYPE_F64);
    else if(strcmp(identifier, "char") == 0)
        primary = newPrimaryDataType(AST_PRIMARY_DATA_TYPE_CHAR);
    else if(strcmp(identifier, "bool") == 0)
        primary = newPrimaryDataType(AST_PRIMARY_DATA_TYPE_BOOL);
    else if(strcmp(identifier, "string") == 0)
    {
        ASTDataType *data_type = newStringDataType();
        setASTDataTypeSourceSpanFromToken(data_type, start_token);
        return data_type;
    }
    else
        primary = newNamedDataType(identifier);
    setASTDataTypeSourceSpanFromToken(primary, start_token);
    return primary;
}

ASTDataType* parseDataType(Token **token)
{
    if((*token)->kind == TK_QUESTION)
    {
        Token *start_token = *token;
        (*token) = (*token)->next;
        ASTDataType *data_type = newWrappedDataType(AST_DATA_TYPE_KIND_OPTIONAL, parseDataType(token));
        setASTDataTypeSourceSpanFromToken(data_type, start_token);
        if(data_type->child != NULL)
            setASTDataTypeSourceSpan(data_type,
                                     start_token->filename,
                                     start_token->line_number, start_token->column_number,
                                     data_type->child->end_line_number, data_type->child->end_column_number);
        return data_type;
    }

    if((*token)->kind == TK_STAR)
    {
        Token *start_token = *token;
        (*token) = (*token)->next;
        ASTDataType *data_type = newWrappedDataType(AST_DATA_TYPE_KIND_POINTER, parseDataType(token));
        setASTDataTypeSourceSpanFromToken(data_type, start_token);
        if(data_type->child != NULL)
            setASTDataTypeSourceSpan(data_type,
                                     start_token->filename,
                                     start_token->line_number, start_token->column_number,
                                     data_type->child->end_line_number, data_type->child->end_column_number);
        return data_type;
    }

    if((*token)->kind == TK_AMPERSAND)
    {
        Token *start_token = *token;
        (*token) = (*token)->next;
        ASTDataType *data_type = newWrappedDataType(AST_DATA_TYPE_KIND_REFERENCE, parseDataType(token));
        setASTDataTypeSourceSpanFromToken(data_type, start_token);
        if(data_type->child != NULL)
            setASTDataTypeSourceSpan(data_type,
                                     start_token->filename,
                                     start_token->line_number, start_token->column_number,
                                     data_type->child->end_line_number, data_type->child->end_column_number);
        return data_type;
    }

    return parseTypeExpr(token);
}

ASTDataType* parseTypeExpr(Token **token)
{
    ASTDataType *data_type = parsePrimaryDataType(token);

    while((*token)->kind == TK_LEFT_PARENTHESIS)
    {
        ASTTypeArgument *arguments = parseTypeArgumentList(token);
        ASTDataType *applied = newAppliedDataType(data_type, arguments);
        if(data_type != NULL && data_type->filename != NULL)
            setASTDataTypeSourceSpan(applied,
                                     data_type->filename,
                                     data_type->line_number, data_type->column_number,
                                     data_type->end_line_number, data_type->end_column_number);
        if(arguments != NULL)
        {
            ASTTypeArgument *tail = arguments;
            while(tail->next != NULL)
                tail = tail->next;
            if(tail->data_type != NULL)
            {
                applied->end_line_number = tail->data_type->end_line_number;
                applied->end_column_number = tail->data_type->end_column_number;
            }
        }
        data_type = applied;
    }

    return data_type;
}

ASTTypeArgument* parseTypeArgumentList(Token **token)
{
    ASTTypeArgument *head = NULL;
    ASTTypeArgument *tail = NULL;

    expectToken(*token, TK_LEFT_PARENTHESIS);
    (*token) = (*token)->next;

    while((*token)->kind != TK_RIGHT_PARENTHESIS)
    {
        ASTTypeArgument *argument = newASTTypeArgument(parseDataType(token));
        if(head == NULL)
            head = argument;
        else
            tail->next = argument;
        tail = argument;

        if((*token)->kind == TK_COMMA)
            (*token) = (*token)->next;
        else
            break;
    }

    expectToken(*token, TK_RIGHT_PARENTHESIS);
    (*token) = (*token)->next;
    return head;
}

ASTNode* parseExpr(Token **token);
ASTNode* parseUnary(Token **token);
ASTNode* parseStatement(Token **token);
ASTNode* parseBlock(Token **token);
ASTNode* parseEnumExpr(Token **token);
ASTNode* parseStructExpr(Token **token);
ASTNode* parseArrayLiteral(Token **token);
ASTNode* parseArgumentList(Token **token);
ASTNode* parseSimpleAssignNoSemicolon(Token **token);
ASTNode* parseExprStatementNoSemicolon(Token **token);

static int parser_condition_without_parens_depth = 0;

static bool skipBracketedSuffix(Token **token)
{
    if(token == NULL || *token == NULL || (*token)->kind != TK_LEFT_BRACKET)
        return false;

    int parenthesis_depth = 0;
    int brace_depth = 0;
    int bracket_depth = 0;
    Token *cursor = *token;

    while(cursor != NULL && cursor->kind != TK_END_OF_CODE)
    {
        if(cursor->kind == TK_LEFT_PARENTHESIS)
            parenthesis_depth++;
        else if(cursor->kind == TK_RIGHT_PARENTHESIS)
            parenthesis_depth--;
        else if(cursor->kind == TK_LEFT_BRACE)
            brace_depth++;
        else if(cursor->kind == TK_RIGHT_BRACE)
            brace_depth--;
        else if(cursor->kind == TK_LEFT_BRACKET)
            bracket_depth++;
        else if(cursor->kind == TK_RIGHT_BRACKET)
            bracket_depth--;

        cursor = cursor->next;

        if(cursor == NULL)
            return false;

        if(parenthesis_depth == 0 &&
           brace_depth == 0 &&
           bracket_depth == 0)
        {
            *token = cursor;
            return true;
        }
    }

    return false;
}

static bool skipAssignablePrefix(Token **token, bool *is_plain_variable)
{
    if(token == NULL || *token == NULL)
        return false;

    Token *cursor = *token;
    bool plain_variable = true;

    while(cursor->kind == TK_STAR)
    {
        plain_variable = false;
        cursor = cursor->next;
        if(cursor == NULL)
            return false;
    }

    if(cursor->kind != TK_IDENTIFIER)
        return false;

    cursor = cursor->next;

    while(cursor != NULL)
    {
        if(cursor->kind == TK_DOT)
        {
            plain_variable = false;
            cursor = cursor->next;
            if(cursor == NULL || cursor->kind != TK_IDENTIFIER)
                return false;
            cursor = cursor->next;
            continue;
        }

        if(cursor->kind == TK_LEFT_BRACKET)
        {
            plain_variable = false;
            if(!skipBracketedSuffix(&cursor))
                return false;
            continue;
        }

        break;
    }

    *token = cursor;
    if(is_plain_variable != NULL)
        *is_plain_variable = plain_variable;
    return true;
}

static bool tokenStartsDataType(Token *token)
{
    if(token == NULL)
        return false;

    if(token->kind == TK_LEFT_BRACKET &&
       token->next != NULL &&
       token->next->kind == TK_RIGHT_BRACKET)
        return true;

    if(token->kind == TK_STAR || token->kind == TK_AMPERSAND || token->kind == TK_QUESTION ||
       token->kind == TK_TYPE || token->kind == TK_VOID || token->kind == TK_OPAQUE)
        return true;

    if(token->kind != TK_IDENTIFIER)
        return false;

    if(strcmp(token->identifier, "Function") == 0 || strcmp(token->identifier, "Array") == 0)
        return true;

    if(strcmp(token->identifier, "void") == 0 || strcmp(token->identifier, "Type") == 0 ||
       strcmp(token->identifier, "opaque") == 0)
        return true;

    if(strcmp(token->identifier, "bool") == 0 || strcmp(token->identifier, "char") == 0)
        return true;

    if(strcmp(token->identifier, "i8") == 0 || strcmp(token->identifier, "i16") == 0 ||
       strcmp(token->identifier, "i32") == 0 || strcmp(token->identifier, "i64") == 0 ||
       strcmp(token->identifier, "u8") == 0 || strcmp(token->identifier, "u16") == 0 ||
       strcmp(token->identifier, "u32") == 0 || strcmp(token->identifier, "u64") == 0 ||
       strcmp(token->identifier, "f8") == 0 || strcmp(token->identifier, "f16") == 0 ||
       strcmp(token->identifier, "f32") == 0 || strcmp(token->identifier, "f64") == 0)
        return true;

    return false;
}

ASTNode* parseBuiltinExpr(Token **token)
{
    Token *at_token = expectToken(*token, TK_AT);
    ASTNode *node = newASTNodeFromToken(AST_EXPR_BUILTIN, at_token);
    (*token) = (*token)->next;

    Token *builtin_token = expectToken(*token, TK_IDENTIFIER);
    strcpy(node->identifier, builtin_token->identifier);
    (*token) = (*token)->next;

    if(strcmp(node->identifier, "as") == 0 || strcmp(node->identifier, "slice") == 0)
    {
        ASTNode *head = NULL;
        ASTNode *tail = NULL;

        expectToken(*token, TK_LEFT_PARENTHESIS);
        (*token) = (*token)->next;

        while((*token)->kind != TK_RIGHT_PARENTHESIS)
        {
            ASTNode *argument = NULL;
            if(head == NULL && tokenStartsDataType(*token))
            {
                argument = newASTNodeFromToken(AST_EXPR_TYPE_LITERAL, *token);
                argument->data_type = parseDataType(token);
            }
            else
                argument = parseExpr(token);

            if(head == NULL)
                head = argument;
            else
                tail->next = argument;

            tail = argument;
            while(tail->next)
                tail = tail->next;

            if((*token)->kind == TK_COMMA)
                (*token) = (*token)->next;
            else
                break;
        }

        expectToken(*token, TK_RIGHT_PARENTHESIS);
        (*token) = (*token)->next;
        node->lhs = head;
        return node;
    }

    if(strcmp(node->identifier, "debug") == 0)
    {
        ASTNode *head = NULL;
        ASTNode *tail = NULL;

        expectToken(*token, TK_LEFT_PARENTHESIS);
        (*token) = (*token)->next;

        while((*token)->kind != TK_RIGHT_PARENTHESIS)
        {
            ASTNode *argument = parseExpr(token);
            if(head == NULL)
                head = argument;
            else
                tail->next = argument;

            tail = argument;
            while(tail->next)
                tail = tail->next;

            if((*token)->kind == TK_COMMA)
                (*token) = (*token)->next;
            else
                break;
        }

        expectToken(*token, TK_RIGHT_PARENTHESIS);
        (*token) = (*token)->next;
        node->lhs = head;
        return node;
    }

    if(strcmp(node->identifier, "zero") == 0 ||
       strcmp(node->identifier, "len") == 0 ||
       strcmp(node->identifier, "sizeof") == 0 ||
       strcmp(node->identifier, "alignof") == 0)
    {
        ASTNode *argument = NULL;

        expectToken(*token, TK_LEFT_PARENTHESIS);
        (*token) = (*token)->next;

        if(tokenStartsDataType(*token))
        {
            argument = newASTNodeFromToken(AST_EXPR_TYPE_LITERAL, *token);
            argument->data_type = parseDataType(token);
        }
        else
            argument = parseExpr(token);

        expectToken(*token, TK_RIGHT_PARENTHESIS);
        (*token) = (*token)->next;
        node->lhs = argument;
        return node;
    }

    node->lhs = parseArgumentList(token);
    return node;
}

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
    else if((*token)->kind == TK_NULL)
    {
        node->kind = AST_EXPR_LITERAL_NULL;
        (*token) = (*token)->next;
    }
    else if((*token)->kind == TK_LITERAL_CHAR)
    {
        node->kind = AST_EXPR_LITERAL_CHAR;
        node->literal_char = (*token)->literal_char;
        (*token) = (*token)->next;
    }
    else if((*token)->kind == TK_LITERAL_STRING)
    {
        node->kind = AST_EXPR_LITERAL_STRING;
        strcpy(node->literal_string, (*token)->literal_string);
        (*token) = (*token)->next;
    }
    else if((*token)->kind == TK_LITERAL_INTEGER)
    {
        node->literal_integer = makeASTIntegerLiteralValue((*token)->literal_integer);
        (*token) = (*token)->next;
    }
    else if((*token)->kind == TK_LITERAL_FLOAT)
    {
        node->kind = AST_EXPR_LITERAL_FLOAT;
        node->literal_float = (*token)->literal_float;
        (*token) = (*token)->next;
    }
    else {
        diagnosticAbortSimple("P1003",
                              "expected a literal expression",
                              tokenSourceSpan(*token),
                              "expected `true`, `false`, a character, string, integer, or float literal");
    }

    return node;
}

bool isStatementAssign(Token *token)
{
    bool is_plain_variable = false;

    if(token != NULL &&
       token->kind == TK_AT &&
       token->next != NULL &&
       token->next->kind == TK_IDENTIFIER &&
       strcmp(token->next->identifier, "operator") == 0)
    {
        token = token->next->next;
        if(token == NULL || token->kind != TK_LEFT_PARENTHESIS)
            return false;
        token = token->next;
        if(token == NULL)
            return false;
        if(token->kind != TK_PLUS &&
           token->kind != TK_MINUS &&
           token->kind != TK_STAR &&
           token->kind != TK_SLASH &&
           token->kind != TK_DOUBLE_EQUAL)
            return false;
        token = token->next;
        if(token == NULL || token->kind != TK_RIGHT_PARENTHESIS)
            return false;
        token = token->next;
    }

    if(token != NULL && token->kind == TK_PUB)
        token = token->next;

    if(token == NULL)
        return false;

    if(!skipAssignablePrefix(&token, &is_plain_variable))
        return false;

    if(token == NULL)
        return false;

    if(token->kind == TK_EQUAL)
        return true;

    if(is_plain_variable &&
       (token->kind == TK_COLON ||
        token->kind == TK_COLON_EQUAL ||
        token->kind == TK_DOUBLE_COLON))
        return true;

    return false;
}

bool parseFunctionParameterList(Token **token, bool for_type_syntax, ASTFunctionParameter **out_head, bool *out_is_variadic)
{
    ASTFunctionParameter *head = NULL;
    ASTFunctionParameter *tail = NULL;
    bool is_variadic = false;

    TokenKind end_token = for_type_syntax ? TK_RIGHT_BRACKET : TK_RIGHT_PARENTHESIS;
    while((*token)->kind != end_token)
    {
        if((*token)->kind == TK_ELLIPSIS)
        {
            is_variadic = true;
            (*token) = (*token)->next;
            break;
        }

        ASTFunctionParameter *parameter = NULL;
        if(for_type_syntax)
        {
            parameter = newASTFunctionParameterFromToken(*token);
            parameter->data_type = parseDataType(token);
        }
        else
        {
            Token *parameter_token = expectToken(*token, TK_IDENTIFIER);
            parameter = newASTFunctionParameterFromToken(parameter_token);
            strcpy(parameter->identifier, parameter_token->identifier);
            (*token) = (*token)->next;

            expectToken(*token, TK_COLON);
            (*token) = (*token)->next;
            parameter->data_type = parseDataType(token);
        }

        if(head == NULL)
            head = parameter;
        else
            tail->next = parameter;
        tail = parameter;

        if((*token)->kind == TK_COMMA)
        {
            (*token) = (*token)->next;
            if((*token)->kind == end_token)
                break;
            continue;
        }
        else
            break;
    }

    *out_head = head;
    *out_is_variadic = is_variadic;
    return is_variadic;
}

ASTFunctionParameter* parseFunctionParameters(Token **token, bool *out_is_variadic)
{
    ASTFunctionParameter *head = NULL;
    bool is_variadic = false;

    expectToken(*token, TK_LEFT_PARENTHESIS);
    (*token) = (*token)->next;

    parseFunctionParameterList(token, false, &head, &is_variadic);

    expectToken(*token, TK_RIGHT_PARENTHESIS);
    (*token) = (*token)->next;

    if(out_is_variadic != NULL)
        *out_is_variadic = is_variadic;
    return head;
}

ASTNode* parseFunctionExpr(Token **token)
{
    ASTNode *node = newASTNodeFromToken(AST_EXPR_FUNCTION, *token);
    expectToken(*token, TK_FN);
    (*token) = (*token)->next;

    if((*token)->kind == TK_PIPE)
        node->captures = parseFunctionCaptures(token);

    node->parameters = parseFunctionParameters(token, &(node->is_variadic));
    if((*token)->kind != TK_LEFT_BRACE)
        node->return_data_type = parseDataType(token);
    node->data_type = newFunctionDataType(cloneFunctionParameters(node->parameters), node->is_variadic, cloneDataType(node->return_data_type));
    node->body = parseBlock(token);
    return node;
}

ASTFunctionCapture* parseFunctionCaptures(Token **token)
{
    ASTFunctionCapture *head = NULL;
    ASTFunctionCapture *tail = NULL;

    expectToken(*token, TK_PIPE);
    (*token) = (*token)->next;

    while((*token)->kind != TK_PIPE)
    {
        ASTFunctionCaptureKind kind = AST_FUNCTION_CAPTURE_VALUE;
        if((*token)->kind == TK_AMPERSAND)
        {
            (*token) = (*token)->next;
            kind = AST_FUNCTION_CAPTURE_REFERENCE;
        }

        Token *capture_token = expectToken(*token, TK_IDENTIFIER);
        ASTFunctionCapture *capture = newASTFunctionCaptureFromToken(capture_token);
        capture->kind = kind;
        strcpy(capture->identifier, capture_token->identifier);
        (*token) = (*token)->next;

        if(head == NULL)
            head = capture;
        else
            tail->next = capture;
        tail = capture;

        if((*token)->kind == TK_COMMA)
            (*token) = (*token)->next;
        else
            break;
    }

    expectToken(*token, TK_PIPE);
    (*token) = (*token)->next;
    return head;
}

ASTNode* parseParenthesis(Token **token)
{
    expectToken(*token, TK_LEFT_PARENTHESIS);
    (*token) = (*token)->next;

    ASTNode *node = parseExpr(token);

    expectToken(*token, TK_RIGHT_PARENTHESIS);
    (*token) = (*token)->next;

    return node;
}

ASTNode* parsePrimary(Token **token)
{
    if((*token)->kind == TK_FN)
        return parseFunctionExpr(token);

    if((*token)->kind == TK_QUESTION)
    {
        ASTNode *node = newASTNodeFromToken(AST_EXPR_TYPE_LITERAL, *token);
        node->data_type = parseDataType(token);
        return node;
    }

    if((*token)->kind == TK_TYPE || (*token)->kind == TK_VOID || (*token)->kind == TK_OPAQUE)
    {
        ASTNode *node = newASTNodeFromToken(AST_EXPR_TYPE_LITERAL, *token);
        node->data_type = parseDataType(token);
        return node;
    }

    if((*token)->kind == TK_IDENTIFIER &&
       (strcmp((*token)->identifier, "Function") == 0 ||
        strcmp((*token)->identifier, "Array") == 0))
    {
        ASTNode *node = newASTNodeFromToken(AST_EXPR_TYPE_LITERAL, *token);
        node->data_type = parseDataType(token);
        return node;
    }

    if((*token)->kind == TK_ENUM)
        return parseEnumExpr(token);

    if((*token)->kind == TK_STRUCT)
        return parseStructExpr(token);

    if((*token)->kind == TK_LEFT_BRACKET)
        return parseArrayLiteral(token);

    if((*token)->kind == TK_LEFT_PARENTHESIS)
    {
        ASTNode *parenthesis_node = newASTNodeFromToken(AST_EXPR_PARENTHESIS, *token);
        parenthesis_node->lhs = parseParenthesis(token);
        return parenthesis_node;
    }

    if((*token)->kind == TK_IDENTIFIER)
    {
        ASTNode *node = newASTNodeFromToken(AST_EXPR_VARIABLE, *token);
        strcpy(node->identifier, (*token)->identifier);
        (*token) = (*token)->next;
        return node;
    }

    if((*token)->kind == TK_AT)
    {
        if((*token)->next != NULL &&
           (*token)->next->kind == TK_IDENTIFIER &&
           strcmp((*token)->next->identifier, "operator") == 0)
        {
            diagnosticAbortSimple("P1007",
                                  "@operator is only valid before a declaration",
                                  tokenSourceSpan(*token),
                                  "move this annotation in front of a `name: fn(...) ...` declaration");
        }
        return parseBuiltinExpr(token);
    }

    return parseLiteralValue(token);
}

ASTNode* parseArrayLiteral(Token **token)
{
    ASTNode *node = newASTNodeFromToken(AST_EXPR_ARRAY_LITERAL, *token);
    ASTNode *head = NULL;
    ASTNode *tail = NULL;

    expectToken(*token, TK_LEFT_BRACKET);
    (*token) = (*token)->next;

    while((*token)->kind != TK_RIGHT_BRACKET)
    {
        ASTNode *element = parseExpr(token);
        if(head == NULL)
            head = element;
        else
            tail->next = element;

        tail = element;
        while(tail->next)
            tail = tail->next;

        if((*token)->kind == TK_COMMA)
            (*token) = (*token)->next;
        else
            break;
    }

    expectToken(*token, TK_RIGHT_BRACKET);
    (*token) = (*token)->next;
    node->lhs = head;
    return node;
}

ASTStructLiteralField* parseStructLiteralFields(Token **token)
{
    ASTStructLiteralField *head = NULL;
    ASTStructLiteralField *tail = NULL;

    expectToken(*token, TK_LEFT_BRACE);
    (*token) = (*token)->next;

    while((*token)->kind != TK_RIGHT_BRACE)
    {
        expectToken(*token, TK_DOT);
        (*token) = (*token)->next;

        Token *field_token = expectToken(*token, TK_IDENTIFIER);
        ASTStructLiteralField *field = newASTStructLiteralFieldFromToken(field_token);
        strcpy(field->identifier, field_token->identifier);
        (*token) = (*token)->next;

        expectToken(*token, TK_EQUAL);
        (*token) = (*token)->next;
        field->value = parseExpr(token);

        if(head == NULL)
            head = field;
        else
            tail->next = field;
        tail = field;

        if((*token)->kind == TK_COMMA)
            (*token) = (*token)->next;
        else
            break;
    }

    expectToken(*token, TK_RIGHT_BRACE);
    (*token) = (*token)->next;

    return head;
}

ASTNode* parseArgumentList(Token **token)
{
    ASTNode *head = NULL;
    ASTNode *tail = NULL;

    expectToken(*token, TK_LEFT_PARENTHESIS);
    (*token) = (*token)->next;

    while((*token)->kind != TK_RIGHT_PARENTHESIS)
    {
        ASTNode *argument = parseExpr(token);
        if(head == NULL)
            head = argument;
        else
            tail->next = argument;

        tail = argument;
        while(tail->next)
            tail = tail->next;

        if((*token)->kind == TK_COMMA)
            (*token) = (*token)->next;
        else
            break;
    }

    expectToken(*token, TK_RIGHT_PARENTHESIS);
    (*token) = (*token)->next;
    return head;
}

ASTNode* parsePostfix(Token **token)
{
    ASTNode *node = parsePrimary(token);

    while(true)
    {
        if((*token)->kind == TK_LEFT_PARENTHESIS)
        {
            ASTNode *call_node = newASTNodeFromToken(AST_EXPR_CALL, *token);
            call_node->lhs = node;
            call_node->rhs = parseArgumentList(token);
            node = call_node;
            continue;
        }

        if((*token)->kind == TK_DOT)
        {
            (*token) = (*token)->next;
            Token *member_token = expectToken(*token, TK_IDENTIFIER);
            ASTNode *member_node = newASTNodeFromToken(AST_EXPR_MEMBER, member_token);
            member_node->lhs = node;
            strcpy(member_node->identifier, member_token->identifier);
            (*token) = (*token)->next;
            node = member_node;
            continue;
        }

        if((*token)->kind == TK_LEFT_BRACKET)
        {
            ASTNode *index_node = newASTNodeFromToken(AST_EXPR_INDEX, *token);
            index_node->lhs = node;
            (*token) = (*token)->next;
            index_node->rhs = parseExpr(token);
            expectToken(*token, TK_RIGHT_BRACKET);
            (*token) = (*token)->next;
            node = index_node;
            continue;
        }

        if((*token)->kind == TK_LEFT_BRACE)
        {
            if(parser_condition_without_parens_depth > 0)
                break;

            ASTNode *literal_node = newASTNodeFromToken(AST_EXPR_STRUCT_LITERAL, *token);
            literal_node->lhs = node;
            literal_node->struct_literal_fields = parseStructLiteralFields(token);
            node = literal_node;
            continue;
        }

        break;
    }

    return node;
}

ASTNode* parseLValue(Token **token)
{
    ASTNode *node = parseUnary(token);
    if(node->kind == AST_EXPR_VARIABLE || node->kind == AST_EXPR_DEREF || node->kind == AST_EXPR_MEMBER || node->kind == AST_EXPR_INDEX)
        return node;

    diagnosticAbortSimple("P1004",
                          "expected an assignable expression",
                          astNodeSourceSpan(node),
                          "only variables, dereferences, member access, and indexing can appear on the left side of `=`");
}

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
    else if((*token)->kind == TK_STAR)
        kind = AST_EXPR_DEREF;
    else if((*token)->kind == TK_AMPERSAND)
    {
        kind = AST_EXPR_ADDRESS_OF;
        (*token) = (*token)->next;

        ASTNode *node = newASTNodeFromToken(kind, (*token));
        node->lhs = parseUnary(token);
        return node;
    }
    else
        return parsePostfix(token);

    ASTNode *node = newASTNodeFromToken(kind, *token);
    (*token) = (*token)->next;
    node->lhs = parseUnary(token);
    return node;
}

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

ASTNode* parseStructExpr(Token **token)
{
    ASTNode *node = newASTNodeFromToken(AST_EXPR_STRUCT, *token);
    expectToken(*token, TK_STRUCT);
    (*token) = (*token)->next;

    expectToken(*token, TK_LEFT_BRACE);
    (*token) = (*token)->next;

    ASTStructMember *head = NULL;
    ASTStructMember *tail = NULL;

    while((*token)->kind != TK_RIGHT_BRACE)
    {
        ASTOperatorKind operator_kind = parseOperatorAnnotation(token);
        Token *member_token = expectToken(*token, TK_IDENTIFIER);
        ASTStructMember *member = newASTStructMemberFromToken(member_token);
        strcpy(member->identifier, member_token->identifier);
        member->operator_kind = operator_kind;
        (*token) = (*token)->next;

        expectToken(*token, TK_COLON);
        (*token) = (*token)->next;

        if((*token)->kind == TK_FN)
        {
            member->value = parseFunctionExpr(token);
            member->data_type = cloneDataType(member->value->data_type);
        }
        else
            member->data_type = parseDataType(token);

        if(head == NULL)
            head = member;
        else
            tail->next = member;
        tail = member;

        if((*token)->kind == TK_COMMA)
            (*token) = (*token)->next;
        else
            break;
    }

    expectToken(*token, TK_RIGHT_BRACE);
    (*token) = (*token)->next;

    node->members = head;
    node->data_type = newStructDataType("", cloneStructMembers(head));
    return node;
}

ASTNode* parseEnumExpr(Token **token)
{
    ASTNode *node = newASTNodeFromToken(AST_EXPR_ENUM, *token);
    expectToken(*token, TK_ENUM);
    (*token) = (*token)->next;

    expectToken(*token, TK_LEFT_BRACE);
    (*token) = (*token)->next;

    ASTEnumVariant *head = NULL;
    ASTEnumVariant *tail = NULL;

    while((*token)->kind != TK_RIGHT_BRACE)
    {
        Token *variant_token = expectToken(*token, TK_IDENTIFIER);
        ASTEnumVariant *variant = newASTEnumVariantFromToken(variant_token);
        strcpy(variant->identifier, variant_token->identifier);
        (*token) = (*token)->next;

        if(head == NULL)
            head = variant;
        else
            tail->next = variant;
        tail = variant;

        if((*token)->kind == TK_COMMA)
            (*token) = (*token)->next;
        else
            break;
    }

    expectToken(*token, TK_RIGHT_BRACE);
    (*token) = (*token)->next;

    node->variants = head;
    node->data_type = newEnumDataType("", cloneEnumVariants(head));
    return node;
}

ASTNode* parseAssign(Token **token)
{
    ASTNode *node = parseSimpleAssignNoSemicolon(token);

    // expectToken(*token, TK_SEMICOLON);
    // *token = (*token)->next;

    // if the next token is a semicolon, consume it, but don't require it,
    // to allow for more flexible formatting of the package directive
    if((*token)->kind == TK_SEMICOLON)
        *token = (*token)->next;

    return node;
}

ASTNode* parseSimpleAssignNoSemicolon(Token **token)
{
    ASTNode *node = newASTNodeFromToken(AST_ASSIGN, *token);

    node->operator_kind = parseOperatorAnnotation(token);
    if((*token)->kind == TK_PUB)
    {
        node->is_pub = true;
        (*token) = (*token)->next;
    }
    node->modifier = parseModifier(token);
    node->lhs = parseLValue(token);
    if(node->lhs->kind == AST_EXPR_VARIABLE)
        strcpy(node->identifier, node->lhs->identifier);

    node->data_type = newInferDataType();
    if(node->lhs->kind == AST_EXPR_VARIABLE && (*token)->kind == TK_COLON)
    {
        node->modifier.explicit_type = true;
        (*token) = (*token)->next;
        node->data_type = parseDataType(token);
        if((*token)->kind == TK_EQUAL)
            node->modifier.is_runtime_binding = true;
        else if((*token)->kind == TK_COLON)
            node->modifier.is_compile_time_binding = true;
        else
            expectToken(*token, TK_EQUAL);
    }
    else if(node->lhs->kind == AST_EXPR_VARIABLE && (*token)->kind == TK_COLON_EQUAL)
    {
        node->modifier.is_runtime_binding = true;
    }
    else if(node->lhs->kind == AST_EXPR_VARIABLE && (*token)->kind == TK_DOUBLE_COLON)
    {
        node->modifier.is_compile_time_binding = true;
    }
    else
        expectToken(*token, TK_EQUAL);

    (*token) = (*token)->next;
    node->rhs = parseExpr(token);

    return node;
}

ASTNode* parseReturnStatement(Token **token)
{
    ASTNode *node = newASTNodeFromToken(AST_STATEMENT_RETURN, *token);
    expectToken(*token, TK_RETURN);
    (*token) = (*token)->next;

    if((*token)->kind != TK_SEMICOLON)
        node->lhs = parseExpr(token);

    // expectToken(*token, TK_SEMICOLON);
    // *token = (*token)->next;

    // if the next token is a semicolon, consume it, but don't require it,
    // to allow for more flexible formatting of the package directive
    if((*token)->kind == TK_SEMICOLON)
        *token = (*token)->next;

    return node;
}

ASTNode* parseExprStatement(Token **token)
{
    ASTNode *node = parseExprStatementNoSemicolon(token);

    // expectToken(*token, TK_SEMICOLON);
    // *token = (*token)->next;

    // if the next token is a semicolon, consume it, but don't require it,
    // to allow for more flexible formatting of the package directive
    if((*token)->kind == TK_SEMICOLON)
        *token = (*token)->next;

    return node;
}

ASTNode* parseExprStatementNoSemicolon(Token **token)
{
    ASTNode *node = newASTNodeFromToken(AST_STATEMENT_EXPR, *token);
    node->lhs = parseExpr(token);

    return node;
}

ASTNode* parseParenCondition(Token **token)
{
    if((*token)->kind == TK_LEFT_PARENTHESIS)
    {
        expectToken(*token, TK_LEFT_PARENTHESIS);
        (*token) = (*token)->next;

        ASTNode *condition = parseExpr(token);

        expectToken(*token, TK_RIGHT_PARENTHESIS);
        (*token) = (*token)->next;

        return condition;
    }

    parser_condition_without_parens_depth++;
    ASTNode *condition = parseExpr(token);
    parser_condition_without_parens_depth--;
    return condition;
}

ASTNode* parseIfStatement(Token **token)
{
    ASTNode *node = newASTNodeFromToken(AST_STATEMENT_IF, *token);
    expectToken(*token, TK_IF);
    (*token) = (*token)->next;

    node->lhs = parseParenCondition(token);
    node->rhs = parseStatement(token);

    if((*token)->kind == TK_ELSE)
    {
        (*token) = (*token)->next;
        node->body = parseStatement(token);
    }

    return node;
}

ASTNode* parseWhileStatement(Token **token)
{
    ASTNode *node = newASTNodeFromToken(AST_STATEMENT_WHILE, *token);
    expectToken(*token, TK_WHILE);
    (*token) = (*token)->next;

    node->lhs = parseParenCondition(token);
    node->body = parseStatement(token);
    return node;
}

ASTNode* parseDoWhileStatement(Token **token)
{
    ASTNode *node = newASTNodeFromToken(AST_STATEMENT_DO_WHILE, *token);
    expectToken(*token, TK_DO);
    (*token) = (*token)->next;

    node->body = parseStatement(token);

    expectToken(*token, TK_WHILE);
    (*token) = (*token)->next;
    node->lhs = parseParenCondition(token);

    // expectToken(*token, TK_SEMICOLON);
    // *token = (*token)->next;

    // if the next token is a semicolon, consume it, but don't require it,
    // to allow for more flexible formatting of the package directive
    if((*token)->kind == TK_SEMICOLON)
        *token = (*token)->next;

    return node;
}

ASTNode* parseBreakStatement(Token **token)
{
    ASTNode *node = newASTNodeFromToken(AST_STATEMENT_BREAK, *token);
    expectToken(*token, TK_BREAK);
    (*token) = (*token)->next;

    // expectToken(*token, TK_SEMICOLON);
    // *token = (*token)->next;

    // if the next token is a semicolon, consume it, but don't require it,
    // to allow for more flexible formatting of the package directive
    if((*token)->kind == TK_SEMICOLON)
        *token = (*token)->next;
    return node;
}

ASTNode* parseContinueStatement(Token **token)
{
    ASTNode *node = newASTNodeFromToken(AST_STATEMENT_CONTINUE, *token);
    expectToken(*token, TK_CONTINUE);
    (*token) = (*token)->next;


    // expectToken(*token, TK_SEMICOLON);
    // *token = (*token)->next;

    // if the next token is a semicolon, consume it, but don't require it,
    // to allow for more flexible formatting of the package directive
    if((*token)->kind == TK_SEMICOLON)
        *token = (*token)->next;

    return node;
}

ASTNode* parseDeferStatement(Token **token)
{
    ASTNode *node = newASTNodeFromToken(AST_STATEMENT_DEFER, *token);
    expectToken(*token, TK_DEFER);
    (*token) = (*token)->next;

    node->lhs = parseStatement(token);
    return node;
}

ASTNode* parseForClause(Token **token)
{
    if(isStatementAssign(*token))
        return parseSimpleAssignNoSemicolon(token);
    return parseExprStatementNoSemicolon(token);
}

ASTNode* parseForStatement(Token **token)
{
    ASTNode *node = newASTNodeFromToken(AST_STATEMENT_FOR, *token);
    expectToken(*token, TK_FOR);
    (*token) = (*token)->next;

    expectToken(*token, TK_LEFT_PARENTHESIS);
    (*token) = (*token)->next;

    if((*token)->kind != TK_SEMICOLON)
        node->lhs = parseForClause(token);
    expectToken(*token, TK_SEMICOLON);
    (*token) = (*token)->next;

    if((*token)->kind != TK_SEMICOLON)
        node->rhs = parseExpr(token);
    expectToken(*token, TK_SEMICOLON);
    (*token) = (*token)->next;

    if((*token)->kind != TK_RIGHT_PARENTHESIS)
        node->extra = parseForClause(token);
    expectToken(*token, TK_RIGHT_PARENTHESIS);
    (*token) = (*token)->next;

    node->body = parseStatement(token);
    return node;
}

ASTNode* parseStatement(Token **token)
{
    if((*token)->kind == TK_PUB)
    {
        diagnosticAbortSimple("P1005",
                              "`pub` is only allowed at the top level",
                              tokenSourceSpan(*token),
                              "remove `pub` here or move this declaration to the top level");
    }

    if((*token)->kind == TK_LEFT_BRACE)
        return parseBlock(token);

    if((*token)->kind == TK_RETURN)
        return parseReturnStatement(token);

    if((*token)->kind == TK_IF)
        return parseIfStatement(token);

    if((*token)->kind == TK_WHILE)
        return parseWhileStatement(token);

    if((*token)->kind == TK_DO)
        return parseDoWhileStatement(token);

    if((*token)->kind == TK_FOR)
        return parseForStatement(token);

    if((*token)->kind == TK_BREAK)
        return parseBreakStatement(token);

    if((*token)->kind == TK_CONTINUE)
        return parseContinueStatement(token);

    if((*token)->kind == TK_DEFER)
        return parseDeferStatement(token);

    if((*token)->kind == TK_AT &&
       (*token)->next != NULL &&
       (*token)->next->kind == TK_IDENTIFIER &&
       strcmp((*token)->next->identifier, "operator") == 0)
        return parseAssign(token);

    if(isStatementAssign(*token))
        return parseAssign(token);

    return parseExprStatement(token);
}

ASTNode* parseBlock(Token **token)
{
    ASTNode *block = newASTNodeFromToken(AST_BLOCK, *token);
    ASTNode *head = NULL;
    ASTNode *tail = NULL;

    expectToken(*token, TK_LEFT_BRACE);
    (*token) = (*token)->next;

    while((*token)->kind != TK_RIGHT_BRACE)
    {
        ASTNode *stmt = parseStatement(token);
        if(head == NULL)
            head = stmt;
        else
            tail->next = stmt;

        tail = stmt;
        while(tail->next)
            tail = tail->next;
    }

    expectToken(*token, TK_RIGHT_BRACE);
    (*token) = (*token)->next;

    block->lhs = head;
    return block;
}

ASTNode* parse(Token *token)
{
    expectToken(token, TK_START_OF_CODE);
    token = token->next;

    ASTNode *root = newASTNode(AST_START_OF_CODE);
    ASTNode *top_level_block = newASTNode(AST_BLOCK);
    ASTNode *head = NULL;
    ASTNode *tail = NULL;

    if(token == NULL || token->kind == TK_END_OF_CODE)
    {
        diagnosticAbortSimple("P1020",
                              "missing leading package declaration",
                              tokenSourceSpan(token),
                              "each source file must start with @package(\"name\");");
    }
    parsePackageDirective(&token, root);

    while(token && token->kind != TK_END_OF_CODE)
    {
        if(token->kind == TK_AT &&
           token->next != NULL &&
           token->next->kind == TK_IDENTIFIER &&
           strcmp(token->next->identifier, "package") == 0)
        {
            diagnosticAbortSimple("P1022",
                                  "@package must be the first top-level declaration",
                                  tokenSourceSpan(token),
                                  "move this package declaration to the beginning of the file");
        }

        bool is_pub = false;
        if(token->kind == TK_PUB)
        {
            is_pub = true;
            token = token->next;
        }
        ASTNode *stmt = NULL;
        if(is_pub ||
           (token->kind == TK_AT &&
            token->next != NULL &&
            token->next->kind == TK_IDENTIFIER &&
            strcmp(token->next->identifier, "operator") == 0))
            stmt = parseAssign(&token);
        else if(isStatementAssign(token))
            stmt = parseAssign(&token);
        else if(token->kind == TK_AT &&
            token->next != NULL &&
            token->next->kind == TK_IDENTIFIER &&
            strcmp(token->next->identifier, "import") == 0)
        {
            // Allow anonymous import statement `@import(...)`.
            // Use the final path segment as the implicit import alias.
            stmt = parseAnonymousImport(&token);
        }
        else
        {
            diagnosticAbortSimple("P1023",
                                  "top-level code must be a declaration",
                                  tokenSourceSpan(token),
                                  "move executable statements into a function such as main()");
        }
        if(is_pub)
            stmt->is_pub = true;
        if(head == NULL)
            head = stmt;
        else
            tail->next = stmt;

        tail = stmt;
        while(tail->next)
            tail = tail->next;
    }

    top_level_block->lhs = head;
    root->lhs = top_level_block;
    root->next = newASTNodeFromToken(AST_END_OF_CODE, token);
    return root;
}

#endif /* PARSER_H */
