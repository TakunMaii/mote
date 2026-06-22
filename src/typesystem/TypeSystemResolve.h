#ifndef TYPE_SYSTEM_RESOLVE_H
#define TYPE_SYSTEM_RESOLVE_H

#include "TypeSystemData.h"

static ASTDataType* resolveNamedDataTypeInternal(ASTDataType *data_type, ScopeFrame *scope,
                                                 ASTDataType *self_data_type,
                                                 ResolveDataTypeEntry **memo,
                                                 bool allow_recursive_factory_result)
{
    if(data_type == NULL)
        return NULL;

    ResolveDataTypeEntry *entry = memo == NULL ? NULL : *memo;
    while(entry != NULL)
    {
        if(entry->source == data_type)
            return entry->resolved;
        entry = entry->next;
    }

    switch(data_type->kind)
    {
        case AST_DATA_TYPE_KIND_INFER:
        case AST_DATA_TYPE_KIND_PRIMARY:
        case AST_DATA_TYPE_KIND_OPAQUE:
            return cloneDataType(data_type);
        case AST_DATA_TYPE_KIND_STRUCT: {
            if(data_type->identifier[0] != '\0')
            {
                TypeInfo *type_info = findTypeInfo(scope, data_type->identifier);
                if(type_info != NULL &&
                   type_info->data_type != NULL &&
                   type_info->data_type->kind == AST_DATA_TYPE_KIND_STRUCT)
                {
                    if(type_info->data_type == data_type)
                        return cloneDataType(data_type);
                    return resolveNamedDataTypeInternal(type_info->data_type, scope, self_data_type, memo,
                                                        allow_recursive_factory_result);
                }
            }

            ASTDataType *resolved_struct = newStructDataType(data_type->identifier, NULL);
            ResolveDataTypeEntry *new_entry = (ResolveDataTypeEntry*) malloc(sizeof(ResolveDataTypeEntry));
            new_entry->source = data_type;
            new_entry->resolved = resolved_struct;
            new_entry->next = memo == NULL ? NULL : *memo;
            if(memo != NULL)
                *memo = new_entry;
            resolved_struct->members = resolveStructMembersInternal(data_type->members, scope, resolved_struct, memo, false);
            return resolved_struct;
        }
        case AST_DATA_TYPE_KIND_NAMED: {
            ASTDataType *builtin_type = builtinIdentifierToDataType(data_type->identifier);
            if(builtin_type != NULL)
                return builtin_type;

            if(strcmp(data_type->identifier, "Self") == 0)
            {
                if(self_data_type == NULL)
                    diagnosticAbortFormatted("T1201",
                                             data_type->filename != NULL
                                                 ? astDataTypeSourceSpan(data_type)
                                                 : (scope != NULL && scope->instantiation_site != NULL
                                                        ? astNodeSourceSpan(scope->instantiation_site)
                                                        : makeSourceSpan(NULL, 0, 0, 0, 0)),
                                             NULL,
                                             "`Self` is only allowed inside a struct method");
                return self_data_type;
            }

            TypeInfo *type_info = findTypeInfo(scope, data_type->identifier);
            if(type_info == NULL)
            {
                VariableInfo *variable_info = findVariableInfo(scope, data_type->identifier);
                if(variable_info != NULL && variable_info->predeclared)
                    typeSystemResolvePredeclaredVariableType(variable_info, scope);
                if(variable_info != NULL && variable_info->type_value != NULL)
                    return cloneDataType(variable_info->type_value);
                diagnosticAbortFormatted("T1202",
                                         data_type->filename != NULL
                                             ? astDataTypeSourceSpan(data_type)
                                             : (scope != NULL && scope->instantiation_site != NULL
                                                    ? astNodeSourceSpan(scope->instantiation_site)
                                                    : makeSourceSpan(NULL, 0, 0, 0, 0)),
                                         NULL,
                                         "unknown data type `%s`",
                                         astUserFacingIdentifier(data_type->identifier));
            }
            return cloneDataType(type_info->data_type);
        }
        case AST_DATA_TYPE_KIND_ENUM: {
            if(data_type->identifier[0] != '\0')
            {
                TypeInfo *type_info = findTypeInfo(scope, data_type->identifier);
                if(type_info != NULL &&
                   type_info->data_type != NULL &&
                   type_info->data_type->kind == AST_DATA_TYPE_KIND_ENUM)
                {
                    if(type_info->data_type == data_type)
                        return cloneDataType(data_type);
                    return resolveNamedDataTypeInternal(type_info->data_type, scope, self_data_type, memo,
                                                        allow_recursive_factory_result);
                }
            }
            return cloneDataType(data_type);
        }
        case AST_DATA_TYPE_KIND_POINTER:
        case AST_DATA_TYPE_KIND_REFERENCE:
            return newWrappedDataType(data_type->kind,
                                      resolveNamedDataTypeInternal(data_type->child, scope, self_data_type, memo, true));
        case AST_DATA_TYPE_KIND_OPTIONAL:
            return newWrappedDataType(data_type->kind,
                                      resolveNamedDataTypeInternal(data_type->child, scope, self_data_type, memo, false));
        case AST_DATA_TYPE_KIND_ARRAY:
            return newArrayDataType(resolveNamedDataTypeInternal(data_type->child, scope, self_data_type, memo, false),
                                    data_type->array_length);
        case AST_DATA_TYPE_KIND_SLICE:
            return newSliceDataType(resolveNamedDataTypeInternal(data_type->child, scope, self_data_type, memo, true));
        case AST_DATA_TYPE_KIND_STRING:
            return newStringDataType();
        case AST_DATA_TYPE_KIND_FUNCTION: {
            ScopeFrame *signature_scope = newScopeFrame(scope);
            for(ASTFunctionParameter *scan = data_type->parameters; scan != NULL; scan = scan->next)
            {
                ASTDataType *placeholder_type = NULL;
                if(scan->data_type != NULL &&
                   scan->data_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
                   scan->data_type->primary == AST_PRIMARY_DATA_TYPE_TYPE)
                    placeholder_type = cloneDataType(scan->data_type);
                else
                    placeholder_type = newInferDataType();

                VariableInfo *variable_info = declareVariableInfo(signature_scope, scan->identifier);
                variable_info->is_compile_time_constant = false;
                variable_info->data_type = placeholder_type;
                if(placeholder_type != NULL &&
                   placeholder_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
                   placeholder_type->primary == AST_PRIMARY_DATA_TYPE_TYPE)
                    variable_info->type_value = newNamedDataType(scan->identifier);
            }

            ASTFunctionParameter *head = NULL;
            ASTFunctionParameter *tail = NULL;
            ASTFunctionParameter *parameter = data_type->parameters;
            while(parameter)
            {
                ASTFunctionParameter *new_parameter = (ASTFunctionParameter*) malloc(sizeof(ASTFunctionParameter));
                *new_parameter = *parameter;
                new_parameter->next = NULL;
                if(parameter->data_type != NULL &&
                   parameter->data_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
                   parameter->data_type->primary == AST_PRIMARY_DATA_TYPE_TYPE)
                    new_parameter->data_type = cloneDataType(parameter->data_type);
                else
                    new_parameter->data_type = resolveNamedDataTypeInternal(parameter->data_type, signature_scope, self_data_type, memo, true);

                VariableInfo *variable_info = findVariableInfo(signature_scope, new_parameter->identifier);
                if(variable_info != NULL)
                    variable_info->data_type = cloneDataType(new_parameter->data_type);

                if(head == NULL)
                    head = new_parameter;
                else
                    tail->next = new_parameter;
                tail = new_parameter;
                parameter = parameter->next;
            }

            ASTDataType *resolved_return_type = resolveNamedDataTypeInternal(data_type->return_data_type, signature_scope,
                                                                             self_data_type, memo, true);
            deleteScopeFrame(signature_scope);
            return newFunctionDataType(head, data_type->is_variadic, resolved_return_type);
        }
        case AST_DATA_TYPE_KIND_APPLY: {
            if(data_type->callee != NULL && data_type->callee->kind == AST_DATA_TYPE_KIND_NAMED)
            {
                VariableInfo *callee_variable = findVariableInfo(scope, data_type->callee->identifier);
                if(callee_variable != NULL && callee_variable->function_value != NULL)
                {
                    ScopeFrame *active_instantiation = findInstantiatingFunctionScope(scope, callee_variable->function_value);
                    if(active_instantiation != NULL)
                    {
                        if(active_instantiation->instantiating_type_result != NULL &&
                           allow_recursive_factory_result)
                            return active_instantiation->instantiating_type_result;

                        diagnosticAbortSimple("T1240",
                                              "recursive generic instantiation is not supported",
                                              active_instantiation->instantiation_site != NULL
                                                  ? astNodeSourceSpan(active_instantiation->instantiation_site)
                                                  : makeSourceSpan(NULL, 0, 0, 0, 0),
                                              "this recursive type use requires an explicit indirection such as `*T`, `&T`, `Function(...)`, or `[]T`");
                    }

                    TypeSystemExprType applied_type = instantiateFunctionCallExprType(
                        callee_variable->function_value,
                        buildTypeLiteralArgumentExprs(data_type->arguments, scope, self_data_type),
                        scope != NULL ? scope->instantiation_site : NULL,
                        scope
                        );
                    if(applied_type.kind != TYPE_SYSTEM_EXPR_TYPE_TYPE)
                        diagnosticAbortSimple("T1203",
                                              "type application requires a constructor returning `Type`",
                                              scope != NULL && scope->instantiation_site != NULL
                                                  ? astNodeSourceSpan(scope->instantiation_site)
                                                  : makeSourceSpan(NULL, 0, 0, 0, 0),
                                              NULL);
                    return cloneDataType(applied_type.data_type);
                }
            }

            ASTDataType *resolved_callee = resolveNamedDataTypeInternal(data_type->callee, scope, self_data_type, memo, false);
            ASTTypeArgument *resolved_arguments = NULL;
            ASTTypeArgument *resolved_tail = NULL;
            ASTTypeArgument *argument = data_type->arguments;
            while(argument)
            {
                ASTTypeArgument *new_argument = newASTTypeArgument(resolveNamedDataTypeInternal(argument->data_type, scope, self_data_type, memo, false));
                if(resolved_arguments == NULL)
                    resolved_arguments = new_argument;
                else
                    resolved_tail->next = new_argument;
                resolved_tail = new_argument;
                argument = argument->next;
            }
            return newAppliedDataType(resolved_callee, resolved_arguments);
        }
        default:
            diagnosticAbortSimple("ICE0201",
                                  "resolveNamedDataType hit unsupported AST data type kind",
                                  scope != NULL && scope->instantiation_site != NULL
                                      ? astNodeSourceSpan(scope->instantiation_site)
                                      : makeSourceSpan(NULL, 0, 0, 0, 0),
                                  NULL);
    }
}

