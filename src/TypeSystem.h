#ifndef TYPE_SYSTEM_H
#define TYPE_SYSTEM_H

#include "AST.h"
#include "SymbolTable.h"
#include <stdbool.h>
#include <stdio.h>

typedef enum TypeSystemDataType {
    TYPE_SYSTEM_DATA_TYPE_LITERAL_INTEGER,
    TYPE_SYSTEM_DATA_TYPE_LITERAL_FLOAT,
    TYPE_SYSTEM_DATA_TYPE_I8,
    TYPE_SYSTEM_DATA_TYPE_I16,
    TYPE_SYSTEM_DATA_TYPE_I32,
    TYPE_SYSTEM_DATA_TYPE_I64,
    TYPE_SYSTEM_DATA_TYPE_U8,
    TYPE_SYSTEM_DATA_TYPE_U16,
    TYPE_SYSTEM_DATA_TYPE_U32,
    TYPE_SYSTEM_DATA_TYPE_U64,
    TYPE_SYSTEM_DATA_TYPE_F8,
    TYPE_SYSTEM_DATA_TYPE_F16,
    TYPE_SYSTEM_DATA_TYPE_F32,
    TYPE_SYSTEM_DATA_TYPE_F64,
    TYPE_SYSTEM_DATA_TYPE_CHAR,
    TYPE_SYSTEM_DATA_TYPE_BOOL,
} TypeSystemDataType;

const char* typeSystemDataTypeToString(TypeSystemDataType data_type)
{
    switch(data_type)
    {
        case TYPE_SYSTEM_DATA_TYPE_LITERAL_INTEGER: return "literal integer";
        case TYPE_SYSTEM_DATA_TYPE_LITERAL_FLOAT: return "literal float";
        case TYPE_SYSTEM_DATA_TYPE_I8: return "i8";
        case TYPE_SYSTEM_DATA_TYPE_I16: return "i16";
        case TYPE_SYSTEM_DATA_TYPE_I32: return "i32";
        case TYPE_SYSTEM_DATA_TYPE_I64: return "i64";
        case TYPE_SYSTEM_DATA_TYPE_U8: return "u8";
        case TYPE_SYSTEM_DATA_TYPE_U16: return "u16";
        case TYPE_SYSTEM_DATA_TYPE_U32: return "u32";
        case TYPE_SYSTEM_DATA_TYPE_U64: return "u64";
        case TYPE_SYSTEM_DATA_TYPE_F8: return "f8";
        case TYPE_SYSTEM_DATA_TYPE_F16: return "f16";
        case TYPE_SYSTEM_DATA_TYPE_F32: return "f32";
        case TYPE_SYSTEM_DATA_TYPE_F64: return "f64";
        case TYPE_SYSTEM_DATA_TYPE_CHAR: return "char";
        case TYPE_SYSTEM_DATA_TYPE_BOOL: return "bool";
        default:
            printf("typeSystemDataTypeToString: unknown type system data type\n");
            exit(1);
    }
}

TypeSystemDataType astDataTypeToTypeSystemDataType(ASTDataType data_type)
{
    switch(data_type)
    {
        case AST_DATA_TYPE_I8: return TYPE_SYSTEM_DATA_TYPE_I8;
        case AST_DATA_TYPE_I16: return TYPE_SYSTEM_DATA_TYPE_I16;
        case AST_DATA_TYPE_I32: return TYPE_SYSTEM_DATA_TYPE_I32;
        case AST_DATA_TYPE_I64: return TYPE_SYSTEM_DATA_TYPE_I64;
        case AST_DATA_TYPE_U8: return TYPE_SYSTEM_DATA_TYPE_U8;
        case AST_DATA_TYPE_U16: return TYPE_SYSTEM_DATA_TYPE_U16;
        case AST_DATA_TYPE_U32: return TYPE_SYSTEM_DATA_TYPE_U32;
        case AST_DATA_TYPE_U64: return TYPE_SYSTEM_DATA_TYPE_U64;
        case AST_DATA_TYPE_F8: return TYPE_SYSTEM_DATA_TYPE_F8;
        case AST_DATA_TYPE_F16: return TYPE_SYSTEM_DATA_TYPE_F16;
        case AST_DATA_TYPE_F32: return TYPE_SYSTEM_DATA_TYPE_F32;
        case AST_DATA_TYPE_F64: return TYPE_SYSTEM_DATA_TYPE_F64;
        case AST_DATA_TYPE_CHAR: return TYPE_SYSTEM_DATA_TYPE_CHAR;
        case AST_DATA_TYPE_BOOL: return TYPE_SYSTEM_DATA_TYPE_BOOL;
        default:
            printf("astDataTypeToTypeSystemDataType: unsupported AST data type %s\n",
                   astDataTypeToString(data_type));
            exit(1);
    }
}

