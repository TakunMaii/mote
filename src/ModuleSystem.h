#ifndef MODULE_SYSTEM_H
#define MODULE_SYSTEM_H

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Lexer.h"
#include "Parser.h"

#define MODULE_MAX_PATH_LENGTH 1024
#define MODULE_MAX_IMPORTS 256
#define MODULE_MAX_PACKAGES 128
#define MODULE_MAX_TOP_LEVEL_BINDINGS 1024
#define MODULE_MAX_SCOPE_VALUE_BINDINGS 1024
#define MODULE_MAX_SCOPE_TYPE_BINDINGS 512

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
} ModulePackage;

struct ModuleSourceFile {
    char canonical_path[MODULE_MAX_PATH_LENGTH];
    char directory[MODULE_MAX_PATH_LENGTH];
    char symbol_prefix[32];
    ASTNode *ast_root;
    int visit_state;
    ModuleImportBinding imports[MODULE_MAX_IMPORTS];
    int import_count;
    ModuleTopLevelBinding top_level_bindings[MODULE_MAX_TOP_LEVEL_BINDINGS];
    int top_level_binding_count;
    bool rewritten;
};

typedef struct ModuleCompileContext {
    ModuleSourceFile *modules;
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
} RewriteValueBinding;

typedef struct RewriteTypeBinding {
    char original[MAX_IDENTIFIER_LENGTH];
    char rewritten[MAX_IDENTIFIER_LENGTH];
} RewriteTypeBinding;

typedef struct RewriteScope {
    struct RewriteScope *parent;
    RewriteValueBinding value_bindings[MODULE_MAX_SCOPE_VALUE_BINDINGS];
    int value_count;
    RewriteTypeBinding type_bindings[MODULE_MAX_SCOPE_TYPE_BINDINGS];
    int type_count;
} RewriteScope;

static void moduleSystemError(const char *message, const char *filename, int line, int column)
{
    if(filename != NULL)
        printf("Module error: %s at file %s, line %d, column %d\n", message, filename, line, column);
    else
        printf("Module error: %s\n", message);
    exit(1);
}

static char* moduleReadFile(const char *path)
{
    FILE *f = fopen(path, "rb");
    if(f == NULL)
        return NULL;

    if(fseek(f, 0, SEEK_END) != 0)
    {
        fclose(f);
        return NULL;
    }

    long size = ftell(f);
    if(size < 0)
    {
        fclose(f);
        return NULL;
    }

    rewind(f);
    char *buffer = (char*) malloc(size + 1);
    if(buffer == NULL)
    {
        fclose(f);
        return NULL;
    }

    size_t read_bytes = fread(buffer, 1, size, f);
    fclose(f);
    if(read_bytes != (size_t)size)
    {
        free(buffer);
        return NULL;
    }

    buffer[size] = '\0';
    return buffer;
}

static bool moduleIsAbsolutePath(const char *path)
{
    if(path == NULL || path[0] == '\0')
        return false;
#if defined(_WIN32)
    return (strlen(path) >= 2 && path[1] == ':') || path[0] == '\\' || path[0] == '/';
#else
    return path[0] == '/';
#endif
}

static bool moduleFileExists(const char *path)
{
    FILE *f = fopen(path, "rb");
    if(f == NULL)
        return false;
    fclose(f);
    return true;
}

static void moduleDirectoryName(const char *path, char *buffer)
{
    const char *last_backslash = strrchr(path, '\\');
    const char *last_slash = strrchr(path, '/');
    const char *separator = last_backslash;
    if(separator == NULL || (last_slash != NULL && last_slash > separator))
        separator = last_slash;

    if(separator == NULL)
    {
        strcpy(buffer, ".");
        return;
    }

    size_t length = (size_t)(separator - path);
    if(length == 0)
        length = 1;
    if(length >= MODULE_MAX_PATH_LENGTH)
        moduleSystemError("directory path is too long", path, 0, 0);

    memcpy(buffer, path, length);
    buffer[length] = '\0';
}

static void moduleJoinPath(const char *base_dir, const char *suffix, char *buffer)
{
    if(moduleIsAbsolutePath(suffix))
    {
        snprintf(buffer, MODULE_MAX_PATH_LENGTH, "%s", suffix);
        return;
    }

    if(strcmp(base_dir, ".") == 0)
        snprintf(buffer, MODULE_MAX_PATH_LENGTH, "%s", suffix);
    else
        snprintf(buffer, MODULE_MAX_PATH_LENGTH, "%s/%s", base_dir, suffix);
}

