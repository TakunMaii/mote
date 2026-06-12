#ifndef MIR_LOWERING_H
#define MIR_LOWERING_H

#include "../AST.h"
#include "../Semantic.h"
#include "../TypeSystem.h"
#include <stdarg.h>
#include <stdlib.h>

#define MIR_MAX_NAME_LENGTH 128

typedef int MirValueId;
typedef int MirBlockId;

static MOTE_NORETURN void mirLoweringAbortNode(const char *code, ASTNode *node, const char *message, const char *label)
{
    diagnosticAbortSimple(code, message, astNodeSourceSpan(node), label);
}

static MOTE_NORETURN void mirLoweringAbortNodeFormatted(const char *code, ASTNode *node, const char *label, const char *format, ...)
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

static MOTE_NORETURN void mirLoweringAbortPointFormatted(const char *code, const char *filename, int line, int column,
                                                         const char *label, const char *format, ...)
{
    Diagnostic diagnostic = diagnosticMake(DIAGNOSTIC_SEVERITY_ERROR, code,
                                           makePointSourceSpan(filename, line, column), "");
    va_list args;
    va_start(args, format);
    diagnosticVFormat(diagnostic.message, sizeof(diagnostic.message), format, args);
    va_end(args);
    if(label != NULL)
        diagnosticSetPrimaryLabel(&diagnostic, "%s", label);
    diagnosticAbort(diagnostic);
}

static MOTE_NORETURN void mirLoweringAbortInternal(const char *code, const char *context, const char *detail)
{
    Diagnostic diagnostic = diagnosticMake(DIAGNOSTIC_SEVERITY_ERROR, code,
                                           makeSourceSpan(NULL, 0, 0, 0, 0),
                                           "compiler internal error");
    if(context != NULL && context[0] != '\0')
        diagnosticAddNote(&diagnostic, "%s", context);
    if(detail != NULL && detail[0] != '\0')
        diagnosticAddNote(&diagnostic, "%s", detail);
    diagnosticAbort(diagnostic);
}

typedef enum MirInstKind {
    MIR_INST_ZERO,
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
    MIR_INST_PTR_DIFF,
    MIR_INST_ARRAY_LITERAL,
    MIR_INST_STRUCT_LITERAL,
    MIR_INST_ENUM_LITERAL,
    MIR_INST_CALL,
    MIR_INST_EXTERN_CALL,
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
            bool base_is_element_pointer;
        } index_ptr;
        struct {
            MirValueId lhs;
            MirValueId rhs;
        } ptr_diff;
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
        struct {
            char symbol_name[MIR_MAX_NAME_LENGTH];
            ASTDataType *function_type;
            MirOperandList arguments;
        } extern_call;
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
    bool has_const_string_initializer;
    char const_string_initializer[MAX_STRING_LITERAL_LENGTH];
} MirGlobal;

typedef enum MirExternFunctionKind {
    MIR_EXTERN_FUNCTION_NATIVE,
    MIR_EXTERN_FUNCTION_DYNAMIC_POINTER,
} MirExternFunctionKind;

typedef struct MirExternFunction {
    char wrapper_name[MIR_MAX_NAME_LENGTH];
    char symbol_name[MIR_MAX_NAME_LENGTH];
    ASTDataType *function_type;
    MirExternFunctionKind kind;
    bool is_direct;
} MirExternFunction;

typedef struct MirProgram {
    MirGlobal *globals;
    int global_count;
    MirExternFunction *extern_functions;
    int extern_function_count;
    MirFunction **functions;
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
    ASTNode *extern_value;
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
    struct MirSpecializedFunctionCacheEntry *specialized_functions;
    int specialized_function_count;
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

typedef struct MirSpecializedFunctionCacheEntry {
    ASTNode *source_function;
    ASTDataType *self_data_type;
    ASTDataType *function_type;
    int function_index;
} MirSpecializedFunctionCacheEntry;

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
    program->functions = (MirFunction**) realloc(
        program->functions,
        sizeof(MirFunction*) * (program->function_count + 1)
    );
    MirFunction *function = (MirFunction*) malloc(sizeof(MirFunction));
    program->functions[program->function_count++] = function;
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
    return state->lowering->program->functions[state->function_index];
}

static const char* mirEnsureExternFunction(MirLowering *lowering, const char *symbol_name, ASTDataType *function_type,
                                           const char *filename, int line, int column)
{
    for(int i = 0; i < lowering->program->extern_function_count; i++)
    {
        MirExternFunction *extern_function = &(lowering->program->extern_functions[i]);
        if(extern_function->kind == MIR_EXTERN_FUNCTION_NATIVE &&
           strcmp(extern_function->symbol_name, symbol_name) == 0)
        {
            if(!isSameDataType(extern_function->function_type, function_type))
                mirLoweringAbortPointFormatted("M2001", filename, line, column,
                                               "conflicting extern symbol declaration",
                                               "extern symbol `%s` is declared with conflicting function types",
                                               symbol_name);
            return extern_function->is_direct ? extern_function->symbol_name : extern_function->wrapper_name;
        }
    }

    MirExternFunction *extern_function = mirAppendExternFunction(lowering->program);
    if(!function_type->is_variadic)
        snprintf(extern_function->wrapper_name, sizeof(extern_function->wrapper_name),
                 "__mote_extern_%d", lowering->unique_extern_counter++);
    strcpy(extern_function->symbol_name, symbol_name);
    extern_function->function_type = cloneDataType(function_type);
    extern_function->kind = MIR_EXTERN_FUNCTION_NATIVE;
    extern_function->is_direct = function_type->is_variadic;
    return extern_function->is_direct ? extern_function->symbol_name : extern_function->wrapper_name;
}

static const char* mirEnsureDynamicFunctionWrapper(MirLowering *lowering, ASTDataType *function_type)
{
    for(int i = 0; i < lowering->program->extern_function_count; i++)
    {
        MirExternFunction *extern_function = &(lowering->program->extern_functions[i]);
        if(extern_function->kind == MIR_EXTERN_FUNCTION_DYNAMIC_POINTER &&
           isSameDataType(extern_function->function_type, function_type))
            return extern_function->wrapper_name;
    }

    MirExternFunction *extern_function = mirAppendExternFunction(lowering->program);
    snprintf(extern_function->wrapper_name, sizeof(extern_function->wrapper_name),
             "__mote_dynfn_%d", lowering->unique_extern_counter++);
    extern_function->function_type = cloneDataType(function_type);
    extern_function->kind = MIR_EXTERN_FUNCTION_DYNAMIC_POINTER;
    extern_function->is_direct = false;
    return extern_function->wrapper_name;
}

static ASTDataType* mirGetValueType(MirFunctionState *state, MirValueId value)
{
    MirFunction *function = mirCurrentFunction(state);
    if(value < 0 || value >= function->value_count)
        mirLoweringAbortInternal("ICE0301", "invalid MIR value id",
                                 value < 0
                                     ? "negative MIR value ids are invalid"
                                     : "value id is outside the current function value table");
    return function->values[value].data_type;
}

static bool mirIsValueTypeVoid(ASTDataType *data_type)
{
    return data_type != NULL &&
           data_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
           data_type->primary == AST_PRIMARY_DATA_TYPE_VOID;
}

static bool mirIsCompileTimeTypeFactory(ASTDataType *data_type)
{
    return data_type != NULL &&
           data_type->kind == AST_DATA_TYPE_KIND_FUNCTION &&
           data_type->return_data_type != NULL &&
           data_type->return_data_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
           data_type->return_data_type->primary == AST_PRIMARY_DATA_TYPE_TYPE;
}

static ASTDataType* mirOptionalBoolType(void)
{
    return newPrimaryDataType(AST_PRIMARY_DATA_TYPE_BOOL);
}

static ASTDataType* mirResolvedExprValueType(ASTNode *node, ScopeFrame *scope)
{
    TypeSystemExprType expr_type = inferExprType(node, scope);
    if(expr_type.kind == TYPE_SYSTEM_EXPR_TYPE_NULL)
        mirLoweringAbortNode("M2003", node,
                             "`null` requires an expected optional type at lowering time",
                             "add an explicit target type like `?T`");
    if(expr_type.kind == TYPE_SYSTEM_EXPR_TYPE_LITERAL_INTEGER)
        return defaultIntegerDataType();
    if(expr_type.kind == TYPE_SYSTEM_EXPR_TYPE_LITERAL_FLOAT)
        return defaultFloatDataType();
    if(expr_type.kind == TYPE_SYSTEM_EXPR_TYPE_TYPE)
        mirLoweringAbortNode("M2002", node,
                             "type-valued expression cannot be lowered directly",
                             "this expression is compile-time only and has no runtime representation");
    return cloneDataType(expr_type.data_type);
}

