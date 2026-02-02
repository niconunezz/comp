#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <iostream>
#include "llvm/IR/Value.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Verifier.h"
#include "llvm/ADT/APFloat.h"
#include <chrono>
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/StandardInstrumentations.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include "llvm/Transforms/Scalar/Reassociate.h"
#include "llvm/Transforms/Scalar/SimplifyCFG.h"
#include "llvm/Support/CommandLine.h"

#include "parser/parser.hpp"
#include "KaleidoscopeJIT.h"
#include "llvm/Support/TargetSelect.h"

#define DLLEXPORT 
#define DEBUG_TYPE "main"

using namespace llvm;
using namespace llvm::orc;

std::unique_ptr<LLVMContext> TheContext;
std::unique_ptr<IRBuilder<>> Builder;
std::unique_ptr<Module> TheModule;
std::map<std::string, Value*> NamedValues;

static std::unique_ptr<KaleidoscopeJIT> TheJIT;
static std::unique_ptr<FunctionPassManager> TheFPM;
static std::unique_ptr<LoopAnalysisManager> TheLAM;
static std::unique_ptr<FunctionAnalysisManager> TheFAM;
static std::unique_ptr<CGSCCAnalysisManager> TheCGAM;
static std::unique_ptr<ModuleAnalysisManager> TheMAM;
static std::unique_ptr<PassInstrumentationCallbacks> ThePIC;
static std::unique_ptr<StandardInstrumentations> TheSI;
static std::map<std::string, std::unique_ptr<SignatureAST>> FunctionProtos;
static ExitOnError ExitOnErr;


extern "C" DLLEXPORT double putchard(double X) {
  fputc((char)X, stderr);
  return 0;
}

extern "C" DLLEXPORT double printstar(double n) {
    std::string out = "\n";
    for (unsigned i = 0; i < n; i++) 
        out = out + "*";
    
    out = out + "\n";
    fprintf(stderr, "%s", out.c_str());
    return 0;
}

Function *getFunction(std::string Name) {
    if (auto *F = TheModule->getFunction(Name)) 
        return F;

    auto FI = FunctionProtos.find(Name);
    if (FI != FunctionProtos.end())
        return FI->second->codegen();
    
    return nullptr;
}


void InitializeModuleAndPassManagers(void) {
    TheContext = std::make_unique<LLVMContext>();
    TheModule = std::make_unique<Module>("my cool jit", *TheContext);
    TheModule->setDataLayout(TheJIT->getDataLayout());
    Builder = std::make_unique<IRBuilder<>>(*TheContext);

    // 1. Inicializar Instrumentación PRIMERO
    ThePIC = std::make_unique<PassInstrumentationCallbacks>();
    TheSI = std::make_unique<StandardInstrumentations>(/*DebugLogging*/ true);
    
    // 2. Inicializar los Managers (especialmente FAM) para poder registrarlo
    TheFAM = std::make_unique<FunctionAnalysisManager>();
    TheMAM = std::make_unique<ModuleAnalysisManager>();
    TheLAM = std::make_unique<LoopAnalysisManager>();
    TheCGAM = std::make_unique<CGSCCAnalysisManager>();

    // 3. Registrar callbacks ANTES de crear el PassBuilder
    // TheSI->registerCallbacks(*ThePIC, TheFAM.get()); 

    // 4. Crear PassBuilder con el PIC ya configurado
    PassBuilder PB(nullptr, PipelineTuningOptions(), None, ThePIC.get());

    // 5. Registro normal de análisis
    PB.registerModuleAnalyses(*TheMAM);
    PB.registerFunctionAnalyses(*TheFAM);
    PB.crossRegisterProxies(*TheLAM, *TheFAM, *TheCGAM, *TheMAM);

    // 6. Configurar el FunctionPassManager
    TheFPM = std::make_unique<FunctionPassManager>();
    TheFPM->addPass(InstCombinePass());
    TheFPM->addPass(ReassociatePass());
    TheFPM->addPass(GVNPass());
    TheFPM->addPass(SimplifyCFGPass());
}



Value* NumExprAST::codegen() {
    assert(TheContext && "TheContext is null");
  return ConstantFP::get(*TheContext, APFloat(Val));
}

Value* VariableExprAST::codegen() {
    Value* V = NamedValues[name];
    if (!V) {
        LogErrorV("Unkown variable name");
    }
    return V;
}


