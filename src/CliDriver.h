#ifndef CLI_DRIVER_H
#define CLI_DRIVER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <unistd.h>
#else
#include <unistd.h>
#endif

#include "Diagnostic.h"
#include "ModuleSystem.h"

#define CLI_MAX_PACKAGES 128
#define CLI_MAX_LINK_ARGS 256
#define CLI_MAX_LINKER_ARGS 256
#define CLI_MAX_EXTRA_C_SOURCES 32
#define CLI_PATH_BUFFER_SIZE 4096

#if defined(_WIN32)
#define MOTE_RUNTIME_RELATIVE_PATH "runtime\\mote_runtime.c"
#else
#define MOTE_RUNTIME_RELATIVE_PATH "runtime/mote_runtime.c"
#endif

typedef struct CliOptions {
    bool emit_llvm;
    bool emit_exe;
    bool dump_ast;
    bool dump_mir;
    bool emit_debug_info;
    bool keep_llvm_output;
    const char *input_path;
    const char *requested_output_path;
    const char *llvm_output_path;
    const char *exe_output_path;
} CliOptions;

static bool starts_with(const char *value, const char *prefix)
{
    return strncmp(value, prefix, strlen(prefix)) == 0;
}

static MOTE_NORETURN void cliError(const char *message)
{
    diagnosticAbortSimple("C1001", message, makeSourceSpan(NULL, 0, 0, 0, 0), NULL);
}

static MOTE_NORETURN void cliErrorFormatted(const char *format, ...)
{
    Diagnostic diagnostic = diagnosticMake(DIAGNOSTIC_SEVERITY_ERROR, "C1001",
                                           makeSourceSpan(NULL, 0, 0, 0, 0), "");
    va_list args;
    va_start(args, format);
    diagnosticVFormat(diagnostic.message, sizeof(diagnostic.message), format, args);
    va_end(args);
    diagnosticAbort(diagnostic);
}

static void print_usage(const char *argv0)
{
    printf("Usage:\n");
    printf("  %s [options] <package_dir>\n", argv0);
    printf("\n");
    printf("Options:\n");
    printf("  -o <file>         Write output to <file>\n");
    printf("  -S                Write LLVM IR to a .ll file and stop\n");
    printf("  -I <dir>          Add a package search root for @import(\"name\")\n");
    printf("  -C <name=dir>     Add or override a package collection for @import(\"name:path\")\n");
    printf("  -L <dir>          Add a linker search directory\n");
    printf("  -l<name>          Link against <name>\n");
    printf("  -Wl,<args>        Forward comma-separated arguments to the linker\n");
    printf("  -g                Emit debug information for debuggers\n");
    printf("  --dump-ast        Print AST after parsing and rewriting\n");
    printf("  --dump-mir        Print MIR after lowering\n");
    printf("  --help, -h        Show this help text\n");
}

static void build_output_path(char *buffer, size_t buffer_size, const char *basename, const char *extension)
{
    int written = snprintf(buffer, buffer_size, "%s%s", basename, extension);
    if(written < 0 || (size_t) written >= buffer_size)
        cliErrorFormatted("output path is too long: %s%s", basename, extension);
}

static void build_temp_llvm_output_path(char *buffer, size_t buffer_size, const char *exe_output_path)
{
    int written = snprintf(buffer, buffer_size, "%s.mote-tmp.ll", exe_output_path);
    if(written < 0 || (size_t) written >= buffer_size)
        cliErrorFormatted("temporary llvm output path is too long: %s.mote-tmp.ll", exe_output_path);
}

static void copy_cli_path(char *buffer, size_t buffer_size, const char *value, const char *label)
{
    int written = snprintf(buffer, buffer_size, "%s", value);
    if(written < 0 || (size_t) written >= buffer_size)
        cliErrorFormatted("%s is too long: %s", label, value);
}

static void copy_cli_identifier(char *buffer, size_t buffer_size, const char *value, const char *label)
{
    int written = snprintf(buffer, buffer_size, "%s", value);
    if(written < 0 || (size_t) written >= buffer_size)
        cliErrorFormatted("%s is too long: %s", label, value);
}

static bool file_exists(const char *path)
{
    FILE *stream = fopen(path, "rb");
    if(stream == NULL)
        return false;
    fclose(stream);
    return true;
}

static bool directory_exists(const char *path)
{
    struct stat info;
    if(stat(path, &info) != 0)
        return false;
    return S_ISDIR(info.st_mode);
}