static ASTDataType* mirPreferredExprValueType(ASTNode *node, ScopeFrame *scope, ASTDataType *expected_type)
{
    if(expected_type == NULL)
        return mirResolvedExprValueType(node, scope);

    TypeSystemExprType expr_type = inferExprType(node, scope);
    if((expr_type.kind == TYPE_SYSTEM_EXPR_TYPE_LITERAL_INTEGER ||
        expr_type.kind == TYPE_SYSTEM_EXPR_TYPE_LITERAL_FLOAT) &&
       expected_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
       (isIntegerPrimary(expected_type->primary) || isFloatPrimary(expected_type->primary)))
        return cloneDataType(expected_type);

    return mirResolvedExprValueType(node, scope);
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
    if(struct_type != NULL && struct_type->kind == AST_DATA_TYPE_KIND_SLICE)
    {
        if(strcmp(identifier, "ptr") == 0)
            return 0;
        if(strcmp(identifier, "len") == 0)
            return 1;
        return -1;
    }

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

static ASTDataType* mirDynamicFunctionEnvType(void)
{
    ASTStructMember *member = (ASTStructMember*) malloc(sizeof(ASTStructMember));
    memset(member, 0, sizeof(ASTStructMember));
    strcpy(member->identifier, "fn_ptr");
    member->data_type = newWrappedDataType(AST_DATA_TYPE_KIND_POINTER, false,
                                           newPrimaryDataType(AST_PRIMARY_DATA_TYPE_VOID));
    return newStructDataType("", member);
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

static MirValueId lowerExprAsValue(MirFunctionState *state, MirLowerScope *scope, ASTNode *node, ASTDataType *expected_type);
static MirValueId lowerExprAsAddress(MirFunctionState *state, MirLowerScope *scope, ASTNode *node);
static MirValueId lowerExprMaterializedAddress(MirFunctionState *state, MirLowerScope *scope, ASTNode *node);
static void lowerStatement(MirFunctionState *state, MirLowerScope *scope, ASTNode *node);
static const char* mirEnsureStringLiteralGlobal(MirLowering *lowering, const char *value);
static int lowerFunctionExprDefinition(MirLowering *lowering, MirLowerScope *scope, ASTNode *function_expr,
                                       const char *name_hint, ASTDataType *self_data_type);
static MirValueId lowerFunctionExprAsValue(MirFunctionState *state, MirLowerScope *scope, ASTNode *function_expr,
                                           const char *name_hint, ASTDataType *self_data_type);

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

static ASTOperatorKind mirBinaryExprOperatorKind(ASTNodeKind kind)
{
    switch(kind)
    {
        case AST_EXPR_ADD: return AST_OPERATOR_ADD;
        case AST_EXPR_SUB: return AST_OPERATOR_SUB;
        case AST_EXPR_MUL: return AST_OPERATOR_MUL;
        case AST_EXPR_DIV: return AST_OPERATOR_DIV;
        case AST_EXPR_EQUAL:
        case AST_EXPR_NOT_EQUAL: return AST_OPERATOR_EQ;
        default: return AST_OPERATOR_NONE;
    }
}

static bool functionParameterIsComptimeType(ASTFunctionParameter *parameter)
{
    return parameter != NULL &&
           parameter->data_type != NULL &&
           parameter->data_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
           parameter->data_type->primary == AST_PRIMARY_DATA_TYPE_TYPE;
}

static int countRuntimeCallArguments(ASTFunctionParameter *parameter, ASTNode *argument)
{
    int count = 0;
    while(parameter != NULL && argument != NULL)
    {
        if(!functionParameterIsComptimeType(parameter))
            count++;
        parameter = parameter->next;
        argument = argument->next;
    }
    return count;
}

static MirValueId lowerReferenceArgument(MirFunctionState *state, MirLowerScope *scope, ASTNode *argument_node,
                                         ASTDataType *parameter_type)
{
    if(argument_node->kind == AST_EXPR_ADDRESS_OF ||
       argument_node->kind == AST_EXPR_ADDRESS_OF_MUT)
        return lowerExprAsValue(state, scope, argument_node, parameter_type);
    return lowerExprAsAddress(state, scope, argument_node);
}

static MirOperandList lowerSpecializedCallArguments(MirFunctionState *state, MirLowerScope *scope,
                                                    ASTFunctionParameter *source_parameters,
                                                    ASTFunctionParameter *specialized_parameters,
                                                    ASTNode *call_arguments)
{
    MirOperandList arguments = newMirOperandList(countRuntimeCallArguments(source_parameters, call_arguments));
    int index = 0;

    ASTFunctionParameter *source_parameter = source_parameters;
    ASTFunctionParameter *specialized_parameter = specialized_parameters;
    ASTNode *argument = call_arguments;
    while(source_parameter != NULL && argument != NULL)
    {
        if(!functionParameterIsComptimeType(source_parameter))
        {
            ASTDataType *parameter_type = specialized_parameter == NULL ? NULL : specialized_parameter->data_type;
            if(parameter_type != NULL && parameter_type->kind == AST_DATA_TYPE_KIND_REFERENCE)
                arguments.items[index++] = lowerReferenceArgument(state, scope, argument, parameter_type);
            else
                arguments.items[index++] = lowerExprAsValue(state, scope, argument, parameter_type);
            if(specialized_parameter != NULL)
                specialized_parameter = specialized_parameter->next;
        }

        source_parameter = source_parameter->next;
        argument = argument->next;
    }

    return arguments;
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

static MirValueId mirEmitZero(MirFunctionState *state, ASTDataType *data_type,
                              const char *filename, int line, int column)
{
    return mirEmitResultInst(state, MIR_INST_ZERO, data_type, filename, line, column);
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
                                  ASTDataType *element_type, bool base_is_element_pointer,
                                  const char *filename, int line, int column)
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
    inst->data.index_ptr.base_is_element_pointer = base_is_element_pointer;
    return result;
}

static MirValueId mirEmitPtrDiff(MirFunctionState *state, MirValueId lhs, MirValueId rhs,
                                 const char *filename, int line, int column)
{
    MirValueId result = mirEmitResultInst(
        state,
        MIR_INST_PTR_DIFF,
        newPrimaryDataType(AST_PRIMARY_DATA_TYPE_I64),
        filename, line, column
    );
    MirInst *inst = mirGetLastInst(state);
    inst->data.ptr_diff.lhs = lhs;
    inst->data.ptr_diff.rhs = rhs;
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

static MirValueId mirEmitOptionalSome(MirFunctionState *state, MirValueId value, ASTDataType *optional_type,
                                      const char *filename, int line, int column)
{
    MirFieldValueList fields = newMirFieldValueList(2);
    strcpy(fields.items[0].identifier, "has_value");
    fields.items[0].value = mirEmitConstBool(state, true, filename, line, column);
    strcpy(fields.items[1].identifier, "value");
    fields.items[1].value = value;
    return mirEmitStructLiteral(state, fields, optional_type, filename, line, column);
}

static MirValueId mirLowerOptionalNull(MirFunctionState *state, ASTDataType *optional_type,
                                       const char *filename, int line, int column)
{
    return mirEmitZero(state, optional_type, filename, line, column);
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

static MirValueId mirEmitExternCall(MirFunctionState *state, const char *symbol_name, ASTDataType *function_type,
                                    MirOperandList arguments, ASTDataType *return_type,
                                    const char *filename, int line, int column)
{
    MirValueId result = -1;
    if(!mirIsValueTypeVoid(return_type))
        result = mirEmitResultInst(state, MIR_INST_EXTERN_CALL, return_type, filename, line, column);
    else
    {
        MirBlock *block = &(mirCurrentFunction(state)->blocks[state->current_block]);
        MirInst *inst = mirAppendInst(block);
        inst->kind = MIR_INST_EXTERN_CALL;
        inst->result = -1;
        inst->result_type = cloneDataType(return_type);
        inst->filename = filename;
        inst->line_number = line;
        inst->column_number = column;
    }

    MirInst *inst = mirGetLastInst(state);
    strcpy(inst->data.extern_call.symbol_name, symbol_name);
    inst->data.extern_call.function_type = cloneDataType(function_type);
    inst->data.extern_call.arguments = arguments;
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

static bool mirIsCharPointerTarget(ASTDataType *data_type)
{
    if(data_type == NULL)
        return false;
    if(data_type->kind == AST_DATA_TYPE_KIND_OPTIONAL)
        data_type = data_type->child;
    return data_type != NULL &&
           data_type->kind == AST_DATA_TYPE_KIND_POINTER &&
           data_type->child != NULL &&
           data_type->child->kind == AST_DATA_TYPE_KIND_PRIMARY &&
           data_type->child->primary == AST_PRIMARY_DATA_TYPE_CHAR;
}

static MirValueId lowerStringLiteralAsPointer(MirFunctionState *state, ASTNode *node, ASTDataType *target_type)
{
    const char *global_name = mirEnsureStringLiteralGlobal(state->lowering, node->literal_string);
    ASTDataType *global_type = newArrayDataType(
        newPrimaryDataType(AST_PRIMARY_DATA_TYPE_CHAR),
        strlen(node->literal_string) + 1
    );
    MirValueId global_addr = mirEmitGlobalAddr(state, global_name, global_type,
                                               node->filename, node->line_number, node->column_number);
    return mirEmitConvert(state, global_addr, target_type,
                          node->filename, node->line_number, node->column_number);
}

static MirValueId mirMaybeConvertValue(MirFunctionState *state, MirLowerScope *scope, ASTNode *node,
                                       MirValueId value, ASTDataType *target_type)
{
    (void) scope;
    if(target_type == NULL)
        return value;
    if(node != NULL && node->kind == AST_EXPR_LITERAL_NULL && target_type->kind == AST_DATA_TYPE_KIND_OPTIONAL)
        return mirLowerOptionalNull(state, target_type, node->filename, node->line_number, node->column_number);
    ASTDataType *value_type = mirGetValueType(state, value);
    if(isSameDataType(value_type, target_type))
        return value;
    if(target_type->kind == AST_DATA_TYPE_KIND_OPTIONAL && isSameDataType(value_type, target_type->child))
        return mirEmitOptionalSome(state, value, target_type, node->filename, node->line_number, node->column_number);
    TypeSystemExprType source_type = newValueExprType(value_type);
    if(canImplicitConvertDataType(source_type, node, target_type))
        return mirEmitConvert(state, value, target_type, node->filename, node->line_number, node->column_number);
    return value;
}

static ASTFunctionParameter* mirNewDebugParam(ASTDataType *data_type)
{
    ASTFunctionParameter *parameter = (ASTFunctionParameter*) malloc(sizeof(ASTFunctionParameter));
    memset(parameter, 0, sizeof(ASTFunctionParameter));
    parameter->data_type = data_type;
    return parameter;
}

static ASTDataType* mirDebugExternType0Void(void)
{
    return newFunctionDataType(NULL, false, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_VOID));
}

static ASTDataType* mirDebugExternTypeCharPtrVoid(void)
{
    ASTFunctionParameter *parameter = mirNewDebugParam(
        newWrappedDataType(AST_DATA_TYPE_KIND_POINTER, false, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_CHAR))
    );
    return newFunctionDataType(parameter, false, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_VOID));
}

static ASTDataType* mirDebugExternTypeCharPtrI64Void(void)
{
    ASTFunctionParameter *file_param = mirNewDebugParam(
        newWrappedDataType(AST_DATA_TYPE_KIND_POINTER, false, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_CHAR))
    );
    ASTFunctionParameter *line_param = mirNewDebugParam(newPrimaryDataType(AST_PRIMARY_DATA_TYPE_I64));
    file_param->next = line_param;
    return newFunctionDataType(file_param, false, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_VOID));
}

static ASTDataType* mirDebugExternTypeI32Void(void)
{
    ASTFunctionParameter *parameter = mirNewDebugParam(newPrimaryDataType(AST_PRIMARY_DATA_TYPE_I32));
    return newFunctionDataType(parameter, false, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_VOID));
}

static ASTDataType* mirDebugExternTypeI64Void(void)
{
    ASTFunctionParameter *parameter = mirNewDebugParam(newPrimaryDataType(AST_PRIMARY_DATA_TYPE_I64));
    return newFunctionDataType(parameter, false, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_VOID));
}

static ASTDataType* mirDebugExternTypeU64Void(void)
{
    ASTFunctionParameter *parameter = mirNewDebugParam(newPrimaryDataType(AST_PRIMARY_DATA_TYPE_U64));
    return newFunctionDataType(parameter, false, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_VOID));
}

static ASTDataType* mirDebugExternTypeF64Void(void)
{
    ASTFunctionParameter *parameter = mirNewDebugParam(newPrimaryDataType(AST_PRIMARY_DATA_TYPE_F64));
    return newFunctionDataType(parameter, false, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_VOID));
}

static ASTDataType* mirDebugExternTypeVoidPtrVoid(void)
{
    ASTFunctionParameter *parameter = mirNewDebugParam(
        newWrappedDataType(AST_DATA_TYPE_KIND_POINTER, false, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_VOID))
    );
    return newFunctionDataType(parameter, false, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_VOID));
}

static void mirEmitDebugRuntimeCall(MirFunctionState *state, ASTNode *node,
                                    const char *symbol_name,
                                    ASTDataType *function_type,
                                    MirOperandList arguments)
{
    (void) mirEnsureExternFunction(
        state->lowering,
        symbol_name,
        function_type,
        node->filename,
        node->line_number,
        node->column_number
    );
    (void) mirEmitExternCall(state, symbol_name, function_type, arguments,
                             newPrimaryDataType(AST_PRIMARY_DATA_TYPE_VOID),
                             node->filename, node->line_number, node->column_number);
}

static void mirEmitDebugWriteCStrLiteral(MirFunctionState *state, ASTNode *node, const char *text)
{
    ASTDataType *char_ptr_type = newWrappedDataType(AST_DATA_TYPE_KIND_POINTER, false,
                                                    newPrimaryDataType(AST_PRIMARY_DATA_TYPE_CHAR));
    ASTDataType *global_type = newArrayDataType(newPrimaryDataType(AST_PRIMARY_DATA_TYPE_CHAR), strlen(text) + 1);
    const char *global_name = mirEnsureStringLiteralGlobal(state->lowering, text);
    MirValueId global_addr = mirEmitGlobalAddr(state, global_name, global_type,
                                               node->filename, node->line_number, node->column_number);
    MirValueId ptr_value = mirEmitConvert(state, global_addr, char_ptr_type,
                                          node->filename, node->line_number, node->column_number);
    MirOperandList arguments = newMirOperandList(1);
    arguments.items[0] = ptr_value;
    mirEmitDebugRuntimeCall(state, node, "mote_debug_write_cstr", mirDebugExternTypeCharPtrVoid(), arguments);
}

static void mirEmitDebugWriteCharLiteral(MirFunctionState *state, ASTNode *node, int ch)
{
    MirOperandList arguments = newMirOperandList(1);
    arguments.items[0] = mirEmitConstInt(state, ch, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_I32),
                                         node->filename, node->line_number, node->column_number);
    mirEmitDebugRuntimeCall(state, node, "mote_debug_write_char", mirDebugExternTypeI32Void(), arguments);
}

static MirValueId mirDebugValueAddress(MirFunctionState *state, MirLowerScope *scope, ASTNode *node)
{
    if(isAddressableExpr(node))
        return lowerExprAsAddress(state, scope, node);
    return lowerExprMaterializedAddress(state, scope, node);
}

static void mirEmitDebugTypeAndOpen(MirFunctionState *state, ASTNode *node, ASTDataType *data_type)
{
    char type_buffer[512] = {0};
    appendASTDataTypeString(data_type, type_buffer, sizeof(type_buffer));
    mirEmitDebugWriteCStrLiteral(state, node, type_buffer);
    mirEmitDebugWriteCharLiteral(state, node, '(');
}

static void mirEmitDebugClose(MirFunctionState *state, ASTNode *node)
{
    mirEmitDebugWriteCharLiteral(state, node, ')');
}

static void mirEmitDebugWriteBool(MirFunctionState *state, ASTNode *node, MirValueId value)
{
    MirFunction *function = mirCurrentFunction(state);
    MirBlockId true_block = mirCreateBlock(state->lowering, function, "debug_bool_true");
    MirBlockId false_block = mirCreateBlock(state->lowering, function, "debug_bool_false");
    MirBlockId end_block = mirCreateBlock(state->lowering, function, "debug_bool_end");
    mirEmitCondBr(state, value, true_block, false_block);

    mirSwitchToBlock(state, true_block);
    mirEmitDebugWriteCStrLiteral(state, node, "true");
    mirEmitBr(state, end_block);

    mirSwitchToBlock(state, false_block);
    mirEmitDebugWriteCStrLiteral(state, node, "false");
    mirEmitBr(state, end_block);

    mirSwitchToBlock(state, end_block);
}

static void mirEmitDebugValue(MirFunctionState *state, MirLowerScope *scope, ASTNode *node,
                              ASTDataType *data_type, MirValueId value, int depth);

static bool mirDebugBodyOwnsTypeEnvelope(ASTDataType *resolved_type)
{
    return resolved_type != NULL &&
           (resolved_type->kind == AST_DATA_TYPE_KIND_ENUM ||
            resolved_type->kind == AST_DATA_TYPE_KIND_FUNCTION);
}

