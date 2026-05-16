#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Lexer.h"
#include "Token.h"
#include "AST.h"
#include "Parser.h"
#include "Semantic.h"
#include "MIR.h"
#include "LLVMBackend.h"

char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        printf("Cannot open file %s\n", path);
        return NULL;
    }

    long size = ftell(f);
    if (size < 0) {
        fclose(f);
        return NULL;
    }

    rewind(f);

    char *buf = malloc(size + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }

    size_t read_bytes = fread(buf, 1, size, f);
    if (read_bytes != (size_t)size) {
        free(buf);
        fclose(f);
        return NULL;
    }

    buf[size] = '\0';
    fclose(f);
    return buf;
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

static int run_clang_link(const char *llvm_input_path, const char *exe_output_path)
{
    char command[4096];
#if defined(_WIN32)
    snprintf(command, sizeof(command),
             "clang \"%s\" -o \"%s\" -Xlinker /subsystem:console",
             llvm_input_path, exe_output_path);
#else
    snprintf(command, sizeof(command),
             "clang \"%s\" -o \"%s\"",
             llvm_input_path, exe_output_path);
#endif
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

    if(argn >= 2 && strcmp(argv[1], "--emit-llvm") == 0)
    {
        emit_llvm = true;
        if(argn < 3)
        {
            printf("Usage: %s [--emit-llvm <input> [output.ll]] | [--emit-exe <input> [output.exe]] | <input>\n", argv[0]);
            exit(1);
        }

        input_path = argv[2];
        if(argn >= 4)
            llvm_output_path = argv[3];
    }
    else if(argn >= 2 && strcmp(argv[1], "--emit-exe") == 0)
    {
        emit_llvm = true;
        emit_exe = true;
        if(argn < 3)
        {
            printf("Usage: %s [--emit-llvm <input> [output.ll]] | [--emit-exe <input> [output.exe]] | <input>\n", argv[0]);
            exit(1);
        }

        input_path = argv[2];
        if(argn >= 4)
            exe_output_path = argv[3];
    }
    else if(argn >= 2)
        input_path = argv[1];

    if(input_path == NULL)
    {
        printf("Usage: %s [--emit-llvm <input> [output.ll]] | [--emit-exe <input> [output.exe]] | <input>\n", argv[0]);
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

    char *filecontent = read_file(input_path);

    Token* tokens = tokenize(filecontent, input_path);

    printf("PRINT TOKENS ===============\n\n");
    Token *tkptr = tokens;
    while(tkptr)
    {
        printToken(*tkptr);
        tkptr = tkptr->next;
    }
    printf("END PRINT TOKENS ===============\n\n");

    ASTNode *root = parse(tokens);
    checkAssignSemantics(root);
    checkAssignTypes(root);
    printf("PRINT AST NODES ===============\n\n");
    ASTNode *ndptr = root;
    while(ndptr)
    {
        printASTNode(*ndptr);
        ndptr = ndptr->next;
    }
    printf("END PRINT AST NODES ===============\n\n");

    MirProgram *mir_program = lowerASTToMIR(root);
    printMIRProgram(mir_program);

    if(emit_llvm)
    {
        emitLLVMProgramToFile(mir_program, input_path, llvm_output_path);
        printf("WROTE LLVM IR ===============\n\n%s\n\n", llvm_output_path);

        if(emit_exe)
        {
            int clang_exit_code = run_clang_link(llvm_output_path, exe_output_path);
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