static bool get_executable_path(char *buffer, size_t buffer_size, const char *argv0)
{
#if defined(_WIN32)
    DWORD executable_length = GetModuleFileNameA(NULL, buffer, (DWORD) buffer_size);
    if(executable_length == 0 || executable_length >= buffer_size)
        return false;
    buffer[executable_length] = '\0';
    return true;
#elif defined(__APPLE__)
    uint32_t size = (uint32_t) buffer_size;
    if(_NSGetExecutablePath(buffer, &size) == 0)
        return true;
#else
    ssize_t length = readlink("/proc/self/exe", buffer, buffer_size - 1);
    if(length >= 0 && (size_t) length < buffer_size)
    {
        buffer[length] = '\0';
        return true;
    }
#endif

    if(argv0 == NULL)
        return false;

    size_t argv0_length = strlen(argv0);
    if(argv0_length + 1 > buffer_size)
        return false;
    memcpy(buffer, argv0, argv0_length + 1);
    return true;
}

static bool trim_to_directory(char *path)
{
    char *last_slash = strrchr(path, '\\');
    char *last_forward_slash = strrchr(path, '/');
    char *separator = last_slash;
    if(separator == NULL || (last_forward_slash != NULL && last_forward_slash > separator))
        separator = last_forward_slash;
    if(separator == NULL)
        return false;
    *separator = '\0';
    return true;
}

static bool join_path(char *buffer, size_t buffer_size, const char *left, const char *right)
{
#if defined(_WIN32)
    int written = snprintf(buffer, buffer_size, "%s\\%s", left, right);
#else
    int written = snprintf(buffer, buffer_size, "%s/%s", left, right);
#endif
    return written >= 0 && (size_t) written < buffer_size;
}

static bool resolve_runtime_source_path(char *buffer, size_t buffer_size, const char *argv0)
{
    char executable_path[CLI_PATH_BUFFER_SIZE] = {0};
    if(!get_executable_path(executable_path, sizeof(executable_path), argv0))
        return false;
    if(!trim_to_directory(executable_path))
        return false;
    return join_path(buffer, buffer_size, executable_path, MOTE_RUNTIME_RELATIVE_PATH);
}

static bool resolve_executable_directory(char *buffer, size_t buffer_size, const char *argv0)
{
    if(!get_executable_path(buffer, buffer_size, argv0))
        return false;
    return trim_to_directory(buffer);
}

static void add_search_root(ModulePackage *packages, int *package_count, const char *path)
{
    for(int i = 0; i < *package_count; i++)
    {
        if(packages[i].is_search_root && strcmp(packages[i].root_path, path) == 0)
            return;
    }

    if(*package_count >= CLI_MAX_PACKAGES)
        cliError("too many module search roots");

    memset(&(packages[*package_count]), 0, sizeof(ModulePackage));
    copy_cli_path(packages[*package_count].root_path, sizeof(packages[*package_count].root_path), path,
                  "package search root");
    packages[*package_count].is_search_root = true;
    (*package_count)++;
}

static void add_collection_root(ModulePackage *packages, int *package_count, const char *name, const char *path)
{
    for(int i = 0; i < *package_count; i++)
    {
        if(packages[i].is_collection && strcmp(packages[i].name, name) == 0)
        {
            copy_cli_path(packages[i].root_path, sizeof(packages[i].root_path), path,
                          "package collection root");
            return;
        }
    }

    if(*package_count >= CLI_MAX_PACKAGES)
        cliError("too many module search roots");

    memset(&(packages[*package_count]), 0, sizeof(ModulePackage));
    copy_cli_identifier(packages[*package_count].name, sizeof(packages[*package_count].name), name,
                        "package collection name");
    copy_cli_path(packages[*package_count].root_path, sizeof(packages[*package_count].root_path), path,
                  "package collection root");
    packages[*package_count].is_collection = true;
    (*package_count)++;
}

static void add_collection_root_if_exists(ModulePackage *packages, int *package_count, const char *name, const char *path)
{
    if(directory_exists(path))
        add_collection_root(packages, package_count, name, path);
}

static void add_search_root_if_exists(ModulePackage *packages, int *package_count, const char *path)
{
    if(directory_exists(path))
        add_search_root(packages, package_count, path);
}

static void add_driver_arg(const char **driver_args, int *driver_arg_count, const char *value)
{
    for(int i = 0; i < *driver_arg_count; i++)
    {
        if(strcmp(driver_args[i], value) == 0)
            return;
    }

    if(*driver_arg_count >= CLI_MAX_LINK_ARGS)
        cliError("too many linker arguments");

    driver_args[(*driver_arg_count)++] = value;
}

static void add_extra_c_source(const char **extra_c_sources, int *extra_c_source_count, const char *path)
{
    for(int i = 0; i < *extra_c_source_count; i++)
    {
        if(strcmp(extra_c_sources[i], path) == 0)
            return;
    }

    if(*extra_c_source_count >= CLI_MAX_EXTRA_C_SOURCES)
        cliError("too many extra C sources");

    extra_c_sources[(*extra_c_source_count)++] = path;
}

