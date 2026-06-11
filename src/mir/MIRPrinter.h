#ifndef MIR_PRINTER_H
#define MIR_PRINTER_H

#include "MIRLowering.h"

static void printMirValue(FILE *stream, MirValueId value)
{
    fprintf(stream, "%%v%d", value);
}

static void printMirOperandList(FILE *stream, MirOperandList list)
{
    for(int i = 0; i < list.count; i++)
    {
        printMirValue(stream, list.items[i]);
        if(i + 1 < list.count)
            fprintf(stream, ", ");
    }
}

static const char* mirInstKindToString(MirInstKind kind)
{
    switch(kind)
    {
        case MIR_INST_CONST_BOOL: return "const_bool";
        case MIR_INST_CONST_CHAR: return "const_char";
        case MIR_INST_CONST_INT: return "const_int";
        case MIR_INST_CONST_FLOAT: return "const_float";
        case MIR_INST_CONST_STRING: return "const_string";
        case MIR_INST_CONVERT: return "convert";
        case MIR_INST_NEG: return "neg";
        case MIR_INST_NOT: return "not";
        case MIR_INST_BIT_NOT: return "bit_not";
        case MIR_INST_ADD: return "add";
        case MIR_INST_SUB: return "sub";
        case MIR_INST_MUL: return "mul";
        case MIR_INST_DIV: return "div";
        case MIR_INST_MOD: return "mod";
        case MIR_INST_SHIFT_LEFT: return "shl";
        case MIR_INST_SHIFT_RIGHT: return "shr";
        case MIR_INST_BIT_AND: return "bit_and";
        case MIR_INST_BIT_OR: return "bit_or";
        case MIR_INST_BIT_XOR: return "bit_xor";
        case MIR_INST_EQ: return "eq";
        case MIR_INST_NE: return "ne";
        case MIR_INST_LT: return "lt";
        case MIR_INST_LE: return "le";
        case MIR_INST_GT: return "gt";
        case MIR_INST_GE: return "ge";
        case MIR_INST_ALLOCA: return "alloca";
        case MIR_INST_LOAD: return "load";
        case MIR_INST_STORE: return "store";
        case MIR_INST_GLOBAL_ADDR: return "global_addr";
        case MIR_INST_FUNCTION_REF: return "function_ref";
        case MIR_INST_MAKE_CLOSURE: return "make_closure";
        case MIR_INST_FIELD_PTR: return "field_ptr";
        case MIR_INST_INDEX_PTR: return "index_ptr";
        case MIR_INST_ARRAY_LITERAL: return "array_literal";
        case MIR_INST_STRUCT_LITERAL: return "struct_literal";
        case MIR_INST_ENUM_LITERAL: return "enum_literal";
        case MIR_INST_CALL: return "call";
        case MIR_INST_EXTERN_CALL: return "extern_call";
        default: return "unknown";
    }
}

static void printMirInst(FILE *stream, MirInst *inst)
{
    if(inst->result >= 0)
    {
        printMirValue(stream, inst->result);
        fprintf(stream, ": ");
        printASTDataType(inst->result_type);
        fprintf(stream, " = ");
    }

    fprintf(stream, "%s ", mirInstKindToString(inst->kind));
    switch(inst->kind)
    {
        case MIR_INST_CONST_BOOL:
            fprintf(stream, "%s", inst->data.const_bool.value ? "true" : "false");
            break;
        case MIR_INST_CONST_CHAR:
            fprintf(stream, "'");
            printEscapedChar(inst->data.const_char.value);
            fprintf(stream, "'");
            break;
        case MIR_INST_CONST_INT:
            fprintf(stream, "%lld", inst->data.const_int.value);
            break;
        case MIR_INST_CONST_FLOAT:
            fprintf(stream, "%Lf", inst->data.const_float.value);
            break;
        case MIR_INST_CONST_STRING:
            fprintf(stream, "\"%s\"", inst->data.const_string.value);
            break;
        case MIR_INST_CONVERT:
            printMirValue(stream, inst->data.convert.operand);
            fprintf(stream, " to ");
            printASTDataType(inst->data.convert.target_type);
            break;
        case MIR_INST_NEG:
        case MIR_INST_NOT:
        case MIR_INST_BIT_NOT:
            printMirValue(stream, inst->data.unary.operand);
            break;
        case MIR_INST_ADD:
        case MIR_INST_SUB:
        case MIR_INST_MUL:
        case MIR_INST_DIV:
        case MIR_INST_MOD:
        case MIR_INST_SHIFT_LEFT:
        case MIR_INST_SHIFT_RIGHT:
        case MIR_INST_BIT_AND:
        case MIR_INST_BIT_OR:
        case MIR_INST_BIT_XOR:
        case MIR_INST_EQ:
        case MIR_INST_NE:
        case MIR_INST_LT:
        case MIR_INST_LE:
        case MIR_INST_GT:
        case MIR_INST_GE:
            printMirValue(stream, inst->data.binary.lhs);
            fprintf(stream, ", ");
            printMirValue(stream, inst->data.binary.rhs);
            break;
        case MIR_INST_ALLOCA:
            printASTDataType(inst->data.alloca_inst.alloca_type);
            break;
        case MIR_INST_LOAD:
            printMirValue(stream, inst->data.load.address);
            break;
        case MIR_INST_STORE:
            printMirValue(stream, inst->data.store.address);
            fprintf(stream, ", ");
            printMirValue(stream, inst->data.store.value);
            break;
        case MIR_INST_GLOBAL_ADDR:
            fprintf(stream, "@%s", inst->data.global_addr.global_name);
            break;
        case MIR_INST_FUNCTION_REF:
            fprintf(stream, "@%s", inst->data.function_ref.function_name);
            break;
        case MIR_INST_MAKE_CLOSURE:
            fprintf(stream, "@%s [", inst->data.make_closure.function_name);
            printMirOperandList(stream, inst->data.make_closure.captures);
            fprintf(stream, "]");
            if(inst->data.make_closure.environment_type != NULL)
            {
                fprintf(stream, " env ");
                printASTDataType(inst->data.make_closure.environment_type);
            }
            break;
        case MIR_INST_FIELD_PTR:
            printMirValue(stream, inst->data.field_ptr.base_address);
            fprintf(stream, ", .%s (#%d)", inst->data.field_ptr.identifier, inst->data.field_ptr.field_index);
            break;
        case MIR_INST_INDEX_PTR:
            printMirValue(stream, inst->data.index_ptr.base_address);
            fprintf(stream, ", ");
            printMirValue(stream, inst->data.index_ptr.index_value);
            break;
        case MIR_INST_ARRAY_LITERAL:
            fprintf(stream, "[");
            printMirOperandList(stream, inst->data.array_literal.elements);
            fprintf(stream, "]");
            break;
        case MIR_INST_STRUCT_LITERAL:
            fprintf(stream, "{");
            for(int i = 0; i < inst->data.struct_literal.fields.count; i++)
            {
                fprintf(stream, ".%s = ", inst->data.struct_literal.fields.items[i].identifier);
                printMirValue(stream, inst->data.struct_literal.fields.items[i].value);
                if(i + 1 < inst->data.struct_literal.fields.count)
                    fprintf(stream, ", ");
            }
            fprintf(stream, "}");
            break;
        case MIR_INST_ENUM_LITERAL:
            fprintf(stream, "%s.%s (#%d)",
                    inst->data.enum_literal.enum_name[0] == '\0' ? "<enum>" : inst->data.enum_literal.enum_name,
                    inst->data.enum_literal.variant_name,
                    inst->data.enum_literal.ordinal);
            break;
        case MIR_INST_CALL:
            printMirValue(stream, inst->data.call.callee);
            fprintf(stream, "(");
            printMirOperandList(stream, inst->data.call.arguments);
            fprintf(stream, ")");
            break;
        case MIR_INST_EXTERN_CALL:
            fprintf(stream, "@%s(", inst->data.extern_call.symbol_name);
            printMirOperandList(stream, inst->data.extern_call.arguments);
            fprintf(stream, ")");
            break;
        default:
            fprintf(stream, "<unknown>");
            break;
    }
}