static void mirEmitDebugValueBody(MirFunctionState *state, MirLowerScope *scope, ASTNode *node,
                                  ASTDataType *data_type, MirValueId value, int depth)
{
    ASTDataType *resolved_type = resolveNamedDataType(data_type, &(scope->type_scope), scope->self_data_type);
    if(resolved_type == NULL)
        resolved_type = data_type;

    switch(resolved_type->kind)
    {
        case AST_DATA_TYPE_KIND_PRIMARY:
            switch(resolved_type->primary)
            {
                case AST_PRIMARY_DATA_TYPE_VOID:
                    mirEmitDebugWriteCStrLiteral(state, node, "void");
                    return;
                case AST_PRIMARY_DATA_TYPE_BOOL:
                    mirEmitDebugWriteBool(state, node, value);
                    return;
                case AST_PRIMARY_DATA_TYPE_CHAR: {
                    ASTDataType *i32_type = newPrimaryDataType(AST_PRIMARY_DATA_TYPE_I32);
                    MirValueId ch_i32 = mirEmitConvert(state, value, i32_type,
                                                       node->filename, node->line_number, node->column_number);
                    mirEmitDebugWriteCharLiteral(state, node, '\'');
                    MirOperandList arguments = newMirOperandList(1);
                    arguments.items[0] = ch_i32;
                    mirEmitDebugRuntimeCall(state, node, "mote_debug_write_char", mirDebugExternTypeI32Void(), arguments);
                    mirEmitDebugWriteCharLiteral(state, node, '\'');
                    return;
                }
                case AST_PRIMARY_DATA_TYPE_I8:
                case AST_PRIMARY_DATA_TYPE_I16:
                case AST_PRIMARY_DATA_TYPE_I32:
                case AST_PRIMARY_DATA_TYPE_I64: {
                    MirValueId widened = value;
                    if(resolved_type->primary != AST_PRIMARY_DATA_TYPE_I64)
                        widened = mirEmitConvert(state, value, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_I64),
                                                 node->filename, node->line_number, node->column_number);
                    MirOperandList arguments = newMirOperandList(1);
                    arguments.items[0] = widened;
                    mirEmitDebugRuntimeCall(state, node, "mote_debug_write_i64", mirDebugExternTypeI64Void(), arguments);
                    return;
                }
                case AST_PRIMARY_DATA_TYPE_U8:
                case AST_PRIMARY_DATA_TYPE_U16:
                case AST_PRIMARY_DATA_TYPE_U32:
                case AST_PRIMARY_DATA_TYPE_U64: {
                    MirValueId widened = value;
                    if(resolved_type->primary != AST_PRIMARY_DATA_TYPE_U64)
                        widened = mirEmitConvert(state, value, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_U64),
                                                 node->filename, node->line_number, node->column_number);
                    MirOperandList arguments = newMirOperandList(1);
                    arguments.items[0] = widened;
                    mirEmitDebugRuntimeCall(state, node, "mote_debug_write_u64", mirDebugExternTypeU64Void(), arguments);
                    return;
                }
                case AST_PRIMARY_DATA_TYPE_F32:
                case AST_PRIMARY_DATA_TYPE_F64: {
                    MirValueId widened = value;
                    if(resolved_type->primary != AST_PRIMARY_DATA_TYPE_F64)
                        widened = mirEmitConvert(state, value, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_F64),
                                                 node->filename, node->line_number, node->column_number);
                    MirOperandList arguments = newMirOperandList(1);
                    arguments.items[0] = widened;
                    mirEmitDebugRuntimeCall(state, node, "mote_debug_write_f64", mirDebugExternTypeF64Void(), arguments);
                    return;
                }
                case AST_PRIMARY_DATA_TYPE_TYPE:
                    mirEmitDebugWriteCStrLiteral(state, node, "<type>");
                    return;
                case AST_PRIMARY_DATA_TYPE_F8:
                case AST_PRIMARY_DATA_TYPE_F16:
                    mirLoweringAbortNode("M2017", node,
                                         "@debug does not support this floating-point width in LLVM lowering yet",
                                         "use f32 or f64 for now");
                    return;
            }
            return;
        case AST_DATA_TYPE_KIND_POINTER:
        case AST_DATA_TYPE_KIND_REFERENCE: {
            MirValueId ptr_value = value;
            if(resolved_type->kind == AST_DATA_TYPE_KIND_REFERENCE)
                ptr_value = mirEmitConvert(state, value,
                                           newWrappedDataType(AST_DATA_TYPE_KIND_POINTER, resolved_type->mutable,
                                                              cloneDataType(resolved_type->child)),
                                           node->filename, node->line_number, node->column_number);
            MirValueId void_ptr = mirEmitConvert(state, ptr_value,
                                                 newWrappedDataType(AST_DATA_TYPE_KIND_POINTER, false,
                                                                    newPrimaryDataType(AST_PRIMARY_DATA_TYPE_VOID)),
                                                 node->filename, node->line_number, node->column_number);
            MirOperandList arguments = newMirOperandList(1);
            arguments.items[0] = void_ptr;
            mirEmitDebugRuntimeCall(state, node, "mote_debug_write_ptr", mirDebugExternTypeVoidPtrVoid(), arguments);
            return;
        }
        case AST_DATA_TYPE_KIND_FUNCTION: {
            MirValueId closure_address = mirDebugValueAddress(state, scope, node);
            ASTDataType *void_ptr_type = newWrappedDataType(AST_DATA_TYPE_KIND_POINTER, false,
                                                            newPrimaryDataType(AST_PRIMARY_DATA_TYPE_VOID));
            mirEmitDebugTypeAndOpen(state, node, data_type);
            mirEmitDebugWriteCStrLiteral(state, node, "code=");
            MirValueId code_ptr = mirEmitFieldPtr(state, closure_address, void_ptr_type, "", 0,
                                                  node->filename, node->line_number, node->column_number);
            MirValueId code_value = mirEmitLoad(state, code_ptr, void_ptr_type,
                                                node->filename, node->line_number, node->column_number);
            MirOperandList code_args = newMirOperandList(1);
            code_args.items[0] = code_value;
            mirEmitDebugRuntimeCall(state, node, "mote_debug_write_ptr", mirDebugExternTypeVoidPtrVoid(), code_args);
            mirEmitDebugWriteCStrLiteral(state, node, ", env=");
            MirValueId env_ptr = mirEmitFieldPtr(state, closure_address, void_ptr_type, "", 1,
                                                 node->filename, node->line_number, node->column_number);
            MirValueId env_value = mirEmitLoad(state, env_ptr, void_ptr_type,
                                               node->filename, node->line_number, node->column_number);
            MirOperandList env_args = newMirOperandList(1);
            env_args.items[0] = env_value;
            mirEmitDebugRuntimeCall(state, node, "mote_debug_write_ptr", mirDebugExternTypeVoidPtrVoid(), env_args);
            mirEmitDebugClose(state, node);
            return;
        }
        case AST_DATA_TYPE_KIND_ENUM: {
            mirEmitDebugTypeAndOpen(state, node, data_type);
            MirFunction *function = mirCurrentFunction(state);
            MirBlockId end_block = mirCreateBlock(state->lowering, function, "debug_enum_end");
            MirBlockId next_block = -1;
            ASTEnumVariant *variant = resolved_type->variants;
            int ordinal = 0;
            while(variant != NULL)
            {
                MirBlockId match_block = mirCreateBlock(state->lowering, function, "debug_enum_match");
                next_block = mirCreateBlock(state->lowering, function, "debug_enum_next");
                MirValueId ordinal_value = mirEmitConstInt(state, ordinal, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_I32),
                                                           node->filename, node->line_number, node->column_number);
                MirValueId is_match = mirEmitBinary(state, MIR_INST_EQ, value, ordinal_value,
                                                    newPrimaryDataType(AST_PRIMARY_DATA_TYPE_BOOL),
                                                    node->filename, node->line_number, node->column_number);
                mirEmitCondBr(state, is_match, match_block, next_block);

                mirSwitchToBlock(state, match_block);
                mirEmitDebugWriteCStrLiteral(state, node, astUserFacingIdentifier(variant->identifier));
                mirEmitBr(state, end_block);

                mirSwitchToBlock(state, next_block);
                variant = variant->next;
                ordinal++;
            }
            mirEmitDebugWriteCStrLiteral(state, node, "<invalid>");
            mirEmitBr(state, end_block);
            mirSwitchToBlock(state, end_block);
            mirEmitDebugClose(state, node);
            return;
        }
        case AST_DATA_TYPE_KIND_ARRAY: {
            MirValueId base_address = mirDebugValueAddress(state, scope, node);
            mirEmitDebugWriteCharLiteral(state, node, '[');
            for(long long int i = 0; i < resolved_type->array_length; i++)
            {
                if(i > 0)
                    mirEmitDebugWriteCStrLiteral(state, node, ", ");
                MirValueId index_value = mirEmitConstInt(state, i, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_I64),
                                                         node->filename, node->line_number, node->column_number);
                MirValueId element_ptr = mirEmitIndexPtr(state, base_address, index_value, resolved_type->child, false,
                                                         node->filename, node->line_number, node->column_number);
                MirValueId element_value = mirEmitLoad(state, element_ptr, resolved_type->child,
                                                       node->filename, node->line_number, node->column_number);
                mirEmitDebugValue(state, scope, node, resolved_type->child, element_value, depth + 1);
            }
            mirEmitDebugWriteCharLiteral(state, node, ']');
            return;
        }
        case AST_DATA_TYPE_KIND_SLICE: {
            MirValueId slice_address = mirDebugValueAddress(state, scope, node);
            ASTDataType *ptr_field_type = newWrappedDataType(AST_DATA_TYPE_KIND_POINTER, true,
                                                             cloneDataType(resolved_type->child));
            MirValueId ptr_field_ptr = mirEmitFieldPtr(state, slice_address, ptr_field_type, "ptr", 0,
                                                       node->filename, node->line_number, node->column_number);
            MirValueId len_field_ptr = mirEmitFieldPtr(state, slice_address, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_I64),
                                                       "len", 1,
                                                       node->filename, node->line_number, node->column_number);
            MirValueId ptr_value = mirEmitLoad(state, ptr_field_ptr, ptr_field_type,
                                               node->filename, node->line_number, node->column_number);
            MirValueId len_value = mirEmitLoad(state, len_field_ptr, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_I64),
                                               node->filename, node->line_number, node->column_number);
            mirEmitDebugWriteCStrLiteral(state, node, "len=");
            MirOperandList len_args = newMirOperandList(1);
            len_args.items[0] = len_value;
            mirEmitDebugRuntimeCall(state, node, "mote_debug_write_i64", mirDebugExternTypeI64Void(), len_args);
            mirEmitDebugWriteCStrLiteral(state, node, ", ptr=");
            MirValueId void_ptr = mirEmitConvert(state, ptr_value,
                                                 newWrappedDataType(AST_DATA_TYPE_KIND_POINTER, false,
                                                                    newPrimaryDataType(AST_PRIMARY_DATA_TYPE_VOID)),
                                                 node->filename, node->line_number, node->column_number);
            MirOperandList ptr_args = newMirOperandList(1);
            ptr_args.items[0] = void_ptr;
            mirEmitDebugRuntimeCall(state, node, "mote_debug_write_ptr", mirDebugExternTypeVoidPtrVoid(), ptr_args);
            mirEmitDebugWriteCStrLiteral(state, node, ", [");

            MirValueId index_slot = mirEmitAlloca(state, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_I64),
                                                  node->filename, node->line_number, node->column_number);
            mirEmitStore(state, index_slot,
                         mirEmitConstInt(state, 0, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_I64),
                                         node->filename, node->line_number, node->column_number),
                         node->filename, node->line_number, node->column_number);

            MirFunction *function = mirCurrentFunction(state);
            MirBlockId cond_block = mirCreateBlock(state->lowering, function, "debug_slice_cond");
            MirBlockId body_block = mirCreateBlock(state->lowering, function, "debug_slice_body");
            MirBlockId end_block = mirCreateBlock(state->lowering, function, "debug_slice_end");
            mirEmitBr(state, cond_block);

            mirSwitchToBlock(state, cond_block);
            MirValueId index_value = mirEmitLoad(state, index_slot, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_I64),
                                                 node->filename, node->line_number, node->column_number);
            MirValueId cond = mirEmitBinary(state, MIR_INST_LT, index_value, len_value,
                                            newPrimaryDataType(AST_PRIMARY_DATA_TYPE_BOOL),
                                            node->filename, node->line_number, node->column_number);
            mirEmitCondBr(state, cond, body_block, end_block);

            mirSwitchToBlock(state, body_block);
            MirValueId is_first = mirEmitBinary(state, MIR_INST_EQ, index_value,
                                                mirEmitConstInt(state, 0, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_I64),
                                                                node->filename, node->line_number, node->column_number),
                                                newPrimaryDataType(AST_PRIMARY_DATA_TYPE_BOOL),
                                                node->filename, node->line_number, node->column_number);
            MirBlockId first_block = mirCreateBlock(state->lowering, function, "debug_slice_first");
            MirBlockId later_block = mirCreateBlock(state->lowering, function, "debug_slice_later");
            MirBlockId after_sep_block = mirCreateBlock(state->lowering, function, "debug_slice_after_sep");
            mirEmitCondBr(state, is_first, first_block, later_block);

            mirSwitchToBlock(state, first_block);
            mirEmitBr(state, after_sep_block);

            mirSwitchToBlock(state, later_block);
            mirEmitDebugWriteCStrLiteral(state, node, ", ");
            mirEmitBr(state, after_sep_block);

            mirSwitchToBlock(state, after_sep_block);
            MirValueId element_ptr = mirEmitIndexPtr(state, ptr_value, index_value, resolved_type->child, true,
                                                     node->filename, node->line_number, node->column_number);
            MirValueId element_value = mirEmitLoad(state, element_ptr, resolved_type->child,
                                                   node->filename, node->line_number, node->column_number);
            mirEmitDebugValue(state, scope, node, resolved_type->child, element_value, depth + 1);
            MirValueId next_index = mirEmitBinary(state, MIR_INST_ADD, index_value,
                                                  mirEmitConstInt(state, 1, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_I64),
                                                                  node->filename, node->line_number, node->column_number),
                                                  newPrimaryDataType(AST_PRIMARY_DATA_TYPE_I64),
                                                  node->filename, node->line_number, node->column_number);
            mirEmitStore(state, index_slot, next_index, node->filename, node->line_number, node->column_number);
            mirEmitBr(state, cond_block);

            mirSwitchToBlock(state, end_block);
            mirEmitDebugWriteCharLiteral(state, node, ']');
            return;
        }
        case AST_DATA_TYPE_KIND_OPTIONAL: {
            MirValueId optional_address = mirDebugValueAddress(state, scope, node);
            MirValueId has_value_ptr = mirEmitFieldPtr(state, optional_address, mirOptionalBoolType(),
                                                       "has_value", 0,
                                                       node->filename, node->line_number, node->column_number);
            MirValueId has_value = mirEmitLoad(state, has_value_ptr, mirOptionalBoolType(),
                                               node->filename, node->line_number, node->column_number);
            MirFunction *function = mirCurrentFunction(state);
            MirBlockId some_block = mirCreateBlock(state->lowering, function, "debug_optional_some");
            MirBlockId none_block = mirCreateBlock(state->lowering, function, "debug_optional_none");
            MirBlockId end_block = mirCreateBlock(state->lowering, function, "debug_optional_end");
            mirEmitCondBr(state, has_value, some_block, none_block);

            mirSwitchToBlock(state, none_block);
            mirEmitDebugWriteCStrLiteral(state, node, "null");
            mirEmitBr(state, end_block);

            mirSwitchToBlock(state, some_block);
            MirValueId inner_ptr = mirEmitFieldPtr(state, optional_address, resolved_type->child,
                                                   "value", 1,
                                                   node->filename, node->line_number, node->column_number);
            MirValueId inner_value = mirEmitLoad(state, inner_ptr, resolved_type->child,
                                                 node->filename, node->line_number, node->column_number);
            mirEmitDebugValue(state, scope, node, resolved_type->child, inner_value, depth + 1);
            mirEmitBr(state, end_block);

            mirSwitchToBlock(state, end_block);
            return;
        }
        case AST_DATA_TYPE_KIND_STRUCT: {
            mirEmitDebugWriteCharLiteral(state, node, '{');
            ASTStructMember *member = resolved_type->members;
            int field_index = 0;
            bool first = true;
            MirValueId base_address = mirDebugValueAddress(state, scope, node);
            while(member != NULL)
            {
                if(member->value == NULL)
                {
                    if(!first)
                        mirEmitDebugWriteCStrLiteral(state, node, ", ");
                    mirEmitDebugWriteCStrLiteral(state, node, astUserFacingIdentifier(member->identifier));
                    mirEmitDebugWriteCStrLiteral(state, node, ": ");
                    MirValueId field_ptr = mirEmitFieldPtr(state, base_address, member->data_type,
                                                           member->identifier, field_index,
                                                           node->filename, node->line_number, node->column_number);
                    MirValueId field_value = mirEmitLoad(state, field_ptr, member->data_type,
                                                         node->filename, node->line_number, node->column_number);
                    mirEmitDebugValue(state, scope, node, member->data_type, field_value, depth + 1);
                    first = false;
                    field_index++;
                }
                member = member->next;
            }
            mirEmitDebugWriteCharLiteral(state, node, '}');
            return;
        }
        default:
            mirLoweringAbortNodeFormatted("M2018", node,
                                          "@debug lowering is missing this type shape",
                                          "unsupported @debug type kind %d",
                                          resolved_type->kind);
    }
}

