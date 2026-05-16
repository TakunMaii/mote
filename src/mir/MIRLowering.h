#ifndef MIR_LOWERING_H
#define MIR_LOWERING_H

#include "../AST.h"
#include "../Semantic.h"
#include "../TypeSystem.h"
#include <stdlib.h>

#define MIR_MAX_NAME_LENGTH 128

typedef int MirValueId;
typedef int MirBlockId;

typedef enum MirInstKind {
    MIR_INST_CONST_BOOL,
    MIR_INST_CONST_CHAR,
    MIR_INST_CONST_INT,
    MIR_INST_CONST_FLOAT,
    MIR_INST_CONST_STRING,
    MIR_INST_CONVERT,
    MIR_INST_NEG,
    MIR_INST_NOT,
    MIR_INST_BIT_NOT,
    MIR_INST_ADD,
    MIR_INST_SUB,
    MIR_INST_MUL,
    MIR_INST_DIV,
    MIR_INST_MOD,
    MIR_INST_SHIFT_LEFT,
    MIR_INST_SHIFT_RIGHT,
    MIR_INST_BIT_AND,
    MIR_INST_BIT_OR,
    MIR_INST_BIT_XOR,
    MIR_INST_EQ,
    MIR_INST_NE,
    MIR_INST_LT,
    MIR_INST_LE,
    MIR_INST_GT,
    MIR_INST_GE,
    MIR_INST_ALLOCA,
    MIR_INST_LOAD,
    MIR_INST_STORE,
    MIR_INST_GLOBAL_ADDR,
    MIR_INST_FUNCTION_REF,
    MIR_INST_MAKE_CLOSURE,
    MIR_INST_FIELD_PTR,
    MIR_INST_INDEX_PTR,
    MIR_INST_ARRAY_LITERAL,
    MIR_INST_STRUCT_LITERAL,
    MIR_INST_ENUM_LITERAL,
    MIR_INST_CALL,
} MirInstKind;

typedef enum MirTerminatorKind {
    MIR_TERM_NONE,
    MIR_TERM_BR,
    MIR_TERM_COND_BR,
    MIR_TERM_RET,
} MirTerminatorKind;

typedef struct MirValueInfo {
    ASTDataType *data_type;
    char name[MIR_MAX_NAME_LENGTH];
    bool is_input;
} MirValueInfo;

typedef struct MirOperandList {
    MirValueId *items;
    int count;
} MirOperandList;

typedef struct MirFieldValue {
    char identifier[MAX_IDENTIFIER_LENGTH];
    MirValueId value;
} MirFieldValue;

typedef struct MirFieldValueList {
    MirFieldValue *items;
    int count;
} MirFieldValueList;

typedef struct MirInst {
    MirInstKind kind;
    MirValueId result;
    ASTDataType *result_type;
    const char *filename;
    int line_number;
    int column_number;
    union {
        struct {
            bool value;
        } const_bool;
        struct {
            char value;
        } const_char;
        struct {
            long long int value;
        } const_int;
        struct {
            long double value;
        } const_float;
        struct {
            char value[MAX_STRING_LITERAL_LENGTH];
        } const_string;
        struct {
            MirValueId operand;
            ASTDataType *target_type;
        } convert;
        struct {
            MirValueId operand;
        } unary;
        struct {
            MirValueId lhs;
            MirValueId rhs;
        } binary;
        struct {
            ASTDataType *alloca_type;
        } alloca_inst;
        struct {
            MirValueId address;
        } load;
        struct {
            MirValueId address;
            MirValueId value;
        } store;
        struct {
            char global_name[MIR_MAX_NAME_LENGTH];
        } global_addr;
        struct {
            char function_name[MIR_MAX_NAME_LENGTH];
        } function_ref;
        struct {
            char function_name[MIR_MAX_NAME_LENGTH];
            ASTDataType *environment_type;
            MirOperandList captures;
        } make_closure;
        struct {
            MirValueId base_address;
            char identifier[MAX_IDENTIFIER_LENGTH];
            int field_index;
        } field_ptr;
        struct {
            MirValueId base_address;
            MirValueId index_value;
        } index_ptr;
        struct {
            MirOperandList elements;
        } array_literal;
        struct {
            MirFieldValueList fields;
        } struct_literal;
        struct {
            char enum_name[MIR_MAX_NAME_LENGTH];
            char variant_name[MAX_IDENTIFIER_LENGTH];
            int ordinal;
        } enum_literal;
        struct {
            MirValueId callee;
            MirOperandList arguments;
        } call;
    } data;
} MirInst;

typedef struct MirTerminator {
    MirTerminatorKind kind;
    union {
        struct {
            MirBlockId target;
        } br;
        struct {
            MirValueId condition;
            MirBlockId then_block;
            MirBlockId else_block;
        } cond_br;
        struct {
            bool has_value;
            MirValueId value;
        } ret;
    } data;
} MirTerminator;

typedef struct MirBlock {
    char name[MIR_MAX_NAME_LENGTH];
    MirInst *insts;
    int inst_count;
    MirTerminator terminator;
} MirBlock;

typedef struct MirCaptureDesc {
    char identifier[MAX_IDENTIFIER_LENGTH];
    ASTDataType *source_data_type;
    ASTDataType *runtime_data_type;
    bool by_reference;
    bool mutable_reference;
    MirValueId input_value;
} MirCaptureDesc;

typedef struct MirParamDesc {
    char identifier[MAX_IDENTIFIER_LENGTH];
    ASTDataType *source_data_type;
    ASTDataType *runtime_data_type;
    bool by_reference;
    MirValueId input_value;
} MirParamDesc;

typedef struct MirFunction {
    char name[MIR_MAX_NAME_LENGTH];
    ASTDataType *return_data_type;
    ASTDataType *closure_env_type;
    MirValueId closure_env_input;
    MirCaptureDesc *captures;
    int capture_count;
    MirParamDesc *parameters;
    int parameter_count;
    MirValueInfo *values;
    int value_count;
    MirBlock *blocks;
    int block_count;
    MirBlockId entry_block;
    ASTNode *source_function;
} MirFunction;

typedef enum MirGlobalKind {
    MIR_GLOBAL_VAR,
} MirGlobalKind;

typedef struct MirGlobal {
    MirGlobalKind kind;
    char name[MIR_MAX_NAME_LENGTH];
    ASTDataType *data_type;
    bool mutable;
} MirGlobal;

typedef struct MirExternFunction {
    char wrapper_name[MIR_MAX_NAME_LENGTH];
    char symbol_name[MIR_MAX_NAME_LENGTH];
    ASTDataType *function_type;
} MirExternFunction;

typedef struct MirProgram {
    MirGlobal *globals;
    int global_count;
    MirExternFunction *extern_functions;
    int extern_function_count;
    MirFunction *functions;
    int function_count;
} MirProgram;

typedef enum MirRuntimeBindingKind {
    MIR_RUNTIME_BINDING_NONE,
    MIR_RUNTIME_BINDING_LOCAL_SLOT,
    MIR_RUNTIME_BINDING_GLOBAL_SLOT,
    MIR_RUNTIME_BINDING_ALIAS_ADDRESS,
    MIR_RUNTIME_BINDING_COMPTIME_ONLY,
} MirRuntimeBindingKind;

typedef struct MirRuntimeBinding {
    char identifier[MAX_IDENTIFIER_LENGTH];
    MirRuntimeBindingKind kind;
    bool mutable;
    ASTDataType *declared_data_type;
    ASTDataType *type_value;
    ASTNode *function_value;
    MirValueId local_value;
    char global_name[MIR_MAX_NAME_LENGTH];
} MirRuntimeBinding;

typedef struct MirLowerScope {
    struct MirLowerScope *parent;
    ScopeFrame type_scope;
    MirRuntimeBinding bindings[1024];
    int binding_count;
    ASTDataType *self_data_type;
    bool declare_as_globals;
} MirLowerScope;

typedef struct MirDeferredStmt {
    ASTNode *statement;
    struct MirDeferredStmt *next;
} MirDeferredStmt;

typedef struct MirCleanupFrame {
    struct MirCleanupFrame *parent;
    MirDeferredStmt *deferred_head;
} MirCleanupFrame;

typedef struct MirLoopContext {
    struct MirLoopContext *parent;
    MirBlockId break_block;
    MirBlockId continue_block;
    MirCleanupFrame *cleanup_stop;
} MirLoopContext;

typedef struct MirLowering {
    MirProgram *program;
    int unique_function_counter;
    int unique_block_counter;
    int unique_global_counter;
    int unique_extern_counter;
} MirLowering;

typedef struct MirFunctionState {
    MirLowering *lowering;
    int function_index;
    MirBlockId current_block;
    MirCleanupFrame *cleanup_top;
    MirLoopContext *loop_top;
    bool is_top_level_init;
} MirFunctionState;

typedef struct MirMaybeValue {
    MirValueId value;
    bool valid;
} MirMaybeValue;

MirProgram* lowerASTToMIR(ASTNode *root);
void printMIRProgram(MirProgram *program);

static MirOperandList newMirOperandList(int count)
{
    MirOperandList list = {0};
    if(count > 0)
        list.items = (MirValueId*) malloc(sizeof(MirValueId) * count);
    list.count = count;
    return list;
}

static MirFieldValueList newMirFieldValueList(int count)
{
    MirFieldValueList list = {0};
    if(count > 0)
        list.items = (MirFieldValue*) malloc(sizeof(MirFieldValue) * count);
    list.count = count;
    return list;
}

static MirProgram* newMirProgram()
{
    MirProgram *program = (MirProgram*) malloc(sizeof(MirProgram));
    memset(program, 0, sizeof(MirProgram));
    return program;
}

static MirFunction* mirAppendFunction(MirProgram *program)
{
    program->functions = (MirFunction*) realloc(
        program->functions,
        sizeof(MirFunction) * (program->function_count + 1)
    );
    MirFunction *function = &(program->functions[program->function_count++]);
    memset(function, 0, sizeof(MirFunction));
    return function;
}

static MirExternFunction* mirAppendExternFunction(MirProgram *program)
{
    program->extern_functions = (MirExternFunction*) realloc(
        program->extern_functions,
        sizeof(MirExternFunction) * (program->extern_function_count + 1)
    );
    MirExternFunction *extern_function = &(program->extern_functions[program->extern_function_count++]);
    memset(extern_function, 0, sizeof(MirExternFunction));
    return extern_function;
}

static MirGlobal* mirAppendGlobal(MirProgram *program)
{
    program->globals = (MirGlobal*) realloc(
        program->globals,
        sizeof(MirGlobal) * (program->global_count + 1)
    );
    MirGlobal *global = &(program->globals[program->global_count++]);
    memset(global, 0, sizeof(MirGlobal));
    return global;
}

static MirBlock* mirAppendBlock(MirFunction *function)
{
    function->blocks = (MirBlock*) realloc(
        function->blocks,
        sizeof(MirBlock) * (function->block_count + 1)
    );
    MirBlock *block = &(function->blocks[function->block_count++]);
    memset(block, 0, sizeof(MirBlock));
    block->terminator.kind = MIR_TERM_NONE;
    return block;
}

static MirInst* mirAppendInst(MirBlock *block)
{
    block->insts = (MirInst*) realloc(
        block->insts,
        sizeof(MirInst) * (block->inst_count + 1)
    );
    MirInst *inst = &(block->insts[block->inst_count++]);
    memset(inst, 0, sizeof(MirInst));
    inst->result = -1;
    return inst;
}

static MirValueId mirAppendValue(MirFunction *function, ASTDataType *data_type, const char *name, bool is_input)
{
    function->values = (MirValueInfo*) realloc(
        function->values,
        sizeof(MirValueInfo) * (function->value_count + 1)
    );
    MirValueInfo *info = &(function->values[function->value_count]);
    memset(info, 0, sizeof(MirValueInfo));
    info->data_type = cloneDataType(data_type);
    info->is_input = is_input;
    if(name != NULL)
        strcpy(info->name, name);
    return function->value_count++;
}

