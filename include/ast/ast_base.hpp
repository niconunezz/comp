#ifndef AST_BASE_HPP
#define AST_BASE_HPP

#include <string>
#include <memory>
#include <vector>
#include "llvm/IR/Value.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Verifier.h"


extern std::unique_ptr<llvm::LLVMContext> TheContext;
extern std::unique_ptr<llvm::IRBuilder<>> Builder;
extern std::unique_ptr<llvm::Module> TheModule;
std::map<std::string, llvm::Value*> NamedValues;


llvm::Value* LogErrorV(const char* Str) {
    fprintf(stderr, "Error: %s\n", Str);
    return nullptr;
}

namespace {
class ExprAST {
public:
    virtual ~ExprAST() = default;
    virtual llvm::Value* codegen() = 0;
};

class NumExprAST : public ExprAST {
private:
    double Val;
public:
    NumExprAST(double Val) : Val(Val) {}
    llvm::Value* codegen() override {
        assert(TheContext && "TheContext is null");
        return llvm::ConstantFP::get(*TheContext, llvm::APFloat(Val));
    }
};

class VariableExprAST : public ExprAST {
    std::string name;
public:

    VariableExprAST(std::string name) : name(name) {}
    llvm::Value* codegen() override {
        llvm::Value* V = NamedValues[name];
        if (!V) {
            LogErrorV("Unkown variable name");
        }
        return V;
    }
};

class BinaryExprAST : public ExprAST {
    char Op;
    std::unique_ptr<ExprAST> LHS, RHS;

public:
    BinaryExprAST(char Op, std::unique_ptr<ExprAST> LHS,
                  std::unique_ptr<ExprAST> RHS) : 
                  Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}

    llvm::Value* codegen() override {
        std::cout << "got here \n";
        llvm::Value* L = LHS->codegen();
        std::cout << "got here \n";
        llvm::Value* R = RHS->codegen();
        std::cout << "got here \n";

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

};

class CallExprAST : public ExprAST {
    std::string Callee;
    std::vector<std::unique_ptr<ExprAST>> Args;
public:
    CallExprAST(const std::string& Callee, std::vector<std::unique_ptr<ExprAST>> Args) :
    Callee(Callee), Args(std::move(Args)) {}

    llvm::Value* codegen() override {
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
};

class SignatureAST {
    std::string Name;
    std::vector<std::string> Args;

public:
    SignatureAST(std::string Name, std::vector<std::string> Args) : Name(Name), Args(std::move(Args)) {}
    const std::string& getName() const { return Name; }

    llvm::Function* codegen() {
        std::vector<llvm::Type*> Doubles(Args.size(), llvm::Type::getDoubleTy(*TheContext));
        llvm::FunctionType *FT = llvm::FunctionType::get(llvm::Type::getDoubleTy(*TheContext), Doubles, false);
        llvm::Function *F = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, Name, TheModule.get());

        unsigned idx = 0;
        for (auto &Arg : F->args()) {
            Arg.setName(Args[idx++]);
        }
        return F;
    }
};

class FunctionAST {
    std::unique_ptr<SignatureAST> signature;
    std::unique_ptr<ExprAST> body;

public:
    FunctionAST(std::unique_ptr<SignatureAST> signature, std::unique_ptr<ExprAST> body) :
                signature(std::move(signature)), body(std::move(body)) {}

    llvm::Function *codegen() {

        llvm::Function *TheFunction = TheModule->getFunction(signature->getName());
        if (!TheFunction)
            signature->codegen();
        
        if (!TheFunction)
            return nullptr;
        
        if (!TheFunction->empty()) 
            return (llvm::Function*)LogErrorV("llvm::Function cannot be redefined.");

        llvm::BasicBlock *BB = llvm::BasicBlock::Create(*TheContext, "entry", TheFunction);
        Builder->SetInsertPoint(BB);

        NamedValues.clear();
        for (auto &Arg : TheFunction->args()) 
            NamedValues[std::string(Arg.getName())] = &Arg;
        
        if (llvm::Value *RetVal = body->codegen()) {
            Builder->CreateRet(RetVal);

            llvm::verifyFunction(*TheFunction);
            return TheFunction;
        }
        TheFunction->eraseFromParent();
        return nullptr;
    }
};
} // end anynomous namespace
#endif