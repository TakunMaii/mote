#ifndef AST_H
#define AST_H

#include "Diagnostic.h"
#include <limits.h>
#include <stdio.h>
#include "Token.h"
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>

typedef enum ASTNodeKind {
    AST_START_OF_CODE,
    AST_END_OF_CODE,
    AST_BLOCK,
    AST_STATEMENT_EXPR,
    AST_STATEMENT_RETURN,
    AST_STATEMENT_IF,
    AST_STATEMENT_WHILE,
    AST_STATEMENT_DO_WHILE,
    AST_STATEMENT_FOR,
    AST_STATEMENT_BREAK,
    AST_STATEMENT_CONTINUE,
    AST_STATEMENT_DEFER,

    AST_ASSIGN,// assign or decl

    AST_EXPR_FUNCTION,
    AST_EXPR_ENUM,
    AST_EXPR_STRUCT,
    AST_EXPR_ARRAY_LITERAL,
    AST_EXPR_STRUCT_LITERAL,
    AST_EXPR_CALL,
    AST_EXPR_MEMBER,
    AST_EXPR_INDEX,
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
    AST_EXPR_ADDRESS_OF,
    AST_EXPR_ADDRESS_OF_MUT,
    AST_EXPR_DEREF,
    AST_EXPR_PARENTHESIS,
    AST_EXPR_VARIABLE,
    AST_EXPR_BUILTIN,
    AST_EXPR_TYPE_LITERAL,
    AST_EXPR_LITERAL_BOOL,
    AST_EXPR_LITERAL_NULL,
    AST_EXPR_LITERAL_CHAR,
    AST_EXPR_LITERAL_STRING,
    AST_EXPR_LITERAL_INTEGER,
    AST_EXPR_LITERAL_FLOAT,
} ASTNodeKind;

typedef enum ASTPrimaryDataType {
    AST_PRIMARY_DATA_TYPE_VOID,
    AST_PRIMARY_DATA_TYPE_TYPE,
    AST_PRIMARY_DATA_TYPE_I8,
    AST_PRIMARY_DATA_TYPE_I16,
    AST_PRIMARY_DATA_TYPE_I32,
    AST_PRIMARY_DATA_TYPE_I64,
    AST_PRIMARY_DATA_TYPE_U8,
    AST_PRIMARY_DATA_TYPE_U16,
    AST_PRIMARY_DATA_TYPE_U32,
    AST_PRIMARY_DATA_TYPE_U64,
    AST_PRIMARY_DATA_TYPE_F8,
    AST_PRIMARY_DATA_TYPE_F16,
    AST_PRIMARY_DATA_TYPE_F32,
    AST_PRIMARY_DATA_TYPE_F64,
    AST_PRIMARY_DATA_TYPE_CHAR,
    AST_PRIMARY_DATA_TYPE_BOOL,
} ASTPrimaryDataType;

typedef enum ASTDataTypeKind {
    AST_DATA_TYPE_KIND_INFER,
    AST_DATA_TYPE_KIND_PRIMARY,
    AST_DATA_TYPE_KIND_POINTER,
    AST_DATA_TYPE_KIND_REFERENCE,
    AST_DATA_TYPE_KIND_OPTIONAL,
    AST_DATA_TYPE_KIND_FUNCTION,
    AST_DATA_TYPE_KIND_NAMED,
    AST_DATA_TYPE_KIND_ARRAY,
    AST_DATA_TYPE_KIND_SLICE,
    AST_DATA_TYPE_KIND_STRING,
    AST_DATA_TYPE_KIND_APPLY,
    AST_DATA_TYPE_KIND_ENUM,
    AST_DATA_TYPE_KIND_STRUCT,
    AST_DATA_TYPE_KIND_OPAQUE,
} ASTDataTypeKind;

typedef struct ASTDataType ASTDataType;
typedef struct ASTNode ASTNode;
typedef struct ASTStructMember ASTStructMember;
typedef struct ASTStructLiteralField ASTStructLiteralField;
typedef struct ASTEnumVariant ASTEnumVariant;
typedef struct ASTTypeArgument ASTTypeArgument;
typedef struct ASTFunctionCapture ASTFunctionCapture;
typedef struct ScopeFrame ScopeFrame;
ScopeFrame* snapshotScopeFrame(ScopeFrame *scope);

typedef enum ASTOperatorKind {
    AST_OPERATOR_NONE = 0,
    AST_OPERATOR_ADD,
    AST_OPERATOR_SUB,
    AST_OPERATOR_MUL,
    AST_OPERATOR_DIV,
    AST_OPERATOR_EQ,
} ASTOperatorKind;

typedef struct ASTFunctionParameter {
    struct ASTFunctionParameter *next;
    const char *filename;
    int line_number;
    int column_number;
    int end_line_number;
    int end_column_number;
    char identifier[MAX_IDENTIFIER_LENGTH];
    ASTDataType *data_type;
} ASTFunctionParameter;

typedef struct ASTDataType {
    const char *filename;
    int line_number;
    int column_number;
    int end_line_number;
    int end_column_number;
    ASTDataTypeKind kind;
    ASTPrimaryDataType primary;
    char identifier[MAX_IDENTIFIER_LENGTH];
    struct ASTDataType *child;
    struct ASTDataType *callee;
    ASTTypeArgument *arguments;
    long long int array_length;
    ASTFunctionParameter *parameters;
    bool is_variadic;
    struct ASTDataType *return_data_type;
    ASTStructMember *members;
    ASTEnumVariant *variants;
} ASTDataType;

struct ASTTypeArgument {
    struct ASTTypeArgument *next;
    ASTDataType *data_type;
};

typedef enum ASTFunctionCaptureKind {
    AST_FUNCTION_CAPTURE_VALUE,
    AST_FUNCTION_CAPTURE_REFERENCE,
} ASTFunctionCaptureKind;

struct ASTFunctionCapture {
    struct ASTFunctionCapture *next;
    const char *filename;
    int line_number;
    int column_number;
    int end_line_number;
    int end_column_number;
    ASTFunctionCaptureKind kind;
    char identifier[MAX_IDENTIFIER_LENGTH];
};

struct ASTEnumVariant {
    struct ASTEnumVariant *next;
    const char *filename;
    int line_number;
    int column_number;
    int end_line_number;
    int end_column_number;
    char identifier[MAX_IDENTIFIER_LENGTH];
};

struct ASTStructMember {
    struct ASTStructMember *next;
    const char *filename;
    int line_number;
    int column_number;
    int end_line_number;
    int end_column_number;
    char identifier[MAX_IDENTIFIER_LENGTH];
    ASTOperatorKind operator_kind;
    ASTDataType *data_type;
    ASTNode *value;
    ScopeFrame *lexical_type_scope;
};

