#ifndef MODULE_ASSEMBLE_H
#define MODULE_ASSEMBLE_H

#include "ModuleRewrite.h"

static void moduleFreeStorage(ModuleCompileContext *context)
{
    if(context == NULL)
        return;
    for(int i = 0; i < context->module_count; i++)
    {
        ModuleSourceFile *module = context->modules[i];
        if(module == NULL)
            continue;
        free(module->imports);
        free(module->expression_imports);
        free(module->top_level_bindings);
        free(module);
    }
    free(context->modules);
    context->modules = NULL;
    context->module_count = 0;
    context->module_capacity = 0;
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

    for(int i = 0; i < module->expression_import_count; i++)
        moduleAppendStatementsDepthFirst(module->expression_imports[i], visited, head, tail);

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

static ASTNode* buildModuleProgramAST(const char *input_path, ModulePackage *packages, int package_count,
                                      bool require_entry)
{
    ModuleCompileContext context = {0};
    if(package_count > MODULE_MAX_PACKAGES)
        moduleSystemError("too many packages", input_path, 0, 0);
    for(int i = 0; i < package_count; i++)
        context.packages[i] = packages[i];
    context.package_count = package_count;
    ModuleSourceFile *root_module = moduleLoadRecursive(&context, input_path);

    ModuleCompileContext *previous_rewrite_context = moduleRewriteContext;
    moduleRewriteContext = &context;
    moduleAssignPrefixes(&context);
    for(int i = 0; i < context.module_count; i++)
        moduleCollectTopLevelBindings(context.modules[i]);
    for(int i = 0; i < context.module_count; i++)
        moduleRewrite(context.modules[i]);
    moduleRewriteContext = previous_rewrite_context;

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
    strcpy(root->package_name, root_module->package_name);

    ModuleTopLevelBinding *entry_binding = moduleFindEntryBinding(root_module, require_entry);
    if(entry_binding != NULL)
    {
        strcpy(root->entry_symbol, entry_binding->mangled);
        root->entry_returns_void = entry_binding->decl->rhs->return_data_type->primary == AST_PRIMARY_DATA_TYPE_VOID;
    }
    moduleFreeStorage(&context);
    return root;
}

#endif /* MODULE_ASSEMBLE_H */
