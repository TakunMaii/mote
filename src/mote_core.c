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
#include "MoteCore.h"

static char g_mote_core_host_argv0[CLI_PATH_BUFFER_SIZE] = {0};

static void mote_core_append_shell_escaped(char *command, size_t command_size, const char *arg)
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

static void mote_core_append_shell_quoted_fragment(char *command, size_t command_size, const char *value)
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

static int mote_core_run_clang_link(const char *llvm_input_path, const char *runtime_source_path,
                                    const char *exe_output_path,
                                    bool emit_debug_info,
                                    const char **driver_args, int driver_arg_count,
                                    const char **extra_c_sources, int extra_c_source_count,
                                    const char **linker_args, int linker_arg_count,
                                    const char *log_path)
{
    char command[4096];
    strcpy(command, "clang");
    mote_core_append_shell_escaped(command, sizeof(command), llvm_input_path);
    mote_core_append_shell_escaped(command, sizeof(command), runtime_source_path);
    mote_core_append_shell_escaped(command, sizeof(command), "-o");
    mote_core_append_shell_escaped(command, sizeof(command), exe_output_path);
    if(emit_debug_info)
        mote_core_append_shell_escaped(command, sizeof(command), "-g");
#if defined(_WIN32)
    mote_core_append_shell_escaped(command, sizeof(command), "-Xlinker");
    mote_core_append_shell_escaped(command, sizeof(command), "/subsystem:console");
#endif

    for(int i = 0; i < driver_arg_count; i++)
        mote_core_append_shell_escaped(command, sizeof(command), driver_args[i]);

    for(int i = 0; i < extra_c_source_count; i++)
        mote_core_append_shell_escaped(command, sizeof(command), extra_c_sources[i]);

    for(int i = 0; i < linker_arg_count; i++)
    {
        mote_core_append_shell_escaped(command, sizeof(command), "-Xlinker");
        mote_core_append_shell_escaped(command, sizeof(command), linker_args[i]);
    }

    if(strlen(command) + strlen(" >") + strlen(log_path) + strlen(" 2>&1") + 1 >= sizeof(command))
    {
        printf("Link command is too long\n");
        exit(1);
    }
    strcat(command, " >");
    mote_core_append_shell_quoted_fragment(command, sizeof(command), log_path);
    strcat(command, " 2>&1");

    return system(command);
}

static void mote_core_print_file_if_exists(const char *path)
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

static CliOptions mote_core_to_cli_options(const MoteCompileOptions *options)
{
    CliOptions cli = {0};
    if(options == NULL)
        return cli;

    cli.emit_llvm = options->emit_llvm;
    cli.emit_exe = options->emit_exe;
    cli.dump_ast = options->dump_ast;
    cli.dump_mir = options->dump_mir;
    cli.emit_debug_info = options->emit_debug_info;
    cli.keep_llvm_output = options->keep_llvm_output;
    cli.input_path = options->input_path;
    cli.requested_output_path = options->requested_output_path;
    cli.llvm_output_path = options->llvm_output_path;
    cli.exe_output_path = options->exe_output_path;
    return cli;
}