static MirFunction* mirCurrentFunction(MirFunctionState *state)
{
    return &(state->lowering->program->functions[state->function_index]);
}

static const char* mirEnsureExternFunction(MirLowering *lowering, const char *symbol_name, ASTDataType *function_type,
                                           const char *filename, int line, int column)
{
    for(int i = 0; i < lowering->program->extern_function_count; i++)
    {
        MirExternFunction *extern_function = &(lowering->program->extern_functions[i]);
        if(strcmp(extern_function->symbol_name, symbol_name) == 0)
        {
            if(!isSameDataType(extern_function->function_type, function_type))
            {
                printf("MIR lowering error: extern symbol %s is declared with conflicting function types at file %s, line %d, column %d\n",
                       symbol_name, filename, line, column);
                exit(1);
            }
            return extern_function->wrapper_name;
        }
    }

    MirExternFunction *extern_function = mirAppendExternFunction(lowering->program);
    snprintf(extern_function->wrapper_name, sizeof(extern_function->wrapper_name),
             "__mote_extern_%d", lowering->unique_extern_counter++);
    strcpy(extern_function->symbol_name, symbol_name);
    extern_function->function_type = cloneDataType(function_type);
    return extern_function->wrapper_name;
}

static ASTDataType* mirGetValueType(MirFunctionState *state, MirValueId value)
{
    MirFunction *function = mirCurrentFunction(state);
    if(value < 0 || value >= function->value_count)
    {
        printf("MIR internal error: invalid value id %d\n", value);
        exit(1);
    }
    return function->values[value].data_type;
}

static bool mirIsValueTypeVoid(ASTDataType *data_type)
{
    return data_type != NULL &&
           data_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
           data_type->primary == AST_PRIMARY_DATA_TYPE_VOID;
}

static ASTDataType* mirResolvedExprValueType(ASTNode *node, ScopeFrame *scope)
{
    TypeSystemExprType expr_type = inferExprType(node, scope);
    if(expr_type.kind == TYPE_SYSTEM_EXPR_TYPE_LITERAL_INTEGER)
        return defaultIntegerDataType();
    if(expr_type.kind == TYPE_SYSTEM_EXPR_TYPE_LITERAL_FLOAT)
        return defaultFloatDataType();
    if(expr_type.kind == TYPE_SYSTEM_EXPR_TYPE_TYPE)
    {
        printf("MIR lowering error: type-valued expression cannot be lowered directly at file %s, line %d, column %d\n",
               node->filename, node->line_number, node->column_number);
        exit(1);
    }
    return cloneDataType(expr_type.data_type);
}

static ASTDataType* mirRuntimeParameterType(ASTDataType *source_type)
{
    if(source_type != NULL && source_type->kind == AST_DATA_TYPE_KIND_REFERENCE)
        return newWrappedDataType(AST_DATA_TYPE_KIND_POINTER, source_type->mutable, cloneDataType(source_type->child));
    return cloneDataType(source_type);
}

static MirRuntimeBinding* findMirRuntimeBindingInScope(MirLowerScope *scope, const char *identifier)
{
    for(int i = 0; i < scope->binding_count; i++)
    {
        if(strcmp(scope->bindings[i].identifier, identifier) == 0)
            return &(scope->bindings[i]);
    }
    return NULL;
}

static MirRuntimeBinding* findMirRuntimeBinding(MirLowerScope *scope, const char *identifier)
{
    MirLowerScope *current = scope;
    while(current)
    {
        MirRuntimeBinding *binding = findMirRuntimeBindingInScope(current, identifier);
        if(binding != NULL)
            return binding;
        current = current->parent;
    }
    return NULL;
}

static MirRuntimeBinding* declareMirRuntimeBinding(MirLowerScope *scope, const char *identifier)
{
    MirRuntimeBinding *binding = &(scope->bindings[scope->binding_count++]);
    memset(binding, 0, sizeof(MirRuntimeBinding));
    strcpy(binding->identifier, identifier);
    return binding;
}

static void initMirLowerScope(MirLowerScope *scope, MirLowerScope *parent, bool declare_as_globals)
{
    memset(scope, 0, sizeof(MirLowerScope));
    scope->parent = parent;
    initScopeFrame(&(scope->type_scope), parent == NULL ? NULL : &(parent->type_scope));
    scope->self_data_type = parent == NULL ? NULL : parent->self_data_type;
    scope->declare_as_globals = declare_as_globals;
}

static int countASTNodes(ASTNode *node)
{
    int count = 0;
    while(node)
    {
        count++;
        node = node->next;
    }
    return count;
}

static int countStructDataFields(ASTDataType *struct_type)
{
    int count = 0;
    ASTStructMember *member = struct_type->members;
    while(member)
    {
        if(member->value == NULL)
            count++;
        member = member->next;
    }
    return count;
}

static int findStructDataFieldIndex(ASTDataType *struct_type, const char *identifier)
{
    int index = 0;
    ASTStructMember *member = struct_type->members;
    while(member)
    {
        if(member->value == NULL)
        {
            if(strcmp(member->identifier, identifier) == 0)
                return index;
            index++;
        }
        member = member->next;
    }
    return -1;
}

static int findEnumVariantOrdinal(ASTDataType *enum_type, const char *identifier)
{
    int ordinal = 0;
    ASTEnumVariant *variant = enum_type->variants;
    while(variant)
    {
        if(strcmp(variant->identifier, identifier) == 0)
            return ordinal;
        ordinal++;
        variant = variant->next;
    }
    return -1;
}

static ASTDataType* mirClosureEnvPointerType(ASTDataType *env_type)
{
    return newWrappedDataType(AST_DATA_TYPE_KIND_POINTER, false, cloneDataType(env_type));
}

static ASTDataType* mirBuildClosureEnvType(MirCaptureDesc *captures, int capture_count)
{
    ASTStructMember *head = NULL;
    ASTStructMember *tail = NULL;

    for(int i = 0; i < capture_count; i++)
    {
        ASTStructMember *member = (ASTStructMember*) malloc(sizeof(ASTStructMember));
        memset(member, 0, sizeof(ASTStructMember));
        strcpy(member->identifier, captures[i].identifier);
        member->data_type = cloneDataType(captures[i].runtime_data_type);

        if(head == NULL)
            head = member;
        else
            tail->next = member;
        tail = member;
    }

    return newStructDataType("", head);
}

static bool functionHasTypeParameters(ASTFunctionParameter *parameter)
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

static bool mirCurrentBlockTerminated(MirFunctionState *state)
{
    return mirCurrentFunction(state)->blocks[state->current_block].terminator.kind != MIR_TERM_NONE;
}

static MirBlockId mirCreateBlock(MirLowering *lowering, MirFunction *function, const char *hint)
{
    MirBlock *block = mirAppendBlock(function);
    if(hint != NULL)
        snprintf(block->name, sizeof(block->name), "%s_%d", hint, lowering->unique_block_counter++);
    else
        snprintf(block->name, sizeof(block->name), "block_%d", lowering->unique_block_counter++);
    return function->block_count - 1;
}

static void mirSwitchToBlock(MirFunctionState *state, MirBlockId block_id)
{
    state->current_block = block_id;
}

static MirValueId mirCreateInput(MirFunction *function, ASTDataType *data_type, const char *name)
{
    return mirAppendValue(function, data_type, name, true);
}

static MirValueId mirEmitResultInst(MirFunctionState *state, MirInstKind kind, ASTDataType *result_type,
                                    const char *filename, int line_number, int column_number)
{
    MirFunction *function = mirCurrentFunction(state);
    MirBlock *block = &(function->blocks[state->current_block]);
    MirInst *inst = mirAppendInst(block);
    inst->kind = kind;
    inst->result_type = cloneDataType(result_type);
    inst->filename = filename;
    inst->line_number = line_number;
    inst->column_number = column_number;
    inst->result = mirAppendValue(function, result_type, NULL, false);
    return inst->result;
}

static MirInst* mirGetLastInst(MirFunctionState *state)
{
    MirBlock *block = &(mirCurrentFunction(state)->blocks[state->current_block]);
    return &(block->insts[block->inst_count - 1]);
}

static MirValueId mirEmitConstBool(MirFunctionState *state, bool value, const char *filename, int line, int column)
{
    MirValueId result = mirEmitResultInst(state, MIR_INST_CONST_BOOL,
                                          newPrimaryDataType(AST_PRIMARY_DATA_TYPE_BOOL),
                                          filename, line, column);
    mirGetLastInst(state)->data.const_bool.value = value;
    return result;
}

static MirValueId mirEmitConstChar(MirFunctionState *state, char value, const char *filename, int line, int column)
{
    MirValueId result = mirEmitResultInst(state, MIR_INST_CONST_CHAR,
                                          newPrimaryDataType(AST_PRIMARY_DATA_TYPE_CHAR),
                                          filename, line, column);
    mirGetLastInst(state)->data.const_char.value = value;
    return result;
}

static MirValueId mirEmitConstInt(MirFunctionState *state, long long int value, ASTDataType *data_type,
                                  const char *filename, int line, int column)
{
    MirValueId result = mirEmitResultInst(state, MIR_INST_CONST_INT, data_type, filename, line, column);
    mirGetLastInst(state)->data.const_int.value = value;
    return result;
}

static MirValueId mirEmitConstFloat(MirFunctionState *state, long double value, ASTDataType *data_type,
                                    const char *filename, int line, int column)
{
    MirValueId result = mirEmitResultInst(state, MIR_INST_CONST_FLOAT, data_type, filename, line, column);
    mirGetLastInst(state)->data.const_float.value = value;
    return result;
}

static MirValueId mirEmitConstString(MirFunctionState *state, const char *value, ASTDataType *data_type,
                                     const char *filename, int line, int column)
{
    MirValueId result = mirEmitResultInst(state, MIR_INST_CONST_STRING, data_type, filename, line, column);
    strcpy(mirGetLastInst(state)->data.const_string.value, value);
    return result;
}

static MirValueId mirEmitConvert(MirFunctionState *state, MirValueId operand, ASTDataType *target_type,
                                 const char *filename, int line, int column)
{
    if(isSameDataType(mirGetValueType(state, operand), target_type))
        return operand;

    MirValueId result = mirEmitResultInst(state, MIR_INST_CONVERT, target_type, filename, line, column);
    MirInst *inst = mirGetLastInst(state);
    inst->data.convert.operand = operand;
    inst->data.convert.target_type = cloneDataType(target_type);
    return result;
}

static MirValueId mirEmitUnary(MirFunctionState *state, MirInstKind kind, MirValueId operand, ASTDataType *result_type,
                               const char *filename, int line, int column)
{
    MirValueId result = mirEmitResultInst(state, kind, result_type, filename, line, column);
    mirGetLastInst(state)->data.unary.operand = operand;
    return result;
}

static MirValueId mirEmitBinary(MirFunctionState *state, MirInstKind kind, MirValueId lhs, MirValueId rhs,
                                ASTDataType *result_type, const char *filename, int line, int column)
{
    MirValueId result = mirEmitResultInst(state, kind, result_type, filename, line, column);
    MirInst *inst = mirGetLastInst(state);
    inst->data.binary.lhs = lhs;
    inst->data.binary.rhs = rhs;
    return result;
}

static MirValueId mirEmitAlloca(MirFunctionState *state, ASTDataType *alloca_type,
                                const char *filename, int line, int column)
{
    ASTDataType *pointer_type = newWrappedDataType(AST_DATA_TYPE_KIND_POINTER, true, cloneDataType(alloca_type));
    MirValueId result = mirEmitResultInst(state, MIR_INST_ALLOCA, pointer_type, filename, line, column);
    mirGetLastInst(state)->data.alloca_inst.alloca_type = cloneDataType(alloca_type);
    return result;
}

