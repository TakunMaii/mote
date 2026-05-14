#include <stdio.h>
#include <stdlib.h>
#include "Lexer.h"
#include "Token.h"
#include "AST.h"
#include "Parser.h"

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

int main(int argn, char** argv)
{
    if(argn < 2)
    {
        printf("Usage: %s <input>\n", argv[0]);
        exit(1); 
    }

    char *filecontent = read_file(argv[1]);

    Token* tokens = tokenize(filecontent, argv[1]);

    printf("PRINT TOKENS ===============\n\n");
    Token *tkptr = tokens;
    while(tkptr)
    {
        printToken(*tkptr);
        tkptr = tkptr->next;
    }
    printf("END PRINT TOKENS ===============\n\n");

    ASTNode *root = parse(tokens);
    printf("PRINT AST NODES ===============\n\n");
    ASTNode *ndptr = root;
    while(ndptr)
    {
        printASTNode(*ndptr);
        ndptr = ndptr->next;
    }
    printf("END PRINT AST NODES ===============\n\n");

    return 0;
}
