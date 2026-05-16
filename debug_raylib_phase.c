#include <stdio.h>
#include "src/Lexer.h"
#include "src/Token.h"
#include "src/AST.h"
#include "src/Parser.h"
#include "src/Semantic.h"
#include "src/MIR.h"
#include "src/LLVMBackend.h"
#include "src/ModuleSystem.h"

int main(void)
{
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    ModulePackage packages[1] = {0};
    snprintf(packages[0].name, sizeof(packages[0].name), "raylib");
    snprintf(packages[0].root_path, sizeof(packages[0].root_path), "lib\\raylib");

    ASTNode *root = buildModuleProgramAST("test\\raylib_basic.mote", packages, 1);
    printf("built ast\n");
    checkAssignSemantics(root);
    printf("passed semantics\n");
    checkAssignTypes(root);
    printf("passed types\n");
    ASTNode *ndptr = root;
    while(ndptr)
    {
        printASTNode(*ndptr);
        ndptr = ndptr->next;
    }
    printf("printed ast\n");
    MirProgram *program = lowerASTToMIR(root);
    printf("lowered mir\n");
    printMIRProgram(program);
    printf("printed mir\n");
    emitLLVMProgramToFile(program, "test\\raylib_basic.mote", "test\\raylib_basic.debug.ll");
    printf("emitted llvm\n");
    return 0;
}