ASTDataType* resolveNamedDataType(ASTDataType *data_type, ScopeFrame *scope, ASTDataType *self_data_type)
{
    ResolveDataTypeEntry *memo = NULL;
    return resolveNamedDataTypeInternal(data_type, scope, self_data_type, &memo, false);
}

static void bindMethodSpecializationScope(ScopeFrame *scope, ASTDataType *struct_type, ASTStructMember *member)
{
    if(scope == NULL || struct_type == NULL || member == NULL)
        return;

    VariableInfo *self_variable = declareVariableInfo(scope, "Self");
    self_variable->is_compile_time_constant = true;
    self_variable->data_type = newPrimaryDataType(AST_PRIMARY_DATA_TYPE_TYPE);
    self_variable->type_value = cloneDataType(struct_type);
    if(member->value != NULL && member->value->data_type != NULL && member->data_type != NULL)
        bindSpecializedNamedTypesInScope(scope, member->value->data_type, member->data_type);
}

static ScopeFrame* buildMethodLexicalTypeScope(ASTStructMember *member, ASTDataType *resolved_member_type,
                                               ScopeFrame *inst_scope, ASTDataType *struct_type)
{
    if(member == NULL || member->value == NULL || member->value->kind != AST_EXPR_FUNCTION ||
       resolved_member_type == NULL)
        return NULL;

    ScopeFrame *method_scope = newScopeFrame(NULL);
    if(struct_type != NULL)
    {
        VariableInfo *self_variable = declareVariableInfo(method_scope, "Self");
        self_variable->is_compile_time_constant = true;
        self_variable->data_type = newPrimaryDataType(AST_PRIMARY_DATA_TYPE_TYPE);
        self_variable->type_value = cloneDataType(struct_type);
    }

    bindSpecializedNamedTypesInScope(method_scope, member->value->data_type, resolved_member_type);

    if(inst_scope != NULL)
    {
        for(int i = 0; i < inst_scope->variable_count; i++)
        {
            VariableInfo *src = &(inst_scope->variable_infos[i]);
            if(findVariableInfoInScope(method_scope, src->identifier) >= 0)
                continue;
            VariableInfo *dst = declareVariableInfo(method_scope, src->identifier);
            *dst = *src;
            dst->data_type = cloneDataType(src->data_type);
            dst->type_value = cloneDataType(src->type_value);
        }

        for(int i = 0; i < inst_scope->type_count; i++)
        {
            TypeInfo *src = &(inst_scope->type_infos[i]);
            if(findTypeInfoInScope(method_scope, src->identifier) >= 0)
                continue;
            TypeInfo *dst = declareTypeInfo(method_scope, src->identifier);
            *dst = *src;
            dst->data_type = cloneDataType(src->data_type);
        }
    }

    return method_scope;
}