struct ASTStructLiteralField {
    struct ASTStructLiteralField *next;
    const char *filename;
    int line_number;
    int column_number;
    int end_line_number;
    int end_column_number;
    char identifier[MAX_IDENTIFIER_LENGTH];
    bool has_name;
    ASTNode *value;
};

typedef struct {
    bool explicit_type;
    bool is_runtime_binding;
    bool is_compile_time_binding;
} ASTAssignModifier;

typedef struct ASTIntegerLiteralValue {
    unsigned long long magnitude;
} ASTIntegerLiteralValue;

struct ASTNode {
    struct ASTNode *next;
    ASTNodeKind kind;

    const char* filename;
    int line_number;
    int column_number;
    int end_line_number;
    int end_column_number;

    struct ASTNode *lhs;// parenthesis, binary expr use this as left hand side
    struct ASTNode *rhs;// assign or decl use this as expr
    struct ASTNode *extra;// else branch or for update clause

    // literal value
    bool literal_bool;
    char literal_char;
    char literal_string[MAX_STRING_LITERAL_LENGTH];
    ASTIntegerLiteralValue literal_integer;
    long double literal_float;

    // assign or decl
    ASTAssignModifier modifier;
    bool is_pub;
    bool entry_returns_void;
    ASTOperatorKind operator_kind;
    ASTDataType *data_type;
    char identifier[MAX_IDENTIFIER_LENGTH];
    char package_name[MAX_IDENTIFIER_LENGTH];
    char entry_symbol[MAX_IDENTIFIER_LENGTH];

    // function literal
    ASTFunctionParameter *parameters;
    bool is_variadic;
    ASTFunctionCapture *captures;
    ASTDataType *return_data_type;
    ASTNode *body;
    ASTStructMember *member_owner;

    // struct literal
    ASTStructMember *members;
    ASTStructLiteralField *struct_literal_fields;
    ASTEnumVariant *variants;
};

ASTNode* newASTNode(ASTNodeKind kind)
{
    ASTNode *node = (ASTNode*) malloc(sizeof(ASTNode));
    if(node == NULL)
        diagnosticAbortInternal("AST allocation failed", NULL);
    memset(node, 0, sizeof(ASTNode));
    node->kind = kind;
    return node;
}

ASTNode* newASTNodeFromToken(ASTNodeKind kind, Token *token)
{
    ASTNode *node = newASTNode(kind);
    node->filename = token->filename;
    node->line_number = token->line_number;
    node->column_number = token->column_number;
    node->end_line_number = token->end_line_number;
    node->end_column_number = token->end_column_number;
    return node;
}

ASTFunctionParameter* newASTFunctionParameterFromToken(Token *token)
{
    ASTFunctionParameter *parameter = (ASTFunctionParameter*) malloc(sizeof(ASTFunctionParameter));
    if(parameter == NULL)
        diagnosticAbortInternal("AST function parameter allocation failed", NULL);
    memset(parameter, 0, sizeof(ASTFunctionParameter));
    parameter->filename = token->filename;
    parameter->line_number = token->line_number;
    parameter->column_number = token->column_number;
    parameter->end_line_number = token->end_line_number;
    parameter->end_column_number = token->end_column_number;
    return parameter;
}

ASTDataType* newInferDataType()
{
    ASTDataType *data_type = (ASTDataType*) malloc(sizeof(ASTDataType));
    memset(data_type, 0, sizeof(ASTDataType));
    data_type->kind = AST_DATA_TYPE_KIND_INFER;
    return data_type;
}

ASTDataType* newPrimaryDataType(ASTPrimaryDataType primary)
{
    ASTDataType *data_type = (ASTDataType*) malloc(sizeof(ASTDataType));
    memset(data_type, 0, sizeof(ASTDataType));
    data_type->kind = AST_DATA_TYPE_KIND_PRIMARY;
    data_type->primary = primary;
    return data_type;
}

ASTDataType* newNamedDataType(const char *identifier)
{
    ASTDataType *data_type = (ASTDataType*) malloc(sizeof(ASTDataType));
    memset(data_type, 0, sizeof(ASTDataType));
    data_type->kind = AST_DATA_TYPE_KIND_NAMED;
    strcpy(data_type->identifier, identifier);
    return data_type;
}

ASTDataType* newAppliedDataType(ASTDataType *callee, ASTTypeArgument *arguments)
{
    ASTDataType *data_type = (ASTDataType*) malloc(sizeof(ASTDataType));
    if(data_type == NULL)
        diagnosticAbortInternal("AST data type allocation failed", NULL);
    memset(data_type, 0, sizeof(ASTDataType));
    data_type->kind = AST_DATA_TYPE_KIND_APPLY;
    data_type->callee = callee;
    data_type->arguments = arguments;
    return data_type;
}

ASTDataType* newArrayDataType(ASTDataType *element_type, long long int length)
{
    ASTDataType *data_type = (ASTDataType*) malloc(sizeof(ASTDataType));
    memset(data_type, 0, sizeof(ASTDataType));
    data_type->kind = AST_DATA_TYPE_KIND_ARRAY;
    data_type->child = element_type;
    data_type->array_length = length;
    return data_type;
}

static ASTIntegerLiteralValue makeASTIntegerLiteralValue(unsigned long long magnitude)
{
    ASTIntegerLiteralValue value = {0};
    value.magnitude = magnitude;
    return value;
}

static bool astIntegerLiteralIsZero(ASTIntegerLiteralValue value)
{
    return value.magnitude == 0;
}

ASTDataType* newSliceDataType(ASTDataType *element_type)
{
    ASTDataType *data_type = (ASTDataType*) malloc(sizeof(ASTDataType));
    memset(data_type, 0, sizeof(ASTDataType));
    data_type->kind = AST_DATA_TYPE_KIND_SLICE;
    data_type->child = element_type;
    return data_type;
}

ASTDataType* newStringDataType(void)
{
    ASTDataType *data_type = (ASTDataType*) malloc(sizeof(ASTDataType));
    memset(data_type, 0, sizeof(ASTDataType));
    data_type->kind = AST_DATA_TYPE_KIND_STRING;
    data_type->child = newPrimaryDataType(AST_PRIMARY_DATA_TYPE_CHAR);
    return data_type;
}

ASTDataType* newWrappedDataType(ASTDataTypeKind kind, ASTDataType *child)
{
    ASTDataType *data_type = (ASTDataType*) malloc(sizeof(ASTDataType));
    memset(data_type, 0, sizeof(ASTDataType));
    data_type->kind = kind;
    data_type->child = child;
    return data_type;
}

ASTDataType* newFunctionDataType(ASTFunctionParameter *parameters, bool is_variadic, ASTDataType *return_data_type)
{
    ASTDataType *data_type = (ASTDataType*) malloc(sizeof(ASTDataType));
    memset(data_type, 0, sizeof(ASTDataType));
    data_type->kind = AST_DATA_TYPE_KIND_FUNCTION;
    data_type->parameters = parameters;
    data_type->is_variadic = is_variadic;
    data_type->return_data_type = return_data_type;
    return data_type;
}

