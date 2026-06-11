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
#include "Lexer.h"
#include "Token.h"
#include "AST.h"
#include "Parser.h"
#include "Semantic.h"
#include "MIR.h"
#include "ModuleSystem.h"
#include "LLVMBackend.h"

#define CLI_MAX_PACKAGES 128
#define CLI_MAX_LINK_ARGS 256
#define CLI_MAX_LINKER_ARGS 256
#define CLI_PATH_BUFFER_SIZE 4096

#if defined(_WIN32)
#define MOTE_RUNTIME_RELATIVE_PATH "runtime\\mote_runtime.c"
#else
#define MOTE_RUNTIME_RELATIVE_PATH "runtime/mote_runtime.c"
#endif

static bool starts_with(const char *value, const char *prefix)
{
    return strncmp(value, prefix, strlen(prefix)) == 0;
}

static void print_usage(const char *argv0)
{
    printf("Usage:\n");
    printf("  %s [options] <input.mote>\n", argv0);
    printf("\n");
    printf("Options:\n");
    printf("  -o <file>         Write output to <file>\n");
    printf("  -S                Write LLVM IR to a .ll file and stop\n");
    printf("  -I <dir>          Add a module search root for @import(\"name/...\" )\n");
    printf("  -L <dir>          Add a linker search directory\n");
    printf("  -l<name>          Link against <name>\n");
    printf("  -Wl,<args>        Forward comma-separated arguments to the linker\n");
    printf("  --dump-ast        Print AST after parsing and rewriting\n");
    printf("  --dump-mir        Print MIR after lowering\n");
    printf("  --help, -h        Show this help text\n");
    printf("\n");
    printf("Notes:\n");
    printf("  - Default behavior emits an executable.\n");
    printf("  - -S writes LLVM IR instead of linking.\n");
    printf("  - mote requires clang to be available in PATH for executable emission.\n");
    printf("  - Official module roots are searched automatically relative to the compiler executable.\n");
    printf("  - If -o is omitted, the compiler derives output from the input file name.\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s test\\\\basic\\\\simple.mote\n", argv0);
    printf("  %s -S test\\\\basic\\\\simple.mote -o test\\\\basic\\\\simple.ll\n", argv0);
    printf("  %s test.mote -o test.exe\n", argv0);
    printf("  %s app.mote -Lthird_party\\\\lib -lfoo -o app.exe\n", argv0);
    printf("  %s main.mote -o cube_demo\n", argv0);
}

static void build_output_path(char *buffer, size_t buffer_size, const char *input_path, const char *extension)
{
    const char *last_slash = strrchr(input_path, '\\');
    const char *last_forward_slash = strrchr(input_path, '/');
    const char *path_separator = last_slash;
    if(path_separator == NULL || (last_forward_slash != NULL && last_forward_slash > path_separator))
        path_separator = last_forward_slash;

    const char *last_dot = strrchr(input_path, '.');
    if(last_dot != NULL && path_separator != NULL && last_dot < path_separator)
        last_dot = NULL;

    if(last_dot == NULL)
        snprintf(buffer, buffer_size, "%s%s", input_path, extension);
    else
        snprintf(buffer, buffer_size, "%.*s%s", (int)(last_dot - input_path), input_path, extension);
}

static void build_temp_llvm_output_path(char *buffer, size_t buffer_size, const char *exe_output_path)
{
    snprintf(buffer, buffer_size, "%s.mote-tmp.ll", exe_output_path);
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
    {
        printf("Too many module search roots\n");
        exit(1);
    }

    memset(&(packages[*package_count]), 0, sizeof(ModulePackage));
    strcpy(packages[*package_count].root_path, path);
    packages[*package_count].is_search_root = true;
    (*package_count)++;
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
    {
        printf("Too many linker arguments\n");
        exit(1);
    }

    driver_args[(*driver_arg_count)++] = value;
}