Value* BinaryExprAST::codegen() {
        // std::cout << "got here \n";
        Value* L = LHS->codegen();
        // std::cout << "got here \n";
        Value* R = RHS->codegen();
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
            L = Builder->CreateFCmpULT(L, R, "lttmp");
            return Builder->CreateUIToFP(L, Type::getDoubleTy(*TheContext), "booltmp");
        case '~':
            L = Builder->CreateFCmpOEQ(L, R, "eqtmp");
            return Builder->CreateUIToFP(L, Type::getDoubleTy(*TheContext), "booltmp");
        default:
            return LogErrorV("invalid binop operator");
        }

}

Value* CallExprAST::codegen() {
        Function* CalleeF = getFunction(Callee);
        if (!CalleeF) 
            LogErrorV("Unkown function");
        
        if (CalleeF->arg_size() != Args.size())
            return LogErrorV("Incorrect # of arguments");
        
        std::vector<Value *> ArgsV;
        for (int i = 0; i < Args.size(); i++) {
            ArgsV.push_back(Args[i]->codegen());
            if (!ArgsV.back()) {
                return nullptr;
            }
        } 

        return Builder->CreateCall(CalleeF, ArgsV, "calltmp");
    
    }

Function *SignatureAST::codegen() {
        std::vector<Type*> Doubles(Args.size(), Type::getDoubleTy(*TheContext));
        FunctionType *FT = FunctionType::get(Type::getDoubleTy(*TheContext), Doubles, false);
        Function *F = Function::Create(FT, Function::ExternalLinkage, Name, TheModule.get());
        // fprintf(stderr, "1.3!\n");

        unsigned idx = 0;
        for (auto &Arg : F->args()) {
            Arg.setName(Args[idx++]);
        }
        // fprintf(stderr, "1.4!\n");

        return F;
}

Function *FunctionAST::codegen() {
        LLVM_DEBUG(dbgs() << "codegenning!" << "\n");
        auto &P = *signature;
        FunctionProtos[signature->getName()] = std::move(signature);
        Function *TheFunction = getFunction(P.getName());
        
        // if (!TheFunction)
        //     return nullptr;
            
        if (TheFunction) {
            LLVM_DEBUG(dbgs() << "found function!" << "\n");
            if (TheFunction->arg_size() != (P.getArgsSize())) {
                return (Function*)LogErrorV("Function definition has different num of arguments than expected.");
            } else {
                LLVM_DEBUG(dbgs() << "redefining names!" << "\n");
                unsigned idx = 0;
                auto& newArgsNames = P.getArgs();
                for (auto &Arg : TheFunction->args()) {
                    Arg.setName(newArgsNames[idx++]);
                }
            }
        } else {
            LLVM_DEBUG(dbgs() << "found signature!" << "\n");
            TheFunction = P.codegen();
        }
                
        LLVM_DEBUG(dbgs() << "function created!" << "\n");
        
        if (!TheFunction)
            return nullptr;

        LLVM_DEBUG(dbgs() << "function really created!" << "\n");


        
        if (!TheFunction->empty()) 
            return (Function*)LogErrorV("Function re-definition is not allowed.");
        

        BasicBlock *BB = BasicBlock::Create(*TheContext, "entry", TheFunction);
        Builder->SetInsertPoint(BB);
        NamedValues.clear();
        for (auto &Arg : TheFunction->args()) 
            NamedValues[std::string(Arg.getName())] = &Arg;
        
        if (Value *RetVal = body->codegen()) {
            
            Builder->CreateRet(RetVal);
            verifyFunction(*TheFunction);

            TheFPM->run(*TheFunction, *TheFAM);
            return TheFunction;
        }
        TheFunction->eraseFromParent();
        return nullptr;
}


Value* IfExprAST::codegen() {
    Value* CondV = Cond->codegen();
    if (!CondV)
        return nullptr;
    
    CondV = Builder->CreateFCmpONE(
        CondV, ConstantFP::get(*TheContext, APFloat(0.0))
    );

    Function* TheFunction = Builder->GetInsertBlock()->getParent();
    BasicBlock* ThenBB = BasicBlock::Create(*TheContext, "then", TheFunction);
    BasicBlock* ElseBB = BasicBlock::Create(*TheContext, "else", TheFunction);
    BasicBlock* MergeBB = BasicBlock::Create(*TheContext, "ifcont", TheFunction);
    
    Builder->CreateCondBr(CondV, ThenBB, ElseBB);
    Builder->SetInsertPoint(ThenBB);
    Value* ThenV = Then->codegen();
    if (!ThenV) 
        return nullptr;
    
    Builder->CreateBr(MergeBB);
    ThenBB = Builder->GetInsertBlock();

    Builder->SetInsertPoint(ElseBB);
    Value* ElseV = Else->codegen();
    if (!ElseV)
        return nullptr;

    Builder->CreateBr(MergeBB);
    ElseBB = Builder->GetInsertBlock();

    Builder->SetInsertPoint(MergeBB);
    PHINode* PN = Builder->CreatePHI(Type::getDoubleTy(*TheContext), 2, "iftmp");

    PN->addIncoming(ThenV, ThenBB);
    PN->addIncoming(ElseV, ElseBB);

    return PN;
}