static void add_driver_path_arg(const char **driver_args, int *driver_arg_count, const char *prefix, const char *path)
{
    char *forwarded = (char*) malloc(MODULE_MAX_PATH_LENGTH);
    if(forwarded == NULL)
        cliError("failed to allocate linker argument");

    snprintf(forwarded, MODULE_MAX_PATH_LENGTH, "%s%s", prefix, path);
    add_driver_arg(driver_args, driver_arg_count, forwarded);
}

static void add_driver_path_arg_if_exists(const char **driver_args, int *driver_arg_count,
                                          const char *prefix, const char *path)
{
    if(directory_exists(path))
        add_driver_path_arg(driver_args, driver_arg_count, prefix, path);
}

static bool resolve_vendor_library_dir(char *buffer, size_t buffer_size, const char *argv0,
                                       const char *vendor_name, const char *platform_name)
{
    char executable_dir[CLI_PATH_BUFFER_SIZE] = {0};
    char vendor_root[CLI_PATH_BUFFER_SIZE] = {0};
    char vendor_lib_dir[CLI_PATH_BUFFER_SIZE] = {0};

    if(!resolve_executable_directory(executable_dir, sizeof(executable_dir), argv0))
        return false;
    if(!join_path(vendor_root, sizeof(vendor_root), executable_dir, "vendor"))
        return false;
    if(!join_path(vendor_root, sizeof(vendor_root), vendor_root, vendor_name))
        return false;
    if(!join_path(vendor_lib_dir, sizeof(vendor_lib_dir), vendor_root, "lib"))
        return false;
    if(!join_path(buffer, buffer_size, vendor_lib_dir, platform_name))
        return false;

    return directory_exists(buffer);
}

static bool resolve_vendor_path(char *buffer, size_t buffer_size, const char *argv0, const char *relative_path)
{
    char executable_dir[CLI_PATH_BUFFER_SIZE] = {0};
    if(!resolve_executable_directory(executable_dir, sizeof(executable_dir), argv0))
        return false;
    return join_path(buffer, buffer_size, executable_dir, relative_path);
}

static void add_extra_c_source_resolved(const char *argv0, const char **extra_c_sources, int *extra_c_source_count,
                                        const char *relative_path)
{
    char *resolved = (char*) malloc(MODULE_MAX_PATH_LENGTH);
    if(resolved == NULL)
        cliError("failed to allocate extra C source path");

    if(!resolve_vendor_path(resolved, MODULE_MAX_PATH_LENGTH, argv0, relative_path))
        cliError("failed to resolve vendor source path");

    add_extra_c_source(extra_c_sources, extra_c_source_count, resolved);
}

#if defined(_WIN32)
static void add_vendor_library_file_resolved(const char *argv0,
                                             const char **driver_args, int *driver_arg_count,
                                             const char *relative_path)
{
    char *resolved = (char*) malloc(MODULE_MAX_PATH_LENGTH);
    if(resolved == NULL)
        cliError("failed to allocate vendor library path");

    if(!resolve_vendor_path(resolved, MODULE_MAX_PATH_LENGTH, argv0, relative_path))
        cliError("failed to resolve vendor library path");

    add_driver_arg(driver_args, driver_arg_count, resolved);
}

static void add_local_file_resolved(const char *argv0,
                                    const char **driver_args, int *driver_arg_count,
                                    const char *relative_path)
{
    char *resolved = (char*) malloc(MODULE_MAX_PATH_LENGTH);
    if(resolved == NULL)
        cliError("failed to allocate local file path");

    if(!resolve_vendor_path(resolved, MODULE_MAX_PATH_LENGTH, argv0, relative_path))
        cliError("failed to resolve local file path");

    add_driver_arg(driver_args, driver_arg_count, resolved);
}
#endif

