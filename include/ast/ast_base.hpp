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
    llvm::Value* codegen() override;
    
};

class VariableExprAST : public ExprAST {
    std::string name;
public:

    VariableExprAST(std::string name) : name(name) {}
    llvm::Value* codegen() override;
};

class BinaryExprAST : public ExprAST {
    char Op;
    std::unique_ptr<ExprAST> LHS, RHS;

public:
    BinaryExprAST(char Op, std::unique_ptr<ExprAST> LHS,
                  std::unique_ptr<ExprAST> RHS) : 
                  Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}
    llvm::Value* codegen() override;
};

class CallExprAST : public ExprAST {
    std::string Callee;
    std::vector<std::unique_ptr<ExprAST>> Args;
public:
    CallExprAST(const std::string& Callee, std::vector<std::unique_ptr<ExprAST>> Args) :
    Callee(Callee), Args(std::move(Args)) {}

    llvm::Value* codegen() override;
};

class SignatureAST {
    std::string Name;
    std::vector<std::string> Args;

public:
    SignatureAST(std::string Name, std::vector<std::string> Args) : Name(Name), Args(std::move(Args)) {}
    const std::string& getName() const { return Name; }

    llvm::Function *codegen();
};

class FunctionAST {
    std::unique_ptr<SignatureAST> signature;
    std::unique_ptr<ExprAST> body;

public:
    FunctionAST(std::unique_ptr<SignatureAST> signature, std::unique_ptr<ExprAST> body) :
                signature(std::move(signature)), body(std::move(body)) {}

    llvm::Function *codegen();
};
} // end anynomous namespace
#endif