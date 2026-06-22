#ifndef MODULE_SCAN_H
#define MODULE_SCAN_H

#include "ModuleShared.h"

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

static bool moduleDirectoryExists(const char *path)
{
    struct stat file_info;
    if(stat(path, &file_info) != 0)
        return false;
    return S_ISDIR(file_info.st_mode);
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
        snprintf(buffer, MODULE_MAX_PATH_LENGTH, ".");
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
    int written = 0;
    if(moduleIsAbsolutePath(suffix))
    {
        written = snprintf(buffer, MODULE_MAX_PATH_LENGTH, "%s", suffix);
        if(written < 0 || written >= MODULE_MAX_PATH_LENGTH)
            moduleSystemError("path is too long", suffix, 0, 0);
        return;
    }

    if(strcmp(base_dir, ".") == 0)
        written = snprintf(buffer, MODULE_MAX_PATH_LENGTH, "%s", suffix);
    else
        written = snprintf(buffer, MODULE_MAX_PATH_LENGTH, "%s/%s", base_dir, suffix);

    if(written < 0 || written >= MODULE_MAX_PATH_LENGTH)
        moduleSystemError("path is too long", suffix, 0, 0);
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

static bool moduleHasMoteExtension(const char *path)
{
    if(path == NULL)
        return false;
    const char *last_dot = strrchr(path, '.');
    return last_dot != NULL && strcmp(last_dot, ".mote") == 0;
}

static char** moduleGrowStringList(char **items, int *capacity, int min_capacity)
{
    int next_capacity = *capacity == 0 ? 8 : *capacity;
    while(next_capacity < min_capacity)
        next_capacity *= 2;
    char **grown = (char**) realloc(items, sizeof(char*) * (size_t) next_capacity);
    if(grown == NULL)
        moduleSystemError("string list allocation failed", NULL, 0, 0);
    *capacity = next_capacity;
    return grown;
}

static void moduleAddOwnedString(char ***items, int *count, int *capacity, const char *value)
{
    if(*count >= *capacity)
        *items = moduleGrowStringList(*items, capacity, *count + 1);
    (*items)[*count] = diagnosticCloneString(value);
    (*count)++;
}

static int moduleCompareStringPointers(const void *lhs, const void *rhs)
{
    const char *const *left = (const char *const *) lhs;
    const char *const *right = (const char *const *) rhs;
    return strcmp(*left, *right);
}

static int moduleListPackageFiles(const char *directory_path, char ***paths_out)
{
    int count = 0;
    int capacity = 0;
    char **paths = NULL;
#if defined(_WIN32)
    char pattern[MODULE_MAX_PATH_LENGTH] = {0};
    snprintf(pattern, sizeof(pattern), "%s\\*.mote", directory_path);

    WIN32_FIND_DATAA find_data;
    HANDLE handle = FindFirstFileA(pattern, &find_data);
    if(handle == INVALID_HANDLE_VALUE)
        return 0;

    do
    {
        char full_path[MODULE_MAX_PATH_LENGTH] = {0};
        if(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            continue;
        if(snprintf(full_path, sizeof(full_path), "%s\\%s", directory_path, find_data.cFileName) >= (int) sizeof(full_path))
            moduleSystemError("source file path is too long", directory_path, 0, 0);
        moduleAddOwnedString(&paths, &count, &capacity, full_path);
    } while(FindNextFileA(handle, &find_data));

    FindClose(handle);
#else
    DIR *directory = opendir(directory_path);
    if(directory == NULL)
        return 0;

    struct dirent *entry = NULL;
    while((entry = readdir(directory)) != NULL)
    {
        char full_path[MODULE_MAX_PATH_LENGTH] = {0};
        if(entry->d_name[0] == '.')
            continue;
        if(!moduleHasMoteExtension(entry->d_name))
            continue;
        if(snprintf(full_path, sizeof(full_path), "%s/%s", directory_path, entry->d_name) >= (int) sizeof(full_path))
            moduleSystemError("source file path is too long", directory_path, 0, 0);
        moduleAddOwnedString(&paths, &count, &capacity, full_path);
    }

    closedir(directory);
#endif

    qsort(paths, (size_t)count, sizeof(char*), moduleCompareStringPointers);
    *paths_out = paths;
    return count;
}

static ASTNode* moduleParsePackageDirectory(const char *directory_path, char *package_name_buffer)
{
    char **file_paths = NULL;
    int file_count = moduleListPackageFiles(directory_path, &file_paths);
    if(file_count == 0)
        moduleSystemError("package directory contains no .mote files", directory_path, 0, 0);

    ASTNode *root = newASTNode(AST_START_OF_CODE);
    ASTNode *top_level_block = newASTNode(AST_BLOCK);
    ASTNode *head = NULL;
    ASTNode *tail = NULL;

    for(int i = 0; i < file_count; i++)
    {
        char *source = moduleReadFile(file_paths[i]);
        if(source == NULL)
            moduleSystemError("cannot open source file", file_paths[i], 0, 0);

        Token *tokens = tokenize(source, file_paths[i]);
        ASTNode *file_root = parse(tokens);
        if(file_root->package_name[0] == '\0')
            moduleSystemError("source file is missing a package declaration", file_paths[i], 0, 0);

        if(package_name_buffer[0] == '\0')
        {
            strcpy(package_name_buffer, file_root->package_name);
            root->filename = file_root->filename;
            root->line_number = file_root->line_number;
            root->column_number = file_root->column_number;
            root->end_line_number = file_root->end_line_number;
            root->end_column_number = file_root->end_column_number;
        }
        else if(strcmp(package_name_buffer, file_root->package_name) != 0)
            moduleSystemError("all files in a package directory must declare the same package name",
                              file_paths[i], file_root->line_number, file_root->column_number);

        moduleAppendStatementList(&head, &tail, moduleStatements(file_root));
        free(file_paths[i]);
    }
    free(file_paths);

    strcpy(root->package_name, package_name_buffer);
    root->lhs = top_level_block;
    top_level_block->lhs = head;
    root->next = newASTNode(AST_END_OF_CODE);
    return root;
}

#endif /* MODULE_SCAN_H */