static MirValueId mirEmitLoad(MirFunctionState *state, MirValueId address, ASTDataType *loaded_type,
                              const char *filename, int line, int column)
{
    MirValueId result = mirEmitResultInst(state, MIR_INST_LOAD, loaded_type, filename, line, column);
    mirGetLastInst(state)->data.load.address = address;
    return result;
}

static void mirEmitStore(MirFunctionState *state, MirValueId address, MirValueId value,
                         const char *filename, int line, int column)
{
    MirBlock *block = &(mirCurrentFunction(state)->blocks[state->current_block]);
    MirInst *inst = mirAppendInst(block);
    inst->kind = MIR_INST_STORE;
    inst->filename = filename;
    inst->line_number = line;
    inst->column_number = column;
    inst->data.store.address = address;
    inst->data.store.value = value;
}

static MirValueId mirEmitGlobalAddr(MirFunctionState *state, const char *global_name, ASTDataType *global_type,
                                    const char *filename, int line, int column)
{
    MirValueId result = mirEmitResultInst(
        state,
        MIR_INST_GLOBAL_ADDR,
        newWrappedDataType(AST_DATA_TYPE_KIND_POINTER, true, cloneDataType(global_type)),
        filename, line, column
    );
    strcpy(mirGetLastInst(state)->data.global_addr.global_name, global_name);
    return result;
}

static MirValueId mirEmitFunctionRef(MirFunctionState *state, const char *function_name, ASTDataType *function_type,
                                     const char *filename, int line, int column)
{
    MirValueId result = mirEmitResultInst(state, MIR_INST_FUNCTION_REF, function_type, filename, line, column);
    strcpy(mirGetLastInst(state)->data.function_ref.function_name, function_name);
    return result;
}

static MirValueId mirEmitMakeClosure(MirFunctionState *state, const char *function_name, ASTDataType *function_type,
                                     ASTDataType *environment_type, MirOperandList captures,
                                     const char *filename, int line, int column)
{
    MirValueId result = mirEmitResultInst(state, MIR_INST_MAKE_CLOSURE, function_type, filename, line, column);
    MirInst *inst = mirGetLastInst(state);
    strcpy(inst->data.make_closure.function_name, function_name);
    inst->data.make_closure.environment_type = cloneDataType(environment_type);
    inst->data.make_closure.captures = captures;
    return result;
}

static MirValueId mirEmitFieldPtr(MirFunctionState *state, MirValueId base_address, ASTDataType *field_type,
                                  const char *identifier, int field_index,
                                  const char *filename, int line, int column)
{
    MirValueId result = mirEmitResultInst(
        state,
        MIR_INST_FIELD_PTR,
        newWrappedDataType(AST_DATA_TYPE_KIND_POINTER, true, cloneDataType(field_type)),
        filename, line, column
    );
    MirInst *inst = mirGetLastInst(state);
    inst->data.field_ptr.base_address = base_address;
    strcpy(inst->data.field_ptr.identifier, identifier);
    inst->data.field_ptr.field_index = field_index;
    return result;
}

static MirValueId mirEmitIndexPtr(MirFunctionState *state, MirValueId base_address, MirValueId index_value,
                                  ASTDataType *element_type, const char *filename, int line, int column)
{
    MirValueId result = mirEmitResultInst(
        state,
        MIR_INST_INDEX_PTR,
        newWrappedDataType(AST_DATA_TYPE_KIND_POINTER, true, cloneDataType(element_type)),
        filename, line, column
    );
    MirInst *inst = mirGetLastInst(state);
    inst->data.index_ptr.base_address = base_address;
    inst->data.index_ptr.index_value = index_value;
    return result;
}

static MirValueId mirEmitArrayLiteral(MirFunctionState *state, MirOperandList elements, ASTDataType *array_type,
                                      const char *filename, int line, int column)
{
    MirValueId result = mirEmitResultInst(state, MIR_INST_ARRAY_LITERAL, array_type, filename, line, column);
    mirGetLastInst(state)->data.array_literal.elements = elements;
    return result;
}

static MirValueId mirEmitStructLiteral(MirFunctionState *state, MirFieldValueList fields, ASTDataType *struct_type,
                                       const char *filename, int line, int column)
{
    MirValueId result = mirEmitResultInst(state, MIR_INST_STRUCT_LITERAL, struct_type, filename, line, column);
    mirGetLastInst(state)->data.struct_literal.fields = fields;
    return result;
}

static MirValueId mirEmitEnumLiteral(MirFunctionState *state, ASTDataType *enum_type, const char *enum_name,
                                     const char *variant_name, int ordinal,
                                     const char *filename, int line, int column)
{
    MirValueId result = mirEmitResultInst(state, MIR_INST_ENUM_LITERAL, enum_type, filename, line, column);
    MirInst *inst = mirGetLastInst(state);
    if(enum_name != NULL)
        strcpy(inst->data.enum_literal.enum_name, enum_name);
    strcpy(inst->data.enum_literal.variant_name, variant_name);
    inst->data.enum_literal.ordinal = ordinal;
    return result;
}

static MirValueId mirEmitCall(MirFunctionState *state, MirValueId callee, MirOperandList arguments, ASTDataType *return_type,
                              const char *filename, int line, int column)
{
    MirValueId result = -1;
    if(!mirIsValueTypeVoid(return_type))
        result = mirEmitResultInst(state, MIR_INST_CALL, return_type, filename, line, column);
    else
    {
        MirBlock *block = &(mirCurrentFunction(state)->blocks[state->current_block]);
        MirInst *inst = mirAppendInst(block);
        inst->kind = MIR_INST_CALL;
        inst->result = -1;
        inst->result_type = cloneDataType(return_type);
        inst->filename = filename;
        inst->line_number = line;
        inst->column_number = column;
    }

    MirInst *inst = mirGetLastInst(state);
    inst->data.call.callee = callee;
    inst->data.call.arguments = arguments;
    return result;
}

static void mirEmitBr(MirFunctionState *state, MirBlockId target)
{
    mirCurrentFunction(state)->blocks[state->current_block].terminator.kind = MIR_TERM_BR;
    mirCurrentFunction(state)->blocks[state->current_block].terminator.data.br.target = target;
}

static void mirEmitCondBr(MirFunctionState *state, MirValueId condition, MirBlockId then_block, MirBlockId else_block)
{
    MirTerminator *term = &(mirCurrentFunction(state)->blocks[state->current_block].terminator);
    term->kind = MIR_TERM_COND_BR;
    term->data.cond_br.condition = condition;
    term->data.cond_br.then_block = then_block;
    term->data.cond_br.else_block = else_block;
}

static void mirEmitRetVoid(MirFunctionState *state)
{
    mirCurrentFunction(state)->blocks[state->current_block].terminator.kind = MIR_TERM_RET;
    mirCurrentFunction(state)->blocks[state->current_block].terminator.data.ret.has_value = false;
}

static void mirEmitRetValue(MirFunctionState *state, MirValueId value)
{
    mirCurrentFunction(state)->blocks[state->current_block].terminator.kind = MIR_TERM_RET;
    mirCurrentFunction(state)->blocks[state->current_block].terminator.data.ret.has_value = true;
    mirCurrentFunction(state)->blocks[state->current_block].terminator.data.ret.value = value;
}

static MirValueId lowerExprAsValue(MirFunctionState *state, MirLowerScope *scope, ASTNode *node, ASTDataType *expected_type);
static MirValueId lowerExprAsAddress(MirFunctionState *state, MirLowerScope *scope, ASTNode *node);
static void lowerStatement(MirFunctionState *state, MirLowerScope *scope, ASTNode *node);

static MirValueId mirMaybeConvertValue(MirFunctionState *state, MirLowerScope *scope, ASTNode *node,
                                       MirValueId value, ASTDataType *target_type)
{
    if(target_type == NULL)
        return value;
    ASTDataType *value_type = mirGetValueType(state, value);
    if(isSameDataType(value_type, target_type))
        return value;
    TypeSystemExprType source_type = newValueExprType(value_type);
    if(canImplicitConvertDataType(source_type, node, target_type))
        return mirEmitConvert(state, value, target_type, node->filename, node->line_number, node->column_number);
    return value;
}

static MirValueId mirBindingAddress(MirFunctionState *state, MirRuntimeBinding *binding, ASTNode *use_node)
{
    if(binding->kind == MIR_RUNTIME_BINDING_ALIAS_ADDRESS || binding->kind == MIR_RUNTIME_BINDING_LOCAL_SLOT)
        return binding->local_value;
    if(binding->kind == MIR_RUNTIME_BINDING_GLOBAL_SLOT)
        return mirEmitGlobalAddr(state, binding->global_name, binding->declared_data_type,
                                 use_node->filename, use_node->line_number, use_node->column_number);

    printf("MIR lowering error: identifier %s has no runtime address at file %s, line %d, column %d\n",
           binding->identifier, use_node->filename, use_node->line_number, use_node->column_number);
    exit(1);
}

static MirValueId lowerVariableValue(MirFunctionState *state, MirLowerScope *scope, ASTNode *node, ASTDataType *expected_type)
{
    MirRuntimeBinding *binding = findMirRuntimeBinding(scope, node->identifier);
    if(binding == NULL || binding->kind == MIR_RUNTIME_BINDING_COMPTIME_ONLY)
    {
        printf("MIR lowering error: variable %s is compile-time only or unavailable at file %s, line %d, column %d\n",
               node->identifier, node->filename, node->line_number, node->column_number);
        exit(1);
    }

    ASTDataType *expr_type = mirResolvedExprValueType(node, &(scope->type_scope));
    MirValueId address = mirBindingAddress(state, binding, node);
    MirValueId value = mirEmitLoad(state, address, expr_type, node->filename, node->line_number, node->column_number);
    return mirMaybeConvertValue(state, scope, node, value, expected_type);
}

static MirValueId lowerExprMaterializedAddress(MirFunctionState *state, MirLowerScope *scope, ASTNode *node)
{
    MirValueId value = lowerExprAsValue(state, scope, node, NULL);
    ASTDataType *value_type = mirGetValueType(state, value);
    MirValueId slot = mirEmitAlloca(state, value_type, node->filename, node->line_number, node->column_number);
    mirEmitStore(state, slot, value, node->filename, node->line_number, node->column_number);
    return slot;
}

static void appendDeferredStatement(MirCleanupFrame *cleanup, ASTNode *statement)
{
    MirDeferredStmt *entry = (MirDeferredStmt*) malloc(sizeof(MirDeferredStmt));
    entry->statement = statement;
    entry->next = cleanup->deferred_head;
    cleanup->deferred_head = entry;
}

static void emitCleanupRange(MirFunctionState *state, MirLowerScope *scope,
                             MirCleanupFrame *from, MirCleanupFrame *stop_exclusive)
{
    MirCleanupFrame *frame = from;
    while(frame != stop_exclusive)
    {
        MirDeferredStmt *deferred = frame->deferred_head;
        while(deferred)
        {
            lowerStatement(state, scope, deferred->statement);
            deferred = deferred->next;
            if(mirCurrentBlockTerminated(state))
                return;
        }
        frame = frame->parent;
    }
}

static MirGlobal* mirEnsureGlobal(MirLowering *lowering, const char *name, ASTDataType *data_type, bool mutable)
{
    for(int i = 0; i < lowering->program->global_count; i++)
    {
        if(strcmp(lowering->program->globals[i].name, name) == 0)
            return &(lowering->program->globals[i]);
    }
    MirGlobal *global = mirAppendGlobal(lowering->program);
    global->kind = MIR_GLOBAL_VAR;
    strcpy(global->name, name);
    global->data_type = cloneDataType(data_type);
    global->mutable = mutable;
    return global;
}