static ASTDataType* resolveStructMemberDataType(ASTStructMember *member, ScopeFrame *scope, ASTDataType *struct_type)
{
    if(member == NULL)
        return NULL;

    if(member->value != NULL && member->value->kind == AST_EXPR_FUNCTION)
    {
        ScopeFrame *member_scope = scope;
        if(member->lexical_type_scope != NULL)
        {
            ScopeFrame *combined_scope = newScopeFrame(scope);
            combined_scope->instantiating_function = member->lexical_type_scope->instantiating_function;
            combined_scope->instantiation_site = member->lexical_type_scope->instantiation_site;
            combined_scope->instantiating_type_result = cloneDataType(member->lexical_type_scope->instantiating_type_result);

            for(int i = 0; i < member->lexical_type_scope->variable_count; i++)
            {
                VariableInfo *src = &(member->lexical_type_scope->variable_infos[i]);
                if(findVariableInfoInScope(combined_scope, src->identifier) >= 0)
                    continue;
                VariableInfo *dst = declareVariableInfo(combined_scope, src->identifier);
                *dst = *src;
                dst->data_type = cloneDataType(src->data_type);
                dst->type_value = cloneDataType(src->type_value);
            }

            for(int i = 0; i < member->lexical_type_scope->type_count; i++)
            {
                TypeInfo *src = &(member->lexical_type_scope->type_infos[i]);
                if(findTypeInfoInScope(combined_scope, src->identifier) >= 0)
                    continue;
                TypeInfo *dst = declareTypeInfo(combined_scope, src->identifier);
                *dst = *src;
                dst->data_type = cloneDataType(src->data_type);
            }
            member_scope = combined_scope;
        }
        ASTDataType *resolved = resolveFunctionExprDataType(member->value, member_scope, struct_type);
        if(strcmp(member->identifier, "get") == 0)
        {
            char buffer[256] = {0};
            appendASTDataTypeString(resolved->return_data_type, buffer, sizeof(buffer));
            fprintf(stderr, "DBG get return type: %s\n", buffer);
        }
        return resolved;
    }

    return resolveNamedDataType(member->data_type, scope, struct_type);
}

