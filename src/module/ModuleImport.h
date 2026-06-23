#ifndef MODULE_IMPORT_H
#define MODULE_IMPORT_H

#include "ModuleScan.h"

static ModulePackage* moduleFindPackage(ModuleCompileContext *context, const char *package_name)
{
    for(int i = 0; i < context->package_count; i++)
    {
        if(context->packages[i].is_collection && strcmp(context->packages[i].name, package_name) == 0)
            return &(context->packages[i]);
    }
    return NULL;
}

static bool moduleParseCollectionImportPath(const char *import_path,
                                            char *collection_name,
                                            size_t collection_name_size,
                                            char *package_path,
                                            size_t package_path_size)
{
    if(import_path == NULL || import_path[0] == '\0')
        return false;

    const char *colon = strchr(import_path, ':');
    if(colon == NULL)
        return false;
    if(strchr(colon + 1, ':') != NULL)
        return false;

    size_t collection_length = (size_t)(colon - import_path);
    size_t package_length = strlen(colon + 1);
    if(collection_length == 0 || collection_length >= collection_name_size || package_length == 0 || package_length >= package_path_size)
        return false;

    memcpy(collection_name, import_path, collection_length);
    collection_name[collection_length] = '\0';
    memcpy(package_path, colon + 1, package_length + 1);
    return true;
}

static bool moduleTryResolvePackageDirectoryPath(const char *base_path, const char *import_name, char *buffer)
{
    char candidate[MODULE_MAX_PATH_LENGTH] = {0};
    if(import_name == NULL || import_name[0] == '\0')
        return false;

    moduleJoinPath(base_path, import_name, candidate);
    if(moduleDirectoryExists(candidate))
    {
        moduleCanonicalizePath(candidate, buffer);
        return true;
    }
    return false;
}

static bool moduleTryResolveCollectionImportPath(ModuleCompileContext *context,
                                                 const char *import_path,
                                                 char *buffer)
{
    char collection_name[MAX_IDENTIFIER_LENGTH] = {0};
    char package_path[MODULE_MAX_PATH_LENGTH] = {0};
    if(!moduleParseCollectionImportPath(import_path,
                                        collection_name, sizeof(collection_name),
                                        package_path, sizeof(package_path)))
        return false;

    ModulePackage *collection = moduleFindPackage(context, collection_name);
    if(collection == NULL)
        return false;

    if(package_path[0] == '\0' ||
       package_path[0] == '.' ||
       moduleIsAbsolutePath(package_path) ||
       strstr(package_path, "..") != NULL ||
       strchr(package_path, '\\') != NULL)
        return false;

    return moduleTryResolvePackageDirectoryPath(collection->root_path, package_path, buffer);
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

    char collection_name[MAX_IDENTIFIER_LENGTH] = {0};
    char package_path[MODULE_MAX_PATH_LENGTH] = {0};
    bool is_collection_import = moduleParseCollectionImportPath(import_path,
                                                                collection_name, sizeof(collection_name),
                                                                package_path, sizeof(package_path));
    if(is_collection_import)
    {
        if(collection_name[0] == '\0' ||
           package_path[0] == '\0' ||
           package_path[0] == '.' ||
           moduleIsAbsolutePath(package_path) ||
           strstr(package_path, "..") != NULL ||
           strchr(package_path, '\\') != NULL)
            moduleSystemError("collection imports must use collection:path and cannot use relative or absolute paths",
                              import_expr->filename, import_expr->line_number, import_expr->column_number);

        if(moduleTryResolveCollectionImportPath(context, import_path, buffer))
            return;

        if(moduleFindPackage(context, collection_name) == NULL)
            moduleSystemError("unknown package collection in import path",
                              import_expr->filename, import_expr->line_number, import_expr->column_number);

        moduleSystemError("cannot resolve package inside collection import path",
                          import_expr->filename, import_expr->line_number, import_expr->column_number);
    }

    if(moduleIsAbsolutePath(import_path) || strchr(import_path, '/') != NULL || strchr(import_path, '\\') != NULL || import_path[0] == '.')
        moduleSystemError("plain package imports must use a package name only; use collection:path for collection imports",
                          import_expr->filename, import_expr->line_number, import_expr->column_number);

    if(moduleTryResolvePackageDirectoryPath(importer_path, import_path, buffer))
        return;

    for(int i = 0; i < context->package_count; i++)
    {
        ModulePackage *package = &(context->packages[i]);
        if(package->is_search_root)
        {
            if(moduleTryResolvePackageDirectoryPath(package->root_path, import_path, buffer))
                return;
        }
    }

    moduleSystemError("cannot resolve import path; add -I <dir> for module search roots",
                      import_expr->filename, import_expr->line_number, import_expr->column_number);
}

static ModuleSourceFile* moduleLoadRecursive(ModuleCompileContext *context, const char *path);
static bool rewriteExprLooksLikeTypeValue(ModuleSourceFile *module, RewriteScope *scope, ASTNode *node);

static void moduleRecordExpressionImport(ModuleCompileContext *context, const char *module_canonical_path,
                                         ModuleSourceFile *imported_module)
{
    ModuleSourceFile *module = moduleFindByPath(context, module_canonical_path);
    if(module == NULL)
        moduleSystemError("internal module lookup failed during import record",
                          module_canonical_path, 0, 0);

    if(imported_module == NULL)
        return;

    for(int i = 0; i < module->expression_import_count; i++)
    {
        if(module->expression_imports[i] == imported_module)
            return;
    }

    moduleAppendExpressionImport(module, imported_module);
}