MoteCompileResult moteCompile(const MoteCompileOptions *options,
                              MotePackage *packages, int package_count,
                              const char **driver_args, int driver_arg_count,
                              const char **extra_c_sources, int extra_c_source_count,
                              const char **linker_args, int linker_arg_count,
                              const char *runtime_source_path)
{
    MoteCompileResult result = {0};
    DiagnosticTrap trap = {0};
    diagnosticTrapPush(&trap);

    if(setjmp(trap.jump_buffer) != 0)
    {
        diagnosticTrapPop(&trap);
        result.ok = false;
        if(trap.has_diagnostic)
            result.diagnostic = trap.diagnostic;
        return result;
    }

    CliOptions cli_options = mote_core_to_cli_options(options);
    ASTNode *root = buildModuleProgramAST(options->input_path, (ModulePackage*) packages, package_count, options->emit_exe);
    char default_llvm_output_path[1024] = {0};
    char default_exe_output_path[1024] = {0};
    const char *llvm_output_path = NULL;
    const char *exe_output_path = NULL;
    derive_output_paths(&cli_options, root,
                        default_llvm_output_path, sizeof(default_llvm_output_path),
                        default_exe_output_path, sizeof(default_exe_output_path),
                        &llvm_output_path, &exe_output_path);

    checkAssignSemantics(root);
    checkAssignTypes(root);
    if(options->dump_ast)
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
    if(options->dump_mir)
        printMIRProgram(mir_program);

    if(options->emit_llvm)
    {
        emitLLVMProgramToFile(mir_program, root, options->input_path, llvm_output_path, options->emit_debug_info);
        result.emitted_llvm = true;
        if(options->keep_llvm_output)
            printf("%s\n", llvm_output_path);
    }

    if(options->emit_exe)
    {
        char clang_log_path[CLI_PATH_BUFFER_SIZE] = {0};
        if(runtime_source_path == NULL || runtime_source_path[0] == '\0')
            cliError("runtime source path is required when emitting executables");
        if(!file_exists(runtime_source_path))
            cliErrorFormatted("failed to locate runtime source at %s", runtime_source_path);

        if(!options->emit_llvm)
        {
            build_temp_llvm_output_path(default_llvm_output_path, sizeof(default_llvm_output_path), exe_output_path);
            llvm_output_path = default_llvm_output_path;
            emitLLVMProgramToFile(mir_program, root, options->input_path, llvm_output_path, options->emit_debug_info);
            result.emitted_llvm = true;
        }

        snprintf(clang_log_path, sizeof(clang_log_path), "%s.mote-link.log", exe_output_path);
        int clang_exit_code = mote_core_run_clang_link(llvm_output_path, runtime_source_path, exe_output_path,
                                                       options->emit_debug_info,
                                                       driver_args, driver_arg_count,
                                                       extra_c_sources, extra_c_source_count,
                                                       linker_args, linker_arg_count,
                                                       clang_log_path);
        if(clang_exit_code != 0)
        {
            remove(llvm_output_path);
            mote_core_print_file_if_exists(clang_log_path);
            remove(clang_log_path);
            cliError("LLVM link failed");
        }

        remove(llvm_output_path);
        remove(clang_log_path);
        printf("%s\n", exe_output_path);
        result.emitted_exe = true;
    }

    if(llvm_output_path != NULL)
        snprintf(result.artifacts.llvm_output_path, sizeof(result.artifacts.llvm_output_path), "%s", llvm_output_path);
    if(exe_output_path != NULL)
        snprintf(result.artifacts.exe_output_path, sizeof(result.artifacts.exe_output_path), "%s", exe_output_path);

    diagnosticTrapPop(&trap);
    result.ok = true;
    return result;
}

