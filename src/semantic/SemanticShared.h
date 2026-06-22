#ifndef SEMANTIC_SHARED_H
#define SEMANTIC_SHARED_H

#include "../Diagnostic.h"
#include "../AST.h"
#include "../SymbolTable.h"
#include "../TypeSystem.h"
#include <stdbool.h>
#include <string.h>

void semanticAbortNode(const char *code, ASTNode *node, const char *message, const char *label)
{
    diagnosticAbortSimple(code, message, astNodeSourceSpan(node), label);
}

void semanticAbortNodeFormatted(const char *code, ASTNode *node, const char *label, const char *format, ...)
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

void semanticAbortTypeNode(const char *code, ASTNode *node, const char *message, const char *label)
{
    diagnosticAbortSimple(code, message, astNodeSourceSpan(node), label);
}

void semanticAbortTypeFormatted(const char *code, ASTNode *node, const char *label, const char *format, ...)
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

void semanticAbortTypeDataType(const char *code, ASTNode *node, const char *message, ASTDataType *expected_type)
{
    char type_buffer[256] = {0};
    appendASTDataTypeString(expected_type, type_buffer, sizeof(type_buffer));
    Diagnostic diagnostic = diagnosticMake(DIAGNOSTIC_SEVERITY_ERROR, code, astNodeSourceSpan(node), message);
    diagnosticSetPrimaryLabel(&diagnostic, "expected type %s", type_buffer);
    diagnosticAbort(diagnostic);
}

bool isExplicitDeclared(ASTNode *node)
{
    return node->modifier.is_runtime_binding || node->modifier.is_compile_time_binding;
}

bool isReferenceDataType(ASTDataType *data_type)
{
    return data_type != NULL && data_type->kind == AST_DATA_TYPE_KIND_REFERENCE;
}

static bool semanticFunctionHasTypeParameters(ASTFunctionParameter *parameter)
{
    while(parameter)
    {
        if(parameter->data_type != NULL &&
           parameter->data_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
           parameter->data_type->primary == AST_PRIMARY_DATA_TYPE_TYPE)
            return true;
        parameter = parameter->next;
    }
    return false;
}

bool isPointerOrReferenceDataType(ASTDataType *data_type)
{
    return data_type != NULL &&
           (data_type->kind == AST_DATA_TYPE_KIND_POINTER || data_type->kind == AST_DATA_TYPE_KIND_REFERENCE);
}

bool isStructDeclAssign(ASTNode *node)
{
    return node != NULL &&
           node->kind == AST_ASSIGN &&
           node->lhs != NULL &&
           node->lhs->kind == AST_EXPR_VARIABLE &&
           node->rhs != NULL &&
           node->rhs->kind == AST_EXPR_STRUCT;
}

bool isEnumDeclAssign(ASTNode *node)
{
    return node != NULL &&
           node->kind == AST_ASSIGN &&
           node->lhs != NULL &&
           node->lhs->kind == AST_EXPR_VARIABLE &&
           node->rhs != NULL &&
           node->rhs->kind == AST_EXPR_ENUM;
}

bool exprLooksLikeTypeDeclValue(ASTNode *node, ScopeFrame *scope)
{
    if(node == NULL)
        return false;

    switch(node->kind)
    {
        case AST_EXPR_TYPE_LITERAL:
        case AST_EXPR_STRUCT:
        case AST_EXPR_ENUM:
            return true;
        case AST_EXPR_FUNCTION:
            return node->return_data_type != NULL &&
                   node->return_data_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
                   node->return_data_type->primary == AST_PRIMARY_DATA_TYPE_TYPE;
        case AST_EXPR_PARENTHESIS:
        case AST_EXPR_DEREF:
        case AST_EXPR_ADDRESS_OF:
            return exprLooksLikeTypeDeclValue(node->lhs, scope);
        case AST_EXPR_VARIABLE:
            if(strcmp(node->identifier, "Self") == 0 ||
               strcmp(node->identifier, "opaque") == 0 ||
               builtinIdentifierToDataType(node->identifier) != NULL ||
               findTypeInfo(scope, node->identifier) != NULL)
                return true;
            if(scope != NULL)
            {
                VariableInfo *variable_info = findVariableInfo(scope, node->identifier);
                if(variable_info != NULL)
                {
                    if(variable_info->type_value != NULL)
                        return true;
                    if(variable_info->data_type != NULL &&
                       variable_info->data_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
                       variable_info->data_type->primary == AST_PRIMARY_DATA_TYPE_TYPE)
                        return true;
                }
            }
            return false;
        default:
            return false;
    }
}

bool isTypeDeclAssign(ASTNode *node, ScopeFrame *scope)
{
    if(node == NULL ||
       node->kind != AST_ASSIGN ||
       node->lhs == NULL ||
       node->lhs->kind != AST_EXPR_VARIABLE ||
       node->rhs == NULL)
        return false;

    if(isStructDeclAssign(node) || isEnumDeclAssign(node))
        return true;

    return exprLooksLikeTypeDeclValue(node->rhs, scope);
}