static void moduleScanImportExpressions(ModuleCompileContext *context, const char *module_canonical_path, ASTNode *node)
{
    if(node == NULL)
        return;

    ModuleSourceFile *module = moduleFindByPath(context, module_canonical_path);
    if(module == NULL)
        moduleSystemError("internal module lookup failed during import scan",
                          module_canonical_path, 0, 0);

    if(node->kind == AST_EXPR_BUILTIN && strcmp(node->identifier, "import") == 0)
    {
        char resolved_path[MODULE_MAX_PATH_LENGTH] = {0};
        moduleResolveImportPath(context, module->canonical_path, node, resolved_path);
        moduleRecordExpressionImport(context, module_canonical_path, moduleLoadRecursive(context, resolved_path));
    }

    moduleScanImportExpressions(context, module_canonical_path, node->lhs);
    moduleScanImportExpressions(context, module_canonical_path, node->rhs);
    moduleScanImportExpressions(context, module_canonical_path, node->extra);
    moduleScanImportExpressions(context, module_canonical_path, node->body);

    if(node->kind == AST_EXPR_STRUCT)
    {
        for(ASTStructMember *member = node->members; member != NULL; member = member->next)
            moduleScanImportExpressions(context, module_canonical_path, member->value);
    }

    if(node->kind == AST_EXPR_STRUCT_LITERAL)
    {
        for(ASTStructLiteralField *field = node->struct_literal_fields; field != NULL; field = field->next)
            moduleScanImportExpressions(context, module_canonical_path, field->value);
    }
}

static void moduleScanImports(ModuleCompileContext *context, ModuleSourceFile *module)
{
    ASTNode *statement = moduleStatements(module->ast_root);
    while(statement)
    {
        moduleScanImportExpressions(context, module->canonical_path, statement);

        if(moduleIsImportDecl(statement))
        {
            if(statement->modifier.is_runtime_binding ||
               statement->modifier.explicit_type ||
               !statement->modifier.is_compile_time_binding)
                moduleSystemError("import declarations must use compile-time constant syntax like `name :: @import(...)`",
                                  statement->filename, statement->line_number, statement->column_number);
            if(statement->is_pub)
                moduleSystemError("pub import re-export is not supported yet",
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
            moduleRecordExpressionImport(context, module->canonical_path, imported);
            ModuleImportBinding *binding = moduleAppendImportBinding(module);
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
            moduleSystemError("cyclic package imports are not supported", canonical_path, 0, 0);
        return existing;
    }

    if(!moduleDirectoryExists(canonical_path))
        moduleSystemError("package path must be a directory", canonical_path, 0, 0);

    ModuleSourceFile *module = moduleAppend(context);
    snprintf(module->canonical_path, sizeof(module->canonical_path), "%s", canonical_path);
    moduleDirectoryName(canonical_path, module->directory);
    module->visit_state = 1;
    module->ast_root = moduleParsePackageDirectory(canonical_path, module->package_name);
    if(module->ast_root->filename != NULL)
        snprintf(module->primary_source_path, sizeof(module->primary_source_path), "%s", module->ast_root->filename);

    moduleScanImports(context, module);
    module->visit_state = 2;
    return module;
}

static void moduleAssignPrefixes(ModuleCompileContext *context)
{
    for(int i = 0; i < context->module_count; i++)
        snprintf(context->modules[i]->symbol_prefix, sizeof(context->modules[i]->symbol_prefix), "m%d__", i);
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
                binding = moduleAppendTopLevelBinding(module);
                strcpy(binding->original, statement->identifier);
                snprintf(binding->mangled, sizeof(binding->mangled), "%s%s", module->symbol_prefix, statement->identifier);
                binding->is_type_decl = moduleIsStructDeclAssign(statement) ||
                                        moduleIsEnumDeclAssign(statement) ||
                                        rewriteExprLooksLikeTypeValue(module, NULL, statement->rhs);
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

static ModuleTopLevelBinding* moduleFindEntryBinding(ModuleSourceFile *module, bool required)
{
    if(module == NULL)
        return NULL;

    ModuleTopLevelBinding *binding = moduleFindTopLevelBinding(module, "main");
    if(binding == NULL)
    {
        if(required)
            moduleSystemError("target package does not define a top-level main binding",
                              module->primary_source_path[0] != '\0' ? module->primary_source_path : module->canonical_path,
                              module->ast_root != NULL ? module->ast_root->line_number : 0,
                              module->ast_root != NULL ? module->ast_root->column_number : 0);
        return NULL;
    }
    if(!required)
        return NULL;
    if(binding->decl == NULL || binding->decl->rhs == NULL || binding->decl->rhs->kind != AST_EXPR_FUNCTION)
        moduleSystemError("target package main must be a function",
                          binding->decl != NULL ? binding->decl->filename : module->canonical_path,
                          binding->decl != NULL ? binding->decl->line_number : 0,
                          binding->decl != NULL ? binding->decl->column_number : 0);

    ASTNode *function = binding->decl->rhs;
    if(function->parameters != NULL || function->is_variadic)
        moduleSystemError("target package main must have no parameters",
                          function->filename, function->line_number, function->column_number);
    
    // if the return type is not specified, we treat it as void
    if(function->return_data_type == NULL)
    {
        function->return_data_type = newPrimaryDataType(AST_PRIMARY_DATA_TYPE_VOID);
    }

    ASTDataType *return_type = function->return_data_type;
    if(return_type->kind != AST_DATA_TYPE_KIND_PRIMARY ||
       (return_type->primary != AST_PRIMARY_DATA_TYPE_VOID &&
        return_type->primary != AST_PRIMARY_DATA_TYPE_I32))
        moduleSystemError("target package main must return void or i32",
                          function->filename, function->line_number, function->column_number);

    return binding;
}

#endif /* MODULE_IMPORT_H */
