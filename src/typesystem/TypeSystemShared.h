#ifndef TYPE_SYSTEM_SHARED_H
#define TYPE_SYSTEM_SHARED_H

#include "../Diagnostic.h"
#include "../AST.h"
#include "../SymbolTable.h"
#include <stdbool.h>
#include <stdio.h>

typedef enum TypeSystemExprTypeKind {
    TYPE_SYSTEM_EXPR_TYPE_VALUE,
    TYPE_SYSTEM_EXPR_TYPE_TYPE,
    TYPE_SYSTEM_EXPR_TYPE_NULL,
    TYPE_SYSTEM_EXPR_TYPE_LITERAL_INTEGER,
    TYPE_SYSTEM_EXPR_TYPE_LITERAL_FLOAT,
} TypeSystemExprTypeKind;

typedef struct TypeSystemExprType {
    TypeSystemExprTypeKind kind;
    ASTDataType *data_type;
} TypeSystemExprType;

TypeSystemExprType inferExprType(ASTNode *node, ScopeFrame *scope);
bool isSameDataType(ASTDataType *lhs, ASTDataType *rhs);
ASTDataType* inferDeclaredTypeFromExpr(ASTNode *expr, ScopeFrame *scope);
ASTStructMember* resolveStructMembers(ASTStructMember *member, ScopeFrame *scope, ASTDataType *self_data_type);
ASTDataType* resolveNamedDataType(ASTDataType *data_type, ScopeFrame *scope, ASTDataType *self_data_type);
ASTDataType* resolveFunctionExprDataType(ASTNode *node, ScopeFrame *outer_scope, ASTDataType *self_data_type);
void bindCapturedValuesForInstantiation(ASTFunctionCapture *capture, ScopeFrame *inst_scope, ScopeFrame *outer_scope);
void bindCallArgumentsForInstantiation(ASTFunctionParameter *parameter, ASTNode *argument, ScopeFrame *inst_scope, ScopeFrame *outer_scope);
ASTNode* findReturnedExpr(ASTNode *function_expr);
ASTDataType* instantiateTypeExprValue(ASTNode *expr, ScopeFrame *inst_scope);
TypeSystemExprType instantiateFunctionCallExprType(ASTNode *function_value, ASTNode *call_arguments, ASTNode *call_site, ScopeFrame *outer_scope);
ASTNode* buildTypeLiteralArgumentExprs(ASTTypeArgument *argument, ScopeFrame *scope, ASTDataType *self_data_type);
ASTNode* resolveFunctionValueExpr(ASTNode *expr, ScopeFrame *scope);
ASTNode* resolveExternValueExpr(ASTNode *expr, ScopeFrame *scope);
bool canImplicitConvertExprToType(ASTNode *expr, ScopeFrame *scope, ASTDataType *target_type);
bool canBindReferenceArgument(ASTNode *argument, ScopeFrame *scope, ASTDataType *parameter_type);
void checkFunctionCallArguments(ASTFunctionParameter *parameter, ASTNode *argument, ScopeFrame *scope, bool is_variadic, ASTNode *call_site);

typedef struct ResolvedOperatorOverload {
    ASTNode *function_value;
    ASTDataType *result_type;
} ResolvedOperatorOverload;

static bool resolveOperatorOverload(ASTOperatorKind operator_kind,
                                    ASTNode *lhs_expr,
                                    ASTNode *rhs_expr,
                                    ScopeFrame *scope,
                                    ResolvedOperatorOverload *out);

typedef struct ResolveDataTypeEntry {
    ASTDataType *source;
    ASTDataType *resolved;
    struct ResolveDataTypeEntry *next;
} ResolveDataTypeEntry;

static ScopeFrame* findInstantiatingFunctionScope(ScopeFrame *scope, ASTNode *function_value)
{
    ScopeFrame *current = scope;
    while(current != NULL)
    {
        if(current->instantiating_function == function_value)
            return current;
        current = current->parent;
    }
    return NULL;
}

static ASTDataType* resolveNamedDataTypeInternal(ASTDataType *data_type, ScopeFrame *scope,
                                                 ASTDataType *self_data_type,
                                                 ResolveDataTypeEntry **memo,
                                                 bool allow_recursive_factory_result);
static ASTStructMember* resolveStructMembersInternal(ASTStructMember *member, ScopeFrame *scope,
                                                     ASTDataType *self_data_type,
                                                     ResolveDataTypeEntry **memo,
                                                     bool allow_recursive_factory_result);
static void bindSpecializedNamedTypesInScope(ScopeFrame *scope, ASTDataType *source_type, ASTDataType *resolved_type);
static ASTDataType* resolveStructMemberDataType(ASTStructMember *member, ScopeFrame *scope, ASTDataType *struct_type);
static ScopeFrame* buildMethodLexicalTypeScope(ASTStructMember *member, ASTDataType *resolved_member_type,
                                               ScopeFrame *inst_scope, ASTDataType *struct_type);

TypeSystemExprType newValueExprType(ASTDataType *data_type)
{
    TypeSystemExprType expr_type = {0};
    expr_type.kind = TYPE_SYSTEM_EXPR_TYPE_VALUE;
    expr_type.data_type = cloneDataType(data_type);
    return expr_type;
}

TypeSystemExprType newTypeExprType(ASTDataType *data_type)
{
    TypeSystemExprType expr_type = {0};
    expr_type.kind = TYPE_SYSTEM_EXPR_TYPE_TYPE;
    expr_type.data_type = cloneDataType(data_type);
    return expr_type;
}