static void add_default_official_search_roots(ModulePackage *packages, int *package_count, const char *argv0)
{
    char executable_dir[CLI_PATH_BUFFER_SIZE] = {0};
    char lib_dir[CLI_PATH_BUFFER_SIZE] = {0};
    char std_dir[CLI_PATH_BUFFER_SIZE] = {0};
    char c_dir[CLI_PATH_BUFFER_SIZE] = {0};
    char vendor_dir[CLI_PATH_BUFFER_SIZE] = {0};
    char cwd[CLI_PATH_BUFFER_SIZE] = {0};

    if(!resolve_executable_directory(executable_dir, sizeof(executable_dir), argv0))
        executable_dir[0] = '\0';

    if(executable_dir[0] != '\0')
    {
        add_search_root_if_exists(packages, package_count, executable_dir);
        if(join_path(lib_dir, sizeof(lib_dir), executable_dir, "lib"))
        {
            add_search_root_if_exists(packages, package_count, lib_dir);
            if(join_path(std_dir, sizeof(std_dir), lib_dir, "std"))
                add_collection_root_if_exists(packages, package_count, "std", std_dir);
            if(join_path(c_dir, sizeof(c_dir), lib_dir, "c"))
                add_collection_root_if_exists(packages, package_count, "c", c_dir);
        }
        if(join_path(vendor_dir, sizeof(vendor_dir), executable_dir, "vendor"))
            add_collection_root_if_exists(packages, package_count, "vendor", vendor_dir);
    }

    if(getcwd(cwd, sizeof(cwd)) != NULL)
    {
        add_search_root_if_exists(packages, package_count, cwd);
        if(join_path(lib_dir, sizeof(lib_dir), cwd, "lib"))
        {
            add_search_root_if_exists(packages, package_count, lib_dir);
            if(join_path(std_dir, sizeof(std_dir), lib_dir, "std"))
                add_collection_root_if_exists(packages, package_count, "std", std_dir);
            if(join_path(c_dir, sizeof(c_dir), lib_dir, "c"))
                add_collection_root_if_exists(packages, package_count, "c", c_dir);
        }
        if(join_path(vendor_dir, sizeof(vendor_dir), cwd, "vendor"))
            add_collection_root_if_exists(packages, package_count, "vendor", vendor_dir);
    }
}

static bool module_tree_resolve_import_path(ModulePackage *packages, int package_count,
                                            const char *importer_path, const char *import_path,
                                            char *buffer, size_t buffer_size)
{
    (void) buffer_size;

    if(import_path == NULL || import_path[0] == '\0')
        return false;

    char collection_name[MAX_IDENTIFIER_LENGTH] = {0};
    char package_path[MODULE_MAX_PATH_LENGTH] = {0};
    if(moduleParseCollectionImportPath(import_path,
                                       collection_name, sizeof(collection_name),
                                       package_path, sizeof(package_path)))
    {
        if(package_path[0] == '\0' ||
           package_path[0] == '.' ||
           moduleIsAbsolutePath(package_path) ||
           strstr(package_path, "..") != NULL ||
           strchr(package_path, '\\') != NULL)
            return false;

        for(int i = 0; i < package_count; i++)
        {
            ModulePackage *package = &(packages[i]);
            if(!package->is_collection || strcmp(package->name, collection_name) != 0)
                continue;
            return moduleTryResolvePackageDirectoryPath(package->root_path, package_path, buffer);
        }
        return false;
    }

    if(moduleIsAbsolutePath(import_path) || strchr(import_path, '/') != NULL || strchr(import_path, '\\') != NULL || import_path[0] == '.')
        return false;

    if(moduleTryResolvePackageDirectoryPath(importer_path, import_path, buffer))
        return true;

    for(int i = 0; i < package_count; i++)
    {
        ModulePackage *package = &(packages[i]);
        if(!package->is_search_root)
            continue;
        if(moduleTryResolvePackageDirectoryPath(package->root_path, import_path, buffer))
            return true;
    }

    return false;
}

static bool module_tree_uses_import_impl(ModulePackage *packages, int package_count,
                                         const char *path, const char *needle,
                                         char visited_paths[][CLI_PATH_BUFFER_SIZE], int *visited_count)
{
    if(path == NULL)
        return false;

    for(int i = 0; i < *visited_count; i++)
    {
        if(strcmp(visited_paths[i], path) == 0)
            return false;
    }

    if(*visited_count >= CLI_MAX_PACKAGES)
        return false;
    snprintf(visited_paths[*visited_count], CLI_PATH_BUFFER_SIZE, "%s", path);
    (*visited_count)++;

    char **file_paths = NULL;
    int file_count = moduleListPackageFiles(path, &file_paths);
    bool found = false;
    const char *import_prefix = "@import(\"";
    size_t import_prefix_length = strlen(import_prefix);

    for(int file_index = 0; file_index < file_count && !found; file_index++)
    {
        char *source = moduleReadFile(file_paths[file_index]);
        if(source == NULL)
            continue;

        found = strstr(source, needle) != NULL;
        if(!found)
        {
            const char *cursor = source;
            while(!found)
            {
                const char *import_start = strstr(cursor, import_prefix);
                if(import_start == NULL)
                    break;

                const char *name_start = import_start + import_prefix_length;
                const char *name_end = strchr(name_start, '"');
                if(name_end == NULL)
                    break;

                char import_name[CLI_PATH_BUFFER_SIZE] = {0};
                size_t import_length = (size_t)(name_end - name_start);
                if(import_length > 0 && import_length < sizeof(import_name))
                {
                    memcpy(import_name, name_start, import_length);
                    import_name[import_length] = '\0';

                    char resolved_path[CLI_PATH_BUFFER_SIZE] = {0};
                    if(module_tree_resolve_import_path(packages, package_count, path, import_name,
                                                      resolved_path, sizeof(resolved_path)))
                    {
                        found = module_tree_uses_import_impl(packages, package_count, resolved_path, needle,
                                                             visited_paths, visited_count);
                    }
                }

                cursor = name_end + 1;
            }
        }

        free(source);
        free(file_paths[file_index]);
    }
    free(file_paths);

    return found;
}