ASTDataType typeSystemDataTypeToAstDataType(TypeSystemDataType data_type)
{
    switch(data_type)
    {
        case TYPE_SYSTEM_DATA_TYPE_I8: return AST_DATA_TYPE_I8;
        case TYPE_SYSTEM_DATA_TYPE_I16: return AST_DATA_TYPE_I16;
        case TYPE_SYSTEM_DATA_TYPE_I32: return AST_DATA_TYPE_I32;
        case TYPE_SYSTEM_DATA_TYPE_I64: return AST_DATA_TYPE_I64;
        case TYPE_SYSTEM_DATA_TYPE_U8: return AST_DATA_TYPE_U8;
        case TYPE_SYSTEM_DATA_TYPE_U16: return AST_DATA_TYPE_U16;
        case TYPE_SYSTEM_DATA_TYPE_U32: return AST_DATA_TYPE_U32;
        case TYPE_SYSTEM_DATA_TYPE_U64: return AST_DATA_TYPE_U64;
        case TYPE_SYSTEM_DATA_TYPE_F8: return AST_DATA_TYPE_F8;
        case TYPE_SYSTEM_DATA_TYPE_F16: return AST_DATA_TYPE_F16;
        case TYPE_SYSTEM_DATA_TYPE_F32: return AST_DATA_TYPE_F32;
        case TYPE_SYSTEM_DATA_TYPE_F64: return AST_DATA_TYPE_F64;
        case TYPE_SYSTEM_DATA_TYPE_CHAR: return AST_DATA_TYPE_CHAR;
        case TYPE_SYSTEM_DATA_TYPE_BOOL: return AST_DATA_TYPE_BOOL;
        default:
            printf("typeSystemDataTypeToAstDataType: cannot convert %s to AST data type\n",
                   typeSystemDataTypeToString(data_type));
            exit(1);
    }
}

bool isTypeSystemInteger(TypeSystemDataType data_type)
{
    return data_type == TYPE_SYSTEM_DATA_TYPE_LITERAL_INTEGER ||
           data_type == TYPE_SYSTEM_DATA_TYPE_I8 ||
           data_type == TYPE_SYSTEM_DATA_TYPE_I16 ||
           data_type == TYPE_SYSTEM_DATA_TYPE_I32 ||
           data_type == TYPE_SYSTEM_DATA_TYPE_I64 ||
           data_type == TYPE_SYSTEM_DATA_TYPE_U8 ||
           data_type == TYPE_SYSTEM_DATA_TYPE_U16 ||
           data_type == TYPE_SYSTEM_DATA_TYPE_U32 ||
           data_type == TYPE_SYSTEM_DATA_TYPE_U64 ||
           data_type == TYPE_SYSTEM_DATA_TYPE_CHAR;
}

bool isTypeSystemConcreteInteger(TypeSystemDataType data_type)
{
    return data_type == TYPE_SYSTEM_DATA_TYPE_I8 ||
           data_type == TYPE_SYSTEM_DATA_TYPE_I16 ||
           data_type == TYPE_SYSTEM_DATA_TYPE_I32 ||
           data_type == TYPE_SYSTEM_DATA_TYPE_I64 ||
           data_type == TYPE_SYSTEM_DATA_TYPE_U8 ||
           data_type == TYPE_SYSTEM_DATA_TYPE_U16 ||
           data_type == TYPE_SYSTEM_DATA_TYPE_U32 ||
           data_type == TYPE_SYSTEM_DATA_TYPE_U64;
}