static void mirEmitDebugValue(MirFunctionState *state, MirLowerScope *scope, ASTNode *node,
                              ASTDataType *data_type, MirValueId value, int depth)
{
    (void) depth;
    ASTDataType *resolved_type = resolveNamedDataType(data_type, &(scope->type_scope), scope->self_data_type);
    if(resolved_type == NULL)
        resolved_type = data_type;
    if(!mirDebugBodyOwnsTypeEnvelope(resolved_type))
        mirEmitDebugTypeAndOpen(state, node, data_type);
    mirEmitDebugValueBody(state, scope, node, data_type, value, depth);
    if(!mirDebugBodyOwnsTypeEnvelope(resolved_type))
        mirEmitDebugClose(state, node);
}

static MirValueId mirBindingAddress(MirFunctionState *state, MirRuntimeBinding *binding, ASTNode *use_node)
{
    if(binding->kind == MIR_RUNTIME_BINDING_ALIAS_ADDRESS || binding->kind == MIR_RUNTIME_BINDING_LOCAL_SLOT)
        return binding->local_value;
    if(binding->kind == MIR_RUNTIME_BINDING_GLOBAL_SLOT)
        return mirEmitGlobalAddr(state, binding->global_name, binding->declared_data_type,
                                 use_node->filename, use_node->line_number, use_node->column_number);

    mirLoweringAbortNodeFormatted("M2003", use_node,
                                  "runtime address is unavailable here",
                                  "identifier `%s` has no runtime address",
                                  astUserFacingIdentifier(binding->identifier));
}

static MirValueId lowerVariableValue(MirFunctionState *state, MirLowerScope *scope, ASTNode *node, ASTDataType *expected_type)
{
    MirRuntimeBinding *binding = findMirRuntimeBinding(scope, node->identifier);
    if(binding == NULL || binding->kind == MIR_RUNTIME_BINDING_COMPTIME_ONLY)
    {
        VariableInfo *variable_info = findVariableInfo(&(scope->type_scope), node->identifier);
        if(variable_info != NULL && variable_info->function_value != NULL)
            return lowerFunctionExprAsValue(state, scope, variable_info->function_value, node->identifier, scope->self_data_type);
        mirLoweringAbortNodeFormatted("M2004", node,
                                      "this name does not exist as a runtime value",
                                      "variable `%s` is compile-time only or unavailable",
                                      astUserFacingIdentifier(node->identifier));
    }

    ASTDataType *expr_type = binding->declared_data_type != NULL
        ? cloneDataType(binding->declared_data_type)
        : mirResolvedExprValueType(node, &(scope->type_scope));
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

static const char* mirEnsureStringLiteralGlobal(MirLowering *lowering, const char *value)
{
    char name[MIR_MAX_NAME_LENGTH];
    snprintf(name, sizeof(name), "__mote_str_%d", lowering->unique_global_counter++);

    ASTDataType *string_type = newArrayDataType(
        newPrimaryDataType(AST_PRIMARY_DATA_TYPE_CHAR),
        strlen(value) + 1
    );
    MirGlobal *global = mirEnsureGlobal(lowering, name, string_type, false);
    global->has_const_string_initializer = true;
    strcpy(global->const_string_initializer, value);
    return global->name;
}

static void mirDeclareVariableInfo(MirLowerScope *scope, ASTNode *assign_node, ASTDataType *declared_type,
                                   TypeSystemExprType expr_type)
{
    const char *binding_name = assign_node->lhs != NULL &&
                               assign_node->lhs->kind == AST_EXPR_VARIABLE &&
                               assign_node->lhs->identifier[0] != '\0'
        ? assign_node->lhs->identifier
        : assign_node->identifier;
    int existing_index = findVariableInfoInScope(&(scope->type_scope), binding_name);
    VariableInfo *variable_info = existing_index >= 0
        ? &(scope->type_scope.variable_infos[existing_index])
        : declareVariableInfo(&(scope->type_scope), binding_name);
    variable_info->mutable = assign_node->modifier.mutable;
    variable_info->predeclared = false;
    variable_info->operator_kind = assign_node->operator_kind;
    variable_info->data_type = cloneDataType(declared_type);
    if(expr_type.kind == TYPE_SYSTEM_EXPR_TYPE_TYPE)
        variable_info->type_value = cloneDataType(expr_type.data_type);
    else
        variable_info->type_value = NULL;
    variable_info->function_value = resolveFunctionValueExpr(assign_node->rhs, &(scope->type_scope));
    variable_info->extern_value = resolveExternValueExpr(assign_node->rhs, &(scope->type_scope));
}

static void mirPredeclareTopLevelBindings(MirLowerScope *scope, ASTNode *block_node)
{
    if(scope == NULL || block_node == NULL)
        return;

    for(ASTNode *statement = block_node->lhs; statement != NULL; statement = statement->next)
    {
        if(statement->kind != AST_ASSIGN ||
           statement->lhs == NULL ||
           statement->lhs->kind != AST_EXPR_VARIABLE)
            continue;

        const char *binding_name = statement->lhs->identifier;
        if(statement->rhs != NULL && statement->rhs->kind == AST_EXPR_STRUCT)
        {
            if(findTypeInfoInScope(&(scope->type_scope), binding_name) < 0)
            {
                TypeInfo *type_info = declareTypeInfo(&(scope->type_scope), binding_name);
                type_info->data_type = newStructDataType(binding_name, NULL);
                type_info->predeclared = true;
            }
        }
        else if(statement->rhs != NULL && statement->rhs->kind == AST_EXPR_ENUM)
        {
            if(findTypeInfoInScope(&(scope->type_scope), binding_name) < 0)
            {
                TypeInfo *type_info = declareTypeInfo(&(scope->type_scope), binding_name);
                type_info->data_type = newEnumDataType(binding_name, NULL);
                type_info->predeclared = true;
            }
        }

        if(findVariableInfoInScope(&(scope->type_scope), binding_name) < 0)
        {
            VariableInfo *variable_info = declareVariableInfo(&(scope->type_scope), binding_name);
            variable_info->mutable = statement->modifier.mutable;
            variable_info->predeclared = true;
            if(isTypeDeclAssign(statement, &(scope->type_scope)))
                variable_info->data_type = newPrimaryDataType(AST_PRIMARY_DATA_TYPE_TYPE);
            else if(statement->data_type != NULL && isExplicitDeclared(statement))
                variable_info->data_type = cloneDataType(statement->data_type);
            else
                variable_info->data_type = newInferDataType();
            variable_info->operator_kind = statement->operator_kind;
            variable_info->value_expr = statement->rhs;
            if(statement->rhs != NULL && statement->rhs->kind == AST_EXPR_FUNCTION)
                variable_info->function_value = statement->rhs;
            if(statement->rhs != NULL &&
               statement->rhs->kind == AST_EXPR_BUILTIN &&
               strcmp(statement->rhs->identifier, "extern") == 0)
                variable_info->extern_value = statement->rhs;
        }
    }
}

static void mirPredeclareTopLevelRuntimeBindings(MirLowering *lowering, MirLowerScope *scope, ASTNode *block_node)
{
    if(lowering == NULL || scope == NULL || block_node == NULL)
        return;

    for(ASTNode *statement = block_node->lhs; statement != NULL; statement = statement->next)
    {
        if(statement->kind != AST_ASSIGN ||
           statement->lhs == NULL ||
           statement->lhs->kind != AST_EXPR_VARIABLE)
            continue;

        const char *binding_name = statement->lhs->identifier;
        if(findMirRuntimeBindingInScope(scope, binding_name) != NULL)
            continue;

        ASTNode *resolved_function_value = resolveFunctionValueExpr(statement->rhs, &(scope->type_scope));
        ASTNode *extern_value = resolveExternValueExpr(statement->rhs, &(scope->type_scope));
        ASTDataType *declared_type = resolveNamedDataType(statement->data_type, &(scope->type_scope), scope->self_data_type);
        TypeSystemExprType expr_type = inferExprType(statement->rhs, &(scope->type_scope));

        if(resolved_function_value != NULL)
            continue;

        if(expr_type.kind == TYPE_SYSTEM_EXPR_TYPE_TYPE ||
           mirIsCompileTimeTypeFactory(declared_type) ||
           (statement->rhs->kind == AST_EXPR_BUILTIN &&
            strcmp(statement->rhs->identifier, "extern") == 0 &&
            declared_type->is_variadic))
        {
            MirRuntimeBinding *binding = declareMirRuntimeBinding(scope, binding_name);
            binding->mutable = statement->modifier.mutable;
            binding->declared_data_type = cloneDataType(declared_type);
            binding->type_value = expr_type.kind == TYPE_SYSTEM_EXPR_TYPE_TYPE ? cloneDataType(expr_type.data_type) : NULL;
            binding->function_value = resolved_function_value;
            binding->extern_value = extern_value;
            binding->kind = MIR_RUNTIME_BINDING_COMPTIME_ONLY;
            continue;
        }

        if(declared_type->kind == AST_DATA_TYPE_KIND_REFERENCE)
            continue;

        MirRuntimeBinding *binding = declareMirRuntimeBinding(scope, binding_name);
        binding->mutable = statement->modifier.mutable;
        binding->declared_data_type = cloneDataType(declared_type);
        binding->function_value = resolved_function_value;
        binding->extern_value = extern_value;
        binding->kind = MIR_RUNTIME_BINDING_GLOBAL_SLOT;
        strcpy(binding->global_name, binding_name);
        mirEnsureGlobal(lowering, binding_name, declared_type, statement->modifier.mutable);
    }
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
        mirLoweringAbortInternal("ICE0302",
                                 "function call argument count mismatch during MIR instantiation",
                                 "type checking should reject mismatched call arity before MIR lowering");

    return inst_scope;
}

static ASTFunctionParameter* instantiateRuntimeFunctionParameters(ASTFunctionParameter *parameter,
                                                                  ScopeFrame *scope,
                                                                  ASTDataType *self_data_type)
{
    ASTFunctionParameter *head = NULL;
    ASTFunctionParameter *tail = NULL;
    while(parameter)
    {
        ASTDataType *resolved_type = resolveNamedDataType(parameter->data_type, scope, self_data_type);
        if(!(resolved_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
             resolved_type->primary == AST_PRIMARY_DATA_TYPE_TYPE))
        {
            ASTFunctionParameter *new_parameter = (ASTFunctionParameter*) malloc(sizeof(ASTFunctionParameter));
            memset(new_parameter, 0, sizeof(ASTFunctionParameter));
            new_parameter->filename = parameter->filename;
            new_parameter->line_number = parameter->line_number;
            new_parameter->column_number = parameter->column_number;
            new_parameter->end_line_number = parameter->end_line_number;
            new_parameter->end_column_number = parameter->end_column_number;
            strcpy(new_parameter->identifier, parameter->identifier);
            new_parameter->data_type = resolved_type;

            if(head == NULL)
                head = new_parameter;
            else
                tail->next = new_parameter;
            tail = new_parameter;
        }
        parameter = parameter->next;
    }
    return head;
}

static ASTNode* instantiateRuntimeFunctionExpr(ASTNode *function_expr, MirLowerScope *inst_scope)
{
    ASTNode *specialized = (ASTNode*) malloc(sizeof(ASTNode));
    memset(specialized, 0, sizeof(ASTNode));
    *specialized = *function_expr;
    specialized->parameters = instantiateRuntimeFunctionParameters(function_expr->parameters,
                                                                   &(inst_scope->type_scope),
                                                                   inst_scope->self_data_type);
    specialized->return_data_type = resolveNamedDataType(function_expr->return_data_type,
                                                         &(inst_scope->type_scope),
                                                         inst_scope->self_data_type);
    specialized->data_type = newFunctionDataType(cloneFunctionParameters(specialized->parameters),
                                                 function_expr->is_variadic,
                                                 cloneDataType(specialized->return_data_type));
    return specialized;
}

static int mirFindSpecializedFunctionCacheEntry(MirLowering *lowering, ASTNode *source_function,
                                                ASTDataType *self_data_type, ASTDataType *function_type)
{
    for(int i = 0; i < lowering->specialized_function_count; i++)
    {
        MirSpecializedFunctionCacheEntry *entry = &(lowering->specialized_functions[i]);
        if(entry->source_function != source_function)
            continue;
        if(!isSameDataType(entry->self_data_type, self_data_type))
            continue;
        if(!isSameDataType(entry->function_type, function_type))
            continue;
        return i;
    }
    return -1;
}

static int mirCacheSpecializedFunctionDefinition(MirLowering *lowering, ASTNode *source_function,
                                                 ASTDataType *self_data_type, ASTDataType *function_type,
                                                 int function_index)
{
    lowering->specialized_functions = (MirSpecializedFunctionCacheEntry*) realloc(
        lowering->specialized_functions,
        sizeof(MirSpecializedFunctionCacheEntry) * (lowering->specialized_function_count + 1)
    );
    MirSpecializedFunctionCacheEntry *entry =
        &(lowering->specialized_functions[lowering->specialized_function_count++]);
    memset(entry, 0, sizeof(MirSpecializedFunctionCacheEntry));
    entry->source_function = source_function;
    entry->self_data_type = cloneDataType(self_data_type);
    entry->function_type = cloneDataType(function_type);
    entry->function_index = function_index;
    return function_index;
}

static MirValueId lowerRuntimeSpecializedFunctionValue(MirFunctionState *state, MirLowerScope *scope,
                                                       ASTNode *source_function, ASTNode *call_arguments,
                                                       const char *name_hint, ASTDataType *self_data_type,
                                                       ASTFunctionParameter **out_runtime_parameters,
                                                       ASTDataType **out_return_type)
{
    MirLowerScope *inst_scope = instantiateFunctionCallScope(
        state,
        scope,
        source_function,
        call_arguments,
        self_data_type
    );
    ASTNode *specialized_function = instantiateRuntimeFunctionExpr(source_function, inst_scope);

    if(out_runtime_parameters != NULL)
        *out_runtime_parameters = specialized_function->parameters;
    if(out_return_type != NULL)
        *out_return_type = specialized_function->return_data_type;

    if(source_function->captures == NULL)
    {
        int cache_index = mirFindSpecializedFunctionCacheEntry(
            state->lowering,
            source_function,
            self_data_type,
            specialized_function->data_type
        );
        int function_index = -1;
        if(cache_index >= 0)
            function_index = state->lowering->specialized_functions[cache_index].function_index;
        else
        {
            function_index = lowerFunctionExprDefinition(
                state->lowering,
                inst_scope,
                specialized_function,
                name_hint,
                self_data_type
            );
            mirCacheSpecializedFunctionDefinition(
                state->lowering,
                source_function,
                self_data_type,
                specialized_function->data_type,
                function_index
            );
        }

        MirFunction *mir_function = state->lowering->program->functions[function_index];
        return mirEmitFunctionRef(state, mir_function->name, specialized_function->data_type,
                                  source_function->filename, source_function->line_number, source_function->column_number);
    }

    return lowerFunctionExprAsValue(
        state,
        inst_scope,
        specialized_function,
        name_hint,
        self_data_type
    );
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
    if(!callee_is_comptime_only || returned_expr->kind != AST_EXPR_FUNCTION)
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
            (void) scope;
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
        mirLoweringAbortNode("M2005", function_expr,
                             "generic function value requires compile-time specialization before runtime lowering",
                             "specialize this generic function before using it as a runtime value");

    int function_index = lowerFunctionExprDefinition(state->lowering, scope, function_expr, name_hint, self_data_type);
    MirFunction *mir_function = state->lowering->program->functions[function_index];

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
            mirLoweringAbortInternal("ICE0303",
                                     "lowered function may fall through without return",
                                     lowered_function->name);
    }

    return function_index;
}