static void bindSpecializedNamedTypesInScope(ScopeFrame *scope, ASTDataType *source_type, ASTDataType *resolved_type)
{
    if(scope == NULL || source_type == NULL || resolved_type == NULL)
        return;

    if(source_type->kind == AST_DATA_TYPE_KIND_NAMED)
    {
        ASTDataType *builtin_type = builtinIdentifierToDataType(source_type->identifier);
        bool same_named = resolved_type->kind == AST_DATA_TYPE_KIND_NAMED &&
                          strcmp(source_type->identifier, resolved_type->identifier) == 0;
        if(builtin_type == NULL &&
           strcmp(source_type->identifier, "Self") != 0 &&
           !same_named &&
           findVariableInfo(scope, source_type->identifier) == NULL &&
           findTypeInfo(scope, source_type->identifier) == NULL)
        {
            VariableInfo *type_variable = declareVariableInfo(scope, source_type->identifier);
            type_variable->is_compile_time_constant = true;
            type_variable->data_type = newPrimaryDataType(AST_PRIMARY_DATA_TYPE_TYPE);
            type_variable->type_value = cloneDataType(resolved_type);
        }
        return;
    }

    if(source_type->kind != resolved_type->kind)
        return;

    switch(source_type->kind)
    {
        case AST_DATA_TYPE_KIND_POINTER:
        case AST_DATA_TYPE_KIND_REFERENCE:
        case AST_DATA_TYPE_KIND_ARRAY:
        case AST_DATA_TYPE_KIND_SLICE:
        case AST_DATA_TYPE_KIND_STRING:
        case AST_DATA_TYPE_KIND_OPTIONAL:
            bindSpecializedNamedTypesInScope(scope, source_type->child, resolved_type->child);
            return;
        case AST_DATA_TYPE_KIND_FUNCTION: {
            ASTFunctionParameter *source_parameter = source_type->parameters;
            ASTFunctionParameter *resolved_parameter = resolved_type->parameters;
            while(source_parameter != NULL && resolved_parameter != NULL)
            {
                bindSpecializedNamedTypesInScope(scope, source_parameter->data_type, resolved_parameter->data_type);
                source_parameter = source_parameter->next;
                resolved_parameter = resolved_parameter->next;
            }
            bindSpecializedNamedTypesInScope(scope, source_type->return_data_type, resolved_type->return_data_type);
            return;
        }
        case AST_DATA_TYPE_KIND_APPLY: {
            bindSpecializedNamedTypesInScope(scope, source_type->callee, resolved_type->callee);
            ASTTypeArgument *source_argument = source_type->arguments;
            ASTTypeArgument *resolved_argument = resolved_type->arguments;
            while(source_argument != NULL && resolved_argument != NULL)
            {
                bindSpecializedNamedTypesInScope(scope, source_argument->data_type, resolved_argument->data_type);
                source_argument = source_argument->next;
                resolved_argument = resolved_argument->next;
            }
            return;
        }
        default:
            return;
    }
}