bool isTypeSystemFloat(TypeSystemDataType data_type)
{
    return data_type == TYPE_SYSTEM_DATA_TYPE_LITERAL_FLOAT ||
           data_type == TYPE_SYSTEM_DATA_TYPE_F8 ||
           data_type == TYPE_SYSTEM_DATA_TYPE_F16 ||
           data_type == TYPE_SYSTEM_DATA_TYPE_F32 ||
           data_type == TYPE_SYSTEM_DATA_TYPE_F64;
}

bool isTypeSystemConcreteFloat(TypeSystemDataType data_type)
{
    return data_type == TYPE_SYSTEM_DATA_TYPE_F8 ||
           data_type == TYPE_SYSTEM_DATA_TYPE_F16 ||
           data_type == TYPE_SYSTEM_DATA_TYPE_F32 ||
           data_type == TYPE_SYSTEM_DATA_TYPE_F64;
}

bool isTypeSystemSignedInteger(TypeSystemDataType data_type)
{
    return data_type == TYPE_SYSTEM_DATA_TYPE_I8 ||
           data_type == TYPE_SYSTEM_DATA_TYPE_I16 ||
           data_type == TYPE_SYSTEM_DATA_TYPE_I32 ||
           data_type == TYPE_SYSTEM_DATA_TYPE_I64;
}

bool isTypeSystemUnsignedInteger(TypeSystemDataType data_type)
{
    return data_type == TYPE_SYSTEM_DATA_TYPE_U8 ||
           data_type == TYPE_SYSTEM_DATA_TYPE_U16 ||
           data_type == TYPE_SYSTEM_DATA_TYPE_U32 ||
           data_type == TYPE_SYSTEM_DATA_TYPE_U64;
}

int getIntegerTypeWidth(TypeSystemDataType data_type)
{
    switch(data_type)
    {
        case TYPE_SYSTEM_DATA_TYPE_I8:
        case TYPE_SYSTEM_DATA_TYPE_U8:
            return 8;
        case TYPE_SYSTEM_DATA_TYPE_I16:
        case TYPE_SYSTEM_DATA_TYPE_U16:
            return 16;
        case TYPE_SYSTEM_DATA_TYPE_I32:
        case TYPE_SYSTEM_DATA_TYPE_U32:
            return 32;
        case TYPE_SYSTEM_DATA_TYPE_I64:
        case TYPE_SYSTEM_DATA_TYPE_U64:
            return 64;
        case TYPE_SYSTEM_DATA_TYPE_CHAR:
            return 8;
        default:
            printf("getIntegerTypeWidth: %s is not an integer type\n",
                   typeSystemDataTypeToString(data_type));
            exit(1);
    }
}

int getFloatTypeWidth(TypeSystemDataType data_type)
{
    switch(data_type)
    {
        case TYPE_SYSTEM_DATA_TYPE_F8:
            return 8;
        case TYPE_SYSTEM_DATA_TYPE_F16:
            return 16;
        case TYPE_SYSTEM_DATA_TYPE_F32:
            return 32;
        case TYPE_SYSTEM_DATA_TYPE_F64:
            return 64;
        default:
            printf("getFloatTypeWidth: %s is not a float type\n",
                   typeSystemDataTypeToString(data_type));
            exit(1);
    }
}

bool canLiteralIntegerFitType(long long int literal_integer, TypeSystemDataType target_type)
{
    switch(target_type)
    {
        case TYPE_SYSTEM_DATA_TYPE_I8: return literal_integer >= -128LL && literal_integer <= 127LL;
        case TYPE_SYSTEM_DATA_TYPE_I16: return literal_integer >= -32768LL && literal_integer <= 32767LL;
        case TYPE_SYSTEM_DATA_TYPE_I32: return literal_integer >= -2147483648LL && literal_integer <= 2147483647LL;
        case TYPE_SYSTEM_DATA_TYPE_I64: return true;
        case TYPE_SYSTEM_DATA_TYPE_U8: return literal_integer >= 0LL && literal_integer <= 255LL;
        case TYPE_SYSTEM_DATA_TYPE_U16: return literal_integer >= 0LL && literal_integer <= 65535LL;
        case TYPE_SYSTEM_DATA_TYPE_U32: return literal_integer >= 0LL && literal_integer <= 4294967295LL;
        case TYPE_SYSTEM_DATA_TYPE_U64: return literal_integer >= 0LL;
        case TYPE_SYSTEM_DATA_TYPE_CHAR: return literal_integer >= 0LL && literal_integer <= 255LL;
        default: return false;
    }
}