static void mirDeclareVariableInfo(MirLowerScope *scope, ASTNode *assign_node, ASTDataType *declared_type,
                                   TypeSystemExprType expr_type)
{
    VariableInfo *variable_info = declareVariableInfo(&(scope->type_scope), assign_node->identifier);
    variable_info->mutable = assign_node->modifier.mutable;
    variable_info->data_type = cloneDataType(declared_type);
    if(expr_type.kind == TYPE_SYSTEM_EXPR_TYPE_TYPE)
        variable_info->type_value = cloneDataType(expr_type.data_type);
    if(assign_node->rhs != NULL && assign_node->rhs->kind == AST_EXPR_FUNCTION)
        variable_info->function_value = assign_node->rhs;
}

static MirLowerScope* instantiateFunctionCallScope(MirFunctionState *state, MirLowerScope *outer_scope,
                                                   ASTNode *function_value, ASTNode *call_arguments,
                                                   ASTDataType *self_data_type)
{
    MirLowerScope *inst_scope = (MirLowerScope*) malloc(sizeof(MirLowerScope));
    initMirLowerScope(inst_scope, outer_scope, false);
    inst_scope->self_data_type = self_data_type != NULL ? self_data_type : outer_scope->self_data_type;

    if(self_data_type != NULL)
    {
        VariableInfo *self_variable = declareVariableInfo(&(inst_scope->type_scope), "Self");
        self_variable->mutable = false;
        self_variable->data_type = newPrimaryDataType(AST_PRIMARY_DATA_TYPE_TYPE);
        self_variable->type_value = cloneDataType(self_data_type);
    }

    ASTFunctionCapture *capture = function_value->captures;
    while(capture)
    {
        VariableInfo *outer_variable = findVariableInfo(&(outer_scope->type_scope), capture->identifier);
        if(outer_variable != NULL)
        {
            VariableInfo *inst_variable = declareVariableInfo(&(inst_scope->type_scope), capture->identifier);
            inst_variable->mutable = false;
            inst_variable->data_type = cloneDataType(outer_variable->data_type);
            inst_variable->type_value = cloneDataType(outer_variable->type_value);
            inst_variable->function_value = outer_variable->function_value;

            MirRuntimeBinding *outer_binding = findMirRuntimeBinding(outer_scope, capture->identifier);
            if(outer_binding != NULL)
            {
                MirRuntimeBinding *inst_binding = declareMirRuntimeBinding(inst_scope, capture->identifier);
                *inst_binding = *outer_binding;
            }
        }
        capture = capture->next;
    }

    ASTFunctionParameter *parameter = function_value->parameters;
    ASTNode *argument = call_arguments;
    while(parameter && argument)
    {
        ASTDataType *resolved_parameter_type = resolveNamedDataType(parameter->data_type, &(inst_scope->type_scope), self_data_type);
        VariableInfo *inst_variable = declareVariableInfo(&(inst_scope->type_scope), parameter->identifier);
        inst_variable->mutable = false;
        inst_variable->data_type = cloneDataType(resolved_parameter_type);

        if(resolved_parameter_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
           resolved_parameter_type->primary == AST_PRIMARY_DATA_TYPE_TYPE)
        {
            TypeSystemExprType argument_type = inferExprType(argument, &(outer_scope->type_scope));
            inst_variable->type_value = cloneDataType(argument_type.data_type);
        }
        else
        {
            if(argument->kind == AST_EXPR_FUNCTION)
                inst_variable->function_value = argument;
            else if(argument->kind == AST_EXPR_VARIABLE)
            {
                VariableInfo *outer_variable = findVariableInfo(&(outer_scope->type_scope), argument->identifier);
                if(outer_variable != NULL)
                {
                    inst_variable->type_value = cloneDataType(outer_variable->type_value);
                    inst_variable->function_value = outer_variable->function_value;
                }
            }
        }

        if(resolved_parameter_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
           resolved_parameter_type->primary == AST_PRIMARY_DATA_TYPE_TYPE)
        {
        }
        else if(resolved_parameter_type->kind == AST_DATA_TYPE_KIND_REFERENCE)
        {
            MirRuntimeBinding *binding = declareMirRuntimeBinding(inst_scope, parameter->identifier);
            binding->kind = MIR_RUNTIME_BINDING_ALIAS_ADDRESS;
            binding->mutable = false;
            binding->declared_data_type = cloneDataType(resolved_parameter_type);
            binding->local_value = lowerExprAsAddress(state, outer_scope, argument);
        }
        else
        {
            MirValueId argument_value = lowerExprAsValue(state, outer_scope, argument, resolved_parameter_type);
            MirValueId slot = mirEmitAlloca(state, resolved_parameter_type,
                                            argument->filename, argument->line_number, argument->column_number);
            mirEmitStore(state, slot, argument_value, argument->filename, argument->line_number, argument->column_number);

            MirRuntimeBinding *binding = declareMirRuntimeBinding(inst_scope, parameter->identifier);
            binding->kind = MIR_RUNTIME_BINDING_LOCAL_SLOT;
            binding->mutable = false;
            binding->declared_data_type = cloneDataType(resolved_parameter_type);
            binding->local_value = slot;
        }

        parameter = parameter->next;
        argument = argument->next;
    }

    if(parameter != NULL || argument != NULL)
    {
        printf("MIR lowering error: function call argument count mismatch during instantiation\n");
        exit(1);
    }

    return inst_scope;
}

static MirMaybeValue tryLowerComptimeFunctionCall(MirFunctionState *state, MirLowerScope *scope, ASTNode *call_node)
{
    MirMaybeValue result = {0};
    if(call_node->lhs == NULL || call_node->lhs->kind != AST_EXPR_VARIABLE)
        return result;

    VariableInfo *callee_variable = findVariableInfo(&(scope->type_scope), call_node->lhs->identifier);
    if(callee_variable == NULL || callee_variable->function_value == NULL)
        return result;

    ASTNode *returned_expr = findReturnedExpr(callee_variable->function_value);
    if(returned_expr == NULL)
        return result;

    MirRuntimeBinding *callee_binding = findMirRuntimeBinding(scope, call_node->lhs->identifier);
    bool callee_is_comptime_only = callee_binding != NULL &&
                                   callee_binding->kind == MIR_RUNTIME_BINDING_COMPTIME_ONLY;
    if(!callee_is_comptime_only && returned_expr->kind != AST_EXPR_FUNCTION)
        return result;

    MirLowerScope *inst_scope = instantiateFunctionCallScope(
        state,
        scope,
        callee_variable->function_value,
        call_node->rhs,
        NULL
    );

    result.value = lowerExprAsValue(state, inst_scope, returned_expr, NULL);
    result.valid = true;
    return result;
}