void bindCapturedValuesForInstantiation(ASTFunctionCapture *capture, ScopeFrame *inst_scope, ScopeFrame *outer_scope)
{
    while(capture)
    {
        VariableInfo *outer_variable = findVariableInfo(outer_scope, capture->identifier);
        if(outer_variable == NULL)
            diagnosticAbortFormatted("T1204",
                                     makeSourceSpan(capture->filename,
                                                    capture->line_number, capture->column_number,
                                                    capture->end_line_number, capture->end_column_number),
                                     "unknown capture",
                                     "unknown function capture `%s`",
                                     astUserFacingIdentifier(capture->identifier));

        VariableInfo *inst_variable = declareVariableInfo(inst_scope, capture->identifier);
        inst_variable->is_compile_time_constant = false;
        inst_variable->data_type = cloneDataType(outer_variable->data_type);
        inst_variable->type_value = cloneDataType(outer_variable->type_value);
        inst_variable->function_value = outer_variable->function_value;
        capture = capture->next;
    }
}

void bindCallArgumentsForInstantiation(ASTFunctionParameter *parameter, ASTNode *argument, ScopeFrame *inst_scope, ScopeFrame *outer_scope)
{
    ASTFunctionParameter *expected_parameters = parameter;
    ASTNode *provided_arguments = argument;

    while(parameter && argument)
    {
        VariableInfo *inst_variable = declareVariableInfo(inst_scope, parameter->identifier);
        inst_variable->is_compile_time_constant = false;

        ASTDataType *resolved_parameter_type = resolveNamedDataType(parameter->data_type, inst_scope, NULL);
        inst_variable->data_type = cloneDataType(resolved_parameter_type);

        if(resolved_parameter_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
           resolved_parameter_type->primary == AST_PRIMARY_DATA_TYPE_TYPE)
        {
            TypeSystemExprType argument_type = inferExprType(argument, outer_scope);
            if(argument_type.kind != TYPE_SYSTEM_EXPR_TYPE_TYPE)
                typeSystemAbortNode("T1205", argument,
                                    "expected a `Type` argument",
                                    "this argument does not evaluate to a type");
            inst_variable->type_value = cloneDataType(argument_type.data_type);
        }
        else if(argument->kind == AST_EXPR_FUNCTION)
        {
            inst_variable->function_value = argument;
        }
        else if(argument->kind == AST_EXPR_VARIABLE)
        {
            VariableInfo *outer_variable = findVariableInfo(outer_scope, argument->identifier);
            if(outer_variable != NULL)
            {
                inst_variable->type_value = cloneDataType(outer_variable->type_value);
                inst_variable->function_value = outer_variable->function_value;
                inst_variable->extern_value = outer_variable->extern_value;
            }
        }

        if(resolved_parameter_type->kind == AST_DATA_TYPE_KIND_REFERENCE)
        {
            if(!canBindReferenceArgument(argument, outer_scope, resolved_parameter_type))
                typeSystemAbortExpectedDataTypeFoundExpr("T1220", argument,
                                                         "function reference argument type mismatch",
                                                         resolved_parameter_type,
                                                         inferExprType(argument, outer_scope));
        }
        else if(resolved_parameter_type->kind != AST_DATA_TYPE_KIND_PRIMARY ||
                resolved_parameter_type->primary != AST_PRIMARY_DATA_TYPE_TYPE)
        {
            if(!canImplicitConvertExprToType(argument, outer_scope, resolved_parameter_type))
                typeSystemAbortExpectedDataTypeFoundExpr("T1221", argument,
                                                         "function argument type mismatch",
                                                         resolved_parameter_type,
                                                         inferExprType(argument, outer_scope));
        }

        parameter = parameter->next;
        argument = argument->next;
    }

    if(parameter != NULL || argument != NULL)
    {
        Diagnostic diagnostic = diagnosticMake(DIAGNOSTIC_SEVERITY_ERROR,
                                               "T1206",
                                               astNodeSourceSpan(
                                                   argument != NULL
                                                       ? argument
                                                       : (inst_scope != NULL ? inst_scope->instantiation_site : provided_arguments)
                                               ),
                                               "function argument count mismatch");
        diagnosticSetPrimaryLabel(&diagnostic,
                                  "expected %d argument%s, found %d",
                                  typeSystemCountFunctionParameters(expected_parameters),
                                  typeSystemCountFunctionParameters(expected_parameters) == 1 ? "" : "s",
                                  typeSystemCountCallArguments(provided_arguments));
        diagnosticAbort(diagnostic);
    }
}