static void moduleCanonicalizePath(const char *path, char *buffer)
{
#if defined(_WIN32)
    if(_fullpath(buffer, path, MODULE_MAX_PATH_LENGTH) == NULL)
        moduleSystemError("cannot canonicalize path", path, 0, 0);
#else
    if(realpath(path, buffer) == NULL)
        moduleSystemError("cannot canonicalize path", path, 0, 0);
#endif
}

static bool moduleIsRelativeImportPath(const char *path)
{
    return path != NULL &&
           path[0] == '.' &&
           (path[1] == '/' || path[1] == '\\' ||
            (path[1] == '.' && (path[2] == '/' || path[2] == '\\')));
}

static ModulePackage* moduleFindPackage(ModuleCompileContext *context, const char *package_name)
{
    for(int i = 0; i < context->package_count; i++)
    {
        if(strcmp(context->packages[i].name, package_name) == 0)
            return &(context->packages[i]);
    }
    return NULL;
}

static void moduleResolveModuleFilePath(const char *base_path, const char *import_suffix, const char *filename,
                                        int line, int column, char *buffer)
{
    char candidate[MODULE_MAX_PATH_LENGTH] = {0};
    if(import_suffix == NULL || import_suffix[0] == '\0')
        snprintf(candidate, sizeof(candidate), "%s", base_path);
    else
        moduleJoinPath(base_path, import_suffix, candidate);

    if(moduleFileExists(candidate))
    {
        moduleCanonicalizePath(candidate, buffer);
        return;
    }

    char with_extension[MODULE_MAX_PATH_LENGTH] = {0};
    snprintf(with_extension, sizeof(with_extension), "%s.mote", candidate);
    if(moduleFileExists(with_extension))
    {
        moduleCanonicalizePath(with_extension, buffer);
        return;
    }

    char root_module[MODULE_MAX_PATH_LENGTH] = {0};
    snprintf(root_module, sizeof(root_module), "%s/root.mote", candidate);
    if(moduleFileExists(root_module))
    {
        moduleCanonicalizePath(root_module, buffer);
        return;
    }

    moduleSystemError("cannot resolve import path", filename, line, column);
}

static void moduleResolveImportPath(ModuleCompileContext *context, const char *importer_path, ASTNode *import_expr,
                                    char *buffer)
{
    if(import_expr == NULL || import_expr->kind != AST_EXPR_BUILTIN || strcmp(import_expr->identifier, "import") != 0)
        moduleSystemError("expected @import builtin", importer_path, 0, 0);

    if(import_expr->lhs == NULL || import_expr->lhs->next != NULL || import_expr->lhs->kind != AST_EXPR_LITERAL_STRING)
        moduleSystemError("@import expects exactly one string literal argument",
                          import_expr->filename, import_expr->line_number, import_expr->column_number);

    const char *import_path = import_expr->lhs->literal_string;
    if(import_path[0] == '\0')
        moduleSystemError("@import path cannot be empty",
                          import_expr->filename, import_expr->line_number, import_expr->column_number);

    if(moduleIsAbsolutePath(import_path) || moduleIsRelativeImportPath(import_path))
    {
        char importer_dir[MODULE_MAX_PATH_LENGTH] = {0};
        moduleDirectoryName(importer_path, importer_dir);
        moduleResolveModuleFilePath(importer_dir, import_path,
                                    import_expr->filename, import_expr->line_number, import_expr->column_number,
                                    buffer);
        return;
    }

    char package_name[MAX_IDENTIFIER_LENGTH] = {0};
    const char *suffix = NULL;
    const char *separator = strpbrk(import_path, "/\\");
    if(separator == NULL)
        snprintf(package_name, sizeof(package_name), "%s", import_path);
    else
    {
        size_t package_name_length = (size_t)(separator - import_path);
        if(package_name_length >= sizeof(package_name))
            moduleSystemError("package import name is too long",
                              import_expr->filename, import_expr->line_number, import_expr->column_number);
        memcpy(package_name, import_path, package_name_length);
        package_name[package_name_length] = '\0';
        suffix = separator + 1;
    }

    ModulePackage *package = moduleFindPackage(context, package_name);
    if(package == NULL)
        moduleSystemError("unknown package import; pass --pkg name=path to the compiler",
                          import_expr->filename, import_expr->line_number, import_expr->column_number);

    moduleResolveModuleFilePath(package->root_path, suffix,
                                import_expr->filename, import_expr->line_number, import_expr->column_number,
                                buffer);
}

