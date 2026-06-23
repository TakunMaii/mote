#ifndef TYPE_SYSTEM_INFER_H
#define TYPE_SYSTEM_INFER_H

#include "TypeSystemConvert.h"

TypeSystemExprType inferExprType(ASTNode *node, ScopeFrame *scope)
{
    switch(node->kind)
    {
        case AST_EXPR_LITERAL_BOOL:
            return newValueExprType(newPrimaryDataType(AST_PRIMARY_DATA_TYPE_BOOL));
        case AST_EXPR_LITERAL_NULL:
            return newNullExprType();
        case AST_EXPR_LITERAL_CHAR:
            return newValueExprType(newPrimaryDataType(AST_PRIMARY_DATA_TYPE_CHAR));
        case AST_EXPR_LITERAL_STRING:
            return newValueExprType(newStringDataType());
        case AST_EXPR_LITERAL_INTEGER:
            return newLiteralIntegerExprType();
        case AST_EXPR_LITERAL_FLOAT:
            return newLiteralFloatExprType();
        case AST_EXPR_TYPE_LITERAL:
            return newTypeExprType(node->data_type);
        case AST_EXPR_BUILTIN:
            if(strcmp(node->identifier, "extern") == 0)
                return newValueExprType(inferExternBuiltinFunctionType(node, scope));
            if(strcmp(node->identifier, "zero") == 0)
                return newValueExprType(inferZeroBuiltinValueType(node, scope));
            if(strcmp(node->identifier, "len") == 0)
                return newValueExprType(inferLenBuiltinValueType(node, scope));
            if(strcmp(node->identifier, "ptr_add") == 0)
                return newValueExprType(inferPtrAddBuiltinValueType(node, scope));
            if(strcmp(node->identifier, "ptr_diff") == 0)
                return newValueExprType(inferPtrDiffBuiltinValueType(node, scope));
            if(strcmp(node->identifier, "sizeof") == 0)
                return newValueExprType(inferSizeofBuiltinValueType(node, scope));
            if(strcmp(node->identifier, "alignof") == 0)
                return newValueExprType(inferAlignofBuiltinValueType(node, scope));
            if(strcmp(node->identifier, "debug") == 0)
                return newValueExprType(inferDebugBuiltinValueType(node, scope));
            if(strcmp(node->identifier, "panic") == 0)
                return newValueExprType(inferPanicBuiltinValueType(node, scope));
            if(strcmp(node->identifier, "assert") == 0)
                return newValueExprType(inferAssertBuiltinValueType(node, scope));
            if(strcmp(node->identifier, "as") == 0)
                return newValueExprType(inferAsBuiltinValueType(node, scope));
            if(strcmp(node->identifier, "slice") == 0)
                return newValueExprType(inferSliceBuiltinValueType(node, scope));
            if(strcmp(node->identifier, "unwrap") == 0)
                return newValueExprType(inferUnwrapBuiltinValueType(node, scope));
            typeSystemAbortFormatted("T1227", node,
                                     "unknown builtin",
                                     "unknown builtin `@%s`",
                                     node->identifier);
        case AST_EXPR_VARIABLE: {
            if(strcmp(node->identifier, "Self") == 0)
                return newTypeExprType(newNamedDataType("Self"));

            ASTDataType *builtin_type = builtinIdentifierToDataType(node->identifier);
            if(builtin_type != NULL)
                return newTypeExprType(builtin_type);

            VariableInfo *variable_info = findVariableInfo(scope, node->identifier);
            if(variable_info != NULL)
            {
                if(variable_info->predeclared)
                    typeSystemResolvePredeclaredVariableType(variable_info, scope);
                if(variable_info->type_value != NULL)
                    return newTypeExprType(variable_info->type_value);

                ASTDataType *variable_data_type = variable_info->data_type;
                if(variable_data_type->kind == AST_DATA_TYPE_KIND_REFERENCE)
                    return newValueExprType(variable_data_type->child);
                return newValueExprType(variable_data_type);
            }

            TypeInfo *type_info = findTypeInfo(scope, node->identifier);
            if(type_info != NULL)
                return newTypeExprType(type_info->data_type);

            typeSystemAbortFormatted("T1228", node,
                                     "undeclared variable",
                                     "type inference found undeclared variable `%s`",
                                     astUserFacingIdentifier(node->identifier));
        }
        case AST_EXPR_FUNCTION:
            return newValueExprType(node->data_type);
        case AST_EXPR_ENUM:
            return newTypeExprType(node->data_type);
        case AST_EXPR_STRUCT:
            return newTypeExprType(node->data_type);
        case AST_EXPR_ARRAY_LITERAL: {
            ASTNode *element = node->lhs;
            if(element == NULL)
                return newValueExprType(newArrayDataType(newInferDataType(), 0));

            ASTDataType *element_type = inferDeclaredTypeFromExpr(element, scope);
            long long int length = 0;
            while(element)
            {
                TypeSystemExprType current_type = inferExprType(element, scope);
                if(!canImplicitConvertDataType(current_type, element, element_type))
                    typeSystemAbortExpectedDataTypeFoundExpr("T1230", element,
                                                             "array literal element type mismatch",
                                                             element_type,
                                                             current_type);
                length ++;
                element = element->next;
            }

            return newValueExprType(newArrayDataType(element_type, length));
        }
        case AST_EXPR_STRUCT_LITERAL: {
            TypeSystemExprType type_expr = inferExprType(node->lhs, scope);
            if(type_expr.kind != TYPE_SYSTEM_EXPR_TYPE_TYPE || !isStructDataType(type_expr.data_type))
                typeSystemAbortNode("T1231", node,
                                    "struct literal requires a struct type",
                                    "the expression before `{ ... }` is not a struct type");

            ASTStructLiteralField *literal_field = node->struct_literal_fields;
            bool literal_fields_have_explicit_names = literal_field != NULL && literal_field->has_name;
            ASTStructMember *member = type_expr.data_type->members;
            while(member)
            {
                if(member->value == NULL)
                {
                    ASTStructLiteralField *field = literal_field;
                    if(literal_fields_have_explicit_names)
                    {
                        while(field && strcmp(field->identifier, member->identifier) != 0)
                            field = field->next;
                    }
                    else if(field != NULL)
                    {
                        literal_field = literal_field->next;
                    }

                    if(field == NULL || field->has_name != literal_fields_have_explicit_names)
                        typeSystemAbortFormatted("T1232", node,
                                                 "missing struct field",
                                                 "missing field `%s` in struct literal",
                                                 astUserFacingIdentifier(member->identifier));

                    if(!canImplicitConvertExprToType(field->value, scope, member->data_type))
                    {
                        char expected_buffer[256] = {0};
                        char actual_buffer[256] = {0};
                        appendASTDataTypeString(member->data_type, expected_buffer, sizeof(expected_buffer));
                        typeSystemDescribeExprType(inferExprType(field->value, scope), actual_buffer, sizeof(actual_buffer));
                        typeSystemAbortFormatted("T1233", field->value,
                                                 "struct field type mismatch",
                                                 "struct field `%s` expects %s, found %s",
                                                 astUserFacingIdentifier(member->identifier),
                                                 expected_buffer,
                                                 actual_buffer);
                    }
                }
                member = member->next;
            }

            ASTStructLiteralField *field = node->struct_literal_fields;
            while(field)
            {
                if(field->has_name != literal_fields_have_explicit_names)
                    typeSystemAbortNode("T1260", node,
                                        "struct literal fields must either all have explicit names or no explicit names",
                                        "do not mix `.field = value` with positional fields in the same struct literal");
                if(!literal_fields_have_explicit_names)
                {
                    field = field->next;
                    continue;
                }
                ASTStructMember *declared_member = findStructMember(type_expr.data_type, field->identifier);
                if(declared_member == NULL || declared_member->value != NULL)
                    typeSystemAbortFormatted("T1234", field->value != NULL ? field->value : node,
                                             "unknown struct field",
                                             "unknown struct field `%s`",
                                             astUserFacingIdentifier(field->identifier));
                field = field->next;
            }

            return newValueExprType(type_expr.data_type);
        }
        case AST_EXPR_MEMBER: {
            TypeSystemExprType owner_type = inferExprType(node->lhs, scope);
            ASTDataType *owner_data_type = NULL;

            if(owner_type.kind == TYPE_SYSTEM_EXPR_TYPE_TYPE)
                owner_data_type = inferDeclaredTypeFromExpr(node->lhs, scope);
            else if(owner_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE)
            {
                owner_data_type = owner_type.data_type;
                if(owner_data_type->kind == AST_DATA_TYPE_KIND_POINTER || owner_data_type->kind == AST_DATA_TYPE_KIND_REFERENCE)
                    owner_data_type = owner_data_type->child;
            }

            if(isEnumDataType(owner_data_type))
            {
                if(owner_type.kind != TYPE_SYSTEM_EXPR_TYPE_TYPE)
                    typeSystemAbortNode("T1235", node,
                                        "enum variant access requires the enum type",
                                        "use `EnumType.Variant` syntax");

                if(findEnumVariant(owner_data_type, node->identifier) == NULL)
                    typeSystemAbortFormatted("T1236", node,
                                             "unknown enum variant",
                                             "unknown enum variant `%s`",
                                             astUserFacingIdentifier(node->identifier));

                return newValueExprType(owner_data_type);
            }

            if(isSliceDataType(owner_data_type) || isStringDataType(owner_data_type))
                typeSystemAbortFormatted("T1238", node,
                                         "slice and string members are not exposed",
                                         "use `@len(value)` or explicit conversion helpers instead of `.%s`",
                                         astUserFacingIdentifier(node->identifier));

            ASTDataType *struct_type = owner_data_type;
            if(!isStructDataType(struct_type) || struct_type->members == NULL)
                struct_type = resolveNamedDataType(owner_data_type, scope, NULL);
            if(!isStructDataType(struct_type))
            {
                typeSystemAbortExpectedDescriptionFoundExpr("T1237", node->lhs,
                                                            "member access requires a struct type",
                                                            "a struct value or struct type",
                                                            owner_type);
            }

            ASTStructMember *member = findStructMember(struct_type, node->identifier);
            if(member == NULL)
            {
                typeSystemAbortFormatted("T1238", node,
                                         "unknown struct member",
                                         "unknown struct member `%s`",
                                         astUserFacingIdentifier(node->identifier));
            }

            if(owner_type.kind == TYPE_SYSTEM_EXPR_TYPE_TYPE && member->value == NULL)
                typeSystemAbortFormatted("T1239", node,
                                         "instance field accessed on type",
                                         "struct field `%s` cannot be accessed on the type itself",
                                         astUserFacingIdentifier(node->identifier));

            ASTDataType *resolved_member_type = member->value != NULL
                ? resolveStructMemberDataType(member, scope, struct_type)
                : member->data_type;
            if(resolved_member_type->kind == AST_DATA_TYPE_KIND_REFERENCE)
                return newValueExprType(resolved_member_type->child);
            return newValueExprType(resolved_member_type);
        }
        case AST_EXPR_INDEX: {
            TypeSystemExprType owner_type = inferExprType(node->lhs, scope);
            if(owner_type.kind != TYPE_SYSTEM_EXPR_TYPE_VALUE)
                typeSystemAbortNode("T1240", node,
                                    "indexing requires a value receiver",
                                    "the indexed expression is not a runtime value");

            ASTDataType *owner_data_type = owner_type.data_type;
            if(owner_data_type->kind == AST_DATA_TYPE_KIND_POINTER || owner_data_type->kind == AST_DATA_TYPE_KIND_REFERENCE)
                owner_data_type = owner_data_type->child;

            if(!isArrayDataType(owner_data_type) &&
               !isSliceDataType(owner_data_type) &&
               !isStringDataType(owner_data_type))
                typeSystemAbortExpectedDescriptionFoundExpr("T1241", node->lhs,
                                                            "indexing requires an array, slice, or string type",
                                                            "an array, slice, or string value",
                                                            owner_type);

            TypeSystemExprType index_type = inferExprType(node->rhs, scope);
            if(index_type.kind == TYPE_SYSTEM_EXPR_TYPE_TYPE ||
               (index_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE &&
                (index_type.data_type->kind != AST_DATA_TYPE_KIND_PRIMARY ||
                 !isIntegerPrimary(index_type.data_type->primary))))
                typeSystemAbortExpectedDescriptionFoundExpr("T1242", node->rhs,
                                                            "index must be an integer",
                                                            "an integer value",
                                                            index_type);

            return newValueExprType(owner_data_type->child);
        }
        case AST_EXPR_CALL: {
            if(node->lhs->kind == AST_EXPR_VARIABLE)
            {
                VariableInfo *callee_variable = findVariableInfo(scope, node->lhs->identifier);
                if(callee_variable != NULL && callee_variable->function_value != NULL)
                    return instantiateFunctionCallExprType(callee_variable->function_value, node->rhs, node, scope);
            }

            if(node->lhs->kind == AST_EXPR_MEMBER)
            {
                ASTNode *member_node = node->lhs;
                TypeSystemExprType owner_type = inferExprType(member_node->lhs, scope);
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
                struct_type = resolveNamedDataType(struct_type, scope, NULL);

                if(isStructDataType(struct_type))
                {
                    ASTStructMember *member = findStructMember(struct_type, member_node->identifier);
                    if(member != NULL && member->value != NULL && member->value->kind == AST_EXPR_FUNCTION)
                    {
                        ASTDataType *resolved_member_type = resolveStructMemberDataType(member, scope, struct_type);
                        return newValueExprType(resolved_member_type->return_data_type);
                    }
                }
            }

            if(node->lhs->kind == AST_EXPR_MEMBER)
            {
                ASTNode *member_node = node->lhs;
                TypeSystemExprType owner_type = inferExprType(member_node->lhs, scope);
                ASTDataType *struct_type = NULL;
                bool through_type = owner_type.kind == TYPE_SYSTEM_EXPR_TYPE_TYPE;
                if(through_type)
                    struct_type = inferDeclaredTypeFromExpr(member_node->lhs, scope);
                else if(owner_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE)
                {
                    struct_type = owner_type.data_type;
                    if(struct_type->kind == AST_DATA_TYPE_KIND_POINTER || struct_type->kind == AST_DATA_TYPE_KIND_REFERENCE)
                        struct_type = struct_type->child;
                }
                struct_type = resolveNamedDataType(struct_type, scope, NULL);

                if(isStructDataType(struct_type))
                {
                    ASTStructMember *member = findStructMember(struct_type, member_node->identifier);
                    ASTDataType *resolved_member_type = resolveStructMemberDataType(member, scope, struct_type);
                    if(member != NULL && resolved_member_type != NULL &&
                       resolved_member_type->kind == AST_DATA_TYPE_KIND_FUNCTION)
                    {
                        ASTFunctionParameter *parameter = resolved_member_type->parameters;
                        if(!through_type && parameter != NULL &&
                           canBindMethodReceiver(member_node->lhs, scope, parameter->data_type, struct_type))
                        {
                            checkFunctionCallArguments(parameter->next, node->rhs, scope, resolved_member_type->is_variadic, node);
                            return newValueExprType(resolved_member_type->return_data_type);
                        }
                    }
                }
            }

            TypeSystemExprType callee_type = inferExprType(node->lhs, scope);
            if(callee_type.kind != TYPE_SYSTEM_EXPR_TYPE_VALUE || callee_type.data_type->kind != AST_DATA_TYPE_KIND_FUNCTION)
                typeSystemAbortExpectedDescriptionFoundExpr("T1243", node->lhs,
                                                            "called expression is not a function",
                                                            "a function value",
                                                            callee_type);

            checkFunctionCallArguments(callee_type.data_type->parameters, node->rhs, scope, callee_type.data_type->is_variadic, node);

            return newValueExprType(callee_type.data_type->return_data_type);
        }
        case AST_EXPR_PARENTHESIS:
            return inferExprType(node->lhs, scope);
        case AST_EXPR_UNARY_PLUS:
        case AST_EXPR_UNARY_MINUS: {
            TypeSystemExprType operand_type = inferExprType(node->lhs, scope);
            if(operand_type.kind == TYPE_SYSTEM_EXPR_TYPE_LITERAL_INTEGER)
                return newLiteralIntegerExprType();
            if(operand_type.kind == TYPE_SYSTEM_EXPR_TYPE_LITERAL_FLOAT)
                return newLiteralFloatExprType();
            if(operand_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE &&
               operand_type.data_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
               (isIntegerPrimary(operand_type.data_type->primary) || isFloatPrimary(operand_type.data_type->primary)))
                return newValueExprType(operand_type.data_type);
            if(node->kind == AST_EXPR_UNARY_MINUS)
            {
                ResolvedOperatorOverload overload = {0};
                if(resolveOperatorOverload(AST_OPERATOR_SUB, node->lhs, NULL, scope, &overload))
                    return newValueExprType(overload.result_type);
            }
            typeErrorUnaryOperator(node, node->kind == AST_EXPR_UNARY_PLUS ? "+" : "-", operand_type);
        } break;
        case AST_EXPR_UNARY_LOGICAL_NOT: {
            TypeSystemExprType operand_type = inferExprType(node->lhs, scope);
            if(operand_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE &&
               operand_type.data_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
               isBoolPrimary(operand_type.data_type->primary))
                return newValueExprType(newPrimaryDataType(AST_PRIMARY_DATA_TYPE_BOOL));
            typeErrorUnaryOperator(node, "!", operand_type);
        } break;
        case AST_EXPR_UNARY_BIT_NOT: {
            TypeSystemExprType operand_type = inferExprType(node->lhs, scope);
            if(operand_type.kind == TYPE_SYSTEM_EXPR_TYPE_LITERAL_INTEGER)
                return newValueExprType(defaultIntegerDataType());
            if(operand_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE &&
               operand_type.data_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
               isIntegerPrimary(operand_type.data_type->primary))
            {
                if(isCharPrimary(operand_type.data_type->primary))
                    return newValueExprType(defaultIntegerDataType());
                return newValueExprType(operand_type.data_type);
            }
            typeErrorUnaryOperator(node, "~", operand_type);
        } break;
        case AST_EXPR_ADDRESS_OF:
        case AST_EXPR_ADDRESS_OF_MUT: {
            TypeSystemExprType operand_type = inferExprType(node->lhs, scope);
            if(operand_type.kind == TYPE_SYSTEM_EXPR_TYPE_TYPE)
            {
                return newTypeExprType(newWrappedDataType(
                    AST_DATA_TYPE_KIND_REFERENCE,
                    cloneDataType(operand_type.data_type)
                ));
            }

            if(!isAddressableExpr(node->lhs))
                typeSystemAbortNode("T1244", node,
                                    "cannot take address of non-addressable expression",
                                    "this expression has no stable address");

            if(operand_type.kind != TYPE_SYSTEM_EXPR_TYPE_VALUE)
                typeSystemAbortNode("T1246", node,
                                    "cannot take address of literal",
                                    "literals do not have an addressable storage location");

            if(node->lhs->kind == AST_EXPR_VARIABLE)
            {
                VariableInfo *variable_info = findVariableInfo(scope, node->lhs->identifier);
                if(variable_info != NULL && variable_info->is_compile_time_constant)
                    typeSystemAbortNode("T1245", node,
                                        "cannot take address of compile-time constant",
                                        "compile-time constants do not have runtime storage");
            }

            return newValueExprType(newWrappedDataType(
                AST_DATA_TYPE_KIND_POINTER,
                cloneDataType(getReferenceTargetType(operand_type.data_type))
            ));
        } break;
        case AST_EXPR_DEREF: {
            TypeSystemExprType operand_type = inferExprType(node->lhs, scope);
            if(operand_type.kind == TYPE_SYSTEM_EXPR_TYPE_TYPE)
            {
                return newTypeExprType(newWrappedDataType(
                    AST_DATA_TYPE_KIND_POINTER,
                    cloneDataType(operand_type.data_type)
                ));
            }

            if(operand_type.kind != TYPE_SYSTEM_EXPR_TYPE_VALUE ||
               (operand_type.data_type->kind != AST_DATA_TYPE_KIND_POINTER &&
                operand_type.data_type->kind != AST_DATA_TYPE_KIND_REFERENCE))
            {
                typeErrorUnaryOperator(node, "*", operand_type);
            }

            return newValueExprType(operand_type.data_type->child);
        } break;
        case AST_EXPR_MUL:
        case AST_EXPR_DIV:
        case AST_EXPR_ADD:
        case AST_EXPR_SUB: {
            TypeSystemExprType lhs_type = inferExprType(node->lhs, scope);
            TypeSystemExprType rhs_type = inferExprType(node->rhs, scope);
            const char *operator_name = "+";
            ASTOperatorKind operator_kind = AST_OPERATOR_ADD;
            if(node->kind == AST_EXPR_MUL) operator_name = "*";
            else if(node->kind == AST_EXPR_DIV) operator_name = "/";
            else if(node->kind == AST_EXPR_SUB) operator_name = "-";
            if(node->kind == AST_EXPR_MUL) operator_kind = AST_OPERATOR_MUL;
            else if(node->kind == AST_EXPR_DIV) operator_kind = AST_OPERATOR_DIV;
            else if(node->kind == AST_EXPR_SUB) operator_kind = AST_OPERATOR_SUB;

            if(!isBuiltinNumericOperandType(lhs_type) || !isBuiltinNumericOperandType(rhs_type))
            {
                ResolvedOperatorOverload overload = {0};
                if(resolveOperatorOverload(operator_kind, node->lhs, node->rhs, scope, &overload))
                    return newValueExprType(overload.result_type);
            }
            return getCommonNumericType(node, lhs_type, rhs_type, operator_name);
        }
        case AST_EXPR_MOD:
        case AST_EXPR_SHIFT_LEFT:
        case AST_EXPR_SHIFT_RIGHT:
        case AST_EXPR_BIT_AND:
        case AST_EXPR_BIT_OR:
        case AST_EXPR_BIT_XOR: {
            TypeSystemExprType common = getCommonNumericType(node,
                inferExprType(node->lhs, scope),
                inferExprType(node->rhs, scope),
                node->kind == AST_EXPR_MOD ? "%" :
                node->kind == AST_EXPR_SHIFT_LEFT ? "<<" :
                node->kind == AST_EXPR_SHIFT_RIGHT ? ">>" :
                node->kind == AST_EXPR_BIT_AND ? "&" :
                node->kind == AST_EXPR_BIT_OR ? "|" : "^");
            if(common.data_type->kind != AST_DATA_TYPE_KIND_PRIMARY || !isIntegerPrimary(common.data_type->primary))
                typeErrorBinaryOperator(node, "%", inferExprType(node->lhs, scope), inferExprType(node->rhs, scope));
            return common;
        }
        case AST_EXPR_LOGICAL_AND:
        case AST_EXPR_LOGICAL_OR: {
            TypeSystemExprType lhs_type = inferExprType(node->lhs, scope);
            TypeSystemExprType rhs_type = inferExprType(node->rhs, scope);
            if(lhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE && rhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE &&
               lhs_type.data_type->kind == AST_DATA_TYPE_KIND_PRIMARY && rhs_type.data_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
               isBoolPrimary(lhs_type.data_type->primary) && isBoolPrimary(rhs_type.data_type->primary))
                return newValueExprType(newPrimaryDataType(AST_PRIMARY_DATA_TYPE_BOOL));
            typeErrorBinaryOperator(node, node->kind == AST_EXPR_LOGICAL_AND ? "&&" : "||", lhs_type, rhs_type);
        } break;
        case AST_EXPR_EQUAL:
        case AST_EXPR_NOT_EQUAL: {
            TypeSystemExprType lhs_type = inferExprType(node->lhs, scope);
            TypeSystemExprType rhs_type = inferExprType(node->rhs, scope);
            if(lhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_NULL && rhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_NULL)
                return newValueExprType(newPrimaryDataType(AST_PRIMARY_DATA_TYPE_BOOL));
            if(lhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_NULL &&
               rhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE &&
               isOptionalDataType(rhs_type.data_type))
                return newValueExprType(newPrimaryDataType(AST_PRIMARY_DATA_TYPE_BOOL));
            if(rhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_NULL &&
               lhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE &&
               isOptionalDataType(lhs_type.data_type))
                return newValueExprType(newPrimaryDataType(AST_PRIMARY_DATA_TYPE_BOOL));
            if(isZeroComparablePointerOrFunction(lhs_type, rhs_type, node->rhs) ||
               isZeroComparablePointerOrFunction(rhs_type, lhs_type, node->lhs))
                return newValueExprType(newPrimaryDataType(AST_PRIMARY_DATA_TYPE_BOOL));
            if(lhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE && rhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE &&
               isSameDataType(lhs_type.data_type, rhs_type.data_type))
                return newValueExprType(newPrimaryDataType(AST_PRIMARY_DATA_TYPE_BOOL));

            if(node->kind == AST_EXPR_EQUAL || node->kind == AST_EXPR_NOT_EQUAL)
            {
                ResolvedOperatorOverload overload = {0};
                if(resolveOperatorOverload(AST_OPERATOR_EQ, node->lhs, node->rhs, scope, &overload))
                    return newValueExprType(newPrimaryDataType(AST_PRIMARY_DATA_TYPE_BOOL));
            }

            getCommonNumericType(node, lhs_type, rhs_type, node->kind == AST_EXPR_EQUAL ? "==" : "!=");
            return newValueExprType(newPrimaryDataType(AST_PRIMARY_DATA_TYPE_BOOL));
        }
        case AST_EXPR_LESS:
        case AST_EXPR_LESS_EQUAL:
        case AST_EXPR_GREATER:
        case AST_EXPR_GREATER_EQUAL: {
            getCommonNumericType(node,
                inferExprType(node->lhs, scope),
                inferExprType(node->rhs, scope),
                node->kind == AST_EXPR_LESS ? "<" :
                node->kind == AST_EXPR_LESS_EQUAL ? "<=" :
                node->kind == AST_EXPR_GREATER ? ">" : ">=");
            return newValueExprType(newPrimaryDataType(AST_PRIMARY_DATA_TYPE_BOOL));
        }
        default:
            typeSystemAbortFormatted("ICE0202", node,
                                     NULL,
                                     "inferExprType hit unsupported AST node kind %s",
                                     astNodeKindToString(node->kind));
    }
}