ASTNode* findReturnedExpr(ASTNode *function_expr)
{
    if(function_expr == NULL || function_expr->body == NULL)
        return NULL;

    ASTNode *stmt = function_expr->body->lhs;
    while(stmt)
    {
        if(stmt->kind == AST_STATEMENT_RETURN)
            return stmt->lhs;
        stmt = stmt->next;
    }
    return NULL;
}

ASTDataType* instantiateStructTypeExpr(ASTNode *expr, ScopeFrame *inst_scope)
{
    ASTDataType *struct_type = newStructDataType("", NULL);
    inst_scope->instantiating_type_result = struct_type;
    struct_type->members = resolveStructMembers(expr->members, inst_scope, struct_type);

    ASTStructMember *resolved_member = struct_type->members;
    ASTStructMember *original_member = expr->members;
    while(resolved_member != NULL && original_member != NULL)
    {
        if(original_member->value != NULL && original_member->value->kind == AST_EXPR_FUNCTION)
        {
            resolved_member->data_type = resolveFunctionExprDataType(original_member->value, inst_scope, struct_type);
            resolved_member->lexical_type_scope = buildMethodLexicalTypeScope(
                original_member,
                resolved_member->data_type,
                inst_scope,
                struct_type
            );
        }
        else if(original_member->data_type != NULL)
            resolved_member->data_type = resolveNamedDataType(original_member->data_type, inst_scope, struct_type);
        resolved_member = resolved_member->next;
        original_member = original_member->next;
    }

    return struct_type;
}

