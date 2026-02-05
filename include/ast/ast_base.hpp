#ifndef AST_BASE_HPP
#define AST_BASE_HPP

#include <string>
#include <memory>
#include <vector>



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

class IfExprAST : public ExprAST {
    std::unique_ptr<ExprAST> Cond, Then, Else;
public:
    IfExprAST(std::unique_ptr<ExprAST> Cond, 
              std::unique_ptr<ExprAST> Then,
              std::unique_ptr<ExprAST> Else)
              : Cond(std::move(Cond)), Then(std::move(Then)), Else(std::move(Else)) {}

    IfExprAST(std::unique_ptr<ExprAST> Cond, 
              std::unique_ptr<ExprAST> Then)
              : Cond(std::move(Cond)), Then(std::move(Then)) {}
    llvm::Value *codegen() override;
};

class ForExprAST : public ExprAST {
    std::string VarName;
    std::unique_ptr<ExprAST> Start, End, Step, Body;

public:
    ForExprAST(const std::string &VarName, std::unique_ptr<ExprAST> Start,
            std::unique_ptr<ExprAST> End, 
            std::unique_ptr<ExprAST> Step, 
            std::unique_ptr<ExprAST> Body) :
            VarName(VarName), Start(std::move(Start)), End(std::move(End)), Body(std::move(Body)) {}
    
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
    const int getArgsSize() const { return Args.size(); }
    const std::vector<std::string>& getArgs() const { return Args; }



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