static MirValueId lowerMethodFunctionValue(MirFunctionState *state, MirLowerScope *scope, ASTNode *member_node,
                                           ASTDataType *struct_type)
{
    ASTStructMember *member = findStructMember(struct_type, member_node->identifier);
    if(member == NULL || member->value == NULL)
        mirLoweringAbortNodeFormatted("M2006", member_node,
                                      "this member does not resolve to a method",
                                      "unknown method `%s`",
                                      member_node->identifier);

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

static bool tryGetDirectGenericFunctionValue(MirLowerScope *scope, ASTNode *expr,
                                             ASTNode **out_function_value,
                                             const char **out_name_hint)
{
    if(out_function_value != NULL)
        *out_function_value = NULL;
    if(out_name_hint != NULL)
        *out_name_hint = NULL;

    if(expr == NULL)
        return false;

    if(expr->kind == AST_EXPR_VARIABLE)
    {
        VariableInfo *variable_info = findVariableInfo(&(scope->type_scope), expr->identifier);
        if(variable_info != NULL && variable_info->function_value != NULL)
        {
            if(out_function_value != NULL)
                *out_function_value = variable_info->function_value;
            if(out_name_hint != NULL)
                *out_name_hint = expr->identifier;
            return true;
        }
        return false;
    }

    if(expr->kind == AST_EXPR_MEMBER)
    {
        TypeSystemExprType owner_type = inferExprType(expr->lhs, &(scope->type_scope));
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
        struct_type = resolveNamedDataType(struct_type, &(scope->type_scope), scope->self_data_type);

        if(!isStructDataType(struct_type))
            return false;

        ASTStructMember *member = findStructMember(struct_type, expr->identifier);
        if(member == NULL || member->value == NULL || member->value->kind != AST_EXPR_FUNCTION)
            return false;

        if(out_function_value != NULL)
            *out_function_value = member->value;
        if(out_name_hint != NULL)
            *out_name_hint = expr->identifier;
        return true;
    }

    return false;
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
    if(function_type->is_variadic)
        mirLoweringAbortNode("M2007", node,
                             "variadic extern values cannot be materialized as first-class closures",
                             "call the extern function directly instead of storing it as a runtime closure");

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

static MirValueId lowerTypeLayoutBuiltinExpr(MirFunctionState *state, MirLowerScope *scope, ASTNode *node,
                                             bool want_align)
{
    long long int value = inferTypeBuiltinLayoutValue(node, &(scope->type_scope),
                                                      want_align ? "alignof" : "sizeof",
                                                      want_align ? "expected `@alignof(Type)`" : "expected `@sizeof(Type)`",
                                                      want_align);
    return mirEmitConstInt(state, value, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_I64),
                           node->filename, node->line_number, node->column_number);
}

static MirValueId lowerZeroBuiltinExpr(MirFunctionState *state, MirLowerScope *scope, ASTNode *node,
                                       ASTDataType *expected_type)
{
    ASTDataType *value_type = mirResolvedExprValueType(node, &(scope->type_scope));
    MirValueId zero_value = mirEmitZero(state, value_type,
                                        node->filename, node->line_number, node->column_number);
    return mirMaybeConvertValue(state, scope, node, zero_value, expected_type);
}

static MirValueId lowerDebugBuiltinExpr(MirFunctionState *state, MirLowerScope *scope, ASTNode *node)
{
    MirOperandList begin_args = newMirOperandList(2);
    ASTDataType *char_ptr_type = newWrappedDataType(AST_DATA_TYPE_KIND_POINTER, false,
                                                    newPrimaryDataType(AST_PRIMARY_DATA_TYPE_CHAR));
    ASTDataType *file_global_type = newArrayDataType(newPrimaryDataType(AST_PRIMARY_DATA_TYPE_CHAR),
                                                     strlen(node->filename) + 1);
    const char *file_global = mirEnsureStringLiteralGlobal(state->lowering, node->filename);
    MirValueId file_addr = mirEmitGlobalAddr(state, file_global, file_global_type,
                                             node->filename, node->line_number, node->column_number);
    begin_args.items[0] = mirEmitConvert(state, file_addr, char_ptr_type,
                                         node->filename, node->line_number, node->column_number);
    begin_args.items[1] = mirEmitConstInt(state, node->line_number + 1, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_I64),
                                          node->filename, node->line_number, node->column_number);
    mirEmitDebugRuntimeCall(state, node, "mote_debug_begin", mirDebugExternTypeCharPtrI64Void(), begin_args);

    bool first = true;
    for(ASTNode *argument = node->lhs; argument != NULL; argument = argument->next)
    {
        if(!first)
            mirEmitDebugRuntimeCall(state, node, "mote_debug_sep", mirDebugExternType0Void(), newMirOperandList(0));
        first = false;

        TypeSystemExprType argument_type = inferExprType(argument, &(scope->type_scope));
        if(argument_type.kind == TYPE_SYSTEM_EXPR_TYPE_TYPE)
        {
            char type_buffer[512] = {0};
            appendASTDataTypeString(argument_type.data_type, type_buffer, sizeof(type_buffer));
            mirEmitDebugWriteCStrLiteral(state, node, "Type(");
            mirEmitDebugWriteCStrLiteral(state, node, type_buffer);
            mirEmitDebugWriteCharLiteral(state, node, ')');
            continue;
        }
        if(argument_type.kind == TYPE_SYSTEM_EXPR_TYPE_NULL)
        {
            mirEmitDebugWriteCStrLiteral(state, node, "null");
            continue;
        }

        ASTDataType *value_type = mirResolvedExprValueType(argument, &(scope->type_scope));
        MirValueId value = lowerExprAsValue(state, scope, argument, value_type);
        mirEmitDebugValue(state, scope, argument, value_type, value, 0);
    }

    mirEmitDebugRuntimeCall(state, node, "mote_debug_end", mirDebugExternType0Void(), newMirOperandList(0));
    return -1;
}

static MirValueId lowerSlicePtrValue(MirFunctionState *state, MirLowerScope *scope, ASTNode *slice_expr)
{
    TypeSystemExprType slice_type = inferExprType(slice_expr, &(scope->type_scope));
    if(slice_type.kind != TYPE_SYSTEM_EXPR_TYPE_VALUE || !isSliceDataType(slice_type.data_type))
        mirLoweringAbortNode("M2015", slice_expr,
                             "slice pointer extraction requires a slice value",
                             "type checking should reject non-slice operands here");

    MirValueId slice_address = isAddressableExpr(slice_expr)
                               ? lowerExprAsAddress(state, scope, slice_expr)
                               : lowerExprMaterializedAddress(state, scope, slice_expr);
    ASTDataType *ptr_field_type = newWrappedDataType(AST_DATA_TYPE_KIND_POINTER, true,
                                                     cloneDataType(slice_type.data_type->child));
    MirValueId ptr_field_address = mirEmitFieldPtr(state, slice_address, ptr_field_type, "ptr", 0,
                                                   slice_expr->filename, slice_expr->line_number, slice_expr->column_number);
    return mirEmitLoad(state, ptr_field_address, ptr_field_type,
                       slice_expr->filename, slice_expr->line_number, slice_expr->column_number);
}

static MirValueId lowerSliceLenValue(MirFunctionState *state, MirLowerScope *scope, ASTNode *slice_expr)
{
    TypeSystemExprType slice_type = inferExprType(slice_expr, &(scope->type_scope));
    if(slice_type.kind != TYPE_SYSTEM_EXPR_TYPE_VALUE || !isSliceDataType(slice_type.data_type))
        mirLoweringAbortNode("M2016", slice_expr,
                             "slice length extraction requires a slice value",
                             "type checking should reject non-slice operands here");

    MirValueId slice_address = isAddressableExpr(slice_expr)
                               ? lowerExprAsAddress(state, scope, slice_expr)
                               : lowerExprMaterializedAddress(state, scope, slice_expr);
    ASTDataType *len_field_type = newPrimaryDataType(AST_PRIMARY_DATA_TYPE_I64);
    MirValueId len_field_address = mirEmitFieldPtr(state, slice_address, len_field_type, "len", 1,
                                                   slice_expr->filename, slice_expr->line_number, slice_expr->column_number);
    return mirEmitLoad(state, len_field_address, len_field_type,
                       slice_expr->filename, slice_expr->line_number, slice_expr->column_number);
}

static MirValueId lowerLenBuiltinExpr(MirFunctionState *state, MirLowerScope *scope, ASTNode *node,
                                      ASTDataType *expected_type)
{
    MirValueId length_value = lowerSliceLenValue(state, scope, node->lhs);
    return mirMaybeConvertValue(state, scope, node, length_value, expected_type);
}