ASTDataType* instantiateTypeExprValue(ASTNode *expr, ScopeFrame *inst_scope)
{
    if(expr == NULL)
        diagnosticAbortSimple("T1207",
                              "expected a type-valued return expression",
                              inst_scope != NULL && inst_scope->instantiation_site != NULL
                                  ? astNodeSourceSpan(inst_scope->instantiation_site)
                                  : makeSourceSpan(NULL, 0, 0, 0, 0),
                              NULL);

    if(expr->kind == AST_EXPR_STRUCT)
        return instantiateStructTypeExpr(expr, inst_scope);
    if(expr->kind == AST_EXPR_ENUM)
        return newEnumDataType("", cloneEnumVariants(expr->variants));
    if(expr->kind == AST_EXPR_TYPE_LITERAL)
        return resolveNamedDataType(expr->data_type, inst_scope, NULL);
    if(expr->kind == AST_EXPR_VARIABLE)
    {
        ASTDataType *builtin_type = builtinIdentifierToDataType(expr->identifier);
        if(builtin_type != NULL)
            return builtin_type;

        VariableInfo *variable_info = findVariableInfo(inst_scope, expr->identifier);
        if(variable_info != NULL && variable_info->type_value != NULL)
            return cloneDataType(variable_info->type_value);

        TypeInfo *type_info = findTypeInfo(inst_scope, expr->identifier);
        if(type_info != NULL)
            return cloneDataType(type_info->data_type);
    }

    if(expr->kind == AST_EXPR_CALL && expr->lhs->kind == AST_EXPR_VARIABLE)
    {
        VariableInfo *callee = findVariableInfo(inst_scope, expr->lhs->identifier);
        if(callee != NULL && callee->function_value != NULL)
        {
            ScopeFrame *active_instantiation = findInstantiatingFunctionScope(inst_scope, callee->function_value);
            if(active_instantiation != NULL)
            {
                if(active_instantiation->instantiating_type_result != NULL)
                    return active_instantiation->instantiating_type_result;

                diagnosticAbortSimple("T1240",
                                      "recursive generic instantiation is not supported",
                                      active_instantiation->instantiation_site != NULL
                                          ? astNodeSourceSpan(active_instantiation->instantiation_site)
                                          : astNodeSourceSpan(expr),
                                      "this recursive type use requires an explicit indirection such as `*T`, `&T`, `Function(...)`, or `[]T`");
            }

            ScopeFrame *nested_scope = newScopeFrame(inst_scope);
            nested_scope->instantiating_function = callee->function_value;
            nested_scope->instantiation_site = expr;
            bindCapturedValuesForInstantiation(callee->function_value->captures, nested_scope, inst_scope);
            bindCallArgumentsForInstantiation(callee->function_value->parameters, expr->rhs, nested_scope, inst_scope);
            ASTDataType *result = instantiateTypeExprValue(findReturnedExpr(callee->function_value), nested_scope);
            deleteScopeFrame(nested_scope);
            return result;
        }
    }

    if(expr->kind == AST_EXPR_CALL && expr->lhs->kind == AST_EXPR_MEMBER)
    {
        ASTNode *member_expr = expr->lhs;
        TypeSystemExprType owner_type = inferExprType(member_expr->lhs, inst_scope);
        ASTDataType *struct_type = NULL;
        if(owner_type.kind == TYPE_SYSTEM_EXPR_TYPE_TYPE)
            struct_type = owner_type.data_type;
        else if(owner_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE)
        {
            struct_type = owner_type.data_type;
            if(struct_type != NULL &&
               (struct_type->kind == AST_DATA_TYPE_KIND_POINTER || struct_type->kind == AST_DATA_TYPE_KIND_REFERENCE))
                struct_type = struct_type->child;
        }
        struct_type = resolveNamedDataType(struct_type, inst_scope, NULL);

        if(isStructDataType(struct_type))
        {
            ASTStructMember *member = findStructMember(struct_type, member_expr->identifier);
            if(member != NULL && member->value != NULL && member->value->kind == AST_EXPR_FUNCTION)
            {
                ScopeFrame *active_instantiation = findInstantiatingFunctionScope(inst_scope, member->value);
                if(active_instantiation != NULL)
                {
                    if(active_instantiation->instantiating_type_result != NULL)
                        return active_instantiation->instantiating_type_result;

                    diagnosticAbortSimple("T1240",
                                          "recursive generic instantiation is not supported",
                                          active_instantiation->instantiation_site != NULL
                                              ? astNodeSourceSpan(active_instantiation->instantiation_site)
                                              : astNodeSourceSpan(expr),
                                          "this recursive type use requires an explicit indirection such as `*T`, `&T`, `Function(...)`, or `[]T`");
                }

                ScopeFrame *nested_scope = newScopeFrame(inst_scope);
                nested_scope->instantiating_function = member->value;
                nested_scope->instantiation_site = expr;
                bindMethodSpecializationScope(nested_scope, struct_type, member);
                bindCapturedValuesForInstantiation(member->value->captures, nested_scope, inst_scope);
                bindCallArgumentsForInstantiation(member->value->parameters, expr->rhs, nested_scope, inst_scope);
                ASTDataType *result = instantiateTypeExprValue(findReturnedExpr(member->value), nested_scope);
                deleteScopeFrame(nested_scope);
                return result;
            }
        }
    }

    typeSystemAbortNode("T1208", expr,
                        "unsupported type-valued expression",
                        "this expression cannot be evaluated as a type");
}

ASTNode* buildTypeLiteralArgumentExprs(ASTTypeArgument *argument, ScopeFrame *scope, ASTDataType *self_data_type)
{
    ASTNode *head = NULL;
    ASTNode *tail = NULL;

    while(argument)
    {
        ASTNode *type_literal = newASTNode(AST_EXPR_TYPE_LITERAL);
        type_literal->data_type = resolveNamedDataType(argument->data_type, scope, self_data_type);

        if(head == NULL)
            head = type_literal;
        else
            tail->next = type_literal;
        tail = type_literal;
        argument = argument->next;
    }

    return head;
}

ASTNode* resolveFunctionValueExpr(ASTNode *expr, ScopeFrame *scope)
{
    if(expr == NULL)
        return NULL;

    if(expr->kind == AST_EXPR_FUNCTION)
        return expr;

    if(expr->kind == AST_EXPR_VARIABLE)
    {
        VariableInfo *variable_info = findVariableInfo(scope, expr->identifier);
        if(variable_info != NULL)
            return variable_info->function_value;
        return NULL;
    }

    if(expr->kind == AST_EXPR_MEMBER)
    {
        TypeSystemExprType owner_type = inferExprType(expr->lhs, scope);
        ASTDataType *struct_type = NULL;
        if(owner_type.kind == TYPE_SYSTEM_EXPR_TYPE_TYPE)
            struct_type = inferDeclaredTypeFromExpr(expr->lhs, scope);
        else if(owner_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE)
        {
            struct_type = owner_type.data_type;
            if(struct_type != NULL &&
               (struct_type->kind == AST_DATA_TYPE_KIND_POINTER || struct_type->kind == AST_DATA_TYPE_KIND_REFERENCE))
                struct_type = struct_type->child;
        }
        struct_type = resolveNamedDataType(struct_type, scope, NULL);
        if(!isStructDataType(struct_type))
            return NULL;

        ASTStructMember *member = findStructMember(struct_type, expr->identifier);
        if(member != NULL && member->value != NULL && member->value->kind == AST_EXPR_FUNCTION)
            return member->value;
    }

    return NULL;
}

