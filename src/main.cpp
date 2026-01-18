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


// using namespace llvm;

std::unique_ptr<llvm::LLVMContext> TheContext;
std::unique_ptr<llvm::IRBuilder<>> Builder;
std::unique_ptr<llvm::Module> TheModule;
std::map<std::string, llvm::Value*> NamedValues;


llvm::Value* NumExprAST::codegen() {
    assert(TheContext && "TheContext is null");
  return llvm::ConstantFP::get(*TheContext, llvm::APFloat(Val));
}

llvm::Value* VariableExprAST::codegen() {
    llvm::Value* V = NamedValues[name];
    if (!V) {
        LogErrorV("Unkown variable name");
    }
    return V;
}


llvm::Value* BinaryExprAST::codegen() {
        // std::cout << "got here \n";
        llvm::Value* L = LHS->codegen();
        // std::cout << "got here \n";
        llvm::Value* R = RHS->codegen();
        // std::cout << "got here \n";

        if (!L || !R) {
            return nullptr;
        }
        switch (Op)
        {
        case '+':
            return Builder->CreateFAdd(L, R, "addtmp");
        case '-':
            return Builder->CreateFSub(L, R, "subtmp");
        case '*':
            return Builder->CreateFMul(L, R, "multmp");
        case '<':
            L= Builder->CreateFCmpULT(L, R, "cmptmp");
            return Builder->CreateUIToFP(L, llvm::Type::getDoubleTy(*TheContext), "booltmp");
        
        default:
            return LogErrorV("invalid binop operator");
        }

}

llvm::Value* CallExprAST::codegen() {
        llvm::Function* CalleeF = TheModule->getFunction(Callee);
        if (!CalleeF) 
            LogErrorV("Unkown function");
        
        if (CalleeF->arg_size() != Args.size())
            return LogErrorV("Incorrect # of arguments");
        
        std::vector<llvm::Value *> ArgsV;
        for (int i = 0; i < Args.size(); i++) {
            ArgsV.push_back(Args[i]->codegen());
            if (!ArgsV.back()) {
                return nullptr;
            }
        } 

        return Builder->CreateCall(CalleeF, ArgsV, "calltmp");
    
    }

llvm::Function *SignatureAST::codegen() {
        std::vector<llvm::Type*> Doubles(Args.size(), llvm::Type::getDoubleTy(*TheContext));
        llvm::FunctionType *FT = llvm::FunctionType::get(llvm::Type::getDoubleTy(*TheContext), Doubles, false);
        llvm::Function *F = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, Name, TheModule.get());

        unsigned idx = 0;
        for (auto &Arg : F->args()) {
            Arg.setName(Args[idx++]);
        }
        return F;
}

llvm::Function *FunctionAST::codegen() {
        fprintf(stderr, "1!");
        llvm::Function *TheFunction = TheModule->getFunction(signature->getName());
        if (!TheFunction)
            signature->codegen();
        fprintf(stderr, "2!");
        
        if (!TheFunction)
            return nullptr;
        
        if (!TheFunction->empty()) 
            return (llvm::Function*)LogErrorV("llvm::Function cannot be redefined.");
        fprintf(stderr, "3!");

        llvm::BasicBlock *BB = llvm::BasicBlock::Create(*TheContext, "entry", TheFunction);
        Builder->SetInsertPoint(BB);
        NamedValues.clear();
        for (auto &Arg : TheFunction->args()) 
            NamedValues[std::string(Arg.getName())] = &Arg;
        fprintf(stderr, "4!");
        
        if (llvm::Value *RetVal = body->codegen()) {
            Builder->CreateRet(RetVal);

            llvm::verifyFunction(*TheFunction);
            return TheFunction;
        }
        TheFunction->eraseFromParent();
        return nullptr;
}


static void HandleDefinition() {
    if (auto FnAST = ParseDefinition()) {
        fprintf(stderr, "Parsed definition correctly\n");

        if (auto *FnIr = FnAST->codegen()) {
            fprintf(stderr, "Read function definition\n");
            FnIr->print(llvm::errs());
            fprintf(stderr, "\n");
        }

    } else {
        getNextToken();
    }
}

static void HandleExtern() {
    if (ParseExtern()) {
        fprintf(stderr, "Parsed extern correctly\n");

    } else {
        getNextToken();
    }
}


static void HandleTopLevel() {
    if (auto FnAST = ParseTopLevelExpr()) {
        fprintf(stdout, "Parsed TopLevelExprs correctly\n");
        if (auto *TopLevelExprIR = FnAST->codegen()) {
            fprintf(stderr, "Read top-level expr\n");
            TopLevelExprIR->print(llvm::errs());
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
        case tok_extern:
            HandleExtern();
            break;
        default:
            HandleTopLevel();
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