ASTDataType* newStructDataType(const char *identifier, ASTStructMember *members)
{
    ASTDataType *data_type = (ASTDataType*) malloc(sizeof(ASTDataType));
    memset(data_type, 0, sizeof(ASTDataType));
    data_type->kind = AST_DATA_TYPE_KIND_STRUCT;
    if(identifier)
        strcpy(data_type->identifier, identifier);
    data_type->members = members;
    return data_type;
}

ASTDataType* newEnumDataType(const char *identifier, ASTEnumVariant *variants)
{
    ASTDataType *data_type = (ASTDataType*) malloc(sizeof(ASTDataType));
    memset(data_type, 0, sizeof(ASTDataType));
    data_type->kind = AST_DATA_TYPE_KIND_ENUM;
    if(identifier)
        strcpy(data_type->identifier, identifier);
    data_type->variants = variants;
    return data_type;
}

ASTDataType* newOpaqueDataType(const char *identifier)
{
    ASTDataType *data_type = (ASTDataType*) malloc(sizeof(ASTDataType));
    memset(data_type, 0, sizeof(ASTDataType));
    data_type->kind = AST_DATA_TYPE_KIND_OPAQUE;
    if(identifier)
        strcpy(data_type->identifier, identifier);
    return data_type;
}

ASTDataType* cloneDataType(ASTDataType *data_type);
ASTStructMember* cloneStructMembers(ASTStructMember *member);
ASTEnumVariant* cloneEnumVariants(ASTEnumVariant *variant);
ASTTypeArgument* cloneTypeArguments(ASTTypeArgument *argument);
ASTFunctionCapture* cloneFunctionCaptures(ASTFunctionCapture *capture);

typedef struct ASTDataTypeCloneEntry {
    ASTDataType *source;
    ASTDataType *clone;
    struct ASTDataTypeCloneEntry *next;
} ASTDataTypeCloneEntry;

ASTDataType* cloneDataTypeInternal(ASTDataType *data_type, ASTDataTypeCloneEntry **memo);
ASTStructMember* cloneStructMembersInternal(ASTStructMember *member, ASTDataTypeCloneEntry **memo);
ASTTypeArgument* cloneTypeArgumentsInternal(ASTTypeArgument *argument, ASTDataTypeCloneEntry **memo);
ASTFunctionParameter* cloneFunctionParametersInternal(ASTFunctionParameter *parameter, ASTDataTypeCloneEntry **memo);

ASTFunctionParameter* cloneFunctionParametersInternal(ASTFunctionParameter *parameter, ASTDataTypeCloneEntry **memo)
{
    if(parameter == NULL)
        return NULL;

    ASTFunctionParameter *new_parameter = (ASTFunctionParameter*) malloc(sizeof(ASTFunctionParameter));
    *new_parameter = *parameter;
    new_parameter->data_type = NULL;
    new_parameter->next = NULL;

    if(parameter->data_type)
        new_parameter->data_type = cloneDataTypeInternal(parameter->data_type, memo);
    if(parameter->next)
        new_parameter->next = cloneFunctionParametersInternal(parameter->next, memo);

    return new_parameter;
}

ASTFunctionParameter* cloneFunctionParameters(ASTFunctionParameter *parameter)
{
    ASTDataTypeCloneEntry *memo = NULL;
    return cloneFunctionParametersInternal(parameter, &memo);
}

ASTStructMember* newASTStructMemberFromToken(Token *token)
{
    ASTStructMember *member = (ASTStructMember*) malloc(sizeof(ASTStructMember));
    if(member == NULL)
        diagnosticAbortInternal("AST struct member allocation failed", NULL);
    memset(member, 0, sizeof(ASTStructMember));
    member->filename = token->filename;
    member->line_number = token->line_number;
    member->column_number = token->column_number;
    member->end_line_number = token->end_line_number;
    member->end_column_number = token->end_column_number;
    return member;
}

ASTTypeArgument* newASTTypeArgument(ASTDataType *data_type)
{
    ASTTypeArgument *argument = (ASTTypeArgument*) malloc(sizeof(ASTTypeArgument));
    if(argument == NULL)
        diagnosticAbortInternal("AST type argument allocation failed", NULL);
    memset(argument, 0, sizeof(ASTTypeArgument));
    argument->data_type = data_type;
    return argument;
}

ASTFunctionCapture* newASTFunctionCaptureFromToken(Token *token)
{
    ASTFunctionCapture *capture = (ASTFunctionCapture*) malloc(sizeof(ASTFunctionCapture));
    if(capture == NULL)
        diagnosticAbortInternal("AST function capture allocation failed", NULL);
    memset(capture, 0, sizeof(ASTFunctionCapture));
    capture->filename = token->filename;
    capture->line_number = token->line_number;
    capture->column_number = token->column_number;
    capture->end_line_number = token->end_line_number;
    capture->end_column_number = token->end_column_number;
    return capture;
}

ASTStructLiteralField* newASTStructLiteralFieldFromToken(Token *token)
{
    ASTStructLiteralField *field = (ASTStructLiteralField*) malloc(sizeof(ASTStructLiteralField));
    if(field == NULL)
        diagnosticAbortInternal("AST struct literal field allocation failed", NULL);
    memset(field, 0, sizeof(ASTStructLiteralField));
    field->filename = token->filename;
    field->line_number = token->line_number;
    field->column_number = token->column_number;
    field->end_line_number = token->end_line_number;
    field->end_column_number = token->end_column_number;
    return field;
}

ASTEnumVariant* newASTEnumVariantFromToken(Token *token)
{
    ASTEnumVariant *variant = (ASTEnumVariant*) malloc(sizeof(ASTEnumVariant));
    if(variant == NULL)
        diagnosticAbortInternal("AST enum variant allocation failed", NULL);
    memset(variant, 0, sizeof(ASTEnumVariant));
    variant->filename = token->filename;
    variant->line_number = token->line_number;
    variant->column_number = token->column_number;
    variant->end_line_number = token->end_line_number;
    variant->end_column_number = token->end_column_number;
    return variant;
}

SourceSpan astNodeSourceSpan(ASTNode *node)
{
    if(node == NULL)
        return makeSourceSpan(NULL, 0, 0, 0, 0);
    return makeSourceSpan(node->filename,
                          node->line_number, node->column_number,
                          node->end_line_number, node->end_column_number);
}

SourceSpan astFunctionParameterSourceSpan(ASTFunctionParameter *parameter)
{
    if(parameter == NULL)
        return makeSourceSpan(NULL, 0, 0, 0, 0);
    return makeSourceSpan(parameter->filename,
                          parameter->line_number, parameter->column_number,
                          parameter->end_line_number, parameter->end_column_number);
}