static void bindSpecializedNamedTypes(MirLowerScope *scope, ASTDataType *source_type, ASTDataType *resolved_type)
{
    if(source_type == NULL || resolved_type == NULL)
        return;

    if(source_type->kind == AST_DATA_TYPE_KIND_NAMED)
    {
        ASTDataType *builtin_type = builtinIdentifierToDataType(source_type->identifier);
        bool same_named = resolved_type->kind == AST_DATA_TYPE_KIND_NAMED &&
                          strcmp(source_type->identifier, resolved_type->identifier) == 0;
        if(builtin_type == NULL &&
           strcmp(source_type->identifier, "Self") != 0 &&
           !same_named &&
           findVariableInfo(&(scope->type_scope), source_type->identifier) == NULL &&
           findTypeInfo(&(scope->type_scope), source_type->identifier) == NULL)
        {
            VariableInfo *type_variable = declareVariableInfo(&(scope->type_scope), source_type->identifier);
            type_variable->mutable = false;
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
            bindSpecializedNamedTypes(scope, source_type->child, resolved_type->child);
            return;
        case AST_DATA_TYPE_KIND_FUNCTION: {
            ASTFunctionParameter *source_parameter = source_type->parameters;
            ASTFunctionParameter *resolved_parameter = resolved_type->parameters;
            while(source_parameter != NULL && resolved_parameter != NULL)
            {
                bindSpecializedNamedTypes(scope, source_parameter->data_type, resolved_parameter->data_type);
                source_parameter = source_parameter->next;
                resolved_parameter = resolved_parameter->next;
            }
            bindSpecializedNamedTypes(scope, source_type->return_data_type, resolved_type->return_data_type);
            return;
        }
        case AST_DATA_TYPE_KIND_APPLY: {
            bindSpecializedNamedTypes(scope, source_type->callee, resolved_type->callee);
            ASTTypeArgument *source_argument = source_type->arguments;
            ASTTypeArgument *resolved_argument = resolved_type->arguments;
            while(source_argument != NULL && resolved_argument != NULL)
            {
                bindSpecializedNamedTypes(scope, source_argument->data_type, resolved_argument->data_type);
                source_argument = source_argument->next;
                resolved_argument = resolved_argument->next;
            }
            return;
        }
        case AST_DATA_TYPE_KIND_STRUCT: {
            ASTStructMember *source_member = source_type->members;
            ASTStructMember *resolved_member = resolved_type->members;
            while(source_member != NULL && resolved_member != NULL)
            {
                bindSpecializedNamedTypes(scope, source_member->data_type, resolved_member->data_type);
                source_member = source_member->next;
                resolved_member = resolved_member->next;
            }
            return;
        }
        default:
            return;
    }
}

static int lowerFunctionExprDefinition(MirLowering *lowering, MirLowerScope *scope, ASTNode *function_expr,
                                       const char *name_hint, ASTDataType *self_data_type);

static MirValueId lowerFunctionExprAsValue(MirFunctionState *state, MirLowerScope *scope, ASTNode *function_expr,
                                           const char *name_hint, ASTDataType *self_data_type)
{
    if(functionHasTypeParameters(function_expr->parameters))
    {
        printf("MIR lowering error: generic function value requires compile-time specialization before runtime lowering at file %s, line %d, column %d\n",
               function_expr->filename, function_expr->line_number, function_expr->column_number);
        exit(1);
    }

    int function_index = lowerFunctionExprDefinition(state->lowering, scope, function_expr, name_hint, self_data_type);
    MirFunction *mir_function = &(state->lowering->program->functions[function_index]);

    int runtime_capture_count = 0;
    ASTFunctionCapture *capture = function_expr->captures;
    while(capture)
    {
        VariableInfo *variable_info = findVariableInfo(&(scope->type_scope), capture->identifier);
        MirRuntimeBinding *binding = findMirRuntimeBinding(scope, capture->identifier);
        if(variable_info != NULL && binding != NULL)
            runtime_capture_count++;
        capture = capture->next;
    }

    MirOperandList captures = newMirOperandList(runtime_capture_count);
    int capture_index = 0;
    capture = function_expr->captures;
    while(capture)
    {
        VariableInfo *variable_info = findVariableInfo(&(scope->type_scope), capture->identifier);
        MirRuntimeBinding *binding = findMirRuntimeBinding(scope, capture->identifier);
        if(variable_info != NULL && binding != NULL)
        {
            if(capture->kind == AST_FUNCTION_CAPTURE_VALUE)
            {
                ASTNode fake_var = {0};
                fake_var.kind = AST_EXPR_VARIABLE;
                fake_var.filename = capture->filename;
                fake_var.line_number = capture->line_number;
                fake_var.column_number = capture->column_number;
                strcpy(fake_var.identifier, capture->identifier);
                captures.items[capture_index++] = lowerVariableValue(state, scope, &fake_var, NULL);
            }
            else
                captures.items[capture_index++] = mirBindingAddress(state, binding, function_expr);
        }
        capture = capture->next;
    }

    ASTDataType *function_type = resolveNamedDataType(function_expr->data_type, &(scope->type_scope), self_data_type);
    return mirEmitMakeClosure(state, mir_function->name, function_type, mir_function->closure_env_type, captures,
                              function_expr->filename, function_expr->line_number, function_expr->column_number);
}

static int lowerFunctionExprDefinition(MirLowering *lowering, MirLowerScope *scope, ASTNode *function_expr,
                                       const char *name_hint, ASTDataType *self_data_type)
{
    MirFunction *function = mirAppendFunction(lowering->program);
    int function_index = lowering->program->function_count - 1;
    if(name_hint != NULL)
        snprintf(function->name, sizeof(function->name), "%s_%d", name_hint, lowering->unique_function_counter++);
    else
        snprintf(function->name, sizeof(function->name), "lambda_%d", lowering->unique_function_counter++);
    function->source_function = function_expr;
    function->return_data_type = resolveNamedDataType(function_expr->return_data_type, &(scope->type_scope), self_data_type);
    function->closure_env_input = -1;
    function->entry_block = mirCreateBlock(lowering, function, "entry");

    MirLowerScope *function_scope = (MirLowerScope*) malloc(sizeof(MirLowerScope));
    initMirLowerScope(function_scope, scope, false);
    function_scope->self_data_type = self_data_type;

    if(self_data_type != NULL)
    {
        VariableInfo *self_variable = declareVariableInfo(&(function_scope->type_scope), "Self");
        self_variable->mutable = false;
        self_variable->data_type = newPrimaryDataType(AST_PRIMARY_DATA_TYPE_TYPE);
        self_variable->type_value = cloneDataType(self_data_type);
    }

    ASTFunctionCapture *capture = function_expr->captures;
    while(capture)
    {
        VariableInfo *outer_variable = findVariableInfo(&(scope->type_scope), capture->identifier);
        if(outer_variable != NULL)
        {
            VariableInfo *capture_variable = declareVariableInfo(&(function_scope->type_scope), capture->identifier);
            capture_variable->mutable = false;
            capture_variable->data_type = cloneDataType(outer_variable->data_type);
            capture_variable->type_value = cloneDataType(outer_variable->type_value);
            capture_variable->function_value = outer_variable->function_value;
        }
        capture = capture->next;
    }

    MirFunctionState state = {0};
    state.lowering = lowering;
    state.function_index = function_index;
    state.current_block = function->entry_block;

    capture = function_expr->captures;
    while(capture)
    {
        VariableInfo *outer_variable = findVariableInfo(&(scope->type_scope), capture->identifier);
        MirRuntimeBinding *outer_binding = findMirRuntimeBinding(scope, capture->identifier);
        if(outer_variable != NULL && outer_binding != NULL)
        {
            function->captures = (MirCaptureDesc*) realloc(
                function->captures,
                sizeof(MirCaptureDesc) * (function->capture_count + 1)
            );
            MirCaptureDesc *desc = &(function->captures[function->capture_count++]);
            memset(desc, 0, sizeof(MirCaptureDesc));
            strcpy(desc->identifier, capture->identifier);
            desc->source_data_type = cloneDataType(outer_variable->data_type);
            desc->by_reference = capture->kind != AST_FUNCTION_CAPTURE_VALUE;
            desc->mutable_reference = capture->kind == AST_FUNCTION_CAPTURE_MUT_REFERENCE;
            desc->runtime_data_type = desc->by_reference
                ? newWrappedDataType(AST_DATA_TYPE_KIND_POINTER, desc->mutable_reference,
                                     cloneDataType(getReferenceTargetType(outer_variable->data_type)))
                : cloneDataType(getReferenceTargetType(outer_variable->data_type));
            desc->input_value = -1;
        }
        capture = capture->next;
    }

    if(function->capture_count > 0)
    {
        function->closure_env_type = mirBuildClosureEnvType(function->captures, function->capture_count);
        function->closure_env_input = mirCreateInput(
            function,
            mirClosureEnvPointerType(function->closure_env_type),
            "__env"
        );

        for(int i = 0; i < function->capture_count; i++)
        {
            MirCaptureDesc *desc = &(function->captures[i]);
            MirRuntimeBinding *binding = declareMirRuntimeBinding(function_scope, desc->identifier);
            binding->mutable = false;
            binding->declared_data_type = cloneDataType(desc->source_data_type);
            binding->kind = MIR_RUNTIME_BINDING_ALIAS_ADDRESS;

            MirValueId field_address = mirEmitFieldPtr(
                &state,
                function->closure_env_input,
                desc->runtime_data_type,
                desc->identifier,
                i,
                function_expr->filename,
                function_expr->line_number,
                function_expr->column_number
            );

            if(desc->by_reference)
            {
                binding->local_value = mirEmitLoad(
                    &state,
                    field_address,
                    desc->runtime_data_type,
                    function_expr->filename,
                    function_expr->line_number,
                    function_expr->column_number
                );
            }
            else
                binding->local_value = field_address;
        }
    }

    ASTFunctionParameter *parameter = function_expr->parameters;
    while(parameter)
    {
        ASTDataType *resolved_type = resolveNamedDataType(parameter->data_type, &(function_scope->type_scope), self_data_type);
        VariableInfo *variable_info = declareVariableInfo(&(function_scope->type_scope), parameter->identifier);
        variable_info->mutable = false;
        variable_info->data_type = cloneDataType(resolved_type);
        if(resolved_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
           resolved_type->primary == AST_PRIMARY_DATA_TYPE_TYPE)
        {
            variable_info->type_value = newNamedDataType(parameter->identifier);
            parameter = parameter->next;
            continue;
        }

        function->parameters = (MirParamDesc*) realloc(
            function->parameters,
            sizeof(MirParamDesc) * (function->parameter_count + 1)
        );
        MirParamDesc *desc = &(function->parameters[function->parameter_count++]);
        memset(desc, 0, sizeof(MirParamDesc));
        strcpy(desc->identifier, parameter->identifier);
        desc->source_data_type = cloneDataType(resolved_type);
        desc->runtime_data_type = mirRuntimeParameterType(resolved_type);
        desc->by_reference = resolved_type->kind == AST_DATA_TYPE_KIND_REFERENCE;
        desc->input_value = mirCreateInput(function, desc->runtime_data_type, desc->identifier);

        MirRuntimeBinding *binding = declareMirRuntimeBinding(function_scope, desc->identifier);
        binding->mutable = false;
        binding->declared_data_type = cloneDataType(resolved_type);
        if(desc->by_reference)
        {
            binding->kind = MIR_RUNTIME_BINDING_ALIAS_ADDRESS;
            binding->local_value = desc->input_value;
        }
        else
        {
            binding->kind = MIR_RUNTIME_BINDING_LOCAL_SLOT;
            binding->local_value = mirEmitAlloca(&state, resolved_type,
                                                 parameter->filename, parameter->line_number, parameter->column_number);
            mirEmitStore(&state, binding->local_value, desc->input_value,
                         parameter->filename, parameter->line_number, parameter->column_number);
        }

        parameter = parameter->next;
    }

    MirCleanupFrame function_cleanup = {0};
    state.cleanup_top = &function_cleanup;
    state.loop_top = NULL;

    lowerStatement(&state, function_scope, function_expr->body);

    if(!mirCurrentBlockTerminated(&state))
    {
        MirFunction *lowered_function = mirCurrentFunction(&state);
        if(mirIsValueTypeVoid(lowered_function->return_data_type))
            mirEmitRetVoid(&state);
        else
        {
            printf("MIR lowering error: function %s may fall through without return\n", lowered_function->name);
            exit(1);
        }
    }

    return function_index;
}

static MirValueId lowerMethodFunctionValue(MirFunctionState *state, MirLowerScope *scope, ASTNode *member_node,
                                           ASTDataType *struct_type)
{
    ASTStructMember *member = findStructMember(struct_type, member_node->identifier);
    if(member == NULL || member->value == NULL)
    {
        printf("MIR lowering error: unknown method %s at file %s, line %d, column %d\n",
               member_node->identifier, member_node->filename, member_node->line_number, member_node->column_number);
        exit(1);
    }

    char hint[MIR_MAX_NAME_LENGTH] = {0};
    if(struct_type->identifier[0] != '\0')
        snprintf(hint, sizeof(hint), "%s_%s", struct_type->identifier, member_node->identifier);
    else
        snprintf(hint, sizeof(hint), "method_%s", member_node->identifier);

    MirLowerScope *method_scope = scope;
    if(member_node->lhs != NULL &&
       member_node->lhs->kind == AST_EXPR_CALL &&
       member_node->lhs->lhs != NULL &&
       member_node->lhs->lhs->kind == AST_EXPR_VARIABLE)
    {
        VariableInfo *callee_variable = findVariableInfo(&(scope->type_scope), member_node->lhs->lhs->identifier);
        if(callee_variable != NULL && callee_variable->function_value != NULL)
        {
            method_scope = instantiateFunctionCallScope(
                state,
                scope,
                callee_variable->function_value,
                member_node->lhs->rhs,
                struct_type
            );
        }
    }

    if(member->value->data_type != NULL && member->data_type != NULL)
        bindSpecializedNamedTypes(method_scope, member->value->data_type, member->data_type);

    return lowerFunctionExprAsValue(state, method_scope, member->value, hint, struct_type);
}

static MirValueId lowerLogicalShortCircuit(MirFunctionState *state, MirLowerScope *scope, ASTNode *node,
                                           bool is_and)
{
    MirValueId result_slot = mirEmitAlloca(state, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_BOOL),
                                           node->filename, node->line_number, node->column_number);
    MirValueId lhs = lowerExprAsValue(state, scope, node->lhs, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_BOOL));

    MirFunction *function = mirCurrentFunction(state);
    MirBlockId rhs_block = mirCreateBlock(state->lowering, function, is_and ? "logical_and_rhs" : "logical_or_rhs");
    MirBlockId short_block = mirCreateBlock(state->lowering, function, is_and ? "logical_and_short" : "logical_or_short");
    MirBlockId end_block = mirCreateBlock(state->lowering, function, is_and ? "logical_and_end" : "logical_or_end");

    if(is_and)
        mirEmitCondBr(state, lhs, rhs_block, short_block);
    else
        mirEmitCondBr(state, lhs, short_block, rhs_block);

    mirSwitchToBlock(state, short_block);
    MirValueId short_value = mirEmitConstBool(state, !is_and, node->filename, node->line_number, node->column_number);
    mirEmitStore(state, result_slot, short_value, node->filename, node->line_number, node->column_number);
    mirEmitBr(state, end_block);

    mirSwitchToBlock(state, rhs_block);
    MirValueId rhs = lowerExprAsValue(state, scope, node->rhs, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_BOOL));
    mirEmitStore(state, result_slot, rhs, node->filename, node->line_number, node->column_number);
    mirEmitBr(state, end_block);

    mirSwitchToBlock(state, end_block);
    return mirEmitLoad(state, result_slot, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_BOOL),
                       node->filename, node->line_number, node->column_number);
}

static MirValueId lowerExternBuiltinExpr(MirFunctionState *state, MirLowerScope *scope, ASTNode *node,
                                         ASTDataType *expected_type)
{
    ASTDataType *function_type = mirResolvedExprValueType(node, &(scope->type_scope));
    const char *wrapper_name = mirEnsureExternFunction(
        state->lowering,
        node->lhs->literal_string,
        function_type,
        node->filename,
        node->line_number,
        node->column_number
    );

    MirValueId function_ref = mirEmitFunctionRef(state, wrapper_name, function_type,
                                                 node->filename, node->line_number, node->column_number);
    return mirMaybeConvertValue(state, scope, node, function_ref, expected_type);
}

static ASTDataType* inferComparisonOperandType(ASTNode *lhs, ASTNode *rhs, ScopeFrame *scope)
{
    TypeSystemExprType lhs_type = inferExprType(lhs, scope);
    TypeSystemExprType rhs_type = inferExprType(rhs, scope);

    if(lhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE && rhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE &&
       isSameDataType(lhs_type.data_type, rhs_type.data_type))
        return cloneDataType(lhs_type.data_type);

    if(lhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_LITERAL_INTEGER || lhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_LITERAL_FLOAT)
    {
        if(rhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE)
            return cloneDataType(rhs_type.data_type);
    }

    if(rhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_LITERAL_INTEGER || rhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_LITERAL_FLOAT)
    {
        if(lhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE)
            return cloneDataType(lhs_type.data_type);
    }

    if(lhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE)
        return cloneDataType(lhs_type.data_type);
    if(rhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE)
        return cloneDataType(rhs_type.data_type);
    return defaultIntegerDataType();
}

