#ifndef AST_H
#define AST_H

#include <stdio.h>
#include "Token.h"
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
    AST_DATA_TYPE_KIND_FUNCTION,
    AST_DATA_TYPE_KIND_NAMED,
    AST_DATA_TYPE_KIND_ARRAY,
    AST_DATA_TYPE_KIND_APPLY,
    AST_DATA_TYPE_KIND_ENUM,
    AST_DATA_TYPE_KIND_STRUCT,
} ASTDataTypeKind;

typedef struct ASTDataType ASTDataType;
typedef struct ASTNode ASTNode;
typedef struct ASTStructMember ASTStructMember;
typedef struct ASTStructLiteralField ASTStructLiteralField;
typedef struct ASTEnumVariant ASTEnumVariant;
typedef struct ASTTypeArgument ASTTypeArgument;
typedef struct ASTFunctionCapture ASTFunctionCapture;

typedef struct ASTFunctionParameter {
    struct ASTFunctionParameter *next;
    const char *filename;
    int line_number;
    int column_number;
    char identifier[MAX_IDENTIFIER_LENGTH];
    ASTDataType *data_type;
} ASTFunctionParameter;

typedef struct ASTDataType {
    ASTDataTypeKind kind;
    bool mutable;
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
    AST_FUNCTION_CAPTURE_MUT_REFERENCE,
} ASTFunctionCaptureKind;

struct ASTFunctionCapture {
    struct ASTFunctionCapture *next;
    const char *filename;
    int line_number;
    int column_number;
    ASTFunctionCaptureKind kind;
    char identifier[MAX_IDENTIFIER_LENGTH];
};

struct ASTEnumVariant {
    struct ASTEnumVariant *next;
    const char *filename;
    int line_number;
    int column_number;
    char identifier[MAX_IDENTIFIER_LENGTH];
};

struct ASTStructMember {
    struct ASTStructMember *next;
    const char *filename;
    int line_number;
    int column_number;
    char identifier[MAX_IDENTIFIER_LENGTH];
    ASTDataType *data_type;
    ASTNode *value;
};

struct ASTStructLiteralField {
    struct ASTStructLiteralField *next;
    const char *filename;
    int line_number;
    int column_number;
    char identifier[MAX_IDENTIFIER_LENGTH];
    ASTNode *value;
};

typedef struct {
    bool mutable;
    bool explicit_type;
} ASTAssignModifier;

struct ASTNode {
    struct ASTNode *next;
    ASTNodeKind kind;

    const char* filename;
    int line_number;
    int column_number;

    struct ASTNode *lhs;// parenthesis, binary expr use this as left hand side
    struct ASTNode *rhs;// assign or decl use this as expr
    struct ASTNode *extra;// else branch or for update clause

    // literal value
    bool literal_bool;
    char literal_char;
    char literal_string[MAX_STRING_LITERAL_LENGTH];
    long long int literal_integer;
    long double literal_float;

    // assign or decl
    ASTAssignModifier modifier;
    bool is_pub;
    ASTDataType *data_type;
    char identifier[MAX_IDENTIFIER_LENGTH];

    // function literal
    ASTFunctionParameter *parameters;
    bool is_variadic;
    ASTFunctionCapture *captures;
    ASTDataType *return_data_type;
    ASTNode *body;

    // struct literal
    ASTStructMember *members;
    ASTStructLiteralField *struct_literal_fields;
    ASTEnumVariant *variants;
};

ASTNode* newASTNode(ASTNodeKind kind)
{
    ASTNode *node = (ASTNode*) malloc(sizeof(ASTNode));
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
    return node;
}