ASTDataType* inferDeclaredTypeFromExpr(ASTNode *expr, ScopeFrame *scope)
{
    TypeSystemExprType expr_type = inferExprType(expr, scope);
    if(expr_type.kind == TYPE_SYSTEM_EXPR_TYPE_NULL)
        typeSystemAbortNode("T1247", expr,
                            "cannot infer a type from `null` alone",
                            "add an explicit optional type like `?T`");
    if(expr_type.kind == TYPE_SYSTEM_EXPR_TYPE_LITERAL_INTEGER)
        return defaultIntegerDataType();
    if(expr_type.kind == TYPE_SYSTEM_EXPR_TYPE_LITERAL_FLOAT)
        return defaultFloatDataType();
    if(expr_type.kind == TYPE_SYSTEM_EXPR_TYPE_TYPE)
        return cloneDataType(expr_type.data_type);
    return cloneDataType(expr_type.data_type);
}

static ASTStructMember* resolveStructMembersInternal(ASTStructMember *member, ScopeFrame *scope,
                                                     ASTDataType *self_data_type,
                                                     ResolveDataTypeEntry **memo,
                                                     bool allow_recursive_factory_result)
{
    ASTStructMember *head = NULL;
    ASTStructMember *tail = NULL;

    while(member)
    {
        ASTStructMember *new_member = (ASTStructMember*) malloc(sizeof(ASTStructMember));
        memset(new_member, 0, sizeof(ASTStructMember));
        new_member->filename = member->filename;
        new_member->line_number = member->line_number;
        new_member->column_number = member->column_number;
        new_member->end_line_number = member->end_line_number;
        new_member->end_column_number = member->end_column_number;
        strcpy(new_member->identifier, member->identifier);
        new_member->value = member->value;
        new_member->lexical_type_scope = member->lexical_type_scope;
        if(new_member->value != NULL && new_member->value->kind == AST_EXPR_FUNCTION)
            new_member->value->member_owner = new_member;
        if(member->data_type)
            new_member->data_type = resolveNamedDataTypeInternal(member->data_type,
                                                                 scope,
                                                                 self_data_type, memo,
                                                                 allow_recursive_factory_result);

        if(head == NULL)
            head = new_member;
        else
            tail->next = new_member;
        tail = new_member;
        member = member->next;
    }

    return head;
}

ASTStructMember* resolveStructMembers(ASTStructMember *member, ScopeFrame *scope, ASTDataType *self_data_type)
{
    ResolveDataTypeEntry *memo = NULL;
    return resolveStructMembersInternal(member, scope, self_data_type, &memo, false);
}


#endif /* TYPE_SYSTEM_INFER_H */