static MirValueId lowerPtrAddBuiltinExpr(MirFunctionState *state, MirLowerScope *scope, ASTNode *node,
                                         ASTDataType *expected_type)
{
    ASTNode *element_type_expr = node->lhs;
    ASTNode *pointer_expr = element_type_expr != NULL ? element_type_expr->next : NULL;
    ASTNode *count_expr = pointer_expr != NULL ? pointer_expr->next : NULL;
    TypeSystemExprType ptr_type = inferExprType(pointer_expr, &(scope->type_scope));
    ASTDataType *result_type = mirResolvedExprValueType(node, &(scope->type_scope));
    MirValueId base_ptr = lowerExprAsValue(state, scope, pointer_expr, ptr_type.data_type);
    MirValueId offset = lowerExprAsValue(state, scope, count_expr, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_I64));
    MirValueId result = mirEmitIndexPtr(state, base_ptr, offset, result_type->child, true,
                                        node->filename, node->line_number, node->column_number);
    return mirMaybeConvertValue(state, scope, node, result, expected_type);
}

static MirValueId lowerPtrDiffBuiltinExpr(MirFunctionState *state, MirLowerScope *scope, ASTNode *node,
                                          ASTDataType *expected_type)
{
    ASTNode *element_type_expr = node->lhs;
    ASTNode *lhs_expr = element_type_expr != NULL ? element_type_expr->next : NULL;
    ASTNode *rhs_expr = lhs_expr != NULL ? lhs_expr->next : NULL;
    ASTDataType *ptr_type = mirResolvedExprValueType(lhs_expr, &(scope->type_scope));
    MirValueId lhs = lowerExprAsValue(state, scope, lhs_expr, ptr_type);
    MirValueId rhs = lowerExprAsValue(state, scope, rhs_expr, ptr_type);
    MirValueId result = mirEmitPtrDiff(state, lhs, rhs, node->filename, node->line_number, node->column_number);
    return mirMaybeConvertValue(state, scope, node, result, expected_type);
}

static MirValueId lowerAsBuiltinExpr(MirFunctionState *state, MirLowerScope *scope, ASTNode *node,
                                     ASTDataType *expected_type)
{
    ASTNode *target_type_expr = node->lhs;
    ASTNode *value_expr = target_type_expr != NULL ? target_type_expr->next : NULL;
    ASTDataType *target_type = mirResolvedExprValueType(node, &(scope->type_scope));
    TypeSystemExprType source_type = value_expr != NULL ? inferExprType(value_expr, &(scope->type_scope))
                                                        : (TypeSystemExprType){0};

    if(value_expr != NULL &&
       value_expr->kind == AST_EXPR_LITERAL_STRING &&
       mirIsCharPointerTarget(target_type))
        return mirMaybeConvertValue(state, scope, node,
                                    lowerStringLiteralAsPointer(state, value_expr, target_type),
                                    expected_type);

    if(value_expr != NULL &&
       isLiteralIntegerZero(value_expr) &&
       (target_type->kind == AST_DATA_TYPE_KIND_POINTER ||
        target_type->kind == AST_DATA_TYPE_KIND_FUNCTION))
    {
        return mirMaybeConvertValue(state, scope, node,
                                    mirEmitZero(state, target_type,
                                                node->filename, node->line_number, node->column_number),
                                    expected_type);
    }

    if(value_expr != NULL &&
       source_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE &&
       source_type.data_type != NULL &&
       source_type.data_type->kind == AST_DATA_TYPE_KIND_SLICE &&
       target_type->kind == AST_DATA_TYPE_KIND_POINTER)
        return mirMaybeConvertValue(state, scope, node,
                                    lowerSlicePtrValue(state, scope, value_expr),
                                    expected_type);

    if(value_expr != NULL && target_type->kind == AST_DATA_TYPE_KIND_FUNCTION)
    {
        ASTDataType *pointer_target = newWrappedDataType(AST_DATA_TYPE_KIND_POINTER, false,
                                                         newPrimaryDataType(AST_PRIMARY_DATA_TYPE_VOID));
        if(target_type->is_variadic)
            mirLoweringAbortNode("M2014", node,
                                 "cannot convert a raw function address into a variadic function value",
                                 "variadic function pointers are not supported by this closure bridge");

        MirValueId raw_ptr = lowerExprAsValue(state, scope, value_expr, pointer_target);
        MirOperandList captures = newMirOperandList(1);
        captures.items[0] = raw_ptr;
        ASTDataType *env_type = mirDynamicFunctionEnvType();
        const char *wrapper_name = mirEnsureDynamicFunctionWrapper(state->lowering, target_type);
        return mirMaybeConvertValue(state, scope, node,
                                    mirEmitMakeClosure(state, wrapper_name, target_type, env_type, captures,
                                                       node->filename, node->line_number, node->column_number),
                                    expected_type);
    }

    MirValueId value = lowerExprAsValue(state, scope, value_expr, NULL);
    value = mirEmitConvert(state, value, target_type,
                           node->filename, node->line_number, node->column_number);
    return mirMaybeConvertValue(state, scope, node, value, expected_type);
}

static MirValueId lowerSliceBuiltinExpr(MirFunctionState *state, MirLowerScope *scope, ASTNode *node,
                                        ASTDataType *expected_type)
{
    ASTDataType *slice_type = mirResolvedExprValueType(node, &(scope->type_scope));
    ASTNode *element_type_expr = node->lhs;
    ASTNode *pointer_expr = element_type_expr != NULL ? element_type_expr->next : NULL;
    ASTNode *length_expr = pointer_expr != NULL ? pointer_expr->next : NULL;

    MirFieldValueList fields = newMirFieldValueList(2);
    strcpy(fields.items[0].identifier, "ptr");
    fields.items[0].value = lowerExprAsValue(state, scope, pointer_expr,
                                             newWrappedDataType(AST_DATA_TYPE_KIND_POINTER, true,
                                                                cloneDataType(slice_type->child)));
    strcpy(fields.items[1].identifier, "len");
    fields.items[1].value = lowerExprAsValue(state, scope, length_expr,
                                             newPrimaryDataType(AST_PRIMARY_DATA_TYPE_I64));

    MirValueId value = mirEmitStructLiteral(state, fields, slice_type,
                                            node->filename, node->line_number, node->column_number);
    return mirMaybeConvertValue(state, scope, node, value, expected_type);
}

static MirValueId lowerUnwrapBuiltinExpr(MirFunctionState *state, MirLowerScope *scope, ASTNode *node,
                                         ASTDataType *expected_type)
{
    ASTNode *operand_expr = node->lhs;
    ASTDataType *optional_type = mirResolvedExprValueType(operand_expr, &(scope->type_scope));
    if(optional_type == NULL || optional_type->kind != AST_DATA_TYPE_KIND_OPTIONAL)
        mirLoweringAbortNode("M2004", node,
                             "@unwrap lowering expected an optional operand",
                             "type checking should reject non-optional unwrap operands");

    MirValueId optional_value = lowerExprAsValue(state, scope, operand_expr, optional_type);
    MirValueId optional_slot = mirEmitAlloca(state, optional_type,
                                             node->filename, node->line_number, node->column_number);
    mirEmitStore(state, optional_slot, optional_value,
                 node->filename, node->line_number, node->column_number);

    MirValueId has_value_ptr = mirEmitFieldPtr(state, optional_slot, mirOptionalBoolType(),
                                               "has_value", 0,
                                               node->filename, node->line_number, node->column_number);
    MirValueId has_value = mirEmitLoad(state, has_value_ptr, mirOptionalBoolType(),
                                       node->filename, node->line_number, node->column_number);

    MirFunction *function = mirCurrentFunction(state);
    MirBlockId ok_block = mirCreateBlock(state->lowering, function, "unwrap_ok");
    MirBlockId panic_block = mirCreateBlock(state->lowering, function, "unwrap_panic");
    MirBlockId end_block = mirCreateBlock(state->lowering, function, "unwrap_end");
    MirValueId result_slot = mirEmitAlloca(state, optional_type->child,
                                           node->filename, node->line_number, node->column_number);

    mirEmitCondBr(state, has_value, ok_block, panic_block);

    mirSwitchToBlock(state, panic_block);
    ASTFunctionParameter *panic_param = NULL;
    ASTDataType *panic_type = newFunctionDataType(panic_param, false, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_VOID));
    const char *panic_name = mirEnsureExternFunction(state->lowering, "mote_unwrap_null_panic", panic_type,
                                                     node->filename, node->line_number, node->column_number);
    MirOperandList panic_args = newMirOperandList(0);
    MirValueId panic_call = mirEmitExternCall(state, panic_name, panic_type, panic_args,
                                              newPrimaryDataType(AST_PRIMARY_DATA_TYPE_VOID),
                                              node->filename, node->line_number, node->column_number);
    (void)panic_call;
    mirEmitBr(state, end_block);

    mirSwitchToBlock(state, ok_block);
    MirValueId inner_ptr = mirEmitFieldPtr(state, optional_slot, optional_type->child,
                                           "value", 1,
                                           node->filename, node->line_number, node->column_number);
    MirValueId inner_value = mirEmitLoad(state, inner_ptr, optional_type->child,
                                         node->filename, node->line_number, node->column_number);
    mirEmitStore(state, result_slot, inner_value,
                 node->filename, node->line_number, node->column_number);
    mirEmitBr(state, end_block);

    mirSwitchToBlock(state, end_block);
    MirValueId result = mirEmitLoad(state, result_slot, optional_type->child,
                                    node->filename, node->line_number, node->column_number);
    return mirMaybeConvertValue(state, scope, node, result, expected_type);
}

static ASTDataType* mirInferVariadicArgumentType(ASTNode *argument_node, MirLowerScope *scope)
{
    TypeSystemExprType argument_type = inferExprType(argument_node, &(scope->type_scope));
    ASTDataType *promoted_type = variadicPromotedExprType(argument_type);
    if(promoted_type == NULL)
        mirLoweringAbortNode("M2008", argument_node,
                             "variadic argument must be a runtime value",
                             "compile-time-only expressions cannot be passed through variadic ABI lowering");
    return promoted_type;
}

static MirValueId lowerDirectExternCall(MirFunctionState *state, MirLowerScope *scope, ASTNode *call_node,
                                        ASTNode *extern_node, ASTDataType *function_type,
                                        ASTDataType *expected_type)
{
    const char *symbol_name = mirEnsureExternFunction(
        state->lowering,
        extern_node->lhs->literal_string,
        function_type,
        call_node->filename,
        call_node->line_number,
        call_node->column_number
    );

    MirOperandList arguments = newMirOperandList(countASTNodes(call_node->rhs));
    ASTFunctionParameter *parameter = function_type->parameters;
    ASTNode *argument_node = call_node->rhs;
    int index = 0;
    while(argument_node)
    {
        ASTDataType *parameter_type = NULL;
        if(parameter != NULL)
        {
            parameter_type = parameter->data_type;
            parameter = parameter->next;
        }
        else
            parameter_type = mirInferVariadicArgumentType(argument_node, scope);

        if(parameter_type != NULL && parameter_type->kind == AST_DATA_TYPE_KIND_REFERENCE)
            arguments.items[index++] = lowerReferenceArgument(state, scope, argument_node, parameter_type);
        else
            arguments.items[index++] = lowerExprAsValue(state, scope, argument_node, parameter_type);

        argument_node = argument_node->next;
    }

    ASTDataType *return_type = mirResolvedExprValueType(call_node, &(scope->type_scope));
    MirValueId call_value = mirEmitExternCall(state, symbol_name, function_type, arguments, return_type,
                                              call_node->filename, call_node->line_number, call_node->column_number);
    if(mirIsValueTypeVoid(return_type))
        return call_value;
    return mirMaybeConvertValue(state, scope, call_node, call_value, expected_type);
}