static MirValueId lowerCallExpr(MirFunctionState *state, MirLowerScope *scope, ASTNode *node, ASTDataType *expected_type)
{
    MirMaybeValue comptime_call = tryLowerComptimeFunctionCall(state, scope, node);
    if(comptime_call.valid)
        return mirMaybeConvertValue(state, scope, node, comptime_call.value, expected_type);

    ASTNode *callee_expr = node->lhs;
    MirValueId callee = -1;
    MirOperandList arguments = {0};
    ASTDataType *return_type = mirResolvedExprValueType(node, &(scope->type_scope));

    if(callee_expr->kind == AST_EXPR_MEMBER)
    {
        ASTNode *member_node = callee_expr;
        TypeSystemExprType owner_type = inferExprType(member_node->lhs, &(scope->type_scope));
        ASTDataType *struct_type = NULL;
        bool through_type = owner_type.kind == TYPE_SYSTEM_EXPR_TYPE_TYPE;
        if(through_type)
            struct_type = owner_type.data_type;
        else if(owner_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE)
        {
            struct_type = owner_type.data_type;
            if(struct_type->kind == AST_DATA_TYPE_KIND_POINTER || struct_type->kind == AST_DATA_TYPE_KIND_REFERENCE)
                struct_type = struct_type->child;
        }

        if(isStructDataType(struct_type))
        {
            ASTStructMember *member = findStructMember(struct_type, member_node->identifier);
            if(member != NULL && member->value != NULL && member->data_type != NULL &&
               member->data_type->kind == AST_DATA_TYPE_KIND_FUNCTION)
            {
                callee = lowerMethodFunctionValue(state, scope, member_node, struct_type);
                int receiver_count = 0;
                ASTFunctionParameter *parameter = member->data_type->parameters;
                if(!through_type && parameter != NULL &&
                   canBindMethodReceiver(member_node->lhs, &(scope->type_scope), parameter->data_type, struct_type))
                    receiver_count = 1;

                int arg_count = countASTNodes(node->rhs) + receiver_count;
                arguments = newMirOperandList(arg_count);
                int index = 0;
                if(receiver_count == 1)
                {
                    if(parameter->data_type->kind == AST_DATA_TYPE_KIND_REFERENCE)
                        arguments.items[index++] = lowerExprAsAddress(state, scope, member_node->lhs);
                    else if(parameter->data_type->kind == AST_DATA_TYPE_KIND_POINTER)
                    {
                        TypeSystemExprType receiver_type = inferExprType(member_node->lhs, &(scope->type_scope));
                        if(receiver_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE &&
                           receiver_type.data_type->kind == AST_DATA_TYPE_KIND_POINTER &&
                           canImplicitConvertDataType(receiver_type, member_node->lhs, parameter->data_type))
                            arguments.items[index++] = lowerExprAsValue(state, scope, member_node->lhs, parameter->data_type);
                        else
                            arguments.items[index++] = lowerExprAsAddress(state, scope, member_node->lhs);
                    }
                    else
                        arguments.items[index++] = lowerExprAsValue(state, scope, member_node->lhs, parameter->data_type);
                    parameter = parameter->next;
                }

                ASTNode *argument_node = node->rhs;
                while(argument_node)
                {
                    ASTDataType *parameter_type = parameter == NULL ? NULL : parameter->data_type;
                    if(parameter_type != NULL && parameter_type->kind == AST_DATA_TYPE_KIND_REFERENCE)
                        arguments.items[index++] = lowerExprAsAddress(state, scope, argument_node);
                    else
                        arguments.items[index++] = lowerExprAsValue(state, scope, argument_node, parameter_type);
                    if(parameter != NULL)
                        parameter = parameter->next;
                    argument_node = argument_node->next;
                }

                MirValueId call_value = mirEmitCall(state, callee, arguments, return_type,
                                                    node->filename, node->line_number, node->column_number);
                if(mirIsValueTypeVoid(return_type))
                    return call_value;
                return mirMaybeConvertValue(state, scope, node, call_value, expected_type);
            }
        }
    }

    callee = lowerExprAsValue(state, scope, callee_expr, NULL);
    TypeSystemExprType callee_type = inferExprType(callee_expr, &(scope->type_scope));
    ASTFunctionParameter *parameter = callee_type.data_type == NULL ? NULL : callee_type.data_type->parameters;

    arguments = newMirOperandList(countASTNodes(node->rhs));
    ASTNode *argument_node = node->rhs;
    int index = 0;
    while(argument_node)
    {
        ASTDataType *parameter_type = parameter == NULL ? NULL : parameter->data_type;
        if(parameter_type != NULL && parameter_type->kind == AST_DATA_TYPE_KIND_REFERENCE)
            arguments.items[index++] = lowerExprAsAddress(state, scope, argument_node);
        else
            arguments.items[index++] = lowerExprAsValue(state, scope, argument_node, parameter_type);
        if(parameter != NULL)
            parameter = parameter->next;
        argument_node = argument_node->next;
    }

    MirValueId call_value = mirEmitCall(state, callee, arguments, return_type,
                                        node->filename, node->line_number, node->column_number);
    if(mirIsValueTypeVoid(return_type))
        return call_value;
    return mirMaybeConvertValue(state, scope, node, call_value, expected_type);
}

static MirValueId lowerExprAsValue(MirFunctionState *state, MirLowerScope *scope, ASTNode *node, ASTDataType *expected_type)
{
    if(node->kind == AST_EXPR_ARRAY_LITERAL && node->lhs == NULL)
    {
        ASTDataType *array_type = expected_type != NULL ? cloneDataType(expected_type) : mirResolvedExprValueType(node, &(scope->type_scope));
        MirOperandList elements = newMirOperandList(0);
        return mirEmitArrayLiteral(state, elements, array_type,
                                   node->filename, node->line_number, node->column_number);
    }

    if(node->kind == AST_EXPR_STRUCT_LITERAL)
    {
        TypeSystemExprType type_expr = inferExprType(node->lhs, &(scope->type_scope));
        ASTDataType *struct_type = resolveNamedDataType(type_expr.data_type, &(scope->type_scope), scope->self_data_type);
        int field_count = countStructDataFields(struct_type);
        MirFieldValueList fields = newMirFieldValueList(field_count);
        ASTStructMember *member = struct_type->members;
        int index = 0;
        while(member)
        {
            if(member->value == NULL)
            {
                ASTStructLiteralField *field = node->struct_literal_fields;
                while(field && strcmp(field->identifier, member->identifier) != 0)
                    field = field->next;
                strcpy(fields.items[index].identifier, member->identifier);
                fields.items[index].value = lowerExprAsValue(state, scope, field->value, member->data_type);
                index++;
            }
            member = member->next;
        }
        MirValueId value = mirEmitStructLiteral(state, fields, struct_type,
                                                node->filename, node->line_number, node->column_number);
        return mirMaybeConvertValue(state, scope, node, value, expected_type);
    }

    TypeSystemExprType expr_type = inferExprType(node, &(scope->type_scope));

    switch(node->kind)
    {
        case AST_EXPR_LITERAL_BOOL:
            return mirMaybeConvertValue(state, scope, node,
                                        mirEmitConstBool(state, node->literal_bool, node->filename, node->line_number, node->column_number),
                                        expected_type);
        case AST_EXPR_LITERAL_CHAR:
            return mirMaybeConvertValue(state, scope, node,
                                        mirEmitConstChar(state, node->literal_char, node->filename, node->line_number, node->column_number),
                                        expected_type);
        case AST_EXPR_LITERAL_INTEGER: {
            ASTDataType *literal_type = expected_type;
            if(literal_type == NULL)
                literal_type = defaultIntegerDataType();
            return mirEmitConstInt(state, node->literal_integer, literal_type,
                                   node->filename, node->line_number, node->column_number);
        }
        case AST_EXPR_LITERAL_FLOAT: {
            ASTDataType *literal_type = expected_type;
            if(literal_type == NULL)
                literal_type = defaultFloatDataType();
            return mirEmitConstFloat(state, node->literal_float, literal_type,
                                     node->filename, node->line_number, node->column_number);
        }
        case AST_EXPR_LITERAL_STRING: {
            ASTDataType *string_type = expr_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE
                ? cloneDataType(expr_type.data_type)
                : newArrayDataType(newPrimaryDataType(AST_PRIMARY_DATA_TYPE_CHAR), strlen(node->literal_string));
            return mirEmitConstString(state, node->literal_string, string_type,
                                      node->filename, node->line_number, node->column_number);
        }
        case AST_EXPR_BUILTIN:
            if(strcmp(node->identifier, "extern") == 0)
                return lowerExternBuiltinExpr(state, scope, node, expected_type);
            printf("MIR lowering error: unsupported builtin @%s at file %s, line %d, column %d\n",
                   node->identifier, node->filename, node->line_number, node->column_number);
            exit(1);
        case AST_EXPR_VARIABLE:
            return lowerVariableValue(state, scope, node, expected_type);
        case AST_EXPR_PARENTHESIS:
            return lowerExprAsValue(state, scope, node->lhs, expected_type);
        case AST_EXPR_FUNCTION:
            return lowerFunctionExprAsValue(state, scope, node, NULL, scope->self_data_type);
        case AST_EXPR_ARRAY_LITERAL: {
            ASTDataType *array_type = mirResolvedExprValueType(node, &(scope->type_scope));
            int count = countASTNodes(node->lhs);
            MirOperandList elements = newMirOperandList(count);
            ASTNode *element = node->lhs;
            int index = 0;
            while(element)
            {
                elements.items[index++] = lowerExprAsValue(state, scope, element, array_type->child);
                element = element->next;
            }
            MirValueId value = mirEmitArrayLiteral(state, elements, array_type,
                                                   node->filename, node->line_number, node->column_number);
            return mirMaybeConvertValue(state, scope, node, value, expected_type);
        }
        case AST_EXPR_MEMBER: {
            TypeSystemExprType owner_type = inferExprType(node->lhs, &(scope->type_scope));
            ASTDataType *owner_data_type = NULL;
            if(owner_type.kind == TYPE_SYSTEM_EXPR_TYPE_TYPE)
                owner_data_type = owner_type.data_type;
            else
            {
                owner_data_type = owner_type.data_type;
                if(owner_data_type->kind == AST_DATA_TYPE_KIND_POINTER || owner_data_type->kind == AST_DATA_TYPE_KIND_REFERENCE)
                    owner_data_type = owner_data_type->child;
            }

            if(isEnumDataType(owner_data_type))
            {
                int ordinal = findEnumVariantOrdinal(owner_data_type, node->identifier);
                return mirEmitEnumLiteral(state, owner_data_type, owner_data_type->identifier, node->identifier, ordinal,
                                          node->filename, node->line_number, node->column_number);
            }

            ASTStructMember *member = findStructMember(owner_data_type, node->identifier);
            if(member != NULL && member->value != NULL)
                return lowerMethodFunctionValue(state, scope, node, owner_data_type);

            MirValueId address = lowerExprAsAddress(state, scope, node);
            ASTDataType *value_type = mirResolvedExprValueType(node, &(scope->type_scope));
            MirValueId value = mirEmitLoad(state, address, value_type,
                                           node->filename, node->line_number, node->column_number);
            return mirMaybeConvertValue(state, scope, node, value, expected_type);
        }
        case AST_EXPR_INDEX: {
            MirValueId address = lowerExprAsAddress(state, scope, node);
            ASTDataType *value_type = mirResolvedExprValueType(node, &(scope->type_scope));
            MirValueId value = mirEmitLoad(state, address, value_type,
                                           node->filename, node->line_number, node->column_number);
            return mirMaybeConvertValue(state, scope, node, value, expected_type);
        }
        case AST_EXPR_CALL:
            return lowerCallExpr(state, scope, node, expected_type);
        case AST_EXPR_UNARY_PLUS:
            return lowerExprAsValue(state, scope, node->lhs, expected_type);
        case AST_EXPR_UNARY_MINUS: {
            ASTDataType *result_type = mirResolvedExprValueType(node, &(scope->type_scope));
            MirValueId operand = lowerExprAsValue(state, scope, node->lhs, result_type);
            return mirEmitUnary(state, MIR_INST_NEG, operand, result_type,
                                node->filename, node->line_number, node->column_number);
        }
        case AST_EXPR_UNARY_LOGICAL_NOT: {
            MirValueId operand = lowerExprAsValue(state, scope, node->lhs, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_BOOL));
            return mirEmitUnary(state, MIR_INST_NOT, operand, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_BOOL),
                                node->filename, node->line_number, node->column_number);
        }
        case AST_EXPR_UNARY_BIT_NOT: {
            ASTDataType *result_type = mirResolvedExprValueType(node, &(scope->type_scope));
            MirValueId operand = lowerExprAsValue(state, scope, node->lhs, result_type);
            return mirEmitUnary(state, MIR_INST_BIT_NOT, operand, result_type,
                                node->filename, node->line_number, node->column_number);
        }
        case AST_EXPR_ADDRESS_OF:
        case AST_EXPR_ADDRESS_OF_MUT:
            return lowerExprAsAddress(state, scope, node->lhs);
        case AST_EXPR_DEREF: {
            MirValueId pointer_value = lowerExprAsValue(state, scope, node->lhs, NULL);
            ASTDataType *value_type = mirResolvedExprValueType(node, &(scope->type_scope));
            MirValueId value = mirEmitLoad(state, pointer_value, value_type,
                                           node->filename, node->line_number, node->column_number);
            return mirMaybeConvertValue(state, scope, node, value, expected_type);
        }
        case AST_EXPR_LOGICAL_AND:
            return lowerLogicalShortCircuit(state, scope, node, true);
        case AST_EXPR_LOGICAL_OR:
            return lowerLogicalShortCircuit(state, scope, node, false);
        case AST_EXPR_ADD:
        case AST_EXPR_SUB:
        case AST_EXPR_MUL:
        case AST_EXPR_DIV:
        case AST_EXPR_MOD:
        case AST_EXPR_SHIFT_LEFT:
        case AST_EXPR_SHIFT_RIGHT:
        case AST_EXPR_BIT_AND:
        case AST_EXPR_BIT_OR:
        case AST_EXPR_BIT_XOR: {
            ASTDataType *result_type = mirResolvedExprValueType(node, &(scope->type_scope));
            MirValueId lhs = lowerExprAsValue(state, scope, node->lhs, result_type);
            MirValueId rhs = lowerExprAsValue(state, scope, node->rhs, result_type);
            MirInstKind kind = MIR_INST_ADD;
            if(node->kind == AST_EXPR_SUB) kind = MIR_INST_SUB;
            else if(node->kind == AST_EXPR_MUL) kind = MIR_INST_MUL;
            else if(node->kind == AST_EXPR_DIV) kind = MIR_INST_DIV;
            else if(node->kind == AST_EXPR_MOD) kind = MIR_INST_MOD;
            else if(node->kind == AST_EXPR_SHIFT_LEFT) kind = MIR_INST_SHIFT_LEFT;
            else if(node->kind == AST_EXPR_SHIFT_RIGHT) kind = MIR_INST_SHIFT_RIGHT;
            else if(node->kind == AST_EXPR_BIT_AND) kind = MIR_INST_BIT_AND;
            else if(node->kind == AST_EXPR_BIT_OR) kind = MIR_INST_BIT_OR;
            else if(node->kind == AST_EXPR_BIT_XOR) kind = MIR_INST_BIT_XOR;
            return mirEmitBinary(state, kind, lhs, rhs, result_type,
                                 node->filename, node->line_number, node->column_number);
        }
        case AST_EXPR_EQUAL:
        case AST_EXPR_NOT_EQUAL:
        case AST_EXPR_LESS:
        case AST_EXPR_LESS_EQUAL:
        case AST_EXPR_GREATER:
        case AST_EXPR_GREATER_EQUAL: {
            ASTDataType *operand_type = inferComparisonOperandType(node->lhs, node->rhs, &(scope->type_scope));
            MirValueId lhs = lowerExprAsValue(state, scope, node->lhs, operand_type);
            MirValueId rhs = lowerExprAsValue(state, scope, node->rhs, operand_type);
            MirInstKind kind = MIR_INST_EQ;
            if(node->kind == AST_EXPR_NOT_EQUAL) kind = MIR_INST_NE;
            else if(node->kind == AST_EXPR_LESS) kind = MIR_INST_LT;
            else if(node->kind == AST_EXPR_LESS_EQUAL) kind = MIR_INST_LE;
            else if(node->kind == AST_EXPR_GREATER) kind = MIR_INST_GT;
            else if(node->kind == AST_EXPR_GREATER_EQUAL) kind = MIR_INST_GE;
            return mirEmitBinary(state, kind, lhs, rhs, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_BOOL),
                                 node->filename, node->line_number, node->column_number);
        }
        default:
            printf("MIR lowering error: unsupported expression kind %s\n", astNodeKindToString(node->kind));
            exit(1);
    }
}