static void printMirTerminator(FILE *stream, MirTerminator *terminator)
{
    switch(terminator->kind)
    {
        case MIR_TERM_NONE:
            fprintf(stream, "    <no terminator>\n");
            break;
        case MIR_TERM_BR:
            fprintf(stream, "    br block_%d\n", terminator->data.br.target);
            break;
        case MIR_TERM_COND_BR:
            fprintf(stream, "    cond_br ");
            printMirValue(stream, terminator->data.cond_br.condition);
            fprintf(stream, ", block_%d, block_%d\n",
                    terminator->data.cond_br.then_block,
                    terminator->data.cond_br.else_block);
            break;
        case MIR_TERM_RET:
            if(terminator->data.ret.has_value)
            {
                fprintf(stream, "    ret ");
                printMirValue(stream, terminator->data.ret.value);
                fprintf(stream, "\n");
            }
            else
                fprintf(stream, "    ret\n");
            break;
    }
}

void printMIRProgram(MirProgram *program)
{
    printf("PRINT MIR ===============\n\n");

    if(program->global_count > 0)
    {
        printf("globals:\n");
        for(int i = 0; i < program->global_count; i++)
        {
            MirGlobal *global = &(program->globals[i]);
            printf("  @%s: ", global->name);
            printASTDataType(global->data_type);
            printf(" %s", global->mutable ? "mut" : "const");
            if(global->has_const_string_initializer)
                printf(" = \"%s\"", global->const_string_initializer);
            printf("\n");
        }
        printf("\n");
    }

    for(int i = 0; i < program->function_count; i++)
    {
        MirFunction *function = program->functions[i];
        printf("fn %s(", function->name);
        bool need_comma = false;
        if(function->closure_env_type != NULL)
        {
            printf("__env: ");
            printASTDataType(mirClosureEnvPointerType(function->closure_env_type));
            need_comma = true;
        }
        for(int j = 0; j < function->parameter_count; j++)
        {
            if(need_comma)
                printf(", ");
            printf("%s: ", function->parameters[j].identifier);
            printASTDataType(function->parameters[j].runtime_data_type);
            need_comma = true;
        }
        printf(") ");
        printASTDataType(function->return_data_type);
        if(function->capture_count > 0)
        {
            printf(" captures[");
            for(int j = 0; j < function->capture_count; j++)
            {
                printf("%s: ", function->captures[j].identifier);
                printASTDataType(function->captures[j].runtime_data_type);
                if(j + 1 < function->capture_count)
                    printf(", ");
            }
            printf("]");
        }
        printf("\n");

        for(int block_index = 0; block_index < function->block_count; block_index++)
        {
            MirBlock *block = &(function->blocks[block_index]);
            printf("  %s:\n", block->name);
            for(int inst_index = 0; inst_index < block->inst_count; inst_index++)
            {
                printf("    ");
                printMirInst(stdout, &(block->insts[inst_index]));
                printf("\n");
            }
            printMirTerminator(stdout, &(block->terminator));
        }
        printf("\n");
    }

    printf("END PRINT MIR ===============\n\n");
}

#endif /* MIR_PRINTER_H */
