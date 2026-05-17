#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
    printf("  - If -o is omitted, the compiler derives output from the input file name.\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s test\\\\basic\\\\simple.mote\n", argv0);
    printf("  %s -S test\\\\basic\\\\simple.mote -o test\\\\basic\\\\simple.ll\n", argv0);
    printf("  %s test.mote -I lib -o test.exe\n", argv0);
    printf("  %s app.mote -Lthird_party\\\\lib -lfoo -o app.exe\n", argv0);
    printf("  %s test\\\\ffi\\\\ffi_main.mote -I lib -o test\\\\ffi\\\\ffi_main.exe\n", argv0);
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

static bool file_exists(const char *path)
{
    FILE *stream = fopen(path, "rb");
    if(stream == NULL)
        return false;
    fclose(stream);
    return true;
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

static int run_clang_link(const char *llvm_input_path, const char *runtime_source_path,
                          const char *exe_output_path,
                          const char **driver_args, int driver_arg_count,
                          const char **linker_args, int linker_arg_count)
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

    return system(command);
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
            if(package_count >= CLI_MAX_PACKAGES)
            {
                printf("Too many -I arguments\n");
                exit(1);
            }
            memset(&(packages[package_count]), 0, sizeof(ModulePackage));
            strcpy(packages[package_count].root_path, argv[++i]);
            packages[package_count].is_search_root = true;
            package_count++;
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
        printf("WROTE LLVM IR ===============\n\n%s\n\n", llvm_output_path);
    }

    if(emit_exe)
    {
        char runtime_source_path[CLI_PATH_BUFFER_SIZE] = {0};
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
            build_output_path(default_llvm_output_path, sizeof(default_llvm_output_path), exe_output_path, ".ll");
            llvm_output_path = default_llvm_output_path;
            emitLLVMProgramToFile(mir_program, input_path, llvm_output_path);
            printf("WROTE LLVM IR ===============\n\n%s\n\n", llvm_output_path);
        }

        int clang_exit_code = run_clang_link(llvm_output_path, runtime_source_path, exe_output_path,
                                             driver_args, driver_arg_count,
                                             linker_args, linker_arg_count);
        if(clang_exit_code != 0)
        {
            printf("LLVM link failed; kept intermediate IR at %s\n", llvm_output_path);
            exit(1);
        }

        remove(llvm_output_path);
        printf("WROTE EXECUTABLE ===============\n\n%s\n\n", exe_output_path);
    }

    return 0;
}