SourceSpan astStructMemberSourceSpan(ASTStructMember *member)
{
    if(member == NULL)
        return makeSourceSpan(NULL, 0, 0, 0, 0);
    return makeSourceSpan(member->filename,
                          member->line_number, member->column_number,
                          member->end_line_number, member->end_column_number);
}

SourceSpan astStructLiteralFieldSourceSpan(ASTStructLiteralField *field)
{
    if(field == NULL)
        return makeSourceSpan(NULL, 0, 0, 0, 0);
    return makeSourceSpan(field->filename,
                          field->line_number, field->column_number,
                          field->end_line_number, field->end_column_number);
}

SourceSpan astEnumVariantSourceSpan(ASTEnumVariant *variant)
{
    if(variant == NULL)
        return makeSourceSpan(NULL, 0, 0, 0, 0);
    return makeSourceSpan(variant->filename,
                          variant->line_number, variant->column_number,
                          variant->end_line_number, variant->end_column_number);
}

void setASTDataTypeSourceSpan(ASTDataType *data_type, const char *filename,
                              int line_number, int column_number,
                              int end_line_number, int end_column_number)
{
    if(data_type == NULL)
        return;
    data_type->filename = filename;
    data_type->line_number = line_number;
    data_type->column_number = column_number;
    data_type->end_line_number = end_line_number;
    data_type->end_column_number = end_column_number;
}

void setASTDataTypeSourceSpanFromToken(ASTDataType *data_type, Token *token)
{
    if(data_type == NULL || token == NULL)
        return;
    setASTDataTypeSourceSpan(data_type,
                             token->filename,
                             token->line_number, token->column_number,
                             token->end_line_number, token->end_column_number);
}

void setASTDataTypeEndFromToken(ASTDataType *data_type, Token *token)
{
    if(data_type == NULL || token == NULL)
        return;
    data_type->end_line_number = token->end_line_number;
    data_type->end_column_number = token->end_column_number;
}

SourceSpan astDataTypeSourceSpan(ASTDataType *data_type)
{
    if(data_type == NULL)
        return makeSourceSpan(NULL, 0, 0, 0, 0);
    return makeSourceSpan(data_type->filename,
                          data_type->line_number, data_type->column_number,
                          data_type->end_line_number, data_type->end_column_number);
}

ASTDataType* cloneDataTypeInternal(ASTDataType *data_type, ASTDataTypeCloneEntry **memo)
{
    if(data_type == NULL)
        return NULL;

    ASTDataTypeCloneEntry *entry = *memo;
    while(entry)
    {
        if(entry->source == data_type)
            return entry->clone;
        entry = entry->next;
    }

    ASTDataType *new_data_type = (ASTDataType*) malloc(sizeof(ASTDataType));
    *new_data_type = *data_type;
    new_data_type->child = NULL;
    new_data_type->callee = NULL;
    new_data_type->arguments = NULL;
    new_data_type->parameters = NULL;
    new_data_type->return_data_type = NULL;
    new_data_type->members = NULL;
    new_data_type->variants = NULL;

    ASTDataTypeCloneEntry *new_entry = (ASTDataTypeCloneEntry*) malloc(sizeof(ASTDataTypeCloneEntry));
    new_entry->source = data_type;
    new_entry->clone = new_data_type;
    new_entry->next = *memo;
    *memo = new_entry;

    if(data_type->child)
        new_data_type->child = cloneDataTypeInternal(data_type->child, memo);
    if(data_type->callee)
        new_data_type->callee = cloneDataTypeInternal(data_type->callee, memo);
    if(data_type->arguments)
        new_data_type->arguments = cloneTypeArgumentsInternal(data_type->arguments, memo);
    if(data_type->parameters)
        new_data_type->parameters = cloneFunctionParametersInternal(data_type->parameters, memo);
    if(data_type->return_data_type)
        new_data_type->return_data_type = cloneDataTypeInternal(data_type->return_data_type, memo);
    if(data_type->members)
        new_data_type->members = cloneStructMembersInternal(data_type->members, memo);
    if(data_type->variants)
        new_data_type->variants = cloneEnumVariants(data_type->variants);
    return new_data_type;
}

ASTDataType* cloneDataType(ASTDataType *data_type)
{
    ASTDataTypeCloneEntry *memo = NULL;
    return cloneDataTypeInternal(data_type, &memo);
}

ASTTypeArgument* cloneTypeArgumentsInternal(ASTTypeArgument *argument, ASTDataTypeCloneEntry **memo)
{
    if(argument == NULL)
        return NULL;

    ASTTypeArgument *new_argument = (ASTTypeArgument*) malloc(sizeof(ASTTypeArgument));
    memset(new_argument, 0, sizeof(ASTTypeArgument));
    if(argument->data_type)
        new_argument->data_type = cloneDataTypeInternal(argument->data_type, memo);
    if(argument->next)
        new_argument->next = cloneTypeArgumentsInternal(argument->next, memo);
    return new_argument;
}

ASTTypeArgument* cloneTypeArguments(ASTTypeArgument *argument)
{
    ASTDataTypeCloneEntry *memo = NULL;
    return cloneTypeArgumentsInternal(argument, &memo);
}

ASTStructMember* cloneStructMembersInternal(ASTStructMember *member, ASTDataTypeCloneEntry **memo)
{
    if(member == NULL)
        return NULL;

    ASTStructMember *new_member = (ASTStructMember*) malloc(sizeof(ASTStructMember));
    *new_member = *member;
    new_member->next = NULL;
    new_member->data_type = NULL;
    new_member->lexical_type_scope = member->lexical_type_scope;
    if(new_member->value != NULL && new_member->value->kind == AST_EXPR_FUNCTION)
        new_member->value->member_owner = new_member;

    if(member->data_type)
        new_member->data_type = cloneDataTypeInternal(member->data_type, memo);
    if(member->next)
        new_member->next = cloneStructMembersInternal(member->next, memo);
    return new_member;
}

ASTStructMember* cloneStructMembers(ASTStructMember *member)
{
    ASTDataTypeCloneEntry *memo = NULL;
    return cloneStructMembersInternal(member, &memo);
}

ASTEnumVariant* cloneEnumVariants(ASTEnumVariant *variant)
{
    if(variant == NULL)
        return NULL;

    ASTEnumVariant *new_variant = (ASTEnumVariant*) malloc(sizeof(ASTEnumVariant));
    *new_variant = *variant;
    new_variant->next = NULL;
    if(variant->next)
        new_variant->next = cloneEnumVariants(variant->next);
    return new_variant;
}

ASTFunctionCapture* cloneFunctionCaptures(ASTFunctionCapture *capture)
{
    if(capture == NULL)
        return NULL;

    ASTFunctionCapture *new_capture = (ASTFunctionCapture*) malloc(sizeof(ASTFunctionCapture));
    *new_capture = *capture;
    new_capture->next = NULL;
    if(capture->next)
        new_capture->next = cloneFunctionCaptures(capture->next);
    return new_capture;
}