ASTFunctionParameter* newASTFunctionParameterFromToken(Token *token)
{
    ASTFunctionParameter *parameter = (ASTFunctionParameter*) malloc(sizeof(ASTFunctionParameter));
    memset(parameter, 0, sizeof(ASTFunctionParameter));
    parameter->filename = token->filename;
    parameter->line_number = token->line_number;
    parameter->column_number = token->column_number;
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

ASTDataType* newWrappedDataType(ASTDataTypeKind kind, bool mutable, ASTDataType *child)
{
    ASTDataType *data_type = (ASTDataType*) malloc(sizeof(ASTDataType));
    memset(data_type, 0, sizeof(ASTDataType));
    data_type->kind = kind;
    data_type->mutable = mutable;
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

ASTDataType* cloneDataType(ASTDataType *data_type);
ASTStructMember* cloneStructMembers(ASTStructMember *member);
ASTEnumVariant* cloneEnumVariants(ASTEnumVariant *variant);
ASTTypeArgument* cloneTypeArguments(ASTTypeArgument *argument);
ASTFunctionCapture* cloneFunctionCaptures(ASTFunctionCapture *capture);

ASTFunctionParameter* cloneFunctionParameters(ASTFunctionParameter *parameter)
{
    if(parameter == NULL)
        return NULL;

    ASTFunctionParameter *new_parameter = (ASTFunctionParameter*) malloc(sizeof(ASTFunctionParameter));
    *new_parameter = *parameter;
    new_parameter->data_type = NULL;
    new_parameter->next = NULL;

    if(parameter->data_type)
        new_parameter->data_type = cloneDataType(parameter->data_type);
    if(parameter->next)
        new_parameter->next = cloneFunctionParameters(parameter->next);

    return new_parameter;
}

ASTStructMember* newASTStructMemberFromToken(Token *token)
{
    ASTStructMember *member = (ASTStructMember*) malloc(sizeof(ASTStructMember));
    memset(member, 0, sizeof(ASTStructMember));
    member->filename = token->filename;
    member->line_number = token->line_number;
    member->column_number = token->column_number;
    return member;
}

ASTTypeArgument* newASTTypeArgument(ASTDataType *data_type)
{
    ASTTypeArgument *argument = (ASTTypeArgument*) malloc(sizeof(ASTTypeArgument));
    memset(argument, 0, sizeof(ASTTypeArgument));
    argument->data_type = data_type;
    return argument;
}

ASTFunctionCapture* newASTFunctionCaptureFromToken(Token *token)
{
    ASTFunctionCapture *capture = (ASTFunctionCapture*) malloc(sizeof(ASTFunctionCapture));
    memset(capture, 0, sizeof(ASTFunctionCapture));
    capture->filename = token->filename;
    capture->line_number = token->line_number;
    capture->column_number = token->column_number;
    return capture;
}

ASTStructLiteralField* newASTStructLiteralFieldFromToken(Token *token)
{
    ASTStructLiteralField *field = (ASTStructLiteralField*) malloc(sizeof(ASTStructLiteralField));
    memset(field, 0, sizeof(ASTStructLiteralField));
    field->filename = token->filename;
    field->line_number = token->line_number;
    field->column_number = token->column_number;
    return field;
}

ASTEnumVariant* newASTEnumVariantFromToken(Token *token)
{
    ASTEnumVariant *variant = (ASTEnumVariant*) malloc(sizeof(ASTEnumVariant));
    memset(variant, 0, sizeof(ASTEnumVariant));
    variant->filename = token->filename;
    variant->line_number = token->line_number;
    variant->column_number = token->column_number;
    return variant;
}

ASTDataType* cloneDataType(ASTDataType *data_type)
{
    if(data_type == NULL)
        return NULL;

    ASTDataType *new_data_type = (ASTDataType*) malloc(sizeof(ASTDataType));
    *new_data_type = *data_type;
    new_data_type->child = NULL;
    new_data_type->callee = NULL;
    new_data_type->arguments = NULL;
    new_data_type->parameters = NULL;
    new_data_type->return_data_type = NULL;
    new_data_type->members = NULL;
    new_data_type->variants = NULL;

    if(data_type->child)
        new_data_type->child = cloneDataType(data_type->child);
    if(data_type->callee)
        new_data_type->callee = cloneDataType(data_type->callee);
    if(data_type->arguments)
        new_data_type->arguments = cloneTypeArguments(data_type->arguments);
    if(data_type->parameters)
        new_data_type->parameters = cloneFunctionParameters(data_type->parameters);
    if(data_type->return_data_type)
        new_data_type->return_data_type = cloneDataType(data_type->return_data_type);
    if(data_type->members)
        new_data_type->members = cloneStructMembers(data_type->members);
    if(data_type->variants)
        new_data_type->variants = cloneEnumVariants(data_type->variants);
    return new_data_type;
}

ASTTypeArgument* cloneTypeArguments(ASTTypeArgument *argument)
{
    if(argument == NULL)
        return NULL;

    ASTTypeArgument *new_argument = (ASTTypeArgument*) malloc(sizeof(ASTTypeArgument));
    memset(new_argument, 0, sizeof(ASTTypeArgument));
    if(argument->data_type)
        new_argument->data_type = cloneDataType(argument->data_type);
    if(argument->next)
        new_argument->next = cloneTypeArguments(argument->next);
    return new_argument;
}

ASTStructMember* cloneStructMembers(ASTStructMember *member)
{
    if(member == NULL)
        return NULL;

    ASTStructMember *new_member = (ASTStructMember*) malloc(sizeof(ASTStructMember));
    *new_member = *member;
    new_member->next = NULL;
    new_member->data_type = NULL;

    if(member->data_type)
        new_member->data_type = cloneDataType(member->data_type);
    if(member->next)
        new_member->next = cloneStructMembers(member->next);
    return new_member;
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
        case AST_EXPR_LITERAL_CHAR: return "AST_EXPR_LITERAL_CHAR";
        case AST_EXPR_LITERAL_STRING: return "AST_EXPR_LITERAL_STRING";
        case AST_EXPR_LITERAL_INTEGER: return "AST_EXPR_LITERAL_INTEGER";
        case AST_EXPR_LITERAL_FLOAT: return "AST_EXPR_LITERAL_FLOAT";
        default:
            printf("astNodeKindToString: unknown AST node kind\n");
            exit(1);
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
            printf("astPrimaryDataTypeToString: unknown AST primary data type\n");
            exit(1);
    }
}

void printASTDataType(ASTDataType *data_type);
void printASTNode(ASTNode node);
void printEnumVariants(ASTEnumVariant *variant);
void printFunctionCaptures(ASTFunctionCapture *capture);
void printTypeArguments(ASTTypeArgument *argument);

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
        else if(capture->kind == AST_FUNCTION_CAPTURE_MUT_REFERENCE)
            printf("&mut ");
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
            if(data_type->mutable)
                printf("mut ");
            printASTDataType(data_type->child);
        } break;
        case AST_DATA_TYPE_KIND_REFERENCE: {
            printf("&");
            if(data_type->mutable)
                printf("mut ");
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
            printASTDataType(data_type->return_data_type);
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
        default:
            printf("printASTDataType: unknown AST data type kind\n");
            exit(1);
    }
}

