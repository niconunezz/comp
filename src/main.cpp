#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <iostream>
#include "parser/parser.hpp"
#include "ast/ast_base.hpp"


using namespace llvm;


static void HandleDefinition() {
    if (auto FnAST = ParseDefinition()) {
        fprintf(stderr, "Parsed def correctly\n");

        if (auto *FnIr = FnAST->codegen()) {
            fprintf(stderr, "Read function definition\n");
            FnIr->print(errs());
            fprintf(stderr, "\n");
        }

    } else {
        getNextToken();
    }
}

static void HandleNum() {
    if (auto NumAST = ParseExpression()) {
        fprintf(stderr, "Parsed a exprresion correctly\n");
        if (auto *NumIR = NumAST->codegen()){
            fprintf(stderr, "Read number\n");
            NumIR->print(errs());

        }
    } else {
        getNextToken();
    }
}


static void MainLoop() {
    while (true) {
        fprintf(stderr, "ready> ");
        switch (CurTok)
        {
        case tok_eof:
            return;
        case ';':
            getNextToken();
            break;
        case tok_def:
            HandleDefinition();
            break;
        case tok_number:
            HandleNum();
        default:
            getNextToken();
            break;
        }
    }
}

int main() {
    std::unique_ptr<llvm::LLVMContext> TheContext;
    std::unique_ptr<llvm::IRBuilder<>> Builder;
    std::unique_ptr<llvm::Module> TheModule;
    BinOpPrecedence['<'] = 10;
    BinOpPrecedence['+'] = 20;
    BinOpPrecedence['-'] = 20;
    BinOpPrecedence['*'] = 40; // highest.
    
    fprintf(stderr, "ready> ");
    getNextToken();
    MainLoop();

    return 0;
}