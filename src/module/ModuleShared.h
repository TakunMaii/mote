#ifndef MODULE_SHARED_H
#define MODULE_SHARED_H

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <dirent.h>
#endif

#include "../Diagnostic.h"
#include "../Lexer.h"
#include "../Parser.h"
#include "../TypeSystem.h"

#define MODULE_MAX_PATH_LENGTH 1024
#define MODULE_MAX_PACKAGES 128

typedef struct ModuleSourceFile ModuleSourceFile;

typedef struct ModuleImportBinding {
    char alias[MAX_IDENTIFIER_LENGTH];
    ModuleSourceFile *module;
} ModuleImportBinding;

typedef struct ModuleTopLevelBinding {
    char original[MAX_IDENTIFIER_LENGTH];
    char mangled[MAX_IDENTIFIER_LENGTH];
    bool is_pub;
    bool is_type_decl;
    ASTNode *decl;
} ModuleTopLevelBinding;

typedef struct ModulePackage {
    char name[MAX_IDENTIFIER_LENGTH];
    char root_path[MODULE_MAX_PATH_LENGTH];
    bool is_search_root;
    bool is_collection;
} ModulePackage;

struct ModuleSourceFile {
    char canonical_path[MODULE_MAX_PATH_LENGTH];
    char directory[MODULE_MAX_PATH_LENGTH];
    char package_name[MAX_IDENTIFIER_LENGTH];
    char primary_source_path[MODULE_MAX_PATH_LENGTH];
    char symbol_prefix[32];
    ASTNode *ast_root;
    int visit_state;
    ModuleImportBinding *imports;
    int import_count;
    int import_capacity;
    ModuleSourceFile **expression_imports;
    int expression_import_count;
    int expression_import_capacity;
    ModuleTopLevelBinding *top_level_bindings;
    int top_level_binding_count;
    int top_level_binding_capacity;
    bool rewritten;
};

typedef struct ModuleCompileContext {
    ModuleSourceFile **modules;
    int module_count;
    int module_capacity;
    ModulePackage packages[MODULE_MAX_PACKAGES];
    int package_count;
} ModuleCompileContext;

typedef struct RewriteValueBinding {
    char original[MAX_IDENTIFIER_LENGTH];
    char rewritten[MAX_IDENTIFIER_LENGTH];
    bool is_import_alias;
    ModuleSourceFile *imported_module;
    bool is_type_binding;
} RewriteValueBinding;

typedef struct RewriteTypeBinding {
    char original[MAX_IDENTIFIER_LENGTH];
    char rewritten[MAX_IDENTIFIER_LENGTH];
} RewriteTypeBinding;

typedef struct RewriteScope {
    struct RewriteScope *parent;
    RewriteValueBinding *value_bindings;
    int value_count;
    int value_capacity;
    RewriteTypeBinding *type_bindings;
    int type_count;
    int type_capacity;
} RewriteScope;

static ModuleCompileContext *moduleRewriteContext = NULL;

static void moduleSystemError(const char *message, const char *filename, int line, int column)
{
    SourceSpan span = filename != NULL ? makePointSourceSpan(filename, line, column) : makeSourceSpan(NULL, 0, 0, 0, 0);
    Diagnostic diagnostic = diagnosticMake(DIAGNOSTIC_SEVERITY_ERROR, "M1001", span, message);
    if(filename != NULL)
        diagnosticSetPrimaryLabel(&diagnostic, "module error occurred here");
    diagnosticAbort(diagnostic);
}

static ASTNode* moduleStatements(ASTNode *root)
{
    if(root == NULL || root->lhs == NULL)
        return NULL;
    return root->lhs->lhs;
}

static void moduleAppendStatementList(ASTNode **head, ASTNode **tail, ASTNode *statement)
{
    while(statement)
    {
        ASTNode *next = statement->next;
        statement->next = NULL;
        if(*head == NULL)
            *head = statement;
        else
            (*tail)->next = statement;
        *tail = statement;
        statement = next;
    }
}

static bool moduleIsStructDeclAssign(ASTNode *node)
{
    return node != NULL &&
           node->kind == AST_ASSIGN &&
           node->lhs != NULL &&
           node->lhs->kind == AST_EXPR_VARIABLE &&
           node->rhs != NULL &&
           node->rhs->kind == AST_EXPR_STRUCT;
}

static bool moduleIsEnumDeclAssign(ASTNode *node)
{
    return node != NULL &&
           node->kind == AST_ASSIGN &&
           node->lhs != NULL &&
           node->lhs->kind == AST_EXPR_VARIABLE &&
           node->rhs != NULL &&
           node->rhs->kind == AST_EXPR_ENUM;
}

static bool moduleIsImportDecl(ASTNode *node)
{
    return node != NULL &&
           node->kind == AST_ASSIGN &&
           node->lhs != NULL &&
           node->lhs->kind == AST_EXPR_VARIABLE &&
           node->rhs != NULL &&
           node->rhs->kind == AST_EXPR_BUILTIN &&
           strcmp(node->rhs->identifier, "import") == 0;
}

static ModuleSourceFile* moduleFindByPath(ModuleCompileContext *context, const char *canonical_path)
{
    for(int i = 0; i < context->module_count; i++)
    {
        if(strcmp(context->modules[i]->canonical_path, canonical_path) == 0)
            return context->modules[i];
    }
    return NULL;
}

static ModuleSourceFile* moduleAppend(ModuleCompileContext *context)
{
    if(context->module_count >= context->module_capacity)
    {
        int new_capacity = context->module_capacity == 0 ? 8 : context->module_capacity * 2;
        context->modules = (ModuleSourceFile**) realloc(context->modules, sizeof(ModuleSourceFile*) * new_capacity);
        context->module_capacity = new_capacity;
    }

    ModuleSourceFile *module = (ModuleSourceFile*) malloc(sizeof(ModuleSourceFile));
    context->modules[context->module_count++] = module;
    memset(module, 0, sizeof(ModuleSourceFile));
    return module;
}

static void* moduleGrowItems(void *items, size_t item_size, int *capacity, int min_capacity, const char *label)
{
    int next_capacity = *capacity == 0 ? 8 : *capacity;
    while(next_capacity < min_capacity)
        next_capacity *= 2;
    void *grown = realloc(items, item_size * (size_t) next_capacity);
    if(grown == NULL)
        moduleSystemError(label, NULL, 0, 0);
    *capacity = next_capacity;
    return grown;
}

static ModuleImportBinding* moduleAppendImportBinding(ModuleSourceFile *module)
{
    if(module->import_count >= module->import_capacity)
        module->imports = (ModuleImportBinding*) moduleGrowItems(module->imports, sizeof(ModuleImportBinding),
                                                                 &(module->import_capacity), module->import_count + 1,
                                                                 "module import allocation failed");
    ModuleImportBinding *binding = &(module->imports[module->import_count++]);
    memset(binding, 0, sizeof(ModuleImportBinding));
    return binding;
}

static void moduleAppendExpressionImport(ModuleSourceFile *module, ModuleSourceFile *imported_module)
{
    if(module->expression_import_count >= module->expression_import_capacity)
        module->expression_imports = (ModuleSourceFile**) moduleGrowItems(module->expression_imports, sizeof(ModuleSourceFile*),
                                                                          &(module->expression_import_capacity),
                                                                          module->expression_import_count + 1,
                                                                          "expression import allocation failed");
    module->expression_imports[module->expression_import_count++] = imported_module;
}

static ModuleTopLevelBinding* moduleAppendTopLevelBinding(ModuleSourceFile *module)
{
    if(module->top_level_binding_count >= module->top_level_binding_capacity)
        module->top_level_bindings = (ModuleTopLevelBinding*) moduleGrowItems(module->top_level_bindings,
                                                                              sizeof(ModuleTopLevelBinding),
                                                                              &(module->top_level_binding_capacity),
                                                                              module->top_level_binding_count + 1,
                                                                              "top-level binding allocation failed");
    ModuleTopLevelBinding *binding = &(module->top_level_bindings[module->top_level_binding_count++]);
    memset(binding, 0, sizeof(ModuleTopLevelBinding));
    return binding;
}

static ModuleTopLevelBinding* moduleFindTopLevelBinding(ModuleSourceFile *module, const char *original)
{
    for(int i = 0; i < module->top_level_binding_count; i++)
    {
        if(strcmp(module->top_level_bindings[i].original, original) == 0)
            return &(module->top_level_bindings[i]);
    }
    return NULL;
}

#endif /* MODULE_SHARED_H */