static ASTDataType* inferComparisonOperandType(ASTNode *lhs, ASTNode *rhs, ScopeFrame *scope)
{
    TypeSystemExprType lhs_type = inferExprType(lhs, scope);
    TypeSystemExprType rhs_type = inferExprType(rhs, scope);

    if(lhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE && rhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE &&
       isSameDataType(lhs_type.data_type, rhs_type.data_type))
        return cloneDataType(lhs_type.data_type);

    if(lhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_NULL &&
       rhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE &&
       rhs_type.data_type != NULL &&
       rhs_type.data_type->kind == AST_DATA_TYPE_KIND_OPTIONAL)
        return cloneDataType(rhs_type.data_type);

    if(rhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_NULL &&
       lhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE &&
       lhs_type.data_type != NULL &&
       lhs_type.data_type->kind == AST_DATA_TYPE_KIND_OPTIONAL)
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

    ASTNode *direct_generic_function = NULL;
    const char *direct_name_hint = NULL;
    if(tryGetDirectGenericFunctionValue(scope, node->lhs, &direct_generic_function, &direct_name_hint) &&
       functionHasTypeParameters(direct_generic_function->parameters))
    {
        ASTFunctionParameter *specialized_parameters = NULL;
        ASTDataType *specialized_return_type = NULL;
        MirValueId callee = lowerRuntimeSpecializedFunctionValue(
            state,
            scope,
            direct_generic_function,
            node->rhs,
            direct_name_hint,
            NULL,
            &specialized_parameters,
            &specialized_return_type
        );
        MirOperandList arguments = lowerSpecializedCallArguments(
            state,
            scope,
            direct_generic_function->parameters,
            specialized_parameters,
            node->rhs
        );

        MirValueId call_value = mirEmitCall(state, callee, arguments, specialized_return_type,
                                            node->filename, node->line_number, node->column_number);
        if(mirIsValueTypeVoid(specialized_return_type))
            return call_value;
        return mirMaybeConvertValue(state, scope, node, call_value, expected_type);
    }

    if(node->lhs->kind == AST_EXPR_VARIABLE)
    {
        VariableInfo *callee_variable = findVariableInfo(&(scope->type_scope), node->lhs->identifier);
        MirRuntimeBinding *callee_binding = findMirRuntimeBinding(scope, node->lhs->identifier);
        if(callee_variable != NULL &&
           callee_variable->function_value != NULL &&
           callee_binding != NULL &&
           callee_binding->kind == MIR_RUNTIME_BINDING_COMPTIME_ONLY)
        {
            ASTFunctionParameter *specialized_parameters = NULL;
            ASTDataType *specialized_return_type = NULL;
            MirValueId callee = lowerRuntimeSpecializedFunctionValue(
                state,
                scope,
                callee_variable->function_value,
                node->rhs,
                node->lhs->identifier,
                NULL,
                &specialized_parameters,
                &specialized_return_type
            );
            MirOperandList arguments = lowerSpecializedCallArguments(
                state,
                scope,
                callee_variable->function_value->parameters,
                specialized_parameters,
                node->rhs
            );

            MirValueId call_value = mirEmitCall(state, callee, arguments, specialized_return_type,
                                                node->filename, node->line_number, node->column_number);
            if(mirIsValueTypeVoid(specialized_return_type))
                return call_value;
            return mirMaybeConvertValue(state, scope, node, call_value, expected_type);
        }

        if(callee_variable != NULL && callee_variable->extern_value != NULL)
        {
            ASTDataType *function_type = mirResolvedExprValueType(node->lhs, &(scope->type_scope));
            if(function_type != NULL && function_type->is_variadic)
                return lowerDirectExternCall(state, scope, node, callee_variable->extern_value, function_type, expected_type);
        }
    }

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
        struct_type = resolveNamedDataType(struct_type, &(scope->type_scope), scope->self_data_type);

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
                        arguments.items[index++] = lowerReferenceArgument(state, scope, member_node->lhs, parameter->data_type);
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
                        arguments.items[index++] = lowerReferenceArgument(state, scope, argument_node, parameter_type);
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
            arguments.items[index++] = lowerReferenceArgument(state, scope, argument_node, parameter_type);
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

static MirValueId lowerOperatorOverloadCall(MirFunctionState *state,
                                            MirLowerScope *scope,
                                            ASTNode *node,
                                            ASTOperatorKind operator_kind,
                                            ASTNode *lhs_expr,
                                            ASTNode *rhs_expr,
                                            ASTDataType *expected_type)
{
    ResolvedOperatorOverload overload = {0};
    if(!resolveOperatorOverload(operator_kind, lhs_expr, rhs_expr, &(scope->type_scope), &overload))
        return -1;

    ASTDataType *function_type = overload.function_value->data_type;
    ASTFunctionParameter *parameter = function_type->parameters;
    int argument_count = lhs_expr != NULL ? 1 : 0;
    if(rhs_expr != NULL)
        argument_count++;
    MirOperandList arguments = newMirOperandList(argument_count);
    int argument_index = 0;

    if(lhs_expr != NULL && parameter != NULL)
    {
        MirValueId lhs = lowerExprAsValue(state, scope, lhs_expr, parameter->data_type);
        arguments.items[argument_index++] = lhs;
        parameter = parameter->next;
    }

    if(rhs_expr != NULL && parameter != NULL)
    {
        MirValueId rhs = lowerExprAsValue(state, scope, rhs_expr, parameter->data_type);
        arguments.items[argument_index++] = rhs;
    }

    MirValueId callee = lowerFunctionExprAsValue(state, scope, overload.function_value, "operator", NULL);
    MirValueId call_value = mirEmitCall(state, callee, arguments, overload.result_type,
                                        node->filename, node->line_number, node->column_number);
    if(node->kind == AST_EXPR_NOT_EQUAL)
    {
        MirValueId negated = mirEmitUnary(state, MIR_INST_NOT, call_value, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_BOOL),
                                          node->filename, node->line_number, node->column_number);
        return mirMaybeConvertValue(state, scope, node, negated, expected_type);
    }
    if(mirIsValueTypeVoid(overload.result_type))
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
        ASTDataType *struct_type = NULL;
        if(expected_type != NULL && expected_type->kind == AST_DATA_TYPE_KIND_STRUCT)
            struct_type = cloneDataType(expected_type);
        else
        {
            TypeSystemExprType type_expr = inferExprType(node->lhs, &(scope->type_scope));
            struct_type = resolveNamedDataType(type_expr.data_type, &(scope->type_scope), scope->self_data_type);
        }
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
        case AST_EXPR_LITERAL_NULL:
            if(expected_type == NULL || expected_type->kind != AST_DATA_TYPE_KIND_OPTIONAL)
                mirLoweringAbortNode("M2010", node,
                                     "`null` requires an expected optional type",
                                     "add an explicit optional type like `?T`");
            return mirLowerOptionalNull(state, expected_type,
                                        node->filename, node->line_number, node->column_number);
        case AST_EXPR_LITERAL_CHAR:
            return mirMaybeConvertValue(state, scope, node,
                                        mirEmitConstChar(state, node->literal_char, node->filename, node->line_number, node->column_number),
                                        expected_type);
        case AST_EXPR_LITERAL_INTEGER: {
            ASTDataType *literal_type = expected_type;
            if(literal_type != NULL && literal_type->kind == AST_DATA_TYPE_KIND_OPTIONAL)
                literal_type = literal_type->child;
            if(literal_type == NULL)
                literal_type = defaultIntegerDataType();
            return mirMaybeConvertValue(state, scope, node,
                                        mirEmitConstInt(state, node->literal_integer, literal_type,
                                                        node->filename, node->line_number, node->column_number),
                                        expected_type);
        }
        case AST_EXPR_LITERAL_FLOAT: {
            ASTDataType *literal_type = expected_type;
            if(literal_type != NULL && literal_type->kind == AST_DATA_TYPE_KIND_OPTIONAL)
                literal_type = literal_type->child;
            if(literal_type == NULL)
                literal_type = defaultFloatDataType();
            return mirMaybeConvertValue(state, scope, node,
                                        mirEmitConstFloat(state, node->literal_float, literal_type,
                                                          node->filename, node->line_number, node->column_number),
                                        expected_type);
        }
        case AST_EXPR_LITERAL_STRING: {
            if(mirIsCharPointerTarget(expected_type))
                return mirMaybeConvertValue(state, scope, node,
                                            lowerStringLiteralAsPointer(state, node, expected_type),
                                            expected_type);
            ASTDataType *string_type = expr_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE
                ? cloneDataType(expr_type.data_type)
                : newArrayDataType(newPrimaryDataType(AST_PRIMARY_DATA_TYPE_CHAR), strlen(node->literal_string));
            return mirEmitConstString(state, node->literal_string, string_type,
                                      node->filename, node->line_number, node->column_number);
        }
        case AST_EXPR_BUILTIN:
            if(strcmp(node->identifier, "extern") == 0)
                return lowerExternBuiltinExpr(state, scope, node, expected_type);
            if(strcmp(node->identifier, "sizeof") == 0)
                return lowerTypeLayoutBuiltinExpr(state, scope, node, false);
            if(strcmp(node->identifier, "alignof") == 0)
                return lowerTypeLayoutBuiltinExpr(state, scope, node, true);
            if(strcmp(node->identifier, "zero") == 0)
                return lowerZeroBuiltinExpr(state, scope, node, expected_type);
            if(strcmp(node->identifier, "debug") == 0)
                return lowerDebugBuiltinExpr(state, scope, node);
            if(strcmp(node->identifier, "len") == 0)
                return lowerLenBuiltinExpr(state, scope, node, expected_type);
            if(strcmp(node->identifier, "ptr_add") == 0)
                return lowerPtrAddBuiltinExpr(state, scope, node, expected_type);
            if(strcmp(node->identifier, "ptr_diff") == 0)
                return lowerPtrDiffBuiltinExpr(state, scope, node, expected_type);
            if(strcmp(node->identifier, "as") == 0)
                return lowerAsBuiltinExpr(state, scope, node, expected_type);
            if(strcmp(node->identifier, "slice") == 0)
                return lowerSliceBuiltinExpr(state, scope, node, expected_type);
            if(strcmp(node->identifier, "unwrap") == 0)
                return lowerUnwrapBuiltinExpr(state, scope, node, expected_type);
            mirLoweringAbortNodeFormatted("M2009", node,
                                          "builtin lowering is missing",
                                          "unsupported builtin `@%s`",
                                          node->identifier);
        case AST_EXPR_VARIABLE:
            return lowerVariableValue(state, scope, node, expected_type);
        case AST_EXPR_PARENTHESIS:
            return lowerExprAsValue(state, scope, node->lhs, expected_type);
        case AST_EXPR_FUNCTION:
            return lowerFunctionExprAsValue(state, scope, node, NULL, scope->self_data_type);
        case AST_EXPR_ARRAY_LITERAL: {
            ASTDataType *array_type = expected_type != NULL ? cloneDataType(expected_type)
                                                            : mirResolvedExprValueType(node, &(scope->type_scope));
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
            if(expected_type != NULL)
            {
                TypeSystemExprType operand_type = inferExprType(node->lhs, &(scope->type_scope));
                if((operand_type.kind == TYPE_SYSTEM_EXPR_TYPE_LITERAL_INTEGER ||
                    operand_type.kind == TYPE_SYSTEM_EXPR_TYPE_LITERAL_FLOAT) &&
                   expected_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
                   (isIntegerPrimary(expected_type->primary) || isFloatPrimary(expected_type->primary)))
                    return lowerExprAsValue(state, scope, node->lhs, expected_type);
            }
            return lowerExprAsValue(state, scope, node->lhs, expected_type);
        case AST_EXPR_UNARY_MINUS: {
            MirValueId overload = lowerOperatorOverloadCall(state, scope, node,
                                                            AST_OPERATOR_SUB,
                                                            node->lhs, NULL, expected_type);
            if(overload >= 0)
                return overload;
            ASTDataType *result_type = mirPreferredExprValueType(node, &(scope->type_scope), expected_type);
            TypeSystemExprType operand_type = inferExprType(node->lhs, &(scope->type_scope));
            if(expected_type != NULL &&
               expected_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
               (isIntegerPrimary(expected_type->primary) || isFloatPrimary(expected_type->primary)) &&
               (operand_type.kind == TYPE_SYSTEM_EXPR_TYPE_LITERAL_INTEGER ||
                operand_type.kind == TYPE_SYSTEM_EXPR_TYPE_LITERAL_FLOAT))
                result_type = expected_type;
            if(node->lhs->kind == AST_EXPR_LITERAL_INTEGER)
                return mirMaybeConvertValue(state, scope, node,
                                            mirEmitConstInt(state, -(node->lhs->literal_integer), result_type,
                                                            node->filename, node->line_number, node->column_number),
                                            expected_type);
            if(node->lhs->kind == AST_EXPR_LITERAL_FLOAT)
                return mirMaybeConvertValue(state, scope, node,
                                            mirEmitConstFloat(state, -(node->lhs->literal_float), result_type,
                                                              node->filename, node->line_number, node->column_number),
                                            expected_type);
            MirValueId operand = lowerExprAsValue(state, scope, node->lhs, result_type);
            MirValueId value = mirEmitUnary(state, MIR_INST_NEG, operand, result_type,
                                            node->filename, node->line_number, node->column_number);
            return mirMaybeConvertValue(state, scope, node, value, expected_type);
        }
        case AST_EXPR_UNARY_LOGICAL_NOT: {
            MirValueId operand = lowerExprAsValue(state, scope, node->lhs, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_BOOL));
            MirValueId value = mirEmitUnary(state, MIR_INST_NOT, operand, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_BOOL),
                                            node->filename, node->line_number, node->column_number);
            return mirMaybeConvertValue(state, scope, node, value, expected_type);
        }
        case AST_EXPR_UNARY_BIT_NOT: {
            ASTDataType *result_type = mirResolvedExprValueType(node, &(scope->type_scope));
            MirValueId operand = lowerExprAsValue(state, scope, node->lhs, result_type);
            MirValueId value = mirEmitUnary(state, MIR_INST_BIT_NOT, operand, result_type,
                                            node->filename, node->line_number, node->column_number);
            return mirMaybeConvertValue(state, scope, node, value, expected_type);
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
            ASTOperatorKind operator_kind = mirBinaryExprOperatorKind(node->kind);
            if(operator_kind != AST_OPERATOR_NONE)
            {
                MirValueId overload = lowerOperatorOverloadCall(state, scope, node,
                                                                operator_kind,
                                                                node->lhs, node->rhs, expected_type);
                if(overload >= 0)
                    return overload;
            }
            ASTDataType *result_type = mirResolvedExprValueType(node, &(scope->type_scope));
            if(result_type != NULL && result_type->kind == AST_DATA_TYPE_KIND_OPTIONAL)
                result_type = result_type->child;
            if(expected_type != NULL &&
               result_type != NULL &&
               result_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
               expected_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
               ((isFloatPrimary(result_type->primary) && isFloatPrimary(expected_type->primary)) ||
                (isIntegerPrimary(result_type->primary) && isIntegerPrimary(expected_type->primary))))
                result_type = cloneDataType(expected_type);
            else if(result_type != NULL &&
                    result_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
                    isIntegerPrimary(result_type->primary))
            {
                TypeSystemExprType lhs_type = inferExprType(node->lhs, &(scope->type_scope));
                TypeSystemExprType rhs_type = inferExprType(node->rhs, &(scope->type_scope));
                if(lhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE &&
                   lhs_type.data_type != NULL &&
                   lhs_type.data_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
                   isIntegerPrimary(lhs_type.data_type->primary))
                    result_type = cloneDataType(lhs_type.data_type);
                else if(rhs_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE &&
                        rhs_type.data_type != NULL &&
                        rhs_type.data_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
                        isIntegerPrimary(rhs_type.data_type->primary))
                    result_type = cloneDataType(rhs_type.data_type);
            }
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
            MirValueId value = mirEmitBinary(state, kind, lhs, rhs, result_type,
                                             node->filename, node->line_number, node->column_number);
            return mirMaybeConvertValue(state, scope, node, value, expected_type);
        }
        case AST_EXPR_EQUAL:
        case AST_EXPR_NOT_EQUAL:
        case AST_EXPR_LESS:
        case AST_EXPR_LESS_EQUAL:
        case AST_EXPR_GREATER:
        case AST_EXPR_GREATER_EQUAL: {
            ASTOperatorKind operator_kind = mirBinaryExprOperatorKind(node->kind);
            if(operator_kind != AST_OPERATOR_NONE)
            {
                MirValueId overload = lowerOperatorOverloadCall(state, scope, node,
                                                                operator_kind,
                                                                node->lhs, node->rhs, expected_type);
                if(overload >= 0)
                    return overload;
            }
            ASTDataType *operand_type = inferComparisonOperandType(node->lhs, node->rhs, &(scope->type_scope));
            if((node->kind == AST_EXPR_EQUAL || node->kind == AST_EXPR_NOT_EQUAL) &&
               operand_type != NULL &&
               operand_type->kind == AST_DATA_TYPE_KIND_OPTIONAL)
            {
                MirValueId optional_value = -1;
                if(inferExprType(node->lhs, &(scope->type_scope)).kind == TYPE_SYSTEM_EXPR_TYPE_NULL)
                    optional_value = lowerExprAsValue(state, scope, node->rhs, operand_type);
                else
                    optional_value = lowerExprAsValue(state, scope, node->lhs, operand_type);

                MirValueId optional_slot = mirEmitAlloca(state, operand_type,
                                                         node->filename, node->line_number, node->column_number);
                mirEmitStore(state, optional_slot, optional_value,
                             node->filename, node->line_number, node->column_number);
                MirValueId has_value_ptr = mirEmitFieldPtr(state, optional_slot, mirOptionalBoolType(),
                                                           "has_value", 0,
                                                           node->filename, node->line_number, node->column_number);
                MirValueId has_value = mirEmitLoad(state, has_value_ptr, mirOptionalBoolType(),
                                                   node->filename, node->line_number, node->column_number);
                MirValueId result = has_value;
                if(node->kind == AST_EXPR_EQUAL)
                    result = mirEmitUnary(state, MIR_INST_NOT, has_value, mirOptionalBoolType(),
                                          node->filename, node->line_number, node->column_number);
                return mirMaybeConvertValue(state, scope, node, result, expected_type);
            }
            MirValueId lhs = lowerExprAsValue(state, scope, node->lhs, operand_type);
            MirValueId rhs = lowerExprAsValue(state, scope, node->rhs, operand_type);
            MirInstKind kind = MIR_INST_EQ;
            if(node->kind == AST_EXPR_NOT_EQUAL) kind = MIR_INST_NE;
            else if(node->kind == AST_EXPR_LESS) kind = MIR_INST_LT;
            else if(node->kind == AST_EXPR_LESS_EQUAL) kind = MIR_INST_LE;
            else if(node->kind == AST_EXPR_GREATER) kind = MIR_INST_GT;
            else if(node->kind == AST_EXPR_GREATER_EQUAL) kind = MIR_INST_GE;
            MirValueId value = mirEmitBinary(state, kind, lhs, rhs, newPrimaryDataType(AST_PRIMARY_DATA_TYPE_BOOL),
                                             node->filename, node->line_number, node->column_number);
            return mirMaybeConvertValue(state, scope, node, value, expected_type);
        }
        default:
            mirLoweringAbortNodeFormatted("ICE0304", node,
                                          NULL,
                                          "MIR lowering hit unsupported expression kind %s",
                                          astNodeKindToString(node->kind));
    }
}