const char* modifierToString(ASTAssignModifier modifier)
{
    if(modifier.mutable)
        return "mutable";
    else
        return "immutable";
}

void printASTNode(ASTNode node)
{
    switch(node.kind)
    {
        case AST_ASSIGN: {
            printf("AST_ASSIGN: %smodifier(%s) lhs(",
                node.is_pub ? "pub " : "",
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
            printASTDataType(node.return_data_type);
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
            printf("AST_EXPR_ADDRESS_OF_MUT(&mut ");
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
        case AST_EXPR_LITERAL_CHAR: {
            printf("AST_EXPR_LITERAL_CHAR(");
            printEscapedChar(node.literal_char);
            printf(")");
        } break;
        case AST_EXPR_LITERAL_STRING: {
            printf("AST_EXPR_LITERAL_STRING(\"%s\")", node.literal_string);
        } break;
        case AST_EXPR_LITERAL_INTEGER: {
            printf("AST_EXPR_LITERAL_INTEGER(%lld)", node.literal_integer);
        } break;
        case AST_EXPR_LITERAL_FLOAT: {
            printf("AST_EXPR_LITERAL_FLOAT(%Lf)", node.literal_float);
        } break;
        case AST_START_OF_CODE: {
            printf("AST_START_OF_CODE\n");
            if(node.lhs)
                printASTNode(*(node.lhs));
        } break;
        case AST_END_OF_CODE: {
            printf("AST_END_OF_CODE\n");
        } break;
        default:
            printf("printASTNode: unknown AST node kind\n");
            exit(1);
    }
}

#endif /* AST_H */