static bool module_tree_uses_import(ModulePackage *packages, int package_count,
                                    const char *path, const char *needle)
{
    char visited_paths[CLI_MAX_PACKAGES][CLI_PATH_BUFFER_SIZE];
    memset(visited_paths, 0, sizeof(visited_paths));
    int visited_count = 0;
    return module_tree_uses_import_impl(packages, package_count, path, needle, visited_paths, &visited_count);
}

static void add_default_vendor_link_search_paths(const char *argv0, bool uses_glfw, bool uses_raylib,
                                                 const char **driver_args, int *driver_arg_count)
{
    char vendor_lib_dir[CLI_PATH_BUFFER_SIZE] = {0};

#if defined(_WIN32)
    if(uses_glfw && resolve_vendor_library_dir(vendor_lib_dir, sizeof(vendor_lib_dir), argv0, "glfw", "windows"))
        add_driver_path_arg(driver_args, driver_arg_count, "-L", vendor_lib_dir);
    if(uses_raylib && resolve_vendor_library_dir(vendor_lib_dir, sizeof(vendor_lib_dir), argv0, "raylib", "windows"))
        add_driver_path_arg(driver_args, driver_arg_count, "-L", vendor_lib_dir);
#elif defined(__APPLE__)
    if(uses_glfw && resolve_vendor_library_dir(vendor_lib_dir, sizeof(vendor_lib_dir), argv0, "glfw", "macos"))
        add_driver_path_arg(driver_args, driver_arg_count, "-L", vendor_lib_dir);
    if(uses_raylib && resolve_vendor_library_dir(vendor_lib_dir, sizeof(vendor_lib_dir), argv0, "raylib", "macos"))
        add_driver_path_arg(driver_args, driver_arg_count, "-L", vendor_lib_dir);
#else
    if(uses_raylib && resolve_vendor_library_dir(vendor_lib_dir, sizeof(vendor_lib_dir), argv0, "raylib", "linux"))
        add_driver_path_arg(driver_args, driver_arg_count, "-L", vendor_lib_dir);
#endif
}