static MirValueId lowerExprAsAddress(MirFunctionState *state, MirLowerScope *scope, ASTNode *node)
{
    switch(node->kind)
    {
        case AST_EXPR_VARIABLE: {
            MirRuntimeBinding *binding = findMirRuntimeBinding(scope, node->identifier);
            if(binding == NULL || binding->kind == MIR_RUNTIME_BINDING_COMPTIME_ONLY)
            {
                printf("MIR lowering error: variable %s is not addressable at file %s, line %d, column %d\n",
                       node->identifier, node->filename, node->line_number, node->column_number);
                exit(1);
            }
            return mirBindingAddress(state, binding, node);
        }
        case AST_EXPR_DEREF:
            return lowerExprAsValue(state, scope, node->lhs, NULL);
        case AST_EXPR_MEMBER: {
            TypeSystemExprType owner_type = inferExprType(node->lhs, &(scope->type_scope));
            ASTDataType *struct_type = owner_type.data_type;
            MirValueId base_address = -1;

            if(owner_type.kind != TYPE_SYSTEM_EXPR_TYPE_VALUE)
            {
                printf("MIR lowering error: member base is not a runtime value at file %s, line %d, column %d\n",
                       node->filename, node->line_number, node->column_number);
                exit(1);
            }

            if(struct_type->kind == AST_DATA_TYPE_KIND_POINTER || struct_type->kind == AST_DATA_TYPE_KIND_REFERENCE)
            {
                base_address = lowerExprAsValue(state, scope, node->lhs, NULL);
                struct_type = struct_type->child;
            }
            else if(isAddressableExpr(node->lhs))
                base_address = lowerExprAsAddress(state, scope, node->lhs);
            else
                base_address = lowerExprMaterializedAddress(state, scope, node->lhs);

            ASTStructMember *member = findStructMember(struct_type, node->identifier);
            if(member == NULL || member->value != NULL)
            {
                printf("MIR lowering error: member %s is not an addressable field at file %s, line %d, column %d\n",
                       node->identifier, node->filename, node->line_number, node->column_number);
                exit(1);
            }
            return mirEmitFieldPtr(state, base_address, member->data_type, member->identifier,
                                   findStructDataFieldIndex(struct_type, member->identifier),
                                   node->filename, node->line_number, node->column_number);
        }
        case AST_EXPR_INDEX: {
            TypeSystemExprType owner_type = inferExprType(node->lhs, &(scope->type_scope));
            ASTDataType *array_type = owner_type.data_type;
            MirValueId base_address = -1;
            if(array_type->kind == AST_DATA_TYPE_KIND_POINTER || array_type->kind == AST_DATA_TYPE_KIND_REFERENCE)
            {
                base_address = lowerExprAsValue(state, scope, node->lhs, NULL);
                array_type = array_type->child;
            }
            else if(isAddressableExpr(node->lhs))
                base_address = lowerExprAsAddress(state, scope, node->lhs);
            else
                base_address = lowerExprMaterializedAddress(state, scope, node->lhs);

            ASTDataType *index_type = defaultIntegerDataType();
            MirValueId index_value = lowerExprAsValue(state, scope, node->rhs, index_type);
            return mirEmitIndexPtr(state, base_address, index_value, array_type->child,
                                   node->filename, node->line_number, node->column_number);
        }
        case AST_EXPR_PARENTHESIS:
            return lowerExprAsAddress(state, scope, node->lhs);
        default:
            return lowerExprMaterializedAddress(state, scope, node);
    }
}

static void lowerAssignNode(MirFunctionState *state, MirLowerScope *scope, ASTNode *node)
{
    if(isStructDeclAssign(node))
    {
        declareStructType(node, &(scope->type_scope));
        return;
    }

    if(isEnumDeclAssign(node))
    {
        declareEnumType(node, &(scope->type_scope));
        return;
    }

    if(node->lhs->kind == AST_EXPR_VARIABLE)
    {
        MirRuntimeBinding *local_binding = findMirRuntimeBindingInScope(scope, node->identifier);
        MirRuntimeBinding *existing_binding = findMirRuntimeBinding(scope, node->identifier);
        bool explicit_decl = isExplicitDeclared(node);
        bool is_new_variable = explicit_decl || existing_binding == NULL;

        if(is_new_variable)
        {
            ASTDataType *declared_type = cloneDataType(node->data_type);
            TypeSystemExprType expr_type = {0};
            if(node->rhs->kind == AST_EXPR_ARRAY_LITERAL && node->rhs->lhs == NULL &&
               declared_type->kind == AST_DATA_TYPE_KIND_ARRAY && declared_type->array_length == 0)
                expr_type = newValueExprType(declared_type);
            else
                expr_type = inferExprType(node->rhs, &(scope->type_scope));
            mirDeclareVariableInfo(scope, node, declared_type, expr_type);

            MirRuntimeBinding *binding = declareMirRuntimeBinding(scope, node->identifier);
            binding->mutable = node->modifier.mutable;
            binding->declared_data_type = cloneDataType(declared_type);
            binding->function_value = node->rhs->kind == AST_EXPR_FUNCTION ? node->rhs : NULL;

            if(expr_type.kind == TYPE_SYSTEM_EXPR_TYPE_TYPE ||
               (node->rhs->kind == AST_EXPR_FUNCTION && functionHasTypeParameters(node->rhs->parameters)))
            {
                binding->kind = MIR_RUNTIME_BINDING_COMPTIME_ONLY;
                binding->type_value = expr_type.kind == TYPE_SYSTEM_EXPR_TYPE_TYPE ? cloneDataType(expr_type.data_type) : NULL;
                return;
            }

            if(declared_type->kind == AST_DATA_TYPE_KIND_REFERENCE)
            {
                binding->kind = MIR_RUNTIME_BINDING_ALIAS_ADDRESS;
                binding->local_value = lowerExprAsAddress(state, scope, node->rhs);
                return;
            }

            if(scope->declare_as_globals)
            {
                binding->kind = MIR_RUNTIME_BINDING_GLOBAL_SLOT;
                strcpy(binding->global_name, node->identifier);
                mirEnsureGlobal(state->lowering, node->identifier, declared_type, node->modifier.mutable);
            }
            else
                binding->kind = MIR_RUNTIME_BINDING_LOCAL_SLOT;

            MirValueId value = lowerExprAsValue(state, scope, node->rhs, declared_type);
            value = mirMaybeConvertValue(state, scope, node->rhs, value, declared_type);

            if(scope->declare_as_globals)
            {
                MirValueId global_addr = mirEmitGlobalAddr(state, binding->global_name, declared_type,
                                                          node->filename, node->line_number, node->column_number);
                mirEmitStore(state, global_addr, value, node->filename, node->line_number, node->column_number);
            }
            else
            {
                binding->local_value = mirEmitAlloca(state, declared_type,
                                                     node->filename, node->line_number, node->column_number);
                mirEmitStore(state, binding->local_value, value, node->filename, node->line_number, node->column_number);
            }
            return;
        }

        VariableInfo *variable_info = findVariableInfo(&(scope->type_scope), node->identifier);
        ASTDataType *target_type = variable_info == NULL ? node->data_type : variable_info->data_type;
        if(target_type != NULL && target_type->kind == AST_DATA_TYPE_KIND_REFERENCE)
            target_type = target_type->child;

        MirValueId address = mirBindingAddress(state, existing_binding, node);
        MirValueId value = lowerExprAsValue(state, scope, node->rhs, target_type);
        value = mirMaybeConvertValue(state, scope, node->rhs, value, target_type);
        mirEmitStore(state, address, value, node->filename, node->line_number, node->column_number);

        if(local_binding == NULL && variable_info == NULL)
            local_binding = existing_binding;
        return;
    }

    MirValueId address = -1;
    ASTDataType *target_type = cloneDataType(node->data_type);
    if(node->lhs->kind == AST_EXPR_DEREF)
    {
        TypeSystemExprType lhs_type = inferExprType(node->lhs->lhs, &(scope->type_scope));
        target_type = lhs_type.data_type->child;
        address = lowerExprAsValue(state, scope, node->lhs->lhs, NULL);
    }
    else
    {
        address = lowerExprAsAddress(state, scope, node->lhs);
        target_type = mirResolvedExprValueType(node->lhs, &(scope->type_scope));
    }

    MirValueId value = lowerExprAsValue(state, scope, node->rhs, target_type);
    value = mirMaybeConvertValue(state, scope, node->rhs, value, target_type);
    mirEmitStore(state, address, value, node->filename, node->line_number, node->column_number);
}