static void HandleDefinition() {
    if (auto FnAST = ParseDefinition()) {
        LLVM_DEBUG(dbgs() << "Parsed definition correctly!" << "\n");
        if (auto *FnIr = FnAST->codegen()) {
            LLVM_DEBUG(dbgs() << "Read function definition!" << "\n");
            FnIr->print(errs());
            fprintf(stderr, "\n");
            ExitOnErr(TheJIT->addModule(ThreadSafeModule(std::move(TheModule), std::move(TheContext))));
            InitializeModuleAndPassManagers();
        }

    } else {
        getNextToken();
    }
}

static void HandleExtern() {
    if (auto SnAST = ParseExtern()) {
        LLVM_DEBUG(dbgs() << "Parsed extern correctly!" << "\n");

        if (auto *SnIR = SnAST->codegen()) {
            LLVM_DEBUG(dbgs() << "Read extern correctly!" << "\n");
            SnIR->print(errs());
            fprintf(stderr, "\n");
            FunctionProtos[SnAST->getName()] = std::move(SnAST);
        }

    } else {
        getNextToken();
    }
}


static void HandleTopLevel() {
    if (auto FnAST = ParseTopLevelExpr()) {
        LLVM_DEBUG(dbgs() << "Parsed TopLevelExprs correctly" << "\n");
        auto startCod = std::chrono::high_resolution_clock::now();
        if (auto FnIr = FnAST->codegen()) {
            auto endCod = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::milli> durationCod = endCod - startCod;
            fprintf(stderr, "Codegen in %f\n", durationCod.count());

            FnIr->print(errs());
            auto startOpt = std::chrono::high_resolution_clock::now();
            auto RT = TheJIT->getMainJITDylib().createResourceTracker();
            auto TSM = ThreadSafeModule(std::move(TheModule), std::move(TheContext));
            ExitOnErr(TheJIT->addModule(std::move(TSM), RT));
            auto endOpt = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::milli> duration = endOpt - startOpt;

            InitializeModuleAndPassManagers();

            auto ExprSymbol = ExitOnErr(TheJIT->lookup("__anon_expr"));
            double (*FP)() = (double (*)())(intptr_t)ExprSymbol.getAddress();

            fprintf(stderr, "Evaluated to %f in %f\n", FP(), duration.count());
            
            ExitOnErr(RT->remove());
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

int main(int argc, char **argv) {
    llvm::DebugFlag = false; // Esto equivale a haber pasado -debug por terminal

    std::unique_ptr<LLVMContext> TheContext;
    std::unique_ptr<IRBuilder<>> Builder;
    std::unique_ptr<Module> TheModule;
    InitializeNativeTarget();
    InitializeNativeTargetAsmPrinter();
    InitializeNativeTargetAsmParser();

    LLVM_DEBUG(dbgs() << "targets initialized!" << "\n");

    BinOpPrecedence['<'] = 10;
    BinOpPrecedence['~'] = 10;
    BinOpPrecedence['+'] = 20;
    BinOpPrecedence['-'] = 20;
    BinOpPrecedence['*'] = 40; // highest.
    
    fprintf(stderr, "ready> ");
    getNextToken();


    auto EPC = llvm::orc::SelfExecutorProcessControl::Create();
    if (!EPC) { /* Manejar error */ }
    LLVM_DEBUG(dbgs() << "EPC created" << "\n");
    auto ES = std::make_unique<llvm::orc::ExecutionSession>(std::move(*EPC));
    auto JTMB = llvm::orc::JITTargetMachineBuilder(ES->getExecutorProcessControl().getTargetTriple());
    auto DL = JTMB.getDefaultDataLayoutForTarget();
    if (!DL) { /* Manejar error */ }

    LLVM_DEBUG(dbgs() << "DL created" << "\n");

    
    TheJIT = std::make_unique<llvm::orc::KaleidoscopeJIT>(
        std::move(ES), 
        std::move(JTMB), 
        std::move(*DL)
    );

    TheJIT->getMainJITDylib().addGenerator(
    ExitOnErr(llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(
        TheJIT->getDataLayout().getGlobalPrefix())));

    LLVM_DEBUG(dbgs() << "jit created" << "\n");
    InitializeModuleAndPassManagers();
    LLVM_DEBUG(dbgs() << "Module initialized" << "\n");
    
    MainLoop();

    return 0;
}