static void add_default_official_link_args(const char *argv0, ModulePackage *packages, int package_count,
                                           const char *input_path,
                                           const char **driver_args, int *driver_arg_count,
                                           const char **extra_c_sources, int *extra_c_source_count)
{
    bool uses_glfw = module_tree_uses_import(packages, package_count, input_path, "@import(\"vendor:glfw\")");
    bool uses_raylib = module_tree_uses_import(packages, package_count, input_path, "@import(\"vendor:raylib\")");
    bool uses_miniaudio = module_tree_uses_import(packages, package_count, input_path, "@import(\"vendor:miniaudio\")");
    bool uses_stb_image = module_tree_uses_import(packages, package_count, input_path, "@import(\"vendor:stb/image\")");
    bool uses_stb_truetype = module_tree_uses_import(packages, package_count, input_path, "@import(\"vendor:stb/truetype\")");
    bool uses_stb_easy_font = module_tree_uses_import(packages, package_count, input_path, "@import(\"vendor:stb/easy_font\")");
    bool uses_cgltf = module_tree_uses_import(packages, package_count, input_path, "@import(\"vendor:cgltf\")");
    bool uses_enet = module_tree_uses_import(packages, package_count, input_path, "@import(\"vendor:enet\")");
    bool uses_std_math = module_tree_uses_import(packages, package_count, input_path, "@import(\"std:math\")");
    bool uses_std_linalg = module_tree_uses_import(packages, package_count, input_path, "@import(\"std:linalg\")");
    bool uses_std_thread = module_tree_uses_import(packages, package_count, input_path, "@import(\"std:thread\")");
    bool uses_std_metamote = module_tree_uses_import(packages, package_count, input_path, "@import(\"std:metamote\")");
    bool uses_c_math = module_tree_uses_import(packages, package_count, input_path, "@import(\"c:math\")");
    char vendor_include_dir[CLI_PATH_BUFFER_SIZE] = {0};

    add_default_vendor_link_search_paths(argv0, uses_glfw, uses_raylib, driver_args, driver_arg_count);

    if(uses_enet && resolve_vendor_path(vendor_include_dir, sizeof(vendor_include_dir), argv0, "vendor/enet/src"))
        add_driver_path_arg_if_exists(driver_args, driver_arg_count, "-I", vendor_include_dir);

#if defined(__APPLE__)
    if(directory_exists("/opt/homebrew/lib"))
    {
        add_driver_path_arg(driver_args, driver_arg_count, "-L", "/opt/homebrew/lib");
        add_driver_arg(driver_args, driver_arg_count, "-Wl,-rpath,/opt/homebrew/lib");
    }
    if(directory_exists("/usr/local/lib"))
    {
        add_driver_path_arg(driver_args, driver_arg_count, "-L", "/usr/local/lib");
        add_driver_arg(driver_args, driver_arg_count, "-Wl,-rpath,/usr/local/lib");
    }
#endif

    if(uses_glfw)
    {
#if defined(_WIN32)
        add_vendor_library_file_resolved(argv0, driver_args, driver_arg_count, "vendor/glfw/lib/windows/glfw3_mt.lib");
#else
        add_driver_arg(driver_args, driver_arg_count, "-lglfw3");
#endif
#if defined(__APPLE__)
        add_driver_arg(driver_args, driver_arg_count, "-Wl,-framework,OpenGL");
        add_driver_arg(driver_args, driver_arg_count, "-Wl,-framework,Cocoa");
        add_driver_arg(driver_args, driver_arg_count, "-Wl,-framework,IOKit");
#elif defined(_WIN32)
        add_driver_arg(driver_args, driver_arg_count, "-lgdi32");
        add_driver_arg(driver_args, driver_arg_count, "-luser32");
        add_driver_arg(driver_args, driver_arg_count, "-lshell32");
#endif
    }

    if(uses_raylib)
    {
        add_driver_arg(driver_args, driver_arg_count, "-lraylib");
#if defined(__APPLE__)
        add_driver_arg(driver_args, driver_arg_count, "-Wl,-framework,OpenGL");
        add_driver_arg(driver_args, driver_arg_count, "-Wl,-framework,Cocoa");
        add_driver_arg(driver_args, driver_arg_count, "-Wl,-framework,IOKit");
        add_driver_arg(driver_args, driver_arg_count, "-Wl,-framework,CoreVideo");
        add_driver_arg(driver_args, driver_arg_count, "-Wl,-framework,CoreAudio");
        add_driver_arg(driver_args, driver_arg_count, "-Wl,-framework,AudioToolbox");
        add_driver_arg(driver_args, driver_arg_count, "-Wl,-framework,AudioUnit");
#elif defined(_WIN32)
        add_driver_arg(driver_args, driver_arg_count, "-lgdi32");
        add_driver_arg(driver_args, driver_arg_count, "-lwinmm");
        add_driver_arg(driver_args, driver_arg_count, "-luser32");
        add_driver_arg(driver_args, driver_arg_count, "-lshell32");
#else
        add_driver_arg(driver_args, driver_arg_count, "-ldl");
        add_driver_arg(driver_args, driver_arg_count, "-lpthread");
#endif
    }

    if(uses_enet)
    {
#if defined(_WIN32)
        add_driver_arg(driver_args, driver_arg_count, "-lws2_32");
        add_driver_arg(driver_args, driver_arg_count, "-lwinmm");
#endif
    }

    if(uses_std_math || uses_std_linalg || uses_c_math)
        add_driver_arg(driver_args, driver_arg_count, "-lm");
    if(uses_std_thread)
    {
#if !defined(_WIN32)
        add_driver_arg(driver_args, driver_arg_count, "-lpthread");
#endif
    }
    if(uses_std_metamote)
    {
#if defined(_WIN32)
        add_local_file_resolved(argv0, driver_args, driver_arg_count, "mote_core.lib");
#elif defined(__APPLE__)
        add_local_file_resolved(argv0, driver_args, driver_arg_count, "libmote_core.dylib");
#else
        add_local_file_resolved(argv0, driver_args, driver_arg_count, "libmote_core.so");
#endif
    }

    if(uses_miniaudio)
        add_extra_c_source_resolved(argv0, extra_c_sources, extra_c_source_count, "vendor/miniaudio/src/miniaudio.c");
    if(uses_miniaudio)
        add_extra_c_source_resolved(argv0, extra_c_sources, extra_c_source_count, "vendor/miniaudio/src/mote_miniaudio_shim.c");
    if(uses_stb_image)
        add_extra_c_source_resolved(argv0, extra_c_sources, extra_c_source_count, "vendor/stb/src/stb_image_impl.c");
    if(uses_stb_truetype)
        add_extra_c_source_resolved(argv0, extra_c_sources, extra_c_source_count, "vendor/stb/src/stb_truetype_impl.c");
    if(uses_stb_easy_font)
        add_extra_c_source_resolved(argv0, extra_c_sources, extra_c_source_count, "vendor/stb/src/stb_easy_font_shim.c");
    if(uses_cgltf)
        add_extra_c_source_resolved(argv0, extra_c_sources, extra_c_source_count, "vendor/cgltf/src/cgltf.c");
    if(uses_cgltf)
        add_extra_c_source_resolved(argv0, extra_c_sources, extra_c_source_count, "vendor/cgltf/src/mote_cgltf_shim.c");
    if(uses_enet)
    {
        add_extra_c_source_resolved(argv0, extra_c_sources, extra_c_source_count, "vendor/enet/src/callbacks.c");
        add_extra_c_source_resolved(argv0, extra_c_sources, extra_c_source_count, "vendor/enet/src/compress.c");
        add_extra_c_source_resolved(argv0, extra_c_sources, extra_c_source_count, "vendor/enet/src/host.c");
        add_extra_c_source_resolved(argv0, extra_c_sources, extra_c_source_count, "vendor/enet/src/list.c");
        add_extra_c_source_resolved(argv0, extra_c_sources, extra_c_source_count, "vendor/enet/src/packet.c");
        add_extra_c_source_resolved(argv0, extra_c_sources, extra_c_source_count, "vendor/enet/src/peer.c");
        add_extra_c_source_resolved(argv0, extra_c_sources, extra_c_source_count, "vendor/enet/src/protocol.c");
#if defined(_WIN32)
        add_extra_c_source_resolved(argv0, extra_c_sources, extra_c_source_count, "vendor/enet/src/win32.c");
#else
        add_extra_c_source_resolved(argv0, extra_c_sources, extra_c_source_count, "vendor/enet/src/unix.c");
#endif
    }
}