static ASTNode* moduleStatements(ASTNode *root)
{
    if(root == NULL || root->lhs == NULL)
        return NULL;
    return root->lhs->lhs;
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
        if(strcmp(context->modules[i].canonical_path, canonical_path) == 0)
            return &(context->modules[i]);
    }
    return NULL;
}

static ModuleSourceFile* moduleAppend(ModuleCompileContext *context)
{
    if(context->module_count >= context->module_capacity)
    {
        int new_capacity = context->module_capacity == 0 ? 8 : context->module_capacity * 2;
        context->modules = (ModuleSourceFile*) realloc(context->modules, sizeof(ModuleSourceFile) * new_capacity);
        context->module_capacity = new_capacity;
    }

    ModuleSourceFile *module = &(context->modules[context->module_count++]);
    memset(module, 0, sizeof(ModuleSourceFile));
    return module;
}

static ModuleSourceFile* moduleLoadRecursive(ModuleCompileContext *context, const char *path);

static void moduleScanImports(ModuleCompileContext *context, ModuleSourceFile *module)
{
    ASTNode *statement = moduleStatements(module->ast_root);
    while(statement)
    {
        if(moduleIsImportDecl(statement))
        {
            if(statement->modifier.mutable || statement->modifier.explicit_type)
                moduleSystemError("import declarations cannot use mut or explicit type syntax",
                                  statement->filename, statement->line_number, statement->column_number);
            if(statement->is_pub)
                moduleSystemError("pub import re-export is not supported yet",
                                  statement->filename, statement->line_number, statement->column_number);
            if(module->import_count >= MODULE_MAX_IMPORTS)
                moduleSystemError("too many imports in one module",
                                  statement->filename, statement->line_number, statement->column_number);

            for(int i = 0; i < module->import_count; i++)
            {
                if(strcmp(module->imports[i].alias, statement->identifier) == 0)
                    moduleSystemError("duplicate import alias",
                                      statement->filename, statement->line_number, statement->column_number);
            }

            char resolved_path[MODULE_MAX_PATH_LENGTH] = {0};
            moduleResolveImportPath(context, module->canonical_path, statement->rhs, resolved_path);
            ModuleSourceFile *imported = moduleLoadRecursive(context, resolved_path);
            ModuleImportBinding *binding = &(module->imports[module->import_count++]);
            strcpy(binding->alias, statement->identifier);
            binding->module = imported;
        }

        statement = statement->next;
    }
}

static ModuleSourceFile* moduleLoadRecursive(ModuleCompileContext *context, const char *path)
{
    char canonical_path[MODULE_MAX_PATH_LENGTH] = {0};
    moduleCanonicalizePath(path, canonical_path);

    ModuleSourceFile *existing = moduleFindByPath(context, canonical_path);
    if(existing != NULL)
    {
        if(existing->visit_state == 1)
            moduleSystemError("cyclic imports are not supported yet", canonical_path, 0, 0);
        return existing;
    }

    char *source = moduleReadFile(canonical_path);
    if(source == NULL)
        moduleSystemError("cannot open source file", canonical_path, 0, 0);

    Token *tokens = tokenize(source, canonical_path);
    ASTNode *root = parse(tokens);

    ModuleSourceFile *module = moduleAppend(context);
    strcpy(module->canonical_path, canonical_path);
    moduleDirectoryName(canonical_path, module->directory);
    module->ast_root = root;
    module->visit_state = 1;

    moduleScanImports(context, module);
    module->visit_state = 2;
    return module;
}

