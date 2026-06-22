#ifndef MIR_LOWERING_SHARED_H
#define MIR_LOWERING_SHARED_H

#include "../../AST.h"
#include "../../Semantic.h"
#include "../../TypeSystem.h"
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
    MIR_TERM_UNREACHABLE,
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
    int debug_scope_id;
    union {
        struct {
            bool value;
        } const_bool;
        struct {
            char value;
        } const_char;
        struct {
            unsigned long long value;
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
    const char *filename;
    int line_number;
    int column_number;
    int debug_scope_id;
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
    MirValueId input_value;
} MirCaptureDesc;

typedef struct MirParamDesc {
    char identifier[MAX_IDENTIFIER_LENGTH];
    ASTDataType *source_data_type;
    ASTDataType *runtime_data_type;
    bool by_reference;
    MirValueId input_value;
} MirParamDesc;

typedef struct MirDebugLocal {
    char identifier[MAX_IDENTIFIER_LENGTH];
    const char *filename;
    int line_number;
    int column_number;
    ASTDataType *data_type;
    MirValueId slot_value;
    bool is_parameter;
    int argument_index;
    int debug_scope_id;
} MirDebugLocal;

typedef struct MirDebugScope {
    int parent_scope_id;
    const char *filename;
    int line_number;
    int column_number;
} MirDebugScope;

typedef struct MirFunction {
    char name[MIR_MAX_NAME_LENGTH];
    ASTDataType *return_data_type;
    ASTDataType *closure_env_type;
    MirValueId closure_env_input;
    MirCaptureDesc *captures;
    int capture_count;
    MirParamDesc *parameters;
    int parameter_count;
    MirDebugLocal *debug_locals;
    int debug_local_count;
    MirDebugScope *debug_scopes;
    int debug_scope_count;
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
    bool is_runtime_storage;
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
    bool is_compile_time_constant;
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
    int current_debug_scope_id;
    bool suppress_next_block_debug_scope;
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
static MirValueId lowerSlicePtrValue(MirFunctionState *state, MirLowerScope *scope, ASTNode *slice_expr);

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
        return newWrappedDataType(AST_DATA_TYPE_KIND_POINTER, cloneDataType(source_type->child));
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

static void mirBindLexicalTypeScope(MirLowerScope *scope, ScopeFrame *lexical_scope)
{
    if(scope == NULL || lexical_scope == NULL)
        return;

    for(int i = 0; i < lexical_scope->variable_count; i++)
    {
        VariableInfo *src = &(lexical_scope->variable_infos[i]);
        if(findVariableInfo(&(scope->type_scope), src->identifier) != NULL)
            continue;

        VariableInfo *dst = declareVariableInfo(&(scope->type_scope), src->identifier);
        *dst = *src;
        dst->data_type = cloneDataType(src->data_type);
        dst->type_value = cloneDataType(src->type_value);
    }

    for(int i = 0; i < lexical_scope->type_count; i++)
    {
        TypeInfo *src = &(lexical_scope->type_infos[i]);
        if(findTypeInfo(&(scope->type_scope), src->identifier) != NULL)
            continue;

        TypeInfo *dst = declareTypeInfo(&(scope->type_scope), src->identifier);
        *dst = *src;
        dst->data_type = cloneDataType(src->data_type);
    }
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
    if(struct_type != NULL &&
       (struct_type->kind == AST_DATA_TYPE_KIND_SLICE ||
        struct_type->kind == AST_DATA_TYPE_KIND_STRING))
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
    return newWrappedDataType(AST_DATA_TYPE_KIND_POINTER, cloneDataType(env_type));
}

static ASTDataType* mirDynamicFunctionEnvType(void)
{
    ASTStructMember *member = (ASTStructMember*) malloc(sizeof(ASTStructMember));
    memset(member, 0, sizeof(ASTStructMember));
    strcpy(member->identifier, "fn_ptr");
    member->data_type = newWrappedDataType(AST_DATA_TYPE_KIND_POINTER,
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

#endif /* MIR_LOWERING_SHARED_H */