static void parse_cli_options(int argn, char **argv,
                              CliOptions *options,
                              ModulePackage *packages, int *package_count,
                              const char **driver_args, int *driver_arg_count,
                              const char **extra_c_sources, int *extra_c_source_count,
                              const char **linker_args, int *linker_arg_count)
{
    const char **positionals = (const char**) calloc(2, sizeof(const char*));
    int positional_count = 0;
    if(positionals == NULL)
        cliError("failed to allocate CLI positional buffer");

    memset(options, 0, sizeof(*options));
    options->emit_exe = true;

    add_default_official_search_roots(packages, package_count, argv[0]);

    for(int i = 1; i < argn; i++)
    {
        if(strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0)
        {
            print_usage(argv[0]);
            exit(0);
        }

        if(strcmp(argv[i], "-S") == 0)
        {
            options->emit_llvm = true;
            options->emit_exe = false;
            options->keep_llvm_output = true;
            continue;
        }

        if(strcmp(argv[i], "--dump-ast") == 0)
        {
            options->dump_ast = true;
            continue;
        }

        if(strcmp(argv[i], "-g") == 0)
        {
            options->emit_debug_info = true;
            continue;
        }

        if(strcmp(argv[i], "--dump-mir") == 0)
        {
            options->dump_mir = true;
            continue;
        }

        if(strcmp(argv[i], "-o") == 0)
        {
            if(i + 1 >= argn)
            {
                print_usage(argv[0]);
                exit(1);
            }
            options->requested_output_path = argv[++i];
            continue;
        }

        if(strcmp(argv[i], "-I") == 0)
        {
            if(i + 1 >= argn)
            {
                print_usage(argv[0]);
                exit(1);
            }
            add_search_root(packages, package_count, argv[++i]);
            continue;
        }

        if(strcmp(argv[i], "-C") == 0)
        {
            if(i + 1 >= argn)
            {
                print_usage(argv[0]);
                exit(1);
            }

            const char *mapping = argv[++i];
            const char *equals = strchr(mapping, '=');
            if(equals == NULL || equals == mapping || equals[1] == '\0')
                cliErrorFormatted("invalid collection mapping `%s`; expected name=dir", mapping);

            char collection_name[MAX_IDENTIFIER_LENGTH] = {0};
            size_t name_length = (size_t)(equals - mapping);
            if(name_length >= sizeof(collection_name))
                cliErrorFormatted("collection name is too long in mapping `%s`", mapping);
            memcpy(collection_name, mapping, name_length);
            collection_name[name_length] = '\0';
            add_collection_root(packages, package_count, collection_name, equals + 1);
            continue;
        }

        if(strcmp(argv[i], "-L") == 0)
        {
            if(i + 1 >= argn)
            {
                print_usage(argv[0]);
                exit(1);
            }
            if(*driver_arg_count >= CLI_MAX_LINK_ARGS)
                cliError("too many linker arguments");
            char *forwarded = (char*) malloc(MODULE_MAX_PATH_LENGTH);
            if(forwarded == NULL)
                cliError("failed to allocate linker argument");
            snprintf(forwarded, MODULE_MAX_PATH_LENGTH, "-L%s", argv[++i]);
            driver_args[(*driver_arg_count)++] = forwarded;
            continue;
        }

        if(starts_with(argv[i], "-l"))
        {
            if(strlen(argv[i]) <= 2)
            {
                print_usage(argv[0]);
                exit(1);
            }
            if(*driver_arg_count >= CLI_MAX_LINK_ARGS)
                cliError("too many linker arguments");
            driver_args[(*driver_arg_count)++] = argv[i];
            continue;
        }

        if(starts_with(argv[i], "-Wl,"))
        {
            const char *cursor = argv[i] + 4;
            while(*cursor != '\0')
            {
                const char *comma = strchr(cursor, ',');
                size_t length = comma == NULL ? strlen(cursor) : (size_t)(comma - cursor);
                if(length == 0)
                    moduleSystemError("empty -Wl argument is not allowed", NULL, 0, 0);
                if(*linker_arg_count >= CLI_MAX_LINKER_ARGS)
                    cliError("too many linker arguments");
                char *forwarded = (char*) malloc(length + 1);
                if(forwarded == NULL)
                    cliError("failed to allocate linker argument");
                memcpy(forwarded, cursor, length);
                forwarded[length] = '\0';
                linker_args[(*linker_arg_count)++] = forwarded;
                if(comma == NULL)
                    break;
                cursor = comma + 1;
            }
            continue;
        }

        if(strcmp(argv[i], "--pkg") == 0 || strcmp(argv[i], "--emit-llvm") == 0 ||
           strcmp(argv[i], "--emit-exe") == 0 || strcmp(argv[i], "--link-arg") == 0)
        {
            Diagnostic diagnostic = diagnosticMake(DIAGNOSTIC_SEVERITY_ERROR, "C1001",
                                                   makeSourceSpan(NULL, 0, 0, 0, 0),
                                                   "legacy CLI option is no longer supported");
            diagnosticAddNote(&diagnostic, "option: %s", argv[i]);
            diagnosticAddNote(&diagnostic, "use -I, -S, -L, -l, -Wl and default executable emission instead");
            diagnosticAbort(diagnostic);
            continue;
        }

        if(positional_count >= 2)
        {
            print_usage(argv[0]);
            exit(1);
        }
        positionals[positional_count++] = argv[i];
    }

    if(positional_count >= 1)
        options->input_path = positionals[0];

    if(options->input_path == NULL)
    {
        print_usage(argv[0]);
        exit(1);
    }
    if(!directory_exists(options->input_path))
        cliErrorFormatted("input path must be a package directory: %s", options->input_path);

    add_default_official_link_args(argv[0], packages, *package_count, options->input_path,
                                   driver_args, driver_arg_count,
                                   extra_c_sources, extra_c_source_count);

    free(positionals);
}