const char* astNodeKindToString(ASTNodeKind kind)
{
    switch(kind)
    {
        case AST_START_OF_CODE: return "AST_START_OF_CODE";
        case AST_END_OF_CODE: return "AST_END_OF_CODE";
        case AST_BLOCK: return "AST_BLOCK";
        case AST_STATEMENT_EXPR: return "AST_STATEMENT_EXPR";
        case AST_STATEMENT_RETURN: return "AST_STATEMENT_RETURN";
        case AST_STATEMENT_IF: return "AST_STATEMENT_IF";
        case AST_STATEMENT_WHILE: return "AST_STATEMENT_WHILE";
        case AST_STATEMENT_DO_WHILE: return "AST_STATEMENT_DO_WHILE";
        case AST_STATEMENT_FOR: return "AST_STATEMENT_FOR";
        case AST_STATEMENT_BREAK: return "AST_STATEMENT_BREAK";
        case AST_STATEMENT_CONTINUE: return "AST_STATEMENT_CONTINUE";
        case AST_STATEMENT_DEFER: return "AST_STATEMENT_DEFER";
        case AST_ASSIGN: return "AST_ASSIGN";
        case AST_EXPR_FUNCTION: return "AST_EXPR_FUNCTION";
        case AST_EXPR_ENUM: return "AST_EXPR_ENUM";
        case AST_EXPR_STRUCT: return "AST_EXPR_STRUCT";
        case AST_EXPR_ARRAY_LITERAL: return "AST_EXPR_ARRAY_LITERAL";
        case AST_EXPR_STRUCT_LITERAL: return "AST_EXPR_STRUCT_LITERAL";
        case AST_EXPR_CALL: return "AST_EXPR_CALL";
        case AST_EXPR_MEMBER: return "AST_EXPR_MEMBER";
        case AST_EXPR_INDEX: return "AST_EXPR_INDEX";
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
        case AST_EXPR_ADDRESS_OF: return "AST_EXPR_ADDRESS_OF";
        case AST_EXPR_ADDRESS_OF_MUT: return "AST_EXPR_ADDRESS_OF_MUT";
        case AST_EXPR_DEREF: return "AST_EXPR_DEREF";
        case AST_EXPR_PARENTHESIS: return "AST_EXPR_PARENTHESIS";
        case AST_EXPR_VARIABLE: return "AST_EXPR_VARIABLE";
        case AST_EXPR_BUILTIN: return "AST_EXPR_BUILTIN";
        case AST_EXPR_TYPE_LITERAL: return "AST_EXPR_TYPE_LITERAL";
        case AST_EXPR_LITERAL_BOOL: return "AST_EXPR_LITERAL_BOOL";
        case AST_EXPR_LITERAL_NULL: return "AST_EXPR_LITERAL_NULL";
        case AST_EXPR_LITERAL_CHAR: return "AST_EXPR_LITERAL_CHAR";
        case AST_EXPR_LITERAL_STRING: return "AST_EXPR_LITERAL_STRING";
        case AST_EXPR_LITERAL_INTEGER: return "AST_EXPR_LITERAL_INTEGER";
        case AST_EXPR_LITERAL_FLOAT: return "AST_EXPR_LITERAL_FLOAT";
        default:
            diagnosticAbortInternal("astNodeKindToString", "unknown AST node kind");
    }
}

const char* astPrimaryDataTypeToString(ASTPrimaryDataType primary)
{
    switch(primary)
    {
        case AST_PRIMARY_DATA_TYPE_VOID: return "void";
        case AST_PRIMARY_DATA_TYPE_TYPE: return "Type";
        case AST_PRIMARY_DATA_TYPE_I8: return "i8";
        case AST_PRIMARY_DATA_TYPE_I16: return "i16";
        case AST_PRIMARY_DATA_TYPE_I32: return "i32";
        case AST_PRIMARY_DATA_TYPE_I64: return "i64";
        case AST_PRIMARY_DATA_TYPE_U8: return "u8";
        case AST_PRIMARY_DATA_TYPE_U16: return "u16";
        case AST_PRIMARY_DATA_TYPE_U32: return "u32";
        case AST_PRIMARY_DATA_TYPE_U64: return "u64";
        case AST_PRIMARY_DATA_TYPE_F8: return "f8";
        case AST_PRIMARY_DATA_TYPE_F16: return "f16";
        case AST_PRIMARY_DATA_TYPE_F32: return "f32";
        case AST_PRIMARY_DATA_TYPE_F64: return "f64";
        case AST_PRIMARY_DATA_TYPE_CHAR: return "char";
        case AST_PRIMARY_DATA_TYPE_BOOL: return "bool";
        default:
            diagnosticAbortInternal("astPrimaryDataTypeToString", "unknown AST primary data type");
    }
}

void printASTDataType(ASTDataType *data_type);
void printASTNode(ASTNode node);
void printEnumVariants(ASTEnumVariant *variant);
void printFunctionCaptures(ASTFunctionCapture *capture);
void printTypeArguments(ASTTypeArgument *argument);
void appendASTDataTypeString(ASTDataType *data_type, char *buffer, size_t buffer_size);

void printFunctionParameters(ASTFunctionParameter *parameter)
{
    while(parameter)
    {
        printf("%s: ", parameter->identifier);
        printASTDataType(parameter->data_type);
        if(parameter->next)
            printf(", ");
        parameter = parameter->next;
    }
}

void printFunctionCaptures(ASTFunctionCapture *capture)
{
    while(capture)
    {
        if(capture->kind == AST_FUNCTION_CAPTURE_REFERENCE)
            printf("&");
        printf("%s", capture->identifier);
        if(capture->next)
            printf(", ");
        capture = capture->next;
    }
}

void printTypeArguments(ASTTypeArgument *argument)
{
    while(argument)
    {
        printASTDataType(argument->data_type);
        if(argument->next)
            printf(", ");
        argument = argument->next;
    }
}

void printStructMembers(ASTStructMember *member)
{
    while(member)
    {
        printf("%s: ", member->identifier);
        if(member->value)
            printASTNode(*(member->value));
        else
            printASTDataType(member->data_type);
        if(member->next)
            printf(", ");
        member = member->next;
    }
}

void printEnumVariants(ASTEnumVariant *variant)
{
    while(variant)
    {
        printf("%s", variant->identifier);
        if(variant->next)
            printf(", ");
        variant = variant->next;
    }
}