int moteCliMain(int argn, char **argv)
{
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);
    mote_core_set_host_argv0(argn > 0 ? argv[0] : NULL);

    CliOptions options = {0};
    MotePackage *packages = (MotePackage*) calloc(MOTE_MAX_PACKAGES, sizeof(MotePackage));
    int package_count = 0;
    const char **driver_args = (const char**) calloc(MOTE_MAX_LINK_ARGS, sizeof(const char*));
    int driver_arg_count = 0;
    const char **extra_c_sources = (const char**) calloc(MOTE_MAX_EXTRA_C_SOURCES, sizeof(const char*));
    int extra_c_source_count = 0;
    const char **linker_args = (const char**) calloc(MOTE_MAX_LINKER_ARGS, sizeof(const char*));
    int linker_arg_count = 0;
    char runtime_source_path[CLI_PATH_BUFFER_SIZE] = {0};

    if(packages == NULL || driver_args == NULL || extra_c_sources == NULL || linker_args == NULL)
        cliError("failed to allocate CLI argument buffers");

    parse_cli_options(argn, argv, &options,
                      (ModulePackage*) packages, &package_count,
                      driver_args, &driver_arg_count,
                      extra_c_sources, &extra_c_source_count,
                      linker_args, &linker_arg_count);

    const char *runtime_path = NULL;
    if(options.emit_exe)
    {
        if(!resolve_runtime_source_path(runtime_source_path, sizeof(runtime_source_path), argv[0]))
            cliError("failed to resolve runtime path relative to the compiler executable");
        runtime_path = runtime_source_path;
    }

    MoteCompileOptions compile_options = {
        .emit_llvm = options.emit_llvm,
        .emit_exe = options.emit_exe,
        .dump_ast = options.dump_ast,
        .dump_mir = options.dump_mir,
        .emit_debug_info = options.emit_debug_info,
        .keep_llvm_output = options.keep_llvm_output,
        .input_path = options.input_path,
        .requested_output_path = options.requested_output_path,
        .llvm_output_path = options.llvm_output_path,
        .exe_output_path = options.exe_output_path,
    };

    MoteCompileResult result = moteCompile(&compile_options,
                                           packages, package_count,
                                           driver_args, driver_arg_count,
                                           extra_c_sources, extra_c_source_count,
                                           linker_args, linker_arg_count,
                                           runtime_path);
    if(!result.ok)
    {
        diagnosticEmit(&(result.diagnostic));
        return 1;
    }

    return 0;
}

void mote_core_set_host_argv0(const char *argv0)
{
    if(argv0 == NULL)
    {
        g_mote_core_host_argv0[0] = '\0';
        return;
    }
    snprintf(g_mote_core_host_argv0, sizeof(g_mote_core_host_argv0), "%s", argv0);
}

int mote_core_compile_simple(const char *input_path,
                             const char *output_path,
                             bool emit_exe,
                             bool emit_llvm,
                             bool emit_debug_info)
{
    MoteCompileOptions options = {0};
    options.input_path = input_path;
    options.requested_output_path = output_path;
    options.emit_exe = emit_exe;
    options.emit_llvm = emit_llvm;
    options.emit_debug_info = emit_debug_info;
    options.keep_llvm_output = emit_llvm && !emit_exe;

    MotePackage packages[MOTE_MAX_PACKAGES];
    memset(packages, 0, sizeof(packages));
    int package_count = 0;
    const char *driver_args[MOTE_MAX_LINK_ARGS];
    memset(driver_args, 0, sizeof(driver_args));
    int driver_arg_count = 0;
    const char *extra_c_sources[MOTE_MAX_EXTRA_C_SOURCES];
    memset(extra_c_sources, 0, sizeof(extra_c_sources));
    int extra_c_source_count = 0;
    const char *linker_args[MOTE_MAX_LINKER_ARGS];
    memset(linker_args, 0, sizeof(linker_args));
    int linker_arg_count = 0;

    const char *host_argv0 = g_mote_core_host_argv0[0] != '\0' ? g_mote_core_host_argv0 : "mote";

    add_default_official_search_roots((ModulePackage*) packages, &package_count, host_argv0);
    add_default_official_link_args(host_argv0, (ModulePackage*) packages, package_count, input_path,
                                   driver_args, &driver_arg_count,
                                   extra_c_sources, &extra_c_source_count);

    const char *runtime_path = NULL;
    char runtime_source_path[CLI_PATH_BUFFER_SIZE] = {0};
    if(emit_exe)
    {
        if(!resolve_runtime_source_path(runtime_source_path, sizeof(runtime_source_path), host_argv0))
        {
            printf("failed to resolve runtime path relative to host compiler\n");
            return 1;
        }
        runtime_path = runtime_source_path;
    }

    MoteCompileResult result = moteCompile(&options,
                                           packages, package_count,
                                           driver_args, driver_arg_count,
                                           extra_c_sources, extra_c_source_count,
                                           linker_args, linker_arg_count,
                                           runtime_path);
    if(!result.ok)
    {
        diagnosticEmit(&(result.diagnostic));
        return 1;
    }
    return 0;
}