static MirValueId lowerExprAsAddress(MirFunctionState *state, MirLowerScope *scope, ASTNode *node)
{
    switch(node->kind)
    {
        case AST_EXPR_VARIABLE: {
            MirRuntimeBinding *binding = findMirRuntimeBinding(scope, node->identifier);
            if(binding == NULL || binding->kind == MIR_RUNTIME_BINDING_COMPTIME_ONLY)
                mirLoweringAbortNodeFormatted("M2010", node,
                                              "this variable does not have addressable runtime storage",
                                              "variable `%s` is not addressable",
                                              astUserFacingIdentifier(node->identifier));
            return mirBindingAddress(state, binding, node);
        }
        case AST_EXPR_DEREF:
            return lowerExprAsValue(state, scope, node->lhs, NULL);
        case AST_EXPR_MEMBER: {
            TypeSystemExprType owner_type = inferExprType(node->lhs, &(scope->type_scope));
            ASTDataType *struct_type = owner_type.data_type;
            MirValueId base_address = -1;

            if(owner_type.kind != TYPE_SYSTEM_EXPR_TYPE_VALUE)
                mirLoweringAbortNode("M2011", node,
                                     "member base is not a runtime value",
                                     "only runtime values can be lowered to field addresses");

            if(struct_type->kind == AST_DATA_TYPE_KIND_POINTER || struct_type->kind == AST_DATA_TYPE_KIND_REFERENCE)
            {
                base_address = lowerExprAsValue(state, scope, node->lhs, NULL);
                struct_type = struct_type->child;
            }
            else if(isAddressableExpr(node->lhs))
                base_address = lowerExprAsAddress(state, scope, node->lhs);
            else
                base_address = lowerExprMaterializedAddress(state, scope, node->lhs);

            struct_type = resolveNamedDataType(struct_type, &(scope->type_scope), scope->self_data_type);

            if(isSliceDataType(struct_type))
            {
                ASTDataType *field_type = NULL;
                if(strcmp(node->identifier, "ptr") == 0)
                    field_type = newWrappedDataType(AST_DATA_TYPE_KIND_POINTER, true, cloneDataType(struct_type->child));
                else if(strcmp(node->identifier, "len") == 0)
                    field_type = newPrimaryDataType(AST_PRIMARY_DATA_TYPE_I64);
                else
                    mirLoweringAbortNodeFormatted("M2012", node,
                                                  "this member is not a stored field",
                                                  "member `%s` is not an addressable slice field",
                                                  astUserFacingIdentifier(node->identifier));

                return mirEmitFieldPtr(state, base_address, field_type, node->identifier,
                                       findStructDataFieldIndex(struct_type, node->identifier),
                                       node->filename, node->line_number, node->column_number);
            }

            ASTStructMember *member = findStructMember(struct_type, node->identifier);
            if(member == NULL || member->value != NULL)
                mirLoweringAbortNodeFormatted("M2012", node,
                                              "this member is not a stored field",
                                              "member `%s` is not an addressable field",
                                              astUserFacingIdentifier(node->identifier));
            return mirEmitFieldPtr(state, base_address, member->data_type, member->identifier,
                                   findStructDataFieldIndex(struct_type, member->identifier),
                                   node->filename, node->line_number, node->column_number);
        }
        case AST_EXPR_INDEX: {
            TypeSystemExprType owner_type = inferExprType(node->lhs, &(scope->type_scope));
            ASTDataType *array_type = owner_type.data_type;
            MirValueId base_address = -1;
            bool base_is_element_pointer = false;
            if(array_type->kind == AST_DATA_TYPE_KIND_POINTER || array_type->kind == AST_DATA_TYPE_KIND_REFERENCE)
            {
                base_address = lowerExprAsValue(state, scope, node->lhs, NULL);
                array_type = array_type->child;
            }
            else if(isSliceDataType(array_type))
            {
                MirValueId slice_address = isAddressableExpr(node->lhs)
                                           ? lowerExprAsAddress(state, scope, node->lhs)
                                           : lowerExprMaterializedAddress(state, scope, node->lhs);
                ASTDataType *ptr_field_type = newWrappedDataType(AST_DATA_TYPE_KIND_POINTER, true,
                                                                 cloneDataType(array_type->child));
                MirValueId ptr_field_address = mirEmitFieldPtr(state, slice_address, ptr_field_type, "ptr", 0,
                                                               node->filename, node->line_number, node->column_number);
                base_address = mirEmitLoad(state, ptr_field_address, ptr_field_type,
                                           node->filename, node->line_number, node->column_number);
                base_is_element_pointer = true;
            }
            else if(isAddressableExpr(node->lhs))
                base_address = lowerExprAsAddress(state, scope, node->lhs);
            else
                base_address = lowerExprMaterializedAddress(state, scope, node->lhs);

            ASTDataType *index_type = defaultIntegerDataType();
            TypeSystemExprType index_expr_type = inferExprType(node->rhs, &(scope->type_scope));
            ASTDataType *index_expected_type = index_expr_type.kind == TYPE_SYSTEM_EXPR_TYPE_VALUE
                                               ? index_expr_type.data_type
                                               : index_type;
            MirValueId index_value = lowerExprAsValue(state, scope, node->rhs, index_expected_type);
            index_value = mirMaybeConvertValue(state, scope, node->rhs, index_value, index_type);
            return mirEmitIndexPtr(state, base_address, index_value, array_type->child, base_is_element_pointer,
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
        VariableInfo *existing_variable_info = findVariableInfo(&(scope->type_scope), node->identifier);
        bool explicit_decl = isExplicitDeclared(node);
        bool is_new_variable = explicit_decl ||
                               existing_binding == NULL ||
                               (existing_variable_info != NULL && existing_variable_info->predeclared);

        if(is_new_variable)
        {
            ASTNode *resolved_function_value = resolveFunctionValueExpr(node->rhs, &(scope->type_scope));
            ASTDataType *declared_type = NULL;
            if(resolved_function_value != NULL &&
               functionHasTypeParameters(resolved_function_value->parameters))
                declared_type = cloneDataType(node->data_type);
            else
                declared_type = resolveNamedDataType(node->data_type, &(scope->type_scope), scope->self_data_type);
            TypeSystemExprType expr_type = {0};
            if(node->rhs->kind == AST_EXPR_ARRAY_LITERAL && node->rhs->lhs == NULL &&
               declared_type->kind == AST_DATA_TYPE_KIND_ARRAY && declared_type->array_length == 0)
                expr_type = newValueExprType(declared_type);
            else
                expr_type = inferExprType(node->rhs, &(scope->type_scope));
            mirDeclareVariableInfo(scope, node, declared_type, expr_type);

            MirRuntimeBinding *binding = existing_binding;
            if(binding == NULL)
                binding = declareMirRuntimeBinding(scope, node->identifier);
            binding->mutable = node->modifier.mutable;
            binding->declared_data_type = cloneDataType(declared_type);
            binding->function_value = resolved_function_value;
            binding->extern_value = resolveExternValueExpr(node->rhs, &(scope->type_scope));

            if(expr_type.kind == TYPE_SYSTEM_EXPR_TYPE_TYPE ||
               mirIsCompileTimeTypeFactory(declared_type) ||
               (node->rhs->kind == AST_EXPR_BUILTIN &&
                strcmp(node->rhs->identifier, "extern") == 0 &&
                declared_type->is_variadic) ||
               (binding->function_value != NULL &&
                functionHasTypeParameters(binding->function_value->parameters)))
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

                if(state->is_top_level_init &&
                   node->rhs->kind == AST_EXPR_BUILTIN &&
                   strcmp(node->rhs->identifier, "zero") == 0)
                    return;
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

        VariableInfo *variable_info = existing_variable_info;
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

    if(parent_scope != NULL && parent_scope->parent == NULL)
        mirPredeclareTopLevelBindings(block_scope, block_node);

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
                mirLoweringAbortNode("M2013", node,
                                     "break used outside loop",
                                     "this `break` has no enclosing loop to exit");
            emitCleanupRange(state, scope, state->cleanup_top, state->loop_top->cleanup_stop);
            mirEmitBr(state, state->loop_top->break_block);
            return;
        case AST_STATEMENT_CONTINUE:
            if(state->loop_top == NULL)
                mirLoweringAbortNode("M2014", node,
                                     "continue used outside loop",
                                     "this `continue` has no enclosing loop to target");
            emitCleanupRange(state, scope, state->cleanup_top, state->loop_top->cleanup_stop);
            mirEmitBr(state, state->loop_top->continue_block);
            return;
        case AST_STATEMENT_DEFER:
            appendDeferredStatement(state->cleanup_top, node->lhs);
            return;
        default:
            mirLoweringAbortNodeFormatted("ICE0305", node,
                                          NULL,
                                          "MIR lowering hit unsupported statement kind %s",
                                          astNodeKindToString(node->kind));
    }
}

MirProgram* lowerASTToMIR(ASTNode *root)
{
    if(root == NULL || root->lhs == NULL || root->lhs->kind != AST_BLOCK)
        mirLoweringAbortInternal("ICE0306",
                                 "AST root should contain a top-level block before MIR lowering",
                                 NULL);

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
    mirPredeclareTopLevelBindings(top_scope, root->lhs);
    mirPredeclareTopLevelRuntimeBindings(&lowering, top_scope, root->lhs);

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
