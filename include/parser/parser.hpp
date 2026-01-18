#ifndef PARSER_HPP
#define PARSER_HPP

#include <map>
#include <memory>
#include "ast/ast_base.hpp"

enum tokens {
    tok_eof = -1,
    tok_def = -2,
    tok_extern = -3,
    tok_identifier = -4,
    tok_number = -5,

};


static int NumVal;
static std::string IdentifierStr;

static int getTok() {
    static int LastChar = ' ';
    while (isspace(LastChar)) 
        LastChar = getchar();

    if (isalpha(LastChar)) {
        IdentifierStr = LastChar;
        while (isalnum((LastChar = getchar()))) {
            IdentifierStr += LastChar;
        }
        
        if (IdentifierStr == "def") 
        return tok_def;

        if (IdentifierStr == "extern") 
        return tok_extern;
        
        return tok_identifier;
    }
    

    if (isdigit(LastChar) || LastChar == '.') {
        std::string NumStr;
        do {
            NumStr += LastChar;
            LastChar = getchar();
        } while (isdigit(LastChar) || LastChar == '.');

        NumVal = strtod(NumStr.c_str(), nullptr);
        return tok_number;
    }

    if (LastChar == '#') {
        do
          LastChar = getchar();
        while (LastChar != EOF && LastChar != '\r' && LastChar != '\n');
        if (LastChar != EOF) {
            return getTok();
        }
    }

    if (LastChar == EOF) {
        return tok_eof;
    }
    
    int ThisChar = LastChar;
    LastChar = getchar();
    
    return ThisChar;

    
}

std::unique_ptr<ExprAST> LogError(const char *Str) {
  fprintf(stderr, "Error: %s\n", Str);
  return nullptr;
}
std::unique_ptr<SignatureAST> LogErrorP(const char *Str) {
  LogError(Str);
  return nullptr;
}

static int CurTok;
static int getNextToken() {
    return CurTok = getTok();
}
static std::map<char, int> BinOpPrecedence;

static int getTokPrecedence() {
    if (!isascii(CurTok)) {
        return -1;
    }

    int TokPrec = BinOpPrecedence[CurTok];
    if (TokPrec <= 0) return -1;
    return TokPrec;
}

static std::unique_ptr<ExprAST> ParseExpression();

static std::unique_ptr<ExprAST> parseNumExpr() {
    auto Result = std::make_unique<NumExprAST>(NumVal);
    getNextToken();
    return std::move(Result);
}

/// identifierexpr
///   ::= identifier
///   ::= identifier '(' expression* ')'
static std::unique_ptr<ExprAST> ParseIdentifierExpr() {
  std::string IdName = IdentifierStr;
  getNextToken();

  if (CurTok != '(') {
    return std::make_unique<VariableExprAST>(IdName);
  }

  getNextToken(); // eat '('
  std::vector<std::unique_ptr<ExprAST>> Args;

  if (CurTok != ')') {
    while (true)
    {
        if (auto Arg = ParseExpression()) {
            Args.push_back(std::move(Arg));
        } else {
            return nullptr;
        }

        if (CurTok == ')') {
            break;
        }

        if (CurTok != ',') {
            return LogError("Expected ',' between arguments");
        }
        getNextToken();
    }
  }

  // eat ")"
  getNextToken();

  return std::make_unique<CallExprAST>(IdName, std::move(Args));
}

static std::unique_ptr<ExprAST> parseParenExpr() {
    getNextToken();
    auto V = ParseExpression();
    if (!V) {
        return nullptr;
    }

    if (CurTok != ')') {
        return LogError("expected )");
    }
    getNextToken();
    return V;
}

static std::unique_ptr<ExprAST> ParsePrimary() {
    switch (CurTok) {
    default:
        return LogError("unknown token when expecting an expression");
    case tok_identifier:
        return ParseIdentifierExpr();
        
    case tok_number:
        return parseNumExpr();
    case '(':
        return parseParenExpr();
    }
}

std::unique_ptr<ExprAST> ParseExprRHS(int ExprPrec, std::unique_ptr<ExprAST> LHS) {
    while (true) {
        int TokPrec = getTokPrecedence();

        if (TokPrec < ExprPrec) 
            return LHS;

        int BinOp = CurTok;
        getNextToken(); // eat binOp
        auto RHS = ParsePrimary();
        if (!RHS) {
            return nullptr;
        }
        int NextTokPrec = getTokPrecedence();
        
        if (TokPrec < NextTokPrec) {
            RHS = ParseExprRHS(TokPrec + 1, std::move(RHS));
            if (!RHS) {
                return nullptr;
            }
        }
        
        LHS = std::make_unique<BinaryExprAST>(BinOp, std::move(LHS), std::move(RHS));
    }
}

static std::unique_ptr<ExprAST> ParseExpression() {
    auto LHS = ParsePrimary();
    if (!LHS) {
        return nullptr;
    }
    
    return ParseExprRHS(0, std::move(LHS));
}



static std::unique_ptr<SignatureAST> ParseSignature() {
    if (CurTok != tok_identifier) {
        return LogErrorP("Expecting a function name for the signature");
    }
    std::string fnName = IdentifierStr;
    getNextToken();

    if (CurTok != '(') {
        return LogErrorP("Expected ( containing the params for the signature");
    }

    std::vector<std::string> ArgNames;
    while (getNextToken() == tok_identifier) {

        ArgNames.push_back(IdentifierStr);
    }

    if (CurTok != ')') {
        return LogErrorP("Expected ')' in signature definition");
    }

    getNextToken(); // eat ")"

    return std::make_unique<SignatureAST>(fnName, std::move(ArgNames));

}

static std::unique_ptr<SignatureAST> ParseExtern() {
    getNextToken(); // eat extern;
    return ParseSignature();
}

static std::unique_ptr<FunctionAST> ParseTopLevelExpr() {
    if (auto E = ParseExpression()) {
        auto Signature = std::make_unique<SignatureAST>("", std::vector<std::string>());
        return std::make_unique<FunctionAST>(std::move(Signature), std::move(E));
    }
    return nullptr;
}

static std::unique_ptr<FunctionAST> ParseDefinition() {
    getNextToken(); //eat def
    auto Signature = ParseSignature();
    if (!Signature) {
        return nullptr;
    }

    if (auto E = ParseExpression())
        return std::make_unique<FunctionAST>(std::move(Signature), std::move(E));
    
    return nullptr;
}




#endif