void printASTDataType(ASTDataType *data_type)
{
    if(data_type == NULL)
    {
        printf("<null type>");
        return;
    }

    switch(data_type->kind)
    {
        case AST_DATA_TYPE_KIND_INFER: {
            printf("infer");
        } break;
        case AST_DATA_TYPE_KIND_PRIMARY: {
            printf("%s", astPrimaryDataTypeToString(data_type->primary));
        } break;
        case AST_DATA_TYPE_KIND_POINTER: {
            printf("*");
            printASTDataType(data_type->child);
        } break;
        case AST_DATA_TYPE_KIND_REFERENCE: {
            printf("&");
            printASTDataType(data_type->child);
        } break;
        case AST_DATA_TYPE_KIND_OPTIONAL: {
            printf("?");
            printASTDataType(data_type->child);
        } break;
        case AST_DATA_TYPE_KIND_FUNCTION: {
            printf("Function([");
            printFunctionParameters(data_type->parameters);
            if(data_type->is_variadic)
            {
                if(data_type->parameters != NULL)
                    printf(", ");
                printf("...");
            }
            printf("], ");
            if(data_type->return_data_type != NULL)
                printASTDataType(data_type->return_data_type);
            else
                printf("<infer return>");
            printf(")");
        } break;
        case AST_DATA_TYPE_KIND_NAMED: {
            printf("%s", data_type->identifier);
        } break;
        case AST_DATA_TYPE_KIND_ARRAY: {
            printf("Array(");
            printASTDataType(data_type->child);
            printf(", %lld)", data_type->array_length);
        } break;
        case AST_DATA_TYPE_KIND_SLICE: {
            printf("[]");
            printASTDataType(data_type->child);
        } break;
        case AST_DATA_TYPE_KIND_STRING: {
            printf("string");
        } break;
        case AST_DATA_TYPE_KIND_APPLY: {
            printASTDataType(data_type->callee);
            printf("(");
            printTypeArguments(data_type->arguments);
            printf(")");
        } break;
        case AST_DATA_TYPE_KIND_ENUM: {
            if(data_type->identifier[0] != '\0')
                printf("%s", data_type->identifier);
            else
            {
                printf("enum {");
                printEnumVariants(data_type->variants);
                printf("}");
            }
        } break;
        case AST_DATA_TYPE_KIND_STRUCT: {
            if(data_type->identifier[0] != '\0')
                printf("%s", data_type->identifier);
            else
            {
                printf("struct {");
                printStructMembers(data_type->members);
                printf("}");
            }
        } break;
        case AST_DATA_TYPE_KIND_OPAQUE: {
            if(data_type->identifier[0] != '\0')
                printf("%s", data_type->identifier);
            else
                printf("opaque");
        } break;
        default:
            diagnosticAbortInternal("printASTDataType", "unknown AST data type kind");
    }
}

void appendStringFragment(char *buffer, size_t buffer_size, const char *fragment)
{
    size_t used = strlen(buffer);
    if(used >= buffer_size - 1)
        return;
    snprintf(buffer + used, buffer_size - used, "%s", fragment);
}

void appendFormatFragment(char *buffer, size_t buffer_size, const char *format, ...)
{
    size_t used = strlen(buffer);
    if(used >= buffer_size - 1)
        return;

    va_list args;
    va_start(args, format);
    vsnprintf(buffer + used, buffer_size - used, format, args);
    va_end(args);
}

static const char* astUserFacingIdentifier(const char *identifier)
{
    if(identifier == NULL)
        return "";

    int ignored_index = 0;
    int prefix_length = 0;
    if(sscanf(identifier, "m%d__%n", &ignored_index, &prefix_length) == 1 &&
       prefix_length > 0 &&
       identifier[prefix_length] != '\0')
        return identifier + prefix_length;

    return identifier;
}

typedef struct ASTDataTypePrintStack {
    ASTDataType *items[256];
    int count;
} ASTDataTypePrintStack;

static bool astDataTypePrintStackContains(ASTDataTypePrintStack *stack, ASTDataType *data_type)
{
    for(int i = 0; i < stack->count; i++)
    {
        if(stack->items[i] == data_type)
            return true;
    }
    return false;
}

static void appendASTDataTypeStringInternal(ASTDataType *data_type, char *buffer, size_t buffer_size,
                                            ASTDataTypePrintStack *stack);

void appendFunctionParametersString(ASTFunctionParameter *parameter, char *buffer, size_t buffer_size,
                                    ASTDataTypePrintStack *stack)
{
    while(parameter)
    {
        appendStringFragment(buffer, buffer_size, parameter->identifier);
        appendStringFragment(buffer, buffer_size, ": ");
        appendASTDataTypeStringInternal(parameter->data_type, buffer, buffer_size, stack);
        if(parameter->next)
            appendStringFragment(buffer, buffer_size, ", ");
        parameter = parameter->next;
    }
}

void appendTypeArgumentsString(ASTTypeArgument *argument, char *buffer, size_t buffer_size,
                               ASTDataTypePrintStack *stack)
{
    while(argument)
    {
        appendASTDataTypeStringInternal(argument->data_type, buffer, buffer_size, stack);
        if(argument->next)
            appendStringFragment(buffer, buffer_size, ", ");
        argument = argument->next;
    }
}

void appendStructMembersString(ASTStructMember *member, char *buffer, size_t buffer_size,
                               ASTDataTypePrintStack *stack)
{
    while(member)
    {
        appendStringFragment(buffer, buffer_size, member->identifier);
        appendStringFragment(buffer, buffer_size, ": ");
        if(member->value)
            appendStringFragment(buffer, buffer_size, "<expr>");
        else
            appendASTDataTypeStringInternal(member->data_type, buffer, buffer_size, stack);
        if(member->next)
            appendStringFragment(buffer, buffer_size, ", ");
        member = member->next;
    }
}

void appendEnumVariantsString(ASTEnumVariant *variant, char *buffer, size_t buffer_size)
{
    while(variant)
    {
        appendStringFragment(buffer, buffer_size, variant->identifier);
        if(variant->next)
            appendStringFragment(buffer, buffer_size, ", ");
        variant = variant->next;
    }
}