static void moduleAssignPrefixes(ModuleCompileContext *context)
{
    for(int i = 0; i < context->module_count; i++)
        snprintf(context->modules[i].symbol_prefix, sizeof(context->modules[i].symbol_prefix), "m%d__", i);
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

static void moduleCollectTopLevelBindings(ModuleSourceFile *module)
{
    ASTNode *statement = moduleStatements(module->ast_root);
    while(statement)
    {
        if(moduleIsImportDecl(statement))
        {
            statement = statement->next;
            continue;
        }

        if(statement->kind == AST_ASSIGN &&
           statement->lhs != NULL &&
           statement->lhs->kind == AST_EXPR_VARIABLE)
        {
            ModuleTopLevelBinding *binding = moduleFindTopLevelBinding(module, statement->identifier);
            if(binding == NULL)
            {
                if(module->top_level_binding_count >= MODULE_MAX_TOP_LEVEL_BINDINGS)
                    moduleSystemError("too many top-level bindings in one module",
                                      statement->filename, statement->line_number, statement->column_number);

                binding = &(module->top_level_bindings[module->top_level_binding_count++]);
                memset(binding, 0, sizeof(ModuleTopLevelBinding));
                strcpy(binding->original, statement->identifier);
                snprintf(binding->mangled, sizeof(binding->mangled), "%s%s", module->symbol_prefix, statement->identifier);
                binding->is_type_decl = moduleIsStructDeclAssign(statement) || moduleIsEnumDeclAssign(statement);
                binding->decl = statement;
                binding->is_pub = statement->is_pub;
            }
            else if(statement->is_pub)
                moduleSystemError("pub can only be applied to the first top-level declaration of a name",
                                  statement->filename, statement->line_number, statement->column_number);
        }

        statement = statement->next;
    }
}

static void initRewriteScope(RewriteScope *scope, RewriteScope *parent)
{
    memset(scope, 0, sizeof(RewriteScope));
    scope->parent = parent;
}

static RewriteValueBinding* findRewriteValueBindingInScope(RewriteScope *scope, const char *identifier)
{
    for(int i = 0; i < scope->value_count; i++)
    {
        if(strcmp(scope->value_bindings[i].original, identifier) == 0)
            return &(scope->value_bindings[i]);
    }
    return NULL;
}

static RewriteValueBinding* findRewriteValueBinding(RewriteScope *scope, const char *identifier)
{
    RewriteScope *current = scope;
    while(current)
    {
        RewriteValueBinding *binding = findRewriteValueBindingInScope(current, identifier);
        if(binding != NULL)
            return binding;
        current = current->parent;
    }
    return NULL;
}

static RewriteTypeBinding* findRewriteTypeBindingInScope(RewriteScope *scope, const char *identifier)
{
    for(int i = 0; i < scope->type_count; i++)
    {
        if(strcmp(scope->type_bindings[i].original, identifier) == 0)
            return &(scope->type_bindings[i]);
    }
    return NULL;
}

static RewriteTypeBinding* findRewriteTypeBinding(RewriteScope *scope, const char *identifier)
{
    RewriteScope *current = scope;
    while(current)
    {
        RewriteTypeBinding *binding = findRewriteTypeBindingInScope(current, identifier);
        if(binding != NULL)
            return binding;
        current = current->parent;
    }
    return NULL;
}

static void declareRewriteValueBinding(RewriteScope *scope, const char *original, const char *rewritten)
{
    if(scope->value_count >= MODULE_MAX_SCOPE_VALUE_BINDINGS)
        moduleSystemError("too many value bindings in rewrite scope", NULL, 0, 0);

    RewriteValueBinding *binding = &(scope->value_bindings[scope->value_count++]);
    memset(binding, 0, sizeof(RewriteValueBinding));
    strcpy(binding->original, original);
    strcpy(binding->rewritten, rewritten);
}

static void declareRewriteImportBinding(RewriteScope *scope, const char *alias, ModuleSourceFile *imported_module)
{
    if(scope->value_count >= MODULE_MAX_SCOPE_VALUE_BINDINGS)
        moduleSystemError("too many value bindings in rewrite scope", NULL, 0, 0);

    RewriteValueBinding *binding = &(scope->value_bindings[scope->value_count++]);
    memset(binding, 0, sizeof(RewriteValueBinding));
    strcpy(binding->original, alias);
    binding->is_import_alias = true;
    binding->imported_module = imported_module;
}

static void declareRewriteTypeBinding(RewriteScope *scope, const char *original, const char *rewritten)
{
    if(scope->type_count >= MODULE_MAX_SCOPE_TYPE_BINDINGS)
        moduleSystemError("too many type bindings in rewrite scope", NULL, 0, 0);

    RewriteTypeBinding *binding = &(scope->type_bindings[scope->type_count++]);
    memset(binding, 0, sizeof(RewriteTypeBinding));
    strcpy(binding->original, original);
    strcpy(binding->rewritten, rewritten);
}

static bool moduleDataTypeIsPrimaryTypeKeyword(ASTDataType *data_type)
{
    return data_type != NULL &&
           data_type->kind == AST_DATA_TYPE_KIND_PRIMARY &&
           data_type->primary == AST_PRIMARY_DATA_TYPE_TYPE;
}

static void rewriteDataType(ModuleSourceFile *module, RewriteScope *scope, ASTDataType *data_type);
static void rewriteExpr(ModuleSourceFile *module, RewriteScope *scope, ASTNode *node);
static void rewriteStatement(ModuleSourceFile *module, RewriteScope *scope, ASTNode *node);

static void rewriteExprList(ModuleSourceFile *module, RewriteScope *scope, ASTNode *node)
{
    while(node)
    {
        rewriteExpr(module, scope, node);
        node = node->next;
    }
}

static void rewriteNamedDataTypeIdentifier(ModuleSourceFile *module, RewriteScope *scope, ASTDataType *data_type)
{
    char *dot = strchr(data_type->identifier, '.');
    if(dot != NULL)
    {
        char alias[MAX_IDENTIFIER_LENGTH] = {0};
        char member[MAX_IDENTIFIER_LENGTH] = {0};
        size_t alias_length = (size_t)(dot - data_type->identifier);
        memcpy(alias, data_type->identifier, alias_length);
        alias[alias_length] = '\0';
        strcpy(member, dot + 1);

        if(strchr(member, '.') != NULL)
            moduleSystemError("nested qualified imported type names are not supported yet",
                              NULL, 0, 0);

        RewriteValueBinding *import_binding = findRewriteValueBinding(scope, alias);
        if(import_binding == NULL || !import_binding->is_import_alias)
            moduleSystemError("qualified type name requires an imported module alias",
                              NULL, 0, 0);

        ModuleTopLevelBinding *binding = moduleFindTopLevelBinding(import_binding->imported_module, member);
        if(binding == NULL || !binding->is_pub)
            moduleSystemError("imported module does not export the requested type name",
                              NULL, 0, 0);

        strcpy(data_type->identifier, binding->mangled);
        return;
    }

    RewriteTypeBinding *type_binding = findRewriteTypeBinding(scope, data_type->identifier);
    if(type_binding != NULL)
    {
        strcpy(data_type->identifier, type_binding->rewritten);
        return;
    }

    RewriteValueBinding *value_binding = findRewriteValueBinding(scope, data_type->identifier);
    if(value_binding != NULL)
    {
        if(value_binding->is_import_alias)
            moduleSystemError("a module alias cannot be used as a type without selecting a member",
                              NULL, 0, 0);
        strcpy(data_type->identifier, value_binding->rewritten);
    }
}

static void rewriteStructMembers(ModuleSourceFile *module, RewriteScope *scope, ASTStructMember *member)
{
    while(member)
    {
        rewriteDataType(module, scope, member->data_type);
        if(member->value != NULL)
            rewriteExpr(module, scope, member->value);
        member = member->next;
    }
}

static void rewriteFunctionExpr(ModuleSourceFile *module, RewriteScope *scope, ASTNode *node)
{
    RewriteScope *function_scope = (RewriteScope*) malloc(sizeof(RewriteScope));
    initRewriteScope(function_scope, scope);

    ASTFunctionCapture *capture = node->captures;
    while(capture)
    {
        declareRewriteValueBinding(function_scope, capture->identifier, capture->identifier);
        if(findRewriteTypeBinding(scope, capture->identifier) != NULL)
            declareRewriteTypeBinding(function_scope, capture->identifier, capture->identifier);
        capture = capture->next;
    }

    ASTFunctionParameter *parameter = node->parameters;
    while(parameter)
    {
        rewriteDataType(module, function_scope, parameter->data_type);
        declareRewriteValueBinding(function_scope, parameter->identifier, parameter->identifier);
        if(moduleDataTypeIsPrimaryTypeKeyword(parameter->data_type))
            declareRewriteTypeBinding(function_scope, parameter->identifier, parameter->identifier);
        parameter = parameter->next;
    }

    rewriteDataType(module, function_scope, node->return_data_type);
    rewriteStatement(module, function_scope, node->body);
    free(function_scope);
}

static void rewriteDataType(ModuleSourceFile *module, RewriteScope *scope, ASTDataType *data_type)
{
    if(data_type == NULL)
        return;

    switch(data_type->kind)
    {
        case AST_DATA_TYPE_KIND_NAMED:
            rewriteNamedDataTypeIdentifier(module, scope, data_type);
            return;
        case AST_DATA_TYPE_KIND_POINTER:
        case AST_DATA_TYPE_KIND_REFERENCE:
            rewriteDataType(module, scope, data_type->child);
            return;
        case AST_DATA_TYPE_KIND_FUNCTION: {
            ASTFunctionParameter *parameter = data_type->parameters;
            while(parameter)
            {
                rewriteDataType(module, scope, parameter->data_type);
                parameter = parameter->next;
            }
            rewriteDataType(module, scope, data_type->return_data_type);
            return;
        }
        case AST_DATA_TYPE_KIND_ARRAY:
            rewriteDataType(module, scope, data_type->child);
            return;
        case AST_DATA_TYPE_KIND_APPLY:
            rewriteDataType(module, scope, data_type->callee);
            for(ASTTypeArgument *argument = data_type->arguments; argument != NULL; argument = argument->next)
                rewriteDataType(module, scope, argument->data_type);
            return;
        case AST_DATA_TYPE_KIND_STRUCT:
            rewriteStructMembers(module, scope, data_type->members);
            return;
        default:
            return;
    }
}

static void rewriteExpr(ModuleSourceFile *module, RewriteScope *scope, ASTNode *node)
{
    if(node == NULL)
        return;

    switch(node->kind)
    {
        case AST_EXPR_VARIABLE: {
            RewriteValueBinding *value_binding = findRewriteValueBinding(scope, node->identifier);
            if(value_binding != NULL)
            {
                if(value_binding->is_import_alias)
                    moduleSystemError("module values can only be used through member access",
                                      node->filename, node->line_number, node->column_number);
                strcpy(node->identifier, value_binding->rewritten);
                return;
            }

            RewriteTypeBinding *type_binding = findRewriteTypeBinding(scope, node->identifier);
            if(type_binding != NULL)
                strcpy(node->identifier, type_binding->rewritten);
            return;
        }
        case AST_EXPR_BUILTIN:
            rewriteExprList(module, scope, node->lhs);
            return;
        case AST_EXPR_MEMBER: {
            if(node->lhs != NULL && node->lhs->kind == AST_EXPR_VARIABLE)
            {
                RewriteValueBinding *import_binding = findRewriteValueBinding(scope, node->lhs->identifier);
                if(import_binding != NULL && import_binding->is_import_alias)
                {
                    ModuleTopLevelBinding *binding = moduleFindTopLevelBinding(import_binding->imported_module, node->identifier);
                    if(binding == NULL || !binding->is_pub)
                        moduleSystemError("imported module does not export the requested member",
                                          node->filename, node->line_number, node->column_number);

                    node->kind = AST_EXPR_VARIABLE;
                    node->lhs = NULL;
                    node->rhs = NULL;
                    node->extra = NULL;
                    node->body = NULL;
                    strcpy(node->identifier, binding->mangled);
                    return;
                }
            }

            rewriteExpr(module, scope, node->lhs);
            return;
        }
        case AST_EXPR_FUNCTION:
            rewriteFunctionExpr(module, scope, node);
            return;
        case AST_EXPR_STRUCT:
            rewriteStructMembers(module, scope, node->members);
            return;
        case AST_EXPR_STRUCT_LITERAL:
            rewriteExpr(module, scope, node->lhs);
            for(ASTStructLiteralField *field = node->struct_literal_fields; field != NULL; field = field->next)
                rewriteExpr(module, scope, field->value);
            return;
        case AST_EXPR_ARRAY_LITERAL:
            rewriteExprList(module, scope, node->lhs);
            return;
        case AST_EXPR_CALL:
            rewriteExpr(module, scope, node->lhs);
            rewriteExprList(module, scope, node->rhs);
            return;
        case AST_EXPR_LOGICAL_OR:
        case AST_EXPR_LOGICAL_AND:
        case AST_EXPR_BIT_OR:
        case AST_EXPR_BIT_XOR:
        case AST_EXPR_BIT_AND:
        case AST_EXPR_EQUAL:
        case AST_EXPR_NOT_EQUAL:
        case AST_EXPR_LESS:
        case AST_EXPR_LESS_EQUAL:
        case AST_EXPR_GREATER:
        case AST_EXPR_GREATER_EQUAL:
        case AST_EXPR_SHIFT_LEFT:
        case AST_EXPR_SHIFT_RIGHT:
        case AST_EXPR_MUL:
        case AST_EXPR_DIV:
        case AST_EXPR_MOD:
        case AST_EXPR_ADD:
        case AST_EXPR_SUB:
        case AST_EXPR_UNARY_PLUS:
        case AST_EXPR_UNARY_MINUS:
        case AST_EXPR_UNARY_LOGICAL_NOT:
        case AST_EXPR_UNARY_BIT_NOT:
        case AST_EXPR_ADDRESS_OF:
        case AST_EXPR_ADDRESS_OF_MUT:
        case AST_EXPR_DEREF:
        case AST_EXPR_PARENTHESIS:
        case AST_EXPR_INDEX:
        case AST_STATEMENT_EXPR:
        case AST_STATEMENT_RETURN:
            rewriteExpr(module, scope, node->lhs);
            rewriteExpr(module, scope, node->rhs);
            rewriteExpr(module, scope, node->extra);
            return;
        case AST_EXPR_TYPE_LITERAL:
            rewriteDataType(module, scope, node->data_type);
            return;
        default:
            return;
    }
}

static bool rewriteWouldDeclareNewVariable(RewriteScope *scope, ASTNode *node)
{
    if(node == NULL || node->kind != AST_ASSIGN || node->lhs == NULL || node->lhs->kind != AST_EXPR_VARIABLE)
        return false;

    if(node->modifier.mutable || node->modifier.explicit_type)
        return findRewriteValueBindingInScope(scope, node->identifier) == NULL;

    return findRewriteValueBinding(scope, node->identifier) == NULL;
}

static void rewriteStatementList(ModuleSourceFile *module, RewriteScope *scope, ASTNode *statement)
{
    while(statement)
    {
        rewriteStatement(module, scope, statement);
        statement = statement->next;
    }
}

static void rewriteStatement(ModuleSourceFile *module, RewriteScope *scope, ASTNode *node)
{
    if(node == NULL)
        return;

    switch(node->kind)
    {
        case AST_BLOCK: {
            RewriteScope *block_scope = (RewriteScope*) malloc(sizeof(RewriteScope));
            initRewriteScope(block_scope, scope);
            rewriteStatementList(module, block_scope, node->lhs);
            free(block_scope);
            return;
        }
        case AST_ASSIGN: {
            if(moduleIsImportDecl(node))
            {
                for(int i = 0; i < module->import_count; i++)
                {
                    if(strcmp(module->imports[i].alias, node->identifier) == 0)
                    {
                        declareRewriteImportBinding(scope, node->identifier, module->imports[i].module);
                        return;
                    }
                }
                moduleSystemError("internal import binding resolution failed",
                                  node->filename, node->line_number, node->column_number);
            }

            ModuleTopLevelBinding *top_level_binding = moduleFindTopLevelBinding(module, node->identifier);
            bool is_top_level_binding = top_level_binding != NULL && scope->parent == NULL;

            if(moduleIsStructDeclAssign(node) || moduleIsEnumDeclAssign(node))
            {
                char original_identifier[MAX_IDENTIFIER_LENGTH] = {0};
                strcpy(original_identifier, node->identifier);
                if(is_top_level_binding)
                {
                    strcpy(node->identifier, top_level_binding->mangled);
                    strcpy(node->lhs->identifier, top_level_binding->mangled);
                }

                declareRewriteTypeBinding(scope,
                                          is_top_level_binding ? top_level_binding->original : original_identifier,
                                          node->lhs->identifier);
                rewriteExpr(module, scope, node->rhs);
                return;
            }

            rewriteDataType(module, scope, node->data_type);
            rewriteExpr(module, scope, node->rhs);

            if(node->lhs->kind == AST_EXPR_VARIABLE)
            {
                RewriteValueBinding *existing = findRewriteValueBinding(scope, node->identifier);
                if(existing != NULL && !rewriteWouldDeclareNewVariable(scope, node))
                {
                    if(existing->is_import_alias)
                        moduleSystemError("cannot assign to an imported module alias",
                                          node->filename, node->line_number, node->column_number);
                    strcpy(node->identifier, existing->rewritten);
                    strcpy(node->lhs->identifier, existing->rewritten);
                    return;
                }

                if(is_top_level_binding)
                {
                    strcpy(node->identifier, top_level_binding->mangled);
                    strcpy(node->lhs->identifier, top_level_binding->mangled);
                    declareRewriteValueBinding(scope, top_level_binding->original, top_level_binding->mangled);
                    if(node->rhs != NULL && node->rhs->kind == AST_EXPR_TYPE_LITERAL)
                        declareRewriteTypeBinding(scope, top_level_binding->original, top_level_binding->mangled);
                }
                else if(rewriteWouldDeclareNewVariable(scope, node))
                {
                    declareRewriteValueBinding(scope, node->identifier, node->identifier);
                    if(node->rhs != NULL && node->rhs->kind == AST_EXPR_TYPE_LITERAL)
                        declareRewriteTypeBinding(scope, node->identifier, node->identifier);
                }
                return;
            }

            rewriteExpr(module, scope, node->lhs);
            return;
        }
        case AST_STATEMENT_RETURN:
            rewriteExpr(module, scope, node->lhs);
            return;
        case AST_STATEMENT_EXPR:
            rewriteExpr(module, scope, node->lhs);
            return;
        case AST_STATEMENT_IF:
            rewriteExpr(module, scope, node->lhs);
            rewriteStatement(module, scope, node->rhs);
            rewriteStatement(module, scope, node->body);
            return;
        case AST_STATEMENT_WHILE:
        case AST_STATEMENT_DO_WHILE:
            rewriteExpr(module, scope, node->lhs);
            rewriteStatement(module, scope, node->body);
            return;
        case AST_STATEMENT_FOR: {
            RewriteScope *for_scope = (RewriteScope*) malloc(sizeof(RewriteScope));
            initRewriteScope(for_scope, scope);
            rewriteStatement(module, for_scope, node->lhs);
            rewriteExpr(module, for_scope, node->rhs);
            rewriteStatement(module, for_scope, node->body);
            rewriteStatement(module, for_scope, node->extra);
            free(for_scope);
            return;
        }
        case AST_STATEMENT_DEFER:
            rewriteStatement(module, scope, node->lhs);
            return;
        default:
            return;
    }
}

static void moduleRewrite(ModuleSourceFile *module)
{
    if(module->rewritten)
        return;

    RewriteScope *top_scope = (RewriteScope*) malloc(sizeof(RewriteScope));
    initRewriteScope(top_scope, NULL);
    rewriteStatementList(module, top_scope, moduleStatements(module->ast_root));
    free(top_scope);
    module->rewritten = true;
}

static void moduleAppendStatementsDepthFirst(ModuleSourceFile *module, bool *visited, ASTNode **head, ASTNode **tail)
{
    int module_index = -1;
    int parsed_index = 0;
    while(module->symbol_prefix[0] != '\0')
    {
        if(sscanf(module->symbol_prefix, "m%d__", &parsed_index) == 1)
        {
            module_index = parsed_index;
            break;
        }
        break;
    }

    if(module_index < 0 || visited[module_index])
        return;
    visited[module_index] = true;

    for(int i = 0; i < module->import_count; i++)
        moduleAppendStatementsDepthFirst(module->imports[i].module, visited, head, tail);

    ASTNode *statement = moduleStatements(module->ast_root);
    while(statement)
    {
        ASTNode *next_statement = statement->next;
        if(!moduleIsImportDecl(statement))
        {
            statement->next = NULL;
            if(*head == NULL)
                *head = statement;
            else
                (*tail)->next = statement;
            *tail = statement;
        }
        statement = next_statement;
    }
}

static ASTNode* buildModuleProgramAST(const char *input_path, ModulePackage *packages, int package_count)
{
    ModuleCompileContext context = {0};
    if(package_count > MODULE_MAX_PACKAGES)
        moduleSystemError("too many packages", input_path, 0, 0);
    for(int i = 0; i < package_count; i++)
        context.packages[i] = packages[i];
    context.package_count = package_count;
    ModuleSourceFile *root_module = moduleLoadRecursive(&context, input_path);

    moduleAssignPrefixes(&context);
    for(int i = 0; i < context.module_count; i++)
        moduleCollectTopLevelBindings(&(context.modules[i]));
    for(int i = 0; i < context.module_count; i++)
        moduleRewrite(&(context.modules[i]));

    ASTNode *root = newASTNode(AST_START_OF_CODE);
    ASTNode *top_level_block = newASTNode(AST_BLOCK);
    ASTNode *head = NULL;
    ASTNode *tail = NULL;
    bool *visited = (bool*) calloc((size_t)context.module_count, sizeof(bool));
    moduleAppendStatementsDepthFirst(root_module, visited, &head, &tail);
    free(visited);

    top_level_block->lhs = head;
    root->lhs = top_level_block;
    root->next = newASTNode(AST_END_OF_CODE);
    return root;
}

#endif /* MODULE_SYSTEM_H */