bool canImplicitConvertType(TypeSystemDataType source_type, ASTNode *source_node, TypeSystemDataType target_type)
{
    if(source_type == target_type)
        return true;

    if(source_type == TYPE_SYSTEM_DATA_TYPE_BOOL || target_type == TYPE_SYSTEM_DATA_TYPE_BOOL)
        return false;

    if(source_type == TYPE_SYSTEM_DATA_TYPE_LITERAL_INTEGER)
    {
        if(isTypeSystemInteger(target_type))
            return canLiteralIntegerFitType(source_node->literal_integer, target_type);
        if(isTypeSystemConcreteFloat(target_type))
            return true;
        return false;
    }

    if(source_type == TYPE_SYSTEM_DATA_TYPE_LITERAL_FLOAT)
    {
        return isTypeSystemConcreteFloat(target_type);
    }

    if(source_type == TYPE_SYSTEM_DATA_TYPE_CHAR)
    {
        return target_type == TYPE_SYSTEM_DATA_TYPE_CHAR || isTypeSystemConcreteInteger(target_type);
    }

    if((isTypeSystemConcreteInteger(source_type) || source_type == TYPE_SYSTEM_DATA_TYPE_CHAR) &&
       isTypeSystemConcreteFloat(target_type))
    {
        return true;
    }

    if(isTypeSystemSignedInteger(source_type) && isTypeSystemSignedInteger(target_type))
    {
        return getIntegerTypeWidth(source_type) <= getIntegerTypeWidth(target_type);
    }

    if(isTypeSystemUnsignedInteger(source_type) && isTypeSystemUnsignedInteger(target_type))
    {
        return getIntegerTypeWidth(source_type) <= getIntegerTypeWidth(target_type);
    }

    if(isTypeSystemConcreteFloat(source_type) && isTypeSystemConcreteFloat(target_type))
    {
        return getFloatTypeWidth(source_type) <= getFloatTypeWidth(target_type);
    }

    if(source_type == TYPE_SYSTEM_DATA_TYPE_CHAR && target_type == TYPE_SYSTEM_DATA_TYPE_CHAR)
        return true;

    return false;
}

void typeErrorBinaryOperator(ASTNode *node, const char *operator_name,
                             TypeSystemDataType lhs_type, TypeSystemDataType rhs_type)
{
    printf("Type error: operator %s cannot be applied to %s and %s at file %s, line %d, column %d\n",
           operator_name, typeSystemDataTypeToString(lhs_type), typeSystemDataTypeToString(rhs_type),
           node->filename, node->line_number, node->column_number);
    exit(1);
}

void typeErrorUnaryOperator(ASTNode *node, const char *operator_name, TypeSystemDataType operand_type)
{
    printf("Type error: operator %s cannot be applied to %s at file %s, line %d, column %d\n",
           operator_name, typeSystemDataTypeToString(operand_type),
           node->filename, node->line_number, node->column_number);
    exit(1);
}

void typeErrorAssign(ASTNode *node, ASTNode *source_node, TypeSystemDataType source_type, TypeSystemDataType target_type)
{
    printf("Type error: cannot implicitly convert %s to %s for variable %s at file %s, line %d, column %d\n",
           typeSystemDataTypeToString(source_type), typeSystemDataTypeToString(target_type), node->identifier,
           source_node->filename, source_node->line_number, source_node->column_number);
    exit(1);
}

TypeSystemDataType promoteLiteralInteger(TypeSystemDataType data_type)
{
    if(data_type == TYPE_SYSTEM_DATA_TYPE_LITERAL_INTEGER)
        return TYPE_SYSTEM_DATA_TYPE_I32;
    return data_type;
}

TypeSystemDataType promoteLiteralFloat(TypeSystemDataType data_type)
{
    if(data_type == TYPE_SYSTEM_DATA_TYPE_LITERAL_FLOAT)
        return TYPE_SYSTEM_DATA_TYPE_F64;
    return data_type;
}