static void add_driver_path_arg(const char **driver_args, int *driver_arg_count, const char *prefix, const char *path)
{
    char *forwarded = (char*) malloc(MODULE_MAX_PATH_LENGTH);
    if(forwarded == NULL)
    {
        printf("Failed to allocate linker argument\n");
        exit(1);
    }

    snprintf(forwarded, MODULE_MAX_PATH_LENGTH, "%s%s", prefix, path);
    add_driver_arg(driver_args, driver_arg_count, forwarded);
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

static void add_default_official_search_roots(ModulePackage *packages, int *package_count, const char *argv0)
{
    char executable_dir[CLI_PATH_BUFFER_SIZE] = {0};
    char lib_dir[CLI_PATH_BUFFER_SIZE] = {0};

    if(!resolve_executable_directory(executable_dir, sizeof(executable_dir), argv0))
        return;

    add_search_root_if_exists(packages, package_count, executable_dir);
    if(join_path(lib_dir, sizeof(lib_dir), executable_dir, "lib"))
        add_search_root_if_exists(packages, package_count, lib_dir);
}

static bool module_tree_resolve_import_path(ModulePackage *packages, int package_count,
                                            const char *importer_path, const char *import_path,
                                            char *buffer, size_t buffer_size)
{
    (void) buffer_size;

    if(import_path == NULL || import_path[0] == '\0')
        return false;

    if(moduleIsAbsolutePath(import_path) || moduleIsRelativeImportPath(import_path))
    {
        char importer_dir[MODULE_MAX_PATH_LENGTH] = {0};
        moduleDirectoryName(importer_path, importer_dir);
        return moduleTryResolveModuleFilePath(importer_dir, import_path, buffer);
    }

    for(int i = 0; i < package_count; i++)
    {
        ModulePackage *package = &(packages[i]);
        if(!package->is_search_root)
            continue;
        if(moduleTryResolveModuleFilePath(package->root_path, import_path, buffer))
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

    char *source = moduleReadFile(path);
    if(source == NULL)
        return false;

    bool found = strstr(source, needle) != NULL;
    if(!found)
    {
        const char *cursor = source;
        const char *import_prefix = "@import(\"";
        size_t import_prefix_length = strlen(import_prefix);

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
                                           const char **driver_args, int *driver_arg_count)
{
    bool uses_glfw = module_tree_uses_import(packages, package_count, input_path, "@import(\"vendor/glfw\")");
    bool uses_raylib = module_tree_uses_import(packages, package_count, input_path, "@import(\"vendor/raylib\")");
    bool uses_std_math = module_tree_uses_import(packages, package_count, input_path, "@import(\"std/math\")");
    bool uses_std_linalg = module_tree_uses_import(packages, package_count, input_path, "@import(\"std/linalg\")");
    bool uses_c_math = module_tree_uses_import(packages, package_count, input_path, "@import(\"c/math\")");

    add_default_vendor_link_search_paths(argv0, uses_glfw, uses_raylib, driver_args, driver_arg_count);

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
        add_driver_arg(driver_args, driver_arg_count, "-lglfw3");
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

    if(uses_std_math || uses_std_linalg || uses_c_math)
        add_driver_arg(driver_args, driver_arg_count, "-lm");
}

static void append_shell_escaped(char *command, size_t command_size, const char *arg)
{
    size_t used = strlen(command);
    if(used + 4 >= command_size)
    {
        printf("Link command is too long\n");
        exit(1);
    }

    command[used++] = ' ';
    command[used++] = '"';
    command[used] = '\0';

    for(const char *p = arg; *p != '\0'; p++)
    {
        if(*p == '"')
        {
            if(used + 2 >= command_size)
            {
                printf("Link command is too long\n");
                exit(1);
            }
            command[used++] = '\\';
        }

        if(used + 1 >= command_size)
        {
            printf("Link command is too long\n");
            exit(1);
        }
        command[used++] = *p;
    }

    if(used + 2 >= command_size)
    {
        printf("Link command is too long\n");
        exit(1);
    }
    command[used++] = '"';
    command[used] = '\0';
}

static void append_shell_quoted_fragment(char *command, size_t command_size, const char *value)
{
    size_t used = strlen(command);
    if(used + 3 >= command_size)
    {
        printf("Link command is too long\n");
        exit(1);
    }

    command[used++] = '"';
    command[used] = '\0';

    for(const char *p = value; *p != '\0'; p++)
    {
        if(*p == '"')
        {
            if(used + 2 >= command_size)
            {
                printf("Link command is too long\n");
                exit(1);
            }
            command[used++] = '\\';
        }

        if(used + 1 >= command_size)
        {
            printf("Link command is too long\n");
            exit(1);
        }
        command[used++] = *p;
    }

    if(used + 2 >= command_size)
    {
        printf("Link command is too long\n");
        exit(1);
    }
    command[used++] = '"';
    command[used] = '\0';
}

static int run_clang_link(const char *llvm_input_path, const char *runtime_source_path,
                          const char *exe_output_path,
                          const char **driver_args, int driver_arg_count,
                          const char **linker_args, int linker_arg_count,
                          const char *log_path)
{
    char command[4096];
    strcpy(command, "clang");
    append_shell_escaped(command, sizeof(command), llvm_input_path);
    append_shell_escaped(command, sizeof(command), runtime_source_path);
    append_shell_escaped(command, sizeof(command), "-o");
    append_shell_escaped(command, sizeof(command), exe_output_path);
#if defined(_WIN32)
    append_shell_escaped(command, sizeof(command), "-Xlinker");
    append_shell_escaped(command, sizeof(command), "/subsystem:console");
#endif

    for(int i = 0; i < driver_arg_count; i++)
        append_shell_escaped(command, sizeof(command), driver_args[i]);

    for(int i = 0; i < linker_arg_count; i++)
    {
        append_shell_escaped(command, sizeof(command), "-Xlinker");
        append_shell_escaped(command, sizeof(command), linker_args[i]);
    }

    if(strlen(command) + strlen(" >") + strlen(log_path) + strlen(" 2>&1") + 1 >= sizeof(command))
    {
        printf("Link command is too long\n");
        exit(1);
    }
    strcat(command, " >");
    append_shell_quoted_fragment(command, sizeof(command), log_path);
    strcat(command, " 2>&1");

    return system(command);
}

static void print_file_if_exists(const char *path)
{
    FILE *stream = fopen(path, "rb");
    if(stream == NULL)
        return;

    char buffer[1024];
    size_t count = 0;
    while((count = fread(buffer, 1, sizeof(buffer), stream)) > 0)
        fwrite(buffer, 1, count, stderr);
    fclose(stream);
}

int main(int argn, char** argv)
{
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    bool emit_llvm = false;
    bool emit_exe = true;
    bool dump_ast = false;
    bool dump_mir = false;
    const char *input_path = NULL;
    const char *requested_output_path = NULL;
    const char *llvm_output_path = NULL;
    const char *exe_output_path = NULL;
    bool keep_llvm_output = false;
    ModulePackage *packages = (ModulePackage*) calloc(CLI_MAX_PACKAGES, sizeof(ModulePackage));
    int package_count = 0;
    const char **driver_args = (const char**) calloc(CLI_MAX_LINK_ARGS, sizeof(const char*));
    int driver_arg_count = 0;
    const char **linker_args = (const char**) calloc(CLI_MAX_LINKER_ARGS, sizeof(const char*));
    int linker_arg_count = 0;
    const char **positionals = (const char**) calloc(2, sizeof(const char*));
    int positional_count = 0;

    if(packages == NULL || driver_args == NULL || linker_args == NULL || positionals == NULL)
    {
        printf("Failed to allocate CLI argument buffers\n");
        exit(1);
    }

    add_default_official_search_roots(packages, &package_count, argv[0]);

    for(int i = 1; i < argn; i++)
    {
        if(strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0)
        {
            print_usage(argv[0]);
            return 0;
        }

        if(strcmp(argv[i], "-S") == 0)
        {
            emit_llvm = true;
            emit_exe = false;
            keep_llvm_output = true;
            continue;
        }

        if(strcmp(argv[i], "--dump-ast") == 0)
        {
            dump_ast = true;
            continue;
        }

        if(strcmp(argv[i], "--dump-mir") == 0)
        {
            dump_mir = true;
            continue;
        }

        if(strcmp(argv[i], "-o") == 0)
        {
            if(i + 1 >= argn)
            {
                print_usage(argv[0]);
                exit(1);
            }
            requested_output_path = argv[++i];
            continue;
        }

        if(strcmp(argv[i], "-I") == 0)
        {
            if(i + 1 >= argn)
            {
                print_usage(argv[0]);
                exit(1);
            }
            add_search_root(packages, &package_count, argv[++i]);
            continue;
        }

        if(strcmp(argv[i], "-L") == 0)
        {
            if(i + 1 >= argn)
            {
                print_usage(argv[0]);
                exit(1);
            }
            if(driver_arg_count >= CLI_MAX_LINK_ARGS)
            {
                printf("Too many linker arguments\n");
                exit(1);
            }
            char *forwarded = (char*) malloc(MODULE_MAX_PATH_LENGTH);
            if(forwarded == NULL)
            {
                printf("Failed to allocate linker argument\n");
                exit(1);
            }
            snprintf(forwarded, MODULE_MAX_PATH_LENGTH, "-L%s", argv[++i]);
            driver_args[driver_arg_count++] = forwarded;
            continue;
        }

        if(starts_with(argv[i], "-l"))
        {
            if(strlen(argv[i]) <= 2)
            {
                print_usage(argv[0]);
                exit(1);
            }
            if(driver_arg_count >= CLI_MAX_LINK_ARGS)
            {
                printf("Too many linker arguments\n");
                exit(1);
            }
            driver_args[driver_arg_count++] = argv[i];
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
                if(linker_arg_count >= CLI_MAX_LINKER_ARGS)
                {
                    printf("Too many linker arguments\n");
                    exit(1);
                }
                char *forwarded = (char*) malloc(length + 1);
                if(forwarded == NULL)
                {
                    printf("Failed to allocate linker argument\n");
                    exit(1);
                }
                memcpy(forwarded, cursor, length);
                forwarded[length] = '\0';
                linker_args[linker_arg_count++] = forwarded;
                if(comma == NULL)
                    break;
                cursor = comma + 1;
            }
            continue;
        }

        if(strcmp(argv[i], "--pkg") == 0 || strcmp(argv[i], "--emit-llvm") == 0 ||
           strcmp(argv[i], "--emit-exe") == 0 || strcmp(argv[i], "--link-arg") == 0)
        {
            printf("Legacy option %s is no longer supported in this CLI mode\n", argv[i]);
            printf("Use -I, -S, -L, -l, -Wl and default executable emission instead\n");
            exit(1);
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
        input_path = positionals[0];

    if(input_path == NULL)
    {
        print_usage(argv[0]);
        exit(1);
    }

    add_default_official_link_args(argv[0], packages, package_count, input_path, driver_args, &driver_arg_count);

    char default_llvm_output_path[1024] = {0};
    char default_exe_output_path[1024] = {0};
    if(requested_output_path != NULL)
    {
        if(emit_exe)
            exe_output_path = requested_output_path;
        else if(emit_llvm)
            llvm_output_path = requested_output_path;
    }

    if(emit_exe)
    {
        if(exe_output_path == NULL)
        {
            build_output_path(default_exe_output_path, sizeof(default_exe_output_path), input_path,
#if defined(_WIN32)
                              ".exe"
#else
                              ".out"
#endif
            );
            exe_output_path = default_exe_output_path;
        }

        if(emit_llvm)
        {
            if(llvm_output_path == NULL)
                build_output_path(default_llvm_output_path, sizeof(default_llvm_output_path), exe_output_path, ".ll");
            llvm_output_path = llvm_output_path == NULL ? default_llvm_output_path : llvm_output_path;
        }
    }
    else if(emit_llvm && llvm_output_path == NULL)
        build_output_path(default_llvm_output_path, sizeof(default_llvm_output_path), input_path, ".ll"), llvm_output_path = default_llvm_output_path;

    ASTNode *root = buildModuleProgramAST(input_path, packages, package_count);
    checkAssignSemantics(root);
    checkAssignTypes(root);
    if(dump_ast)
    {
        printf("PRINT AST NODES ===============\n\n");
        ASTNode *ndptr = root;
        while(ndptr)
        {
            printASTNode(*ndptr);
            ndptr = ndptr->next;
        }
        printf("END PRINT AST NODES ===============\n\n");
    }

    MirProgram *mir_program = lowerASTToMIR(root);
    if(dump_mir)
        printMIRProgram(mir_program);

    if(emit_llvm)
    {
        emitLLVMProgramToFile(mir_program, input_path, llvm_output_path);
        if(keep_llvm_output)
            printf("%s\n", llvm_output_path);
    }

    if(emit_exe)
    {
        char runtime_source_path[CLI_PATH_BUFFER_SIZE] = {0};
        char clang_log_path[CLI_PATH_BUFFER_SIZE] = {0};
        if(!resolve_runtime_source_path(runtime_source_path, sizeof(runtime_source_path), argv[0]))
        {
            printf("Failed to resolve runtime path relative to the compiler executable\n");
            exit(1);
        }
        if(!file_exists(runtime_source_path))
        {
            printf("Failed to locate runtime source at %s\n", runtime_source_path);
            exit(1);
        }

        if(!emit_llvm)
        {
            build_temp_llvm_output_path(default_llvm_output_path, sizeof(default_llvm_output_path), exe_output_path);
            llvm_output_path = default_llvm_output_path;
            emitLLVMProgramToFile(mir_program, input_path, llvm_output_path);
        }

        snprintf(clang_log_path, sizeof(clang_log_path), "%s.mote-link.log", exe_output_path);
        int clang_exit_code = run_clang_link(llvm_output_path, runtime_source_path, exe_output_path,
                                             driver_args, driver_arg_count,
                                             linker_args, linker_arg_count,
                                             clang_log_path);
        if(clang_exit_code != 0)
        {
            remove(llvm_output_path);
            print_file_if_exists(clang_log_path);
            remove(clang_log_path);
            printf("LLVM link failed\n");
            exit(1);
        }

        remove(llvm_output_path);
        remove(clang_log_path);
        printf("%s\n", exe_output_path);
    }

    return 0;
}
