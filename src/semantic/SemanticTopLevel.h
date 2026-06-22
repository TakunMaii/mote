#ifndef SEMANTIC_TOP_LEVEL_H
#define SEMANTIC_TOP_LEVEL_H

#include "SemanticShared.h"

void resolveTopLevelTypeDeclarations(ASTNode *block, ScopeFrame *scope, FunctionContext *function_context)
{
    if(block == NULL || scope == NULL)
        return;

    ASTNode *node = block->lhs;
    while(node)
    {
        if(node->kind == AST_ASSIGN &&
           node->lhs != NULL &&
           node->lhs->kind == AST_EXPR_VARIABLE &&
           isTypeDeclAssign(node, scope))
        {
            const char *binding_name = semanticAssignIdentifier(node);
            if(isStructDeclAssign(node))
            {
                TypeInfo *existing_type_info = findTypeInfo(scope, binding_name);
                if(existing_type_info != NULL &&
                   (!existing_type_info->predeclared ||
                    existing_type_info->data_type == NULL ||
                    existing_type_info->data_type->kind != AST_DATA_TYPE_KIND_STRUCT))
                {
                    semanticAbortTypeFormatted("T1108", node,
                                               "duplicate type declaration",
                                               "type `%s` has already been declared in this scope",
                                               astUserFacingIdentifier(binding_name));
                }

                ASTDataType *struct_type = declareStructType(node, scope);
                semanticBindTypeDeclarationValue(node, scope, struct_type);
            }
            else if(isEnumDeclAssign(node))
            {
                TypeInfo *existing_type_info = findTypeInfo(scope, binding_name);
                if(existing_type_info != NULL &&
                   (!existing_type_info->predeclared ||
                    existing_type_info->data_type == NULL ||
                    existing_type_info->data_type->kind != AST_DATA_TYPE_KIND_ENUM))
                {
                    semanticAbortTypeFormatted("T1109", node,
                                               "duplicate type declaration",
                                               "type `%s` has already been declared in this scope",
                                               astUserFacingIdentifier(binding_name));
                }

                ASTDataType *enum_type = declareEnumType(node, scope);
                semanticBindTypeDeclarationValue(node, scope, enum_type);
            }
            else
            {
                TypeInfo *existing_type_info = findTypeInfo(scope, binding_name);
                if(existing_type_info != NULL &&
                   !existing_type_info->predeclared &&
                   existing_type_info->data_type != NULL)
                {
                    continue;
                }

                TypeSystemExprType expr_type = inferExprType(node->rhs, scope);
                TypeInfo *type_info = existing_type_info;
                if(type_info == NULL)
                    type_info = declareTypeInfo(scope, binding_name);
                type_info->predeclared = false;
                type_info->data_type = resolveNamedDataType(expr_type.data_type, scope,
                                                            function_context == NULL ? NULL : function_context->self_data_type);
                if(type_info->data_type != NULL &&
                   type_info->data_type->kind == AST_DATA_TYPE_KIND_OPAQUE &&
                   type_info->data_type->identifier[0] == '\0')
                    strcpy(type_info->data_type->identifier, binding_name);
                node->data_type = cloneDataType(type_info->data_type);
                semanticBindTypeDeclarationValue(node, scope, type_info->data_type);
            }
        }
        node = node->next;
    }
}

void predeclareTopLevelBindings(ASTNode *block, ScopeFrame *scope)
{
    if(block == NULL || scope == NULL)
        return;

    ASTNode *node = block->lhs;
    while(node)
    {
        if(node->kind == AST_ASSIGN && node->lhs != NULL && node->lhs->kind == AST_EXPR_VARIABLE)
        {
            if(node->rhs != NULL && node->rhs->kind == AST_EXPR_STRUCT)
            {
                if(findTypeInfoInScope(scope, node->identifier) < 0)
                {
                    const char *binding_name = semanticAssignIdentifier(node);
                    TypeInfo *type_info = declareTypeInfo(scope, binding_name);
                    type_info->data_type = newStructDataType(binding_name, NULL);
                    type_info->predeclared = true;
                }
            }
            else if(node->rhs != NULL && node->rhs->kind == AST_EXPR_ENUM)
            {
                if(findTypeInfoInScope(scope, semanticAssignIdentifier(node)) < 0)
                {
                    const char *binding_name = semanticAssignIdentifier(node);
                    TypeInfo *type_info = declareTypeInfo(scope, binding_name);
                    type_info->data_type = newEnumDataType(binding_name, NULL);
                    type_info->predeclared = true;
                }
            }

            predeclareTopLevelVariableBinding(node, scope);
        }
        node = node->next;
    }
}

void predeclareTopLevelFunctionTypes(ASTNode *block, ScopeFrame *scope, ASTDataType *self_data_type)
{
    if(block == NULL || scope == NULL)
        return;

    ASTNode *node = block->lhs;
    while(node)
    {
        if(node->kind == AST_ASSIGN &&
           node->lhs != NULL &&
           node->lhs->kind == AST_EXPR_VARIABLE &&
           node->rhs != NULL &&
           node->rhs->kind == AST_EXPR_FUNCTION)
        {
            const char *binding_name = semanticAssignIdentifier(node);
            int variable_index = findVariableInfoInScope(scope, binding_name);
            VariableInfo *variable_info = variable_index >= 0
                ? &(scope->variable_infos[variable_index])
                : NULL;
            if(variable_info != NULL && variable_info->data_type != NULL && !isInferDataType(variable_info->data_type))
            {
                node = node->next;
                continue;
            }

            ASTDataType *function_type = resolveFunctionExprDataType(node->rhs, scope, self_data_type);
            node->rhs->data_type = cloneDataType(function_type);
            node->rhs->return_data_type = cloneDataType(function_type->return_data_type);
            node->data_type = cloneDataType(function_type);

            if(variable_info == NULL)
                variable_info = declareVariableInfo(scope, binding_name);
            variable_info->is_compile_time_constant = node->modifier.is_compile_time_binding;
            variable_info->data_type = cloneDataType(function_type);
            variable_info->function_value = node->rhs;
            variable_info->extern_value = NULL;
            if(function_type->return_data_type != NULL &&
               function_type->return_data_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
               function_type->return_data_type->primary == AST_PRIMARY_DATA_TYPE_TYPE)
                variable_info->type_value = cloneDataType(function_type);
            else
                variable_info->type_value = NULL;
            variable_info->operator_kind = node->operator_kind;
        }
        node = node->next;
    }
}

#endif /* SEMANTIC_TOP_LEVEL_H */