static void appendASTDataTypeStringInternal(ASTDataType *data_type, char *buffer, size_t buffer_size,
                                            ASTDataTypePrintStack *stack)
{
    if(data_type == NULL)
    {
        appendStringFragment(buffer, buffer_size, "<null type>");
        return;
    }

    if(astDataTypePrintStackContains(stack, data_type))
    {
        appendStringFragment(buffer, buffer_size, "<recursive>");
        return;
    }

    bool pushed = false;
    if(stack->count < 256 &&
       (data_type->kind == AST_DATA_TYPE_KIND_STRUCT ||
        data_type->kind == AST_DATA_TYPE_KIND_ENUM ||
        data_type->kind == AST_DATA_TYPE_KIND_FUNCTION ||
        data_type->kind == AST_DATA_TYPE_KIND_POINTER ||
        data_type->kind == AST_DATA_TYPE_KIND_REFERENCE ||
        data_type->kind == AST_DATA_TYPE_KIND_OPTIONAL ||
        data_type->kind == AST_DATA_TYPE_KIND_ARRAY ||
        data_type->kind == AST_DATA_TYPE_KIND_SLICE ||
        data_type->kind == AST_DATA_TYPE_KIND_STRING ||
        data_type->kind == AST_DATA_TYPE_KIND_APPLY))
    {
        stack->items[stack->count++] = data_type;
        pushed = true;
    }

    switch(data_type->kind)
    {
        case AST_DATA_TYPE_KIND_INFER:
            appendStringFragment(buffer, buffer_size, "infer");
            break;
        case AST_DATA_TYPE_KIND_PRIMARY:
            appendStringFragment(buffer, buffer_size, astPrimaryDataTypeToString(data_type->primary));
            break;
        case AST_DATA_TYPE_KIND_POINTER:
            appendStringFragment(buffer, buffer_size, "*");
            appendASTDataTypeStringInternal(data_type->child, buffer, buffer_size, stack);
            break;
        case AST_DATA_TYPE_KIND_REFERENCE:
            appendStringFragment(buffer, buffer_size, "&");
            appendASTDataTypeStringInternal(data_type->child, buffer, buffer_size, stack);
            break;
        case AST_DATA_TYPE_KIND_OPTIONAL:
            appendStringFragment(buffer, buffer_size, "?");
            appendASTDataTypeStringInternal(data_type->child, buffer, buffer_size, stack);
            break;
        case AST_DATA_TYPE_KIND_FUNCTION:
            appendStringFragment(buffer, buffer_size, "Function([");
            appendFunctionParametersString(data_type->parameters, buffer, buffer_size, stack);
            if(data_type->is_variadic)
            {
                if(data_type->parameters != NULL)
                    appendStringFragment(buffer, buffer_size, ", ");
                appendStringFragment(buffer, buffer_size, "...");
            }
            appendStringFragment(buffer, buffer_size, "], ");
            if(data_type->return_data_type != NULL)
                appendASTDataTypeStringInternal(data_type->return_data_type, buffer, buffer_size, stack);
            else
                appendStringFragment(buffer, buffer_size, "<infer return>");
            appendStringFragment(buffer, buffer_size, ")");
            break;
        case AST_DATA_TYPE_KIND_NAMED:
            appendStringFragment(buffer, buffer_size, astUserFacingIdentifier(data_type->identifier));
            break;
        case AST_DATA_TYPE_KIND_ARRAY:
            appendStringFragment(buffer, buffer_size, "Array(");
            appendASTDataTypeStringInternal(data_type->child, buffer, buffer_size, stack);
            appendFormatFragment(buffer, buffer_size, ", %lld)", data_type->array_length);
            break;
        case AST_DATA_TYPE_KIND_SLICE:
            appendStringFragment(buffer, buffer_size, "[]");
            appendASTDataTypeStringInternal(data_type->child, buffer, buffer_size, stack);
            break;
        case AST_DATA_TYPE_KIND_STRING:
            appendStringFragment(buffer, buffer_size, "string");
            break;
        case AST_DATA_TYPE_KIND_APPLY:
            appendASTDataTypeStringInternal(data_type->callee, buffer, buffer_size, stack);
            appendStringFragment(buffer, buffer_size, "(");
            appendTypeArgumentsString(data_type->arguments, buffer, buffer_size, stack);
            appendStringFragment(buffer, buffer_size, ")");
            break;
        case AST_DATA_TYPE_KIND_ENUM:
            if(data_type->identifier[0] != '\0')
                appendStringFragment(buffer, buffer_size, astUserFacingIdentifier(data_type->identifier));
            else
            {
                appendStringFragment(buffer, buffer_size, "enum {");
                appendEnumVariantsString(data_type->variants, buffer, buffer_size);
                appendStringFragment(buffer, buffer_size, "}");
            }
            break;
        case AST_DATA_TYPE_KIND_STRUCT:
            if(data_type->identifier[0] != '\0')
                appendStringFragment(buffer, buffer_size, astUserFacingIdentifier(data_type->identifier));
            else
            {
                appendStringFragment(buffer, buffer_size, "struct {");
                appendStructMembersString(data_type->members, buffer, buffer_size, stack);
                appendStringFragment(buffer, buffer_size, "}");
            }
            break;
        case AST_DATA_TYPE_KIND_OPAQUE:
            if(data_type->identifier[0] != '\0')
                appendStringFragment(buffer, buffer_size, astUserFacingIdentifier(data_type->identifier));
            else
                appendStringFragment(buffer, buffer_size, "opaque");
            break;
        default:
            diagnosticAbortInternal("appendASTDataTypeString", "unknown AST data type kind");
    }

    if(pushed)
        stack->count--;
}

void appendASTDataTypeString(ASTDataType *data_type, char *buffer, size_t buffer_size)
{
    ASTDataTypePrintStack stack = {0};
    appendASTDataTypeStringInternal(data_type, buffer, buffer_size, &stack);
}

const char* modifierToString(ASTAssignModifier modifier)
{
    if(modifier.is_compile_time_binding)
        return modifier.explicit_type ? "compile_time_typed" : "compile_time_infer";
    if(modifier.is_runtime_binding)
        return modifier.explicit_type ? "runtime_typed" : "runtime_infer";
    return modifier.explicit_type ? "typed" : "assign";
}

const char* astOperatorKindToString(ASTOperatorKind kind)
{
    switch(kind)
    {
        case AST_OPERATOR_NONE: return "";
        case AST_OPERATOR_ADD: return "+";
        case AST_OPERATOR_SUB: return "-";
        case AST_OPERATOR_MUL: return "*";
        case AST_OPERATOR_DIV: return "/";
        case AST_OPERATOR_EQ: return "==";
        default:
            diagnosticAbortInternal("astOperatorKindToString", "unknown operator kind");
    }
}