TypeSystemExprType newLiteralIntegerExprType()
{
    TypeSystemExprType expr_type = {0};
    expr_type.kind = TYPE_SYSTEM_EXPR_TYPE_LITERAL_INTEGER;
    return expr_type;
}

TypeSystemExprType newNullExprType()
{
    TypeSystemExprType expr_type = {0};
    expr_type.kind = TYPE_SYSTEM_EXPR_TYPE_NULL;
    return expr_type;
}

TypeSystemExprType newLiteralFloatExprType()
{
    TypeSystemExprType expr_type = {0};
    expr_type.kind = TYPE_SYSTEM_EXPR_TYPE_LITERAL_FLOAT;
    return expr_type;
}

MOTE_NORETURN void typeSystemAbortNode(const char *code, ASTNode *node, const char *message, const char *label)
{
    diagnosticAbortSimple(code, message, astNodeSourceSpan(node), label);
}

MOTE_NORETURN void typeSystemAbortFormatted(const char *code, ASTNode *node, const char *label, const char *format, ...)
{
    Diagnostic diagnostic = diagnosticMake(DIAGNOSTIC_SEVERITY_ERROR, code, astNodeSourceSpan(node), "");
    va_list args;
    va_start(args, format);
    diagnosticVFormat(diagnostic.message, sizeof(diagnostic.message), format, args);
    va_end(args);
    if(label != NULL)
        diagnosticSetPrimaryLabel(&diagnostic, "%s", label);
    diagnosticAbort(diagnostic);
}

MOTE_NORETURN void typeSystemAbortNoSpan(const char *code, const char *message, const char *detail)
{
    diagnosticAbortSimple(code, message, makeSourceSpan(NULL, 0, 0, 0, 0), detail);
}

void typeSystemDescribeExprType(TypeSystemExprType expr_type, char *buffer, size_t buffer_size)
{
    if(buffer_size == 0)
        return;

    if(expr_type.kind == TYPE_SYSTEM_EXPR_TYPE_LITERAL_INTEGER)
    {
        diagnosticFormat(buffer, buffer_size, "literal integer");
        return;
    }
    if(expr_type.kind == TYPE_SYSTEM_EXPR_TYPE_LITERAL_FLOAT)
    {
        diagnosticFormat(buffer, buffer_size, "literal float");
        return;
    }
    if(expr_type.kind == TYPE_SYSTEM_EXPR_TYPE_TYPE)
    {
        diagnosticFormat(buffer, buffer_size, "type");
        return;
    }
    if(expr_type.kind == TYPE_SYSTEM_EXPR_TYPE_NULL)
    {
        diagnosticFormat(buffer, buffer_size, "`null`");
        return;
    }
    if(expr_type.data_type == NULL)
    {
        diagnosticFormat(buffer, buffer_size, "<unknown>");
        return;
    }
    appendASTDataTypeString(expr_type.data_type, buffer, buffer_size);
}

void typeSystemDescribeDataType(ASTDataType *data_type, char *buffer, size_t buffer_size)
{
    if(buffer_size == 0)
        return;

    if(data_type == NULL)
    {
        diagnosticFormat(buffer, buffer_size, "<unknown>");
        return;
    }

    appendASTDataTypeString(data_type, buffer, buffer_size);
}

static int typeSystemCountFunctionParameters(ASTFunctionParameter *parameter)
{
    int count = 0;
    while(parameter != NULL)
    {
        count++;
        parameter = parameter->next;
    }
    return count;
}

static int typeSystemCountCallArguments(ASTNode *argument)
{
    int count = 0;
    while(argument != NULL)
    {
        count++;
        argument = argument->next;
    }
    return count;
}

static MOTE_NORETURN void typeSystemAbortExpectedDescriptionFoundExpr(const char *code,
                                                                      ASTNode *node,
                                                                      const char *message,
                                                                      const char *expected_description,
                                                                      TypeSystemExprType actual_type)
{
    char actual_buffer[256] = {0};
    typeSystemDescribeExprType(actual_type, actual_buffer, sizeof(actual_buffer));

    Diagnostic diagnostic = diagnosticMake(DIAGNOSTIC_SEVERITY_ERROR,
                                           code,
                                           astNodeSourceSpan(node),
                                           message);
    diagnosticSetPrimaryLabel(&diagnostic,
                              "expected %s, found %s",
                              expected_description,
                              actual_buffer);
    diagnosticAbort(diagnostic);
}

static MOTE_NORETURN void typeSystemAbortExpectedDataTypeFoundExpr(const char *code,
                                                                   ASTNode *node,
                                                                   const char *message,
                                                                   ASTDataType *expected_type,
                                                                   TypeSystemExprType actual_type)
{
    char expected_buffer[256] = {0};
    char actual_buffer[256] = {0};
    typeSystemDescribeDataType(expected_type, expected_buffer, sizeof(expected_buffer));
    typeSystemDescribeExprType(actual_type, actual_buffer, sizeof(actual_buffer));

    Diagnostic diagnostic = diagnosticMake(DIAGNOSTIC_SEVERITY_ERROR,
                                           code,
                                           astNodeSourceSpan(node),
                                           message);
    diagnosticSetPrimaryLabel(&diagnostic,
                              "expected %s, found %s",
                              expected_buffer,
                              actual_buffer);
    diagnosticAbort(diagnostic);
}

bool isInferDataType(ASTDataType *data_type)
{
    return data_type != NULL && data_type->kind == AST_DATA_TYPE_KIND_INFER;
}


#endif /* TYPE_SYSTEM_SHARED_H */