ASTNode* resolveExternValueExpr(ASTNode *expr, ScopeFrame *scope)
{
    if(expr == NULL)
        return NULL;

    if(expr->kind == AST_EXPR_BUILTIN && strcmp(expr->identifier, "extern") == 0)
        return expr;

    if(expr->kind == AST_EXPR_VARIABLE)
    {
        VariableInfo *variable_info = findVariableInfo(scope, expr->identifier);
        if(variable_info != NULL)
            return variable_info->extern_value;
    }

    return NULL;
}

TypeSystemExprType instantiateFunctionCallExprType(ASTNode *function_value, ASTNode *call_arguments, ASTNode *call_site, ScopeFrame *outer_scope)
{
    ScopeFrame *inst_scope = newScopeFrame(outer_scope);
    inst_scope->instantiating_function = function_value;
    inst_scope->instantiation_site = call_site != NULL ? call_site : (call_arguments != NULL ? call_arguments : function_value);
    bindCapturedValuesForInstantiation(function_value->captures, inst_scope, outer_scope);
    bindCallArgumentsForInstantiation(function_value->parameters, call_arguments, inst_scope, outer_scope);

    ASTDataType *resolved_return_type = resolveNamedDataType(function_value->return_data_type, inst_scope, NULL);
    if(resolved_return_type != NULL &&
       resolved_return_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
       resolved_return_type->primary == AST_PRIMARY_DATA_TYPE_TYPE)
    {
        TypeSystemExprType result = newTypeExprType(instantiateTypeExprValue(findReturnedExpr(function_value), inst_scope));
        deleteScopeFrame(inst_scope);
        return result;
    }

    TypeSystemExprType result = newValueExprType(resolved_return_type);
    deleteScopeFrame(inst_scope);
    return result;
}

ASTDataType* instantiateFunctionCallResolvedFunctionType(ASTNode *function_value, ASTNode *call_arguments, ASTNode *call_site, ScopeFrame *outer_scope)
{
    ScopeFrame *inst_scope = newScopeFrame(outer_scope);
    inst_scope->instantiating_function = function_value;
    inst_scope->instantiation_site = call_site != NULL ? call_site : (call_arguments != NULL ? call_arguments : function_value);
    bindCapturedValuesForInstantiation(function_value->captures, inst_scope, outer_scope);
    bindCallArgumentsForInstantiation(function_value->parameters, call_arguments, inst_scope, outer_scope);

    ASTFunctionParameter *resolved_head = NULL;
    ASTFunctionParameter *resolved_tail = NULL;
    for(ASTFunctionParameter *parameter = function_value->parameters; parameter != NULL; parameter = parameter->next)
    {
        ASTFunctionParameter *resolved_parameter = (ASTFunctionParameter*) malloc(sizeof(ASTFunctionParameter));
        memset(resolved_parameter, 0, sizeof(ASTFunctionParameter));
        resolved_parameter->filename = parameter->filename;
        resolved_parameter->line_number = parameter->line_number;
        resolved_parameter->column_number = parameter->column_number;
        resolved_parameter->end_line_number = parameter->end_line_number;
        resolved_parameter->end_column_number = parameter->end_column_number;
        strcpy(resolved_parameter->identifier, parameter->identifier);
        resolved_parameter->data_type = resolveNamedDataType(parameter->data_type, inst_scope, NULL);
        if(resolved_head == NULL)
            resolved_head = resolved_parameter;
        else
            resolved_tail->next = resolved_parameter;
        resolved_tail = resolved_parameter;
    }

    ASTDataType *resolved_return_type = resolveNamedDataType(function_value->return_data_type, inst_scope, NULL);
    ASTDataType *result = newFunctionDataType(resolved_head, function_value->is_variadic, resolved_return_type);
    deleteScopeFrame(inst_scope);
    return result;
}


#endif /* TYPE_SYSTEM_RESOLVE_H */