void printASTNode(ASTNode node)
{
    switch(node.kind)
    {
        case AST_ASSIGN: {
            printf("AST_ASSIGN: %s%smodifier(%s) lhs(",
                node.is_pub ? "pub " : "",
                node.operator_kind != AST_OPERATOR_NONE ? astOperatorKindToString(node.operator_kind) : "",
                modifierToString(node.modifier));
            printASTNode(*(node.lhs));
            printf(") type(");
            printASTDataType(node.data_type);
            printf(") = ");
            printASTNode(*(node.rhs));
            printf("\n");
        } break;
        case AST_BLOCK: {
            printf("AST_BLOCK {\n");
            ASTNode *stmt = node.lhs;
            while(stmt)
            {
                printASTNode(*stmt);
                stmt = stmt->next;
            }
            printf("}\n");
        } break;
        case AST_STATEMENT_EXPR: {
            printf("AST_STATEMENT_EXPR(");
            printASTNode(*(node.lhs));
            printf(")\n");
        } break;
        case AST_STATEMENT_RETURN: {
            printf("AST_STATEMENT_RETURN(");
            if(node.lhs)
                printASTNode(*(node.lhs));
            else
                printf("void");
            printf(")\n");
        } break;
        case AST_STATEMENT_IF: {
            printf("AST_STATEMENT_IF(cond(");
            printASTNode(*(node.lhs));
            printf(") then(");
            printASTNode(*(node.rhs));
            printf(")");
            if(node.body)
            {
                printf(" else(");
                printASTNode(*(node.body));
                printf(")");
            }
            printf("\n");
        } break;
        case AST_STATEMENT_WHILE: {
            printf("AST_STATEMENT_WHILE(cond(");
            printASTNode(*(node.lhs));
            printf(") body(");
            printASTNode(*(node.body));
            printf("))\n");
        } break;
        case AST_STATEMENT_DO_WHILE: {
            printf("AST_STATEMENT_DO_WHILE(body(");
            printASTNode(*(node.body));
            printf(") cond(");
            printASTNode(*(node.lhs));
            printf("))\n");
        } break;
        case AST_STATEMENT_FOR: {
            printf("AST_STATEMENT_FOR(init(");
            if(node.lhs)
                printASTNode(*(node.lhs));
            else
                printf("empty");
            printf(") cond(");
            if(node.rhs)
                printASTNode(*(node.rhs));
            else
                printf("empty");
            printf(") post(");
            if(node.extra)
                printASTNode(*(node.extra));
            else
                printf("empty");
            printf(") body(");
            printASTNode(*(node.body));
            printf("))\n");
        } break;
        case AST_STATEMENT_BREAK: {
            printf("AST_STATEMENT_BREAK\n");
        } break;
        case AST_STATEMENT_CONTINUE: {
            printf("AST_STATEMENT_CONTINUE\n");
        } break;
        case AST_STATEMENT_DEFER: {
            printf("AST_STATEMENT_DEFER(");
            printASTNode(*(node.lhs));
            printf(")\n");
        } break;
        case AST_EXPR_FUNCTION: {
            printf("AST_EXPR_FUNCTION(");
            if(node.captures)
            {
                printf("|");
                printFunctionCaptures(node.captures);
                printf("| ");
            }
            printFunctionParameters(node.parameters);
            if(node.is_variadic)
            {
                if(node.parameters != NULL)
                    printf(", ");
                printf("...");
            }
            printf(") ");
            if(node.return_data_type != NULL)
                printASTDataType(node.return_data_type);
            else
                printf("<infer return>");
            printf(" ");
            printASTNode(*(node.body));
        } break;
        case AST_EXPR_ENUM: {
            printf("AST_EXPR_ENUM {");
            printEnumVariants(node.variants);
            printf("}");
        } break;
        case AST_EXPR_STRUCT: {
            printf("AST_EXPR_STRUCT {");
            printStructMembers(node.members);
            printf("}");
        } break;
        case AST_EXPR_ARRAY_LITERAL: {
            printf("AST_EXPR_ARRAY_LITERAL([");
            ASTNode *element = node.lhs;
            while(element)
            {
                printASTNode(*element);
                element = element->next;
                if(element)
                    printf(", ");
            }
            printf("])");
        } break;
        case AST_EXPR_STRUCT_LITERAL: {
            printf("AST_EXPR_STRUCT_LITERAL(");
            printASTNode(*(node.lhs));
            printf(" {");
            ASTStructLiteralField *field = node.struct_literal_fields;
            while(field)
            {
                printf(".%s = ", field->identifier);
                printASTNode(*(field->value));
                if(field->next)
                    printf(", ");
                field = field->next;
            }
            printf("})");
        } break;
        case AST_EXPR_CALL: {
            printf("AST_EXPR_CALL(callee(");
            printASTNode(*(node.lhs));
            printf(") args(");
            ASTNode *argument = node.rhs;
            while(argument)
            {
                printASTNode(*argument);
                argument = argument->next;
                if(argument)
                    printf(", ");
            }
            printf("))");
        } break;
        case AST_EXPR_MEMBER: {
            printf("AST_EXPR_MEMBER(");
            printASTNode(*(node.lhs));
            printf(".%s)", node.identifier);
        } break;
        case AST_EXPR_INDEX: {
            printf("AST_EXPR_INDEX(");
            printASTNode(*(node.lhs));
            printf("[");
            printASTNode(*(node.rhs));
            printf("])");
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
        case AST_EXPR_ADDRESS_OF: {
            printf("AST_EXPR_ADDRESS_OF(&");
            printASTNode(*(node.lhs));
            printf(")");
        } break;
        case AST_EXPR_ADDRESS_OF_MUT: {
            printf("AST_EXPR_ADDRESS_OF_MUT(&");
            printASTNode(*(node.lhs));
            printf(")");
        } break;
        case AST_EXPR_DEREF: {
            printf("AST_EXPR_DEREF(*");
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
        case AST_EXPR_BUILTIN: {
            printf("AST_EXPR_BUILTIN(@%s", node.identifier);
            if(node.lhs != NULL)
            {
                printf("(");
                ASTNode *argument = node.lhs;
                while(argument)
                {
                    printASTNode(*argument);
                    if(argument->next)
                        printf(", ");
                    argument = argument->next;
                }
                printf(")");
            }
            printf(")");
        } break;
        case AST_EXPR_TYPE_LITERAL: {
            printf("AST_EXPR_TYPE_LITERAL(");
            printASTDataType(node.data_type);
            printf(")");
        } break;
        case AST_EXPR_LITERAL_BOOL: {
            printf("AST_EXPR_LITERAL_BOOL(%s)", node.literal_bool ? "true" : "false");
        } break;
        case AST_EXPR_LITERAL_NULL: {
            printf("AST_EXPR_LITERAL_NULL");
        } break;
        case AST_EXPR_LITERAL_CHAR: {
            printf("AST_EXPR_LITERAL_CHAR(");
            printEscapedChar(node.literal_char);
            printf(")");
        } break;
        case AST_EXPR_LITERAL_STRING: {
            printf("AST_EXPR_LITERAL_STRING(\"%s\")", node.literal_string);
        } break;
        case AST_EXPR_LITERAL_INTEGER: {
            printf("AST_EXPR_LITERAL_INTEGER(%llu)", node.literal_integer.magnitude);
        } break;
        case AST_EXPR_LITERAL_FLOAT: {
            printf("AST_EXPR_LITERAL_FLOAT(%Lf)", node.literal_float);
        } break;
        case AST_START_OF_CODE: {
            if(node.package_name[0] != '\0')
                printf("AST_START_OF_CODE(package=%s)\n", node.package_name);
            else
                printf("AST_START_OF_CODE\n");
            if(node.lhs)
                printASTNode(*(node.lhs));
        } break;
        case AST_END_OF_CODE: {
            printf("AST_END_OF_CODE\n");
        } break;
        default:
            diagnosticAbortInternal("printASTNode", "unknown AST node kind");
    }
}

#endif /* AST_H */