static void derive_output_paths(const CliOptions *options, ASTNode *root,
                                char *default_llvm_output_path, size_t default_llvm_output_path_size,
                                char *default_exe_output_path, size_t default_exe_output_path_size,
                                const char **llvm_output_path, const char **exe_output_path)
{
    const char *default_output_basename = root->package_name[0] != '\0' ? root->package_name : options->input_path;
    *llvm_output_path = options->llvm_output_path;
    *exe_output_path = options->exe_output_path;

    if(options->requested_output_path != NULL)
    {
        if(options->emit_exe)
            *exe_output_path = options->requested_output_path;
        else if(options->emit_llvm)
            *llvm_output_path = options->requested_output_path;
    }

    if(options->emit_exe)
    {
        if(*exe_output_path == NULL)
        {
            build_output_path(default_exe_output_path, default_exe_output_path_size, default_output_basename,
#if defined(_WIN32)
                              ".exe"
#else
                              ".out"
#endif
            );
            *exe_output_path = default_exe_output_path;
        }

        if(options->emit_llvm)
        {
            if(*llvm_output_path == NULL)
                build_output_path(default_llvm_output_path, default_llvm_output_path_size, *exe_output_path, ".ll");
            *llvm_output_path = *llvm_output_path == NULL ? default_llvm_output_path : *llvm_output_path;
        }
    }
    else if(options->emit_llvm && *llvm_output_path == NULL)
    {
        build_output_path(default_llvm_output_path, default_llvm_output_path_size, default_output_basename, ".ll");
        *llvm_output_path = default_llvm_output_path;
    }
}

#endif /* CLI_DRIVER_H */
