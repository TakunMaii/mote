#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Lexer.h"
#include "Token.h"
#include "AST.h"
#include "Parser.h"
#include "Semantic.h"
#include "MIR.h"
#include "ModuleSystem.h"
#include "LLVMBackend.h"

#define CLI_MAX_PACKAGES 128
#define CLI_MAX_LINK_ARGS 128

static void print_usage(const char *argv0)
{
    printf("Usage:\n");
    printf("  %s [--pkg name=path]... <input>\n", argv0);
    printf("  %s [--pkg name=path]... --emit-llvm <input> [output.ll]\n", argv0);
    printf("  %s [--pkg name=path]... [--link-arg value]... --emit-exe <input> [output.exe]\n", argv0);
    printf("\n");
    printf("Options:\n");
    printf("  --pkg name=path   Register a package root for @import(\"name/...\")\n");
    printf("  --emit-llvm       Write LLVM IR to a .ll file\n");
    printf("  --emit-exe        Write LLVM IR, invoke clang, and produce an executable\n");
    printf("  --link-arg value  Forward one extra argument to clang during --emit-exe\n");
    printf("  --help, -h        Show this help text\n");
    printf("\n");
    printf("Notes:\n");
    printf("  - With no emit flag, the compiler parses, checks, lowers to MIR, and prints AST/MIR.\n");
    printf("  - --emit-exe requires clang to be available in PATH.\n");
    printf("  - If output is omitted, the compiler derives it from the input file name.\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s test\\\\basic\\\\simple.mote\n", argv0);
    printf("  %s --emit-llvm test\\\\basic\\\\simple.mote\n", argv0);
    printf("  %s --emit-exe test\\\\basic\\\\simple.mote test\\\\basic\\\\simple.exe\n", argv0);
    printf("  %s --link-arg -Lthird_party\\\\lib --link-arg -lfoo --emit-exe app.mote\n", argv0);
    printf("  %s --pkg c=lib\\\\c --emit-exe test\\\\ffi\\\\ffi_main.mote\n", argv0);
    printf("  %s --pkg app=test\\\\pkg\\\\app --emit-exe test\\\\multi\\\\package_main.mote\n", argv0);
}

static void parse_package_argument(const char *arg, ModulePackage *package)
{
    const char *separator = strchr(arg, '=');
    if(separator == NULL || separator == arg || separator[1] == '\0')
    {
        printf("Invalid --pkg argument: expected name=path, got %s\n", arg);
        exit(1);
    }

    size_t name_length = (size_t)(separator - arg);
    if(name_length >= sizeof(package->name))
    {
        printf("Invalid --pkg argument: package name is too long\n");
        exit(1);
    }
    if(strlen(separator + 1) >= sizeof(package->root_path))
    {
        printf("Invalid --pkg argument: package path is too long\n");
        exit(1);
    }

    memset(package, 0, sizeof(ModulePackage));
    memcpy(package->name, arg, name_length);
    package->name[name_length] = '\0';
    strcpy(package->root_path, separator + 1);
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

static int run_clang_link(const char *llvm_input_path, const char *exe_output_path,
                          const char **link_args, int link_arg_count)
{
    char command[4096];
#if defined(_WIN32)
    snprintf(command, sizeof(command),
             "clang \"%s\" \"src\\mote_runtime.c\" -o \"%s\" -Xlinker /subsystem:console",
             llvm_input_path, exe_output_path);
#else
    snprintf(command, sizeof(command),
             "clang \"%s\" \"src/mote_runtime.c\" -o \"%s\"",
             llvm_input_path, exe_output_path);
#endif

    for(int i = 0; i < link_arg_count; i++)
    {
#if defined(_WIN32)
        append_shell_escaped(command, sizeof(command), "-Xlinker");
#endif
        append_shell_escaped(command, sizeof(command), link_args[i]);
    }

    return system(command);
}

int main(int argn, char** argv)
{
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    bool emit_llvm = false;
    bool emit_exe = false;
    const char *input_path = NULL;
    const char *llvm_output_path = NULL;
    const char *exe_output_path = NULL;
    ModulePackage *packages = (ModulePackage*) calloc(CLI_MAX_PACKAGES, sizeof(ModulePackage));
    int package_count = 0;
    const char **link_args = (const char**) calloc(CLI_MAX_LINK_ARGS, sizeof(const char*));
    int link_arg_count = 0;
    const char **positionals = (const char**) calloc(2, sizeof(const char*));
    int positional_count = 0;

    if(packages == NULL || link_args == NULL || positionals == NULL)
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

        if(strcmp(argv[i], "--emit-llvm") == 0)
        {
            emit_llvm = true;
            emit_exe = false;
            continue;
        }

        if(strcmp(argv[i], "--emit-exe") == 0)
        {
            emit_llvm = true;
            emit_exe = true;
            continue;
        }

        if(strcmp(argv[i], "--pkg") == 0)
        {
            if(i + 1 >= argn)
            {
                print_usage(argv[0]);
                exit(1);
            }
            if(package_count >= CLI_MAX_PACKAGES)
            {
                printf("Too many --pkg arguments\n");
                exit(1);
            }
            parse_package_argument(argv[++i], &(packages[package_count++]));
            continue;
        }

        if(strcmp(argv[i], "--link-arg") == 0)
        {
            if(i + 1 >= argn)
            {
                print_usage(argv[0]);
                exit(1);
            }
            if(link_arg_count >= CLI_MAX_LINK_ARGS)
            {
                printf("Too many --link-arg arguments\n");
                exit(1);
            }
            link_args[link_arg_count++] = argv[++i];
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
    if(positional_count >= 2)
    {
        if(emit_exe)
            exe_output_path = positionals[1];
        else if(emit_llvm)
            llvm_output_path = positionals[1];
        else
        {
            print_usage(argv[0]);
            exit(1);
        }
    }

    if(input_path == NULL)
    {
        print_usage(argv[0]);
        exit(1);
    }

    char default_llvm_output_path[1024] = {0};
    char default_exe_output_path[1024] = {0};
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

        build_output_path(default_llvm_output_path, sizeof(default_llvm_output_path), exe_output_path, ".ll");
        llvm_output_path = default_llvm_output_path;
    }
    else if(emit_llvm && llvm_output_path == NULL)
        build_output_path(default_llvm_output_path, sizeof(default_llvm_output_path), input_path, ".ll"), llvm_output_path = default_llvm_output_path;

    ASTNode *root = buildModuleProgramAST(input_path, packages, package_count);
    checkAssignSemantics(root);
    checkAssignTypes(root);
    if(!emit_llvm)
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
    if(!emit_llvm)
        printMIRProgram(mir_program);

    if(emit_llvm)
    {
        emitLLVMProgramToFile(mir_program, input_path, llvm_output_path);
        printf("WROTE LLVM IR ===============\n\n%s\n\n", llvm_output_path);

        if(emit_exe)
        {
            int clang_exit_code = run_clang_link(llvm_output_path, exe_output_path, link_args, link_arg_count);
            if(clang_exit_code != 0)
            {
                printf("LLVM link failed; kept intermediate IR at %s\n", llvm_output_path);
                exit(1);
            }

            remove(llvm_output_path);
            printf("WROTE EXECUTABLE ===============\n\n%s\n\n", exe_output_path);
        }
    }

    return 0;
}