TypeSystemDataType getCommonNumericType(ASTNode *node, TypeSystemDataType lhs_type, ASTNode *lhs_node,
                                        TypeSystemDataType rhs_type, ASTNode *rhs_node, const char *operator_name)
{
    if(isTypeSystemFloat(lhs_type) || isTypeSystemFloat(rhs_type))
    {
        if(lhs_type == TYPE_SYSTEM_DATA_TYPE_LITERAL_INTEGER)
            lhs_type = TYPE_SYSTEM_DATA_TYPE_F64;
        else if(lhs_type == TYPE_SYSTEM_DATA_TYPE_LITERAL_FLOAT)
            lhs_type = TYPE_SYSTEM_DATA_TYPE_F64;
        else if(lhs_type == TYPE_SYSTEM_DATA_TYPE_CHAR)
            lhs_type = TYPE_SYSTEM_DATA_TYPE_I32;

        if(rhs_type == TYPE_SYSTEM_DATA_TYPE_LITERAL_INTEGER)
            rhs_type = TYPE_SYSTEM_DATA_TYPE_F64;
        else if(rhs_type == TYPE_SYSTEM_DATA_TYPE_LITERAL_FLOAT)
            rhs_type = TYPE_SYSTEM_DATA_TYPE_F64;
        else if(rhs_type == TYPE_SYSTEM_DATA_TYPE_CHAR)
            rhs_type = TYPE_SYSTEM_DATA_TYPE_I32;

        if((isTypeSystemConcreteFloat(lhs_type) || isTypeSystemConcreteInteger(lhs_type)) &&
           (isTypeSystemConcreteFloat(rhs_type) || isTypeSystemConcreteInteger(rhs_type)))
        {
            if(!isTypeSystemConcreteFloat(lhs_type))
                lhs_type = TYPE_SYSTEM_DATA_TYPE_F64;
            if(!isTypeSystemConcreteFloat(rhs_type))
                rhs_type = TYPE_SYSTEM_DATA_TYPE_F64;
            return getFloatTypeWidth(lhs_type) >= getFloatTypeWidth(rhs_type) ? lhs_type : rhs_type;
        }

        typeErrorBinaryOperator(node, operator_name, lhs_type, rhs_type);
    }

    if(lhs_type == TYPE_SYSTEM_DATA_TYPE_CHAR)
        lhs_type = TYPE_SYSTEM_DATA_TYPE_I32;
    else if(lhs_type == TYPE_SYSTEM_DATA_TYPE_LITERAL_INTEGER)
        lhs_type = TYPE_SYSTEM_DATA_TYPE_I32;

    if(rhs_type == TYPE_SYSTEM_DATA_TYPE_CHAR)
        rhs_type = TYPE_SYSTEM_DATA_TYPE_I32;
    else if(rhs_type == TYPE_SYSTEM_DATA_TYPE_LITERAL_INTEGER)
        rhs_type = TYPE_SYSTEM_DATA_TYPE_I32;

    if(isTypeSystemSignedInteger(lhs_type) && isTypeSystemSignedInteger(rhs_type))
        return getIntegerTypeWidth(lhs_type) >= getIntegerTypeWidth(rhs_type) ? lhs_type : rhs_type;

    if(isTypeSystemUnsignedInteger(lhs_type) && isTypeSystemUnsignedInteger(rhs_type))
        return getIntegerTypeWidth(lhs_type) >= getIntegerTypeWidth(rhs_type) ? lhs_type : rhs_type;

    typeErrorBinaryOperator(node, operator_name, lhs_type, rhs_type);
    return TYPE_SYSTEM_DATA_TYPE_I32;
}