static void lowerBlockNode(MirFunctionState *state, MirLowerScope *parent_scope, ASTNode *block_node)
{
    MirLowerScope *block_scope = (MirLowerScope*) malloc(sizeof(MirLowerScope));
    initMirLowerScope(block_scope, parent_scope, false);
    block_scope->self_data_type = parent_scope->self_data_type;

    MirCleanupFrame cleanup = {0};
    cleanup.parent = state->cleanup_top;
    state->cleanup_top = &cleanup;

    ASTNode *statement = block_node->lhs;
    while(statement)
    {
        lowerStatement(state, block_scope, statement);
        if(mirCurrentBlockTerminated(state))
            break;
        statement = statement->next;
    }

    if(!mirCurrentBlockTerminated(state))
        emitCleanupRange(state, block_scope, &cleanup, cleanup.parent);

    state->cleanup_top = cleanup.parent;
}

static void lowerStatement(MirFunctionState *state, MirLowerScope *scope, ASTNode *node)
{
    if(node == NULL || mirCurrentBlockTerminated(state))
        return;

    switch(node->kind)
    {
        case AST_BLOCK:
            lowerBlockNode(state, scope, node);
            return;
        case AST_ASSIGN:
            lowerAssignNode(state, scope, node);
            return;
        case AST_STATEMENT_EXPR:
            lowerExprAsValue(state, scope, node->lhs, NULL);
            return;
        case AST_STATEMENT_RETURN: {
            ASTDataType *return_type = mirCurrentFunction(state)->return_data_type;
            MirValueId return_value = -1;
            if(node->lhs != NULL)
            {
                return_value = lowerExprAsValue(state, scope, node->lhs, return_type);
                return_value = mirMaybeConvertValue(state, scope, node->lhs, return_value, return_type);
            }
            emitCleanupRange(state, scope, state->cleanup_top, NULL);
            if(node->lhs == NULL)
                mirEmitRetVoid(state);
            else
                mirEmitRetValue(state, return_value);
            return;
        }
        case AST_STATEMENT_IF: {
            MirFunction *function = mirCurrentFunction(state);
            MirBlockId then_block = mirCreateBlock(state->lowering, function, "if_then");
            MirBlockId else_block = mirCreateBlock(state->lowering, function, "if_else");
            MirBlockId end_block = -1;
            MirValueId condition = lowerExprAsValue(state, scope, node->lhs, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_BOOL));
            mirEmitCondBr(state, condition, then_block, else_block);

            mirSwitchToBlock(state, then_block);
            lowerStatement(state, scope, node->rhs);
            if(!mirCurrentBlockTerminated(state))
            {
                if(end_block < 0)
                    end_block = mirCreateBlock(state->lowering, function, "if_end");
                mirEmitBr(state, end_block);
            }

            mirSwitchToBlock(state, else_block);
            if(node->body != NULL)
                lowerStatement(state, scope, node->body);
            if(!mirCurrentBlockTerminated(state))
            {
                if(end_block < 0)
                    end_block = mirCreateBlock(state->lowering, function, "if_end");
                mirEmitBr(state, end_block);
            }

            if(end_block >= 0)
                mirSwitchToBlock(state, end_block);
            return;
        }
        case AST_STATEMENT_WHILE: {
            MirFunction *function = mirCurrentFunction(state);
            MirBlockId cond_block = mirCreateBlock(state->lowering, function, "while_cond");
            MirBlockId body_block = mirCreateBlock(state->lowering, function, "while_body");
            MirBlockId end_block = mirCreateBlock(state->lowering, function, "while_end");
            mirEmitBr(state, cond_block);

            MirLoopContext loop_context = {0};
            loop_context.parent = state->loop_top;
            loop_context.break_block = end_block;
            loop_context.continue_block = cond_block;
            loop_context.cleanup_stop = state->cleanup_top;
            state->loop_top = &loop_context;

            mirSwitchToBlock(state, cond_block);
            MirValueId condition = lowerExprAsValue(state, scope, node->lhs, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_BOOL));
            mirEmitCondBr(state, condition, body_block, end_block);

            mirSwitchToBlock(state, body_block);
            lowerStatement(state, scope, node->body);
            if(!mirCurrentBlockTerminated(state))
                mirEmitBr(state, cond_block);

            state->loop_top = loop_context.parent;
            mirSwitchToBlock(state, end_block);
            return;
        }
        case AST_STATEMENT_DO_WHILE: {
            MirFunction *function = mirCurrentFunction(state);
            MirBlockId body_block = mirCreateBlock(state->lowering, function, "do_body");
            MirBlockId cond_block = mirCreateBlock(state->lowering, function, "do_cond");
            MirBlockId end_block = mirCreateBlock(state->lowering, function, "do_end");
            mirEmitBr(state, body_block);

            MirLoopContext loop_context = {0};
            loop_context.parent = state->loop_top;
            loop_context.break_block = end_block;
            loop_context.continue_block = cond_block;
            loop_context.cleanup_stop = state->cleanup_top;
            state->loop_top = &loop_context;

            mirSwitchToBlock(state, body_block);
            lowerStatement(state, scope, node->body);
            if(!mirCurrentBlockTerminated(state))
                mirEmitBr(state, cond_block);

            mirSwitchToBlock(state, cond_block);
            MirValueId condition = lowerExprAsValue(state, scope, node->lhs, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_BOOL));
            mirEmitCondBr(state, condition, body_block, end_block);

            state->loop_top = loop_context.parent;
            mirSwitchToBlock(state, end_block);
            return;
        }
        case AST_STATEMENT_FOR: {
            MirLowerScope *loop_scope = (MirLowerScope*) malloc(sizeof(MirLowerScope));
            initMirLowerScope(loop_scope, scope, false);
            loop_scope->self_data_type = scope->self_data_type;

            if(node->lhs != NULL)
                lowerStatement(state, loop_scope, node->lhs);

            MirFunction *function = mirCurrentFunction(state);
            MirBlockId cond_block = mirCreateBlock(state->lowering, function, "for_cond");
            MirBlockId body_block = mirCreateBlock(state->lowering, function, "for_body");
            MirBlockId post_block = mirCreateBlock(state->lowering, function, "for_post");
            MirBlockId end_block = mirCreateBlock(state->lowering, function, "for_end");
            mirEmitBr(state, cond_block);

            MirLoopContext loop_context = {0};
            loop_context.parent = state->loop_top;
            loop_context.break_block = end_block;
            loop_context.continue_block = post_block;
            loop_context.cleanup_stop = state->cleanup_top;
            state->loop_top = &loop_context;

            mirSwitchToBlock(state, cond_block);
            if(node->rhs != NULL)
            {
                MirValueId condition = lowerExprAsValue(state, loop_scope, node->rhs, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_BOOL));
                mirEmitCondBr(state, condition, body_block, end_block);
            }
            else
                mirEmitBr(state, body_block);

            mirSwitchToBlock(state, body_block);
            lowerStatement(state, loop_scope, node->body);
            if(!mirCurrentBlockTerminated(state))
                mirEmitBr(state, post_block);

            mirSwitchToBlock(state, post_block);
            if(node->extra != NULL)
                lowerStatement(state, loop_scope, node->extra);
            if(!mirCurrentBlockTerminated(state))
                mirEmitBr(state, cond_block);

            state->loop_top = loop_context.parent;
            mirSwitchToBlock(state, end_block);
            return;
        }
        case AST_STATEMENT_BREAK:
            if(state->loop_top == NULL)
            {
                printf("MIR lowering error: break used outside loop\n");
                exit(1);
            }
            emitCleanupRange(state, scope, state->cleanup_top, state->loop_top->cleanup_stop);
            mirEmitBr(state, state->loop_top->break_block);
            return;
        case AST_STATEMENT_CONTINUE:
            if(state->loop_top == NULL)
            {
                printf("MIR lowering error: continue used outside loop\n");
                exit(1);
            }
            emitCleanupRange(state, scope, state->cleanup_top, state->loop_top->cleanup_stop);
            mirEmitBr(state, state->loop_top->continue_block);
            return;
        case AST_STATEMENT_DEFER:
            appendDeferredStatement(state->cleanup_top, node->lhs);
            return;
        default:
            printf("MIR lowering error: unsupported statement kind %s\n", astNodeKindToString(node->kind));
            exit(1);
    }
}

MirProgram* lowerASTToMIR(ASTNode *root)
{
    if(root == NULL || root->lhs == NULL || root->lhs->kind != AST_BLOCK)
    {
        printf("MIR lowering error: root should contain a top-level block\n");
        exit(1);
    }

    MirProgram *program = newMirProgram();
    MirLowering lowering = {0};
    lowering.program = program;

    MirFunction *init_function = mirAppendFunction(program);
    strcpy(init_function->name, "__mote_init");
    init_function->return_data_type = newPrimaryDataType(AST_PRIMARY_DATA_TYPE_VOID);
    init_function->entry_block = mirCreateBlock(&lowering, init_function, "entry");

    MirFunctionState init_state = {0};
    init_state.lowering = &lowering;
    init_state.function_index = 0;
    init_state.current_block = init_function->entry_block;
    init_state.is_top_level_init = true;

    MirLowerScope *top_scope = (MirLowerScope*) malloc(sizeof(MirLowerScope));
    initMirLowerScope(top_scope, NULL, true);

    MirCleanupFrame cleanup = {0};
    init_state.cleanup_top = &cleanup;
    init_state.loop_top = NULL;

    ASTNode *statement = root->lhs->lhs;
    while(statement)
    {
        lowerStatement(&init_state, top_scope, statement);
        if(mirCurrentBlockTerminated(&init_state))
            break;
        statement = statement->next;
    }

    if(!mirCurrentBlockTerminated(&init_state))
    {
        emitCleanupRange(&init_state, top_scope, &cleanup, NULL);
        if(!mirCurrentBlockTerminated(&init_state))
            mirEmitRetVoid(&init_state);
    }

    return program;
}

#endif /* MIR_LOWERING_H */