void declareFunctionCaptures(ASTFunctionCapture *capture, ScopeFrame *target_scope, ScopeFrame *source_scope);
void checkExprDeclaredVariable(ASTNode *node, ScopeFrame *scope);
void checkAssignSemanticsInBlock(ASTNode *block, ScopeFrame *parent_scope, FunctionContext *function_context);
void checkAssignTypesInBlock(ASTNode *block, ScopeFrame *parent_scope, FunctionContext *function_context);
void checkStatementSemantics(ASTNode *node, ScopeFrame *scope, FunctionContext *function_context);
void checkStatementTypes(ASTNode *node, ScopeFrame *scope, FunctionContext *function_context);
void checkFunctionExprSemantics(ASTNode *node, ScopeFrame *scope, ASTDataType *self_data_type);
void checkStructExprSemantics(ASTNode *node, ScopeFrame *scope, ASTDataType *self_data_type);
void checkFunctionExprTypes(ASTNode *node, ScopeFrame *scope, ASTDataType *self_data_type);
void declareResolvedFunctionParameters(ASTFunctionParameter *parameter, ScopeFrame *scope, ASTDataType *self_data_type);
void checkFunctionCallArgumentSemantics(ASTNode *call_node, ASTDataType *function_type, ScopeFrame *scope);
ASTDataType* resolveCallSemanticFunctionType(ASTNode *call_node, ScopeFrame *scope);
void checkSpecializedCallArguments(ASTNode *call_node, ScopeFrame *scope);
ASTDataType* declareStructType(ASTNode *node, ScopeFrame *scope);
ASTDataType* declareEnumType(ASTNode *node, ScopeFrame *scope);
bool isBoolConditionType(TypeSystemExprType expr_type);
void checkConditionType(ASTNode *condition, ScopeFrame *scope);
void predeclareTopLevelBindings(ASTNode *block, ScopeFrame *scope);
void predeclareTopLevelFunctionTypes(ASTNode *block, ScopeFrame *scope, ASTDataType *self_data_type);
void resolveTopLevelTypeDeclarations(ASTNode *block, ScopeFrame *scope, FunctionContext *function_context);
ASTDataType* resolveFunctionExprDataType(ASTNode *node, ScopeFrame *outer_scope, ASTDataType *self_data_type);

static const char* semanticAssignIdentifier(ASTNode *node)
{
    if(node != NULL &&
       node->kind == AST_ASSIGN &&
       node->lhs != NULL &&
       node->lhs->kind == AST_EXPR_VARIABLE &&
       node->lhs->identifier[0] != '\0')
        return node->lhs->identifier;
    return node != NULL ? node->identifier : "";
}

static void semanticBindTypeDeclarationValue(ASTNode *node, ScopeFrame *scope, ASTDataType *declared_type)
{
    if(node == NULL || scope == NULL || declared_type == NULL)
        return;

    const char *binding_name = semanticAssignIdentifier(node);
    TypeInfo *resolved_type_info = findTypeInfo(scope, binding_name);
    if(resolved_type_info != NULL && resolved_type_info->data_type != NULL)
        declared_type = resolved_type_info->data_type;

    int variable_index = findVariableInfoInScope(scope, binding_name);
    VariableInfo *variable_info = variable_index >= 0
        ? &(scope->variable_infos[variable_index])
        : declareVariableInfo(scope, binding_name);

    variable_info->is_compile_time_constant = true;
    variable_info->predeclared = false;
    variable_info->data_type = cloneDataType(declared_type);
    variable_info->type_value = cloneDataType(declared_type);
    variable_info->function_value = resolveFunctionValueExpr(node->rhs, scope);
    variable_info->extern_value = resolveExternValueExpr(node->rhs, scope);
    variable_info->value_expr = node->rhs;
    variable_info->operator_kind = node->operator_kind;
}

static void predeclareTopLevelVariableBinding(ASTNode *node, ScopeFrame *scope)
{
    if(node == NULL || scope == NULL || node->kind != AST_ASSIGN || node->lhs == NULL || node->lhs->kind != AST_EXPR_VARIABLE)
        return;

    const char *binding_name = semanticAssignIdentifier(node);
    if(findVariableInfoInScope(scope, binding_name) >= 0)
        return;

    VariableInfo *variable_info = declareVariableInfo(scope, binding_name);
    variable_info->is_compile_time_constant = node->modifier.is_compile_time_binding;
    variable_info->predeclared = true;
    if(isTypeDeclAssign(node, scope))
        variable_info->data_type = newPrimaryDataType(AST_PRIMARY_DATA_TYPE_TYPE);
    else if(node->data_type != NULL && isExplicitDeclared(node))
        variable_info->data_type = cloneDataType(node->data_type);
    else
        variable_info->data_type = newInferDataType();
    variable_info->operator_kind = node->operator_kind;
    variable_info->value_expr = node->rhs;
    if(node->rhs != NULL && node->rhs->kind == AST_EXPR_FUNCTION)
        variable_info->function_value = node->rhs;
    if(node->rhs != NULL && node->rhs->kind == AST_EXPR_BUILTIN && strcmp(node->rhs->identifier, "extern") == 0)
        variable_info->extern_value = node->rhs;
}

bool isInsideFunction(FunctionContext *function_context)
{
    return function_context != NULL && function_context->active;
}

FunctionContext* deriveLoopContext(FunctionContext *parent_context, FunctionContext *loop_context)
{
    memset(loop_context, 0, sizeof(FunctionContext));
    if(parent_context != NULL)
        *loop_context = *parent_context;
    loop_context->parent = parent_context;
    loop_context->loop_depth += 1;
    return loop_context;
}

FunctionContext* deriveDeferContext(FunctionContext *parent_context, FunctionContext *defer_context)
{
    memset(defer_context, 0, sizeof(FunctionContext));
    if(parent_context != NULL)
        *defer_context = *parent_context;
    defer_context->parent = parent_context;
    defer_context->inside_defer = true;
    return defer_context;
}

#endif /* SEMANTIC_SHARED_H */
