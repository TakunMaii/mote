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
#include "CliDriver.h"

static void append_shell_escaped(char *command, size_t command_size, const char *arg)
{
    size_t used = strlen(command);
    if(used + 4 >= command_size)
        cliError("link command is too long");

    command[used++] = ' ';
    command[used++] = '"';
    command[used] = '\0';

    for(const char *p = arg; *p != '\0'; p++)
    {
        if(*p == '"')
        {
            if(used + 2 >= command_size)
                cliError("link command is too long");
            command[used++] = '\\';
        }

        if(used + 1 >= command_size)
            cliError("link command is too long");
        command[used++] = *p;
    }

    if(used + 2 >= command_size)
        cliError("link command is too long");
    command[used++] = '"';
    command[used] = '\0';
}

static void append_shell_quoted_fragment(char *command, size_t command_size, const char *value)
{
    size_t used = strlen(command);
    if(used + 3 >= command_size)
        cliError("link command is too long");

    command[used++] = '"';
    command[used] = '\0';

    for(const char *p = value; *p != '\0'; p++)
    {
        if(*p == '"')
        {
            if(used + 2 >= command_size)
                cliError("link command is too long");
            command[used++] = '\\';
        }

        if(used + 1 >= command_size)
            cliError("link command is too long");
        command[used++] = *p;
    }

    if(used + 2 >= command_size)
        cliError("link command is too long");
    command[used++] = '"';
    command[used] = '\0';
}

static int run_clang_link(const char *llvm_input_path, const char *runtime_source_path,
                          const char *exe_output_path,
                          bool emit_debug_info,
                          const char **driver_args, int driver_arg_count,
                          const char **extra_c_sources, int extra_c_source_count,
                          const char **linker_args, int linker_arg_count,
                          const char *log_path)
{
    char command[4096];
    strcpy(command, "clang");
    append_shell_escaped(command, sizeof(command), llvm_input_path);
    append_shell_escaped(command, sizeof(command), runtime_source_path);
    append_shell_escaped(command, sizeof(command), "-o");
    append_shell_escaped(command, sizeof(command), exe_output_path);
    if(emit_debug_info)
        append_shell_escaped(command, sizeof(command), "-g");
#if defined(_WIN32)
    append_shell_escaped(command, sizeof(command), "-Xlinker");
    append_shell_escaped(command, sizeof(command), "/subsystem:console");
#endif

    for(int i = 0; i < driver_arg_count; i++)
        append_shell_escaped(command, sizeof(command), driver_args[i]);

    for(int i = 0; i < extra_c_source_count; i++)
        append_shell_escaped(command, sizeof(command), extra_c_sources[i]);

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

    CliOptions options = {0};
    ModulePackage *packages = (ModulePackage*) calloc(CLI_MAX_PACKAGES, sizeof(ModulePackage));
    int package_count = 0;
    const char **driver_args = (const char**) calloc(CLI_MAX_LINK_ARGS, sizeof(const char*));
    int driver_arg_count = 0;
    const char **extra_c_sources = (const char**) calloc(CLI_MAX_EXTRA_C_SOURCES, sizeof(const char*));
    int extra_c_source_count = 0;
    const char **linker_args = (const char**) calloc(CLI_MAX_LINKER_ARGS, sizeof(const char*));
    int linker_arg_count = 0;
    const char **positionals = (const char**) calloc(2, sizeof(const char*));

    if(packages == NULL || driver_args == NULL || extra_c_sources == NULL || linker_args == NULL || positionals == NULL)
        cliError("failed to allocate CLI argument buffers");
    parse_cli_options(argn, argv, &options,
                      packages, &package_count,
                      driver_args, &driver_arg_count,
                      extra_c_sources, &extra_c_source_count,
                      linker_args, &linker_arg_count);

    ASTNode *root = buildModuleProgramAST(options.input_path, packages, package_count, options.emit_exe);
    char default_llvm_output_path[1024] = {0};
    char default_exe_output_path[1024] = {0};
    const char *llvm_output_path = NULL;
    const char *exe_output_path = NULL;
    derive_output_paths(&options, root,
                        default_llvm_output_path, sizeof(default_llvm_output_path),
                        default_exe_output_path, sizeof(default_exe_output_path),
                        &llvm_output_path, &exe_output_path);

    checkAssignSemantics(root);
    checkAssignTypes(root);
    if(options.dump_ast)
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
    if(options.dump_mir)
        printMIRProgram(mir_program);

    if(options.emit_llvm)
    {
        emitLLVMProgramToFile(mir_program, root, options.input_path, llvm_output_path, options.emit_debug_info);
        if(options.keep_llvm_output)
            printf("%s\n", llvm_output_path);
    }

    if(options.emit_exe)
    {
        char runtime_source_path[CLI_PATH_BUFFER_SIZE] = {0};
        char clang_log_path[CLI_PATH_BUFFER_SIZE] = {0};
        if(!resolve_runtime_source_path(runtime_source_path, sizeof(runtime_source_path), argv[0]))
            cliError("failed to resolve runtime path relative to the compiler executable");
        if(!file_exists(runtime_source_path))
            cliErrorFormatted("failed to locate runtime source at %s", runtime_source_path);

        if(!options.emit_llvm)
        {
            build_temp_llvm_output_path(default_llvm_output_path, sizeof(default_llvm_output_path), exe_output_path);
            llvm_output_path = default_llvm_output_path;
            emitLLVMProgramToFile(mir_program, root, options.input_path, llvm_output_path, options.emit_debug_info);
        }

        snprintf(clang_log_path, sizeof(clang_log_path), "%s.mote-link.log", exe_output_path);
        int clang_exit_code = run_clang_link(llvm_output_path, runtime_source_path, exe_output_path,
                                             options.emit_debug_info,
                                             driver_args, driver_arg_count,
                                             extra_c_sources, extra_c_source_count,
                                             linker_args, linker_arg_count,
                                             clang_log_path);
        if(clang_exit_code != 0)
        {
            remove(llvm_output_path);
            print_file_if_exists(clang_log_path);
            remove(clang_log_path);
            cliError("LLVM link failed");
        }

        remove(llvm_output_path);
        remove(clang_log_path);
        printf("%s\n", exe_output_path);
    }

    return 0;
}