TypeSystemDataType inferExprType(ASTNode *node, VariableInfo *variable_infos, int variable_count)
{
    switch(node->kind)
    {
        case AST_EXPR_LITERAL_BOOL:
            return TYPE_SYSTEM_DATA_TYPE_BOOL;
        case AST_EXPR_LITERAL_CHAR:
            return TYPE_SYSTEM_DATA_TYPE_CHAR;
        case AST_EXPR_LITERAL_INTEGER:
            return TYPE_SYSTEM_DATA_TYPE_LITERAL_INTEGER;
        case AST_EXPR_LITERAL_FLOAT:
            return TYPE_SYSTEM_DATA_TYPE_LITERAL_FLOAT;
        case AST_EXPR_VARIABLE: {
            int variable_index = findVariableInfo(variable_infos, variable_count, node->identifier);
            if(variable_index < 0)
            {
                printf("Type inference: undeclared variable %s at file %s, line %d, column %d\n",
                       node->identifier, node->filename, node->line_number, node->column_number);
                exit(1);
            }
            return astDataTypeToTypeSystemDataType(variable_infos[variable_index].data_type);
        }
        case AST_EXPR_PARENTHESIS:
            return inferExprType(node->lhs, variable_infos, variable_count);
        case AST_EXPR_UNARY_PLUS:
        case AST_EXPR_UNARY_MINUS: {
            TypeSystemDataType operand_type = inferExprType(node->lhs, variable_infos, variable_count);
            if(operand_type == TYPE_SYSTEM_DATA_TYPE_LITERAL_INTEGER)
                return TYPE_SYSTEM_DATA_TYPE_I32;
            if(operand_type == TYPE_SYSTEM_DATA_TYPE_LITERAL_FLOAT)
                return TYPE_SYSTEM_DATA_TYPE_F64;
            if(isTypeSystemConcreteInteger(operand_type) || isTypeSystemConcreteFloat(operand_type))
                return operand_type;
            typeErrorUnaryOperator(node, node->kind == AST_EXPR_UNARY_PLUS ? "+" : "-", operand_type);
        } break;
        case AST_EXPR_UNARY_LOGICAL_NOT: {
            TypeSystemDataType operand_type = inferExprType(node->lhs, variable_infos, variable_count);
            if(operand_type == TYPE_SYSTEM_DATA_TYPE_BOOL)
                return TYPE_SYSTEM_DATA_TYPE_BOOL;
            typeErrorUnaryOperator(node, "!", operand_type);
        } break;
        case AST_EXPR_UNARY_BIT_NOT: {
            TypeSystemDataType operand_type = inferExprType(node->lhs, variable_infos, variable_count);
            if(operand_type == TYPE_SYSTEM_DATA_TYPE_LITERAL_INTEGER)
                return TYPE_SYSTEM_DATA_TYPE_I32;
            if(operand_type == TYPE_SYSTEM_DATA_TYPE_CHAR)
                return TYPE_SYSTEM_DATA_TYPE_I32;
            if(isTypeSystemConcreteInteger(operand_type))
                return operand_type;
            typeErrorUnaryOperator(node, "~", operand_type);
        } break;
        case AST_EXPR_MUL:
        case AST_EXPR_DIV:
        case AST_EXPR_ADD:
        case AST_EXPR_SUB: {
            TypeSystemDataType lhs_type = inferExprType(node->lhs, variable_infos, variable_count);
            TypeSystemDataType rhs_type = inferExprType(node->rhs, variable_infos, variable_count);
            const char *operator_name = "+";
            if(node->kind == AST_EXPR_MUL) operator_name = "*";
            else if(node->kind == AST_EXPR_DIV) operator_name = "/";
            else if(node->kind == AST_EXPR_SUB) operator_name = "-";
            return getCommonNumericType(node, lhs_type, node->lhs, rhs_type, node->rhs, operator_name);
        }
        case AST_EXPR_MOD:
        case AST_EXPR_SHIFT_LEFT:
        case AST_EXPR_SHIFT_RIGHT:
        case AST_EXPR_BIT_AND:
        case AST_EXPR_BIT_OR:
        case AST_EXPR_BIT_XOR: {
            TypeSystemDataType lhs_type = inferExprType(node->lhs, variable_infos, variable_count);
            TypeSystemDataType rhs_type = inferExprType(node->rhs, variable_infos, variable_count);
            if(lhs_type == TYPE_SYSTEM_DATA_TYPE_LITERAL_INTEGER)
                lhs_type = TYPE_SYSTEM_DATA_TYPE_I32;
            else if(lhs_type == TYPE_SYSTEM_DATA_TYPE_CHAR)
                lhs_type = TYPE_SYSTEM_DATA_TYPE_I32;
            if(rhs_type == TYPE_SYSTEM_DATA_TYPE_LITERAL_INTEGER)
                rhs_type = TYPE_SYSTEM_DATA_TYPE_I32;
            else if(rhs_type == TYPE_SYSTEM_DATA_TYPE_CHAR)
                rhs_type = TYPE_SYSTEM_DATA_TYPE_I32;
            if(!(isTypeSystemConcreteInteger(lhs_type) && isTypeSystemConcreteInteger(rhs_type)))
            {
                const char *operator_name = "%";
                if(node->kind == AST_EXPR_SHIFT_LEFT) operator_name = "<<";
                else if(node->kind == AST_EXPR_SHIFT_RIGHT) operator_name = ">>";
                else if(node->kind == AST_EXPR_BIT_AND) operator_name = "&";
                else if(node->kind == AST_EXPR_BIT_OR) operator_name = "|";
                else if(node->kind == AST_EXPR_BIT_XOR) operator_name = "^";
                typeErrorBinaryOperator(node, operator_name, lhs_type, rhs_type);
            }
            return lhs_type;
        }
        case AST_EXPR_LOGICAL_AND:
        case AST_EXPR_LOGICAL_OR: {
            TypeSystemDataType lhs_type = inferExprType(node->lhs, variable_infos, variable_count);
            TypeSystemDataType rhs_type = inferExprType(node->rhs, variable_infos, variable_count);
            if(lhs_type == TYPE_SYSTEM_DATA_TYPE_BOOL && rhs_type == TYPE_SYSTEM_DATA_TYPE_BOOL)
                return TYPE_SYSTEM_DATA_TYPE_BOOL;
            typeErrorBinaryOperator(node, node->kind == AST_EXPR_LOGICAL_AND ? "&&" : "||", lhs_type, rhs_type);
        } break;
        case AST_EXPR_EQUAL:
        case AST_EXPR_NOT_EQUAL: {
            TypeSystemDataType lhs_type = inferExprType(node->lhs, variable_infos, variable_count);
            TypeSystemDataType rhs_type = inferExprType(node->rhs, variable_infos, variable_count);
            if(lhs_type == TYPE_SYSTEM_DATA_TYPE_BOOL || rhs_type == TYPE_SYSTEM_DATA_TYPE_BOOL)
            {
                if(lhs_type == rhs_type)
                    return TYPE_SYSTEM_DATA_TYPE_BOOL;
                typeErrorBinaryOperator(node, node->kind == AST_EXPR_EQUAL ? "==" : "!=", lhs_type, rhs_type);
            }

            getCommonNumericType(node, lhs_type, node->lhs, rhs_type, node->rhs,
                                 node->kind == AST_EXPR_EQUAL ? "==" : "!=");
            return TYPE_SYSTEM_DATA_TYPE_BOOL;
        }
        case AST_EXPR_LESS:
        case AST_EXPR_LESS_EQUAL:
        case AST_EXPR_GREATER:
        case AST_EXPR_GREATER_EQUAL: {
            TypeSystemDataType lhs_type = inferExprType(node->lhs, variable_infos, variable_count);
            TypeSystemDataType rhs_type = inferExprType(node->rhs, variable_infos, variable_count);
            const char *operator_name = "<";
            if(node->kind == AST_EXPR_LESS_EQUAL) operator_name = "<=";
            else if(node->kind == AST_EXPR_GREATER) operator_name = ">";
            else if(node->kind == AST_EXPR_GREATER_EQUAL) operator_name = ">=";
            getCommonNumericType(node, lhs_type, node->lhs, rhs_type, node->rhs, operator_name);
            return TYPE_SYSTEM_DATA_TYPE_BOOL;
        }
        default:
            printf("inferExprType: unsupported AST node kind %s\n", astNodeKindToString(node->kind));
            exit(1);
    }
}

TypeSystemDataType inferDeclaredTypeFromExpr(ASTNode *expr, VariableInfo *variable_infos, int variable_count)
{
    TypeSystemDataType expr_type = inferExprType(expr, variable_infos, variable_count);
    if(expr_type == TYPE_SYSTEM_DATA_TYPE_LITERAL_INTEGER)
        return TYPE_SYSTEM_DATA_TYPE_I32;
    if(expr_type == TYPE_SYSTEM_DATA_TYPE_LITERAL_FLOAT)
        return TYPE_SYSTEM_DATA_TYPE_F64;
    return expr_type;
}

#endif /* TYPE_SYSTEM_H */
