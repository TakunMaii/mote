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
    ASTNode *root = buildModuleProgramAST("test\\while_else_if_min.mote", NULL, 0);
    printf("built ast\n");
    checkAssignSemantics(root);
    printf("passed semantics\n");
    checkAssignTypes(root);
    printf("passed types\n");
    printf("about to print ast like main\n");
    ASTNode *ndptr = root;
    while(ndptr)
    {
        printASTNode(*ndptr);
        ndptr = ndptr->next;
    }
    printf("printed ast like main\n");
    MirProgram *program = lowerASTToMIR(root);
    printf("lowered mir\n");
    printMIRProgram(program);
    printf("printed mir\n");
    emitLLVMProgramToFile(program, "test\\while_else_if_min.mote", "test\\while_else_if_min.debug.ll");
    printf("emitted llvm\n");
    return 0;
}
