#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <string>
#include <map>

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>

using namespace std;
using namespace llvm;

#define TOK_EOF 1
#define TOK_IDENT 2
#define TOK_NUM 3
#define TOK_IF 6
#define TOK_ELSE 7
#define TOK_THEN 8
#define TOK_WHILE 9
#define TOK_DO 10
#define TOK_END 11
#define TOK_RETURN 12

static string IdentifierStr;
static int NumVal;
static int CurToken;

static int GetToken() {
    static int posCh = ' ';

    while (isspace(posCh))
        posCh = getchar();

    if (isalpha(posCh)) {
        IdentifierStr = posCh;

        while (isalnum((posCh = getchar())))
            IdentifierStr += posCh;

        if (IdentifierStr == "if")
            return TOK_IF;
        if (IdentifierStr == "else")
            return TOK_ELSE;
        if (IdentifierStr == "then")
            return TOK_THEN;
        if (IdentifierStr == "while")
            return TOK_WHILE;
        if (IdentifierStr == "do")
            return TOK_DO;
        if (IdentifierStr == "end")
            return TOK_END;
        if (IdentifierStr == "return")
            return TOK_RETURN;

        return TOK_IDENT;
    }

    if (isdigit(posCh)) {
        string NumStr;

        do {
            NumStr += posCh;
            posCh = getchar();
        } while (isdigit(posCh));

        NumVal = atoi(NumStr.c_str());
        return TOK_NUM;
    }

    if (posCh == EOF)
        return TOK_EOF;

    int thisChar = posCh;
    posCh = getchar();

    return thisChar;
}

static void NextToken() {
    CurToken = GetToken();
}

class ExprAST {
public:
    virtual ~ExprAST() {}
    virtual Value *codegen() = 0;
};

class NumberExprAST : public ExprAST {
    int Val;

public:
    NumberExprAST(int Val) : Val(Val) {}
    Value *codegen() override;
};

class VariableExprAST : public ExprAST {
    string Name;

public:
    VariableExprAST(const string &Name) : Name(Name) {}
    Value *codegen() override;
};

class AssignAST : public ExprAST {
    string Name;
    ExprAST *AssExpr;

public:
    AssignAST(const string &Name, ExprAST *AssExpr)
        : Name(Name), AssExpr(AssExpr) {}

    Value *codegen() override;
};

class BinaryExprAST : public ExprAST {
    char Op;
    ExprAST *LHS;
    ExprAST *RHS;

public:
    BinaryExprAST(char Op, ExprAST *LHS, ExprAST *RHS)
        : Op(Op), LHS(LHS), RHS(RHS) {}

    Value *codegen() override;
};

class IfExprAST : public ExprAST {
    ExprAST *Cond;
    ExprAST *Then;
    ExprAST *Else;

public:
    IfExprAST(ExprAST *Cond, ExprAST *Then, ExprAST *Else)
        : Cond(Cond), Then(Then), Else(Else) {}

    Value *codegen() override;
};

class WhileExprAST : public ExprAST {
    ExprAST *Cond;
    ExprAST *Body;

public:
    WhileExprAST(ExprAST *Cond, ExprAST *Body)
        : Cond(Cond), Body(Body) {}

    Value *codegen() override;
};

class ReturnExprAST : public ExprAST {
    ExprAST *Expr;

public:
    ReturnExprAST(ExprAST *Expr) : Expr(Expr) {}
    Value *codegen() override;
};

static LLVMContext MainContext;
static IRBuilder<> Builder(MainContext);
static Module *MainModule;
static Function *MainFunction;
static map<string, AllocaInst *> NamedValues;

static ExprAST *ParseExpression();

static AllocaInst *CreateAlloca(const string &Name) {
    IRBuilder<> TmpB(
        &MainFunction->getEntryBlock(),
        MainFunction->getEntryBlock().begin()
    );

    return TmpB.CreateAlloca(
        Type::getInt32Ty(MainContext),
        nullptr,
        Name.c_str()
    );
}

Value *NumberExprAST::codegen() {
    return ConstantInt::get(MainContext, APInt(32, Val, true));
}

Value *VariableExprAST::codegen() {
    AllocaInst *Alloca = NamedValues[Name];

    if (!Alloca) {
        printf("unknown variable name: %s\n", Name.c_str());
        return nullptr;
    }

    return Builder.CreateLoad(
        Type::getInt32Ty(MainContext),
        Alloca,
        Name.c_str()
    );
}

Value *AssignAST::codegen() {
    Value *V = AssExpr->codegen();

    if (!V)
        return nullptr;

    AllocaInst *Alloca = NamedValues[Name];

    if (!Alloca) {
        Alloca = CreateAlloca(Name);
        NamedValues[Name] = Alloca;
    }

    Builder.CreateStore(V, Alloca);

    return V;
}

Value *BinaryExprAST::codegen() {
    Value *L = LHS->codegen();
    Value *R = RHS->codegen();

    if (!L || !R)
        return nullptr;

    switch (Op) {
        case '+':
            return Builder.CreateAdd(L, R, "addtmp");
        case '-':
            return Builder.CreateSub(L, R, "subtmp");
        case '*':
            return Builder.CreateMul(L, R, "multmp");
        default:
            printf("invalid binary operator\n");
            return nullptr;
    }
}

Value *IfExprAST::codegen() {
    Value *CondV = Cond->codegen();

    if (!CondV)
        return nullptr;

    CondV = Builder.CreateICmpNE(
        CondV,
        ConstantInt::get(MainContext, APInt(32, 0, true)),
        "ifcond"
    );

    Function *F = Builder.GetInsertBlock()->getParent();

    BasicBlock *ThenBB = BasicBlock::Create(MainContext, "then", F);
    BasicBlock *ElseBB = BasicBlock::Create(MainContext, "else");
    BasicBlock *MergeBB = BasicBlock::Create(MainContext, "ifcont");

    Builder.CreateCondBr(CondV, ThenBB, ElseBB);

    Builder.SetInsertPoint(ThenBB);

    Value *ThenV = Then->codegen();

    if (!ThenV)
        return nullptr;

    if (!Builder.GetInsertBlock()->getTerminator())
        Builder.CreateBr(MergeBB);

    ElseBB->insertInto(F);
    Builder.SetInsertPoint(ElseBB);

    Value *ElseV = Else->codegen();

    if (!ElseV)
        return nullptr;

    if (!Builder.GetInsertBlock()->getTerminator())
        Builder.CreateBr(MergeBB);

    MergeBB->insertInto(F);
    Builder.SetInsertPoint(MergeBB);

    return ConstantInt::get(MainContext, APInt(32, 0, true));
}

Value *WhileExprAST::codegen() {
    Function *F = Builder.GetInsertBlock()->getParent();

    BasicBlock *CondBB = BasicBlock::Create(MainContext, "whilecond", F);
    BasicBlock *BodyBB = BasicBlock::Create(MainContext, "whilebody");
    BasicBlock *AfterBB = BasicBlock::Create(MainContext, "whilecont");

    Builder.CreateBr(CondBB);

    Builder.SetInsertPoint(CondBB);

    Value *CondV = Cond->codegen();

    if (!CondV)
        return nullptr;

    CondV = Builder.CreateICmpNE(
        CondV,
        ConstantInt::get(MainContext, APInt(32, 0, true)),
        "whilecondtmp"
    );

    Builder.CreateCondBr(CondV, BodyBB, AfterBB);

    BodyBB->insertInto(F);
    Builder.SetInsertPoint(BodyBB);

    Value *BodyV = Body->codegen();

    if (!BodyV)
        return nullptr;

    if (!Builder.GetInsertBlock()->getTerminator())
        Builder.CreateBr(CondBB);

    AfterBB->insertInto(F);
    Builder.SetInsertPoint(AfterBB);

    return ConstantInt::get(MainContext, APInt(32, 0, true));
}

Value *ReturnExprAST::codegen() {
    Value *V = Expr->codegen();

    if (!V)
        return nullptr;

    Builder.CreateRet(V);

    return V;
}

static ExprAST *ParseNumberExpr() {
    ExprAST *Result = new NumberExprAST(NumVal);
    NextToken();
    return Result;
}

static ExprAST *ParseParenExpr() {
    NextToken();

    ExprAST *V = ParseExpression();

    if (!V)
        return nullptr;

    if (CurToken != ')') {
        printf("expected ')'\n");
        return nullptr;
    }

    NextToken();

    return V;
}

static ExprAST *ParseVarExpr() {
    string Name = IdentifierStr;

    NextToken();

    if (CurToken != '=')
        return new VariableExprAST(Name);

    NextToken();

    ExprAST *Expr = ParseExpression();

    if (!Expr) {
        printf("expected expression after '='\n");
        return nullptr;
    }

    return new AssignAST(Name, Expr);
}

static ExprAST *ParseIfExpr() {
    NextToken();

    if (CurToken != '(') {
        printf("expected '(' after if\n");
        return nullptr;
    }

    NextToken();

    ExprAST *Cond = ParseExpression();

    if (!Cond)
        return nullptr;

    if (CurToken != ')') {
        printf("expected ')' after if condition\n");
        return nullptr;
    }

    NextToken();

    if (CurToken != TOK_THEN) {
        printf("expected then after if condition\n");
        return nullptr;
    }

    NextToken();

    ExprAST *Then = ParseExpression();

    if (!Then)
        return nullptr;

    if (CurToken != TOK_ELSE) {
        printf("expected else after then branch\n");
        return nullptr;
    }

    NextToken();

    ExprAST *Else = ParseExpression();

    if (!Else)
        return nullptr;

    if (CurToken != TOK_END) {
        printf("expected end after else branch\n");
        return nullptr;
    }

    NextToken();

    return new IfExprAST(Cond, Then, Else);
}

static ExprAST *ParseWhileExpr() {
    NextToken();

    if (CurToken != '(') {
        printf("expected '(' after while\n");
        return nullptr;
    }

    NextToken();

    ExprAST *Cond = ParseExpression();

    if (!Cond)
        return nullptr;

    if (CurToken != ')') {
        printf("expected ')' after while condition\n");
        return nullptr;
    }

    NextToken();

    if (CurToken != TOK_DO) {
        printf("expected do after while condition\n");
        return nullptr;
    }

    NextToken();

    ExprAST *Body = ParseExpression();

    if (!Body)
        return nullptr;

    if (CurToken != TOK_END) {
        printf("expected end after while body\n");
        return nullptr;
    }

    NextToken();

    return new WhileExprAST(Cond, Body);
}

static ExprAST *ParseReturnExpr() {
    NextToken();

    ExprAST *Expr = ParseExpression();

    if (!Expr) {
        printf("expected expression after return\n");
        return nullptr;
    }

    return new ReturnExprAST(Expr);
}

static ExprAST *ParsePrimary() {
    switch (CurToken) {
        case TOK_IDENT:
            return ParseVarExpr();
        case TOK_NUM:
            return ParseNumberExpr();
        case '(':
            return ParseParenExpr();
        case TOK_IF:
            return ParseIfExpr();
        case TOK_WHILE:
            return ParseWhileExpr();
        case TOK_RETURN:
            return ParseReturnExpr();
        default:
            printf("unknown token when expecting an expression: %d\n", CurToken);
            return nullptr;
    }
}

static int GetTokPrecedence() {
    switch (CurToken) {
        case '+':
        case '-':
            return 20;
        case '*':
            return 40;
        default:
            return -1;
    }
}

static ExprAST *ParseBinOpRHS(int ExprPrec, ExprAST *LHS) {
    while (true) {
        int TokPrec = GetTokPrecedence();

        if (TokPrec < ExprPrec)
            return LHS;

        int BinOp = CurToken;
        NextToken();

        ExprAST *RHS = ParsePrimary();

        if (!RHS)
            return nullptr;

        int NextPrec = GetTokPrecedence();

        if (TokPrec < NextPrec) {
            RHS = ParseBinOpRHS(TokPrec + 1, RHS);

            if (!RHS)
                return nullptr;
        }

        LHS = new BinaryExprAST(BinOp, LHS, RHS);
    }
}

static ExprAST *ParseExpression() {
    ExprAST *LHS = ParsePrimary();

    if (!LHS)
        return nullptr;

    return ParseBinOpRHS(0, LHS);
}

static Value *Parse() {
    NextToken();

    Value *LastValue = nullptr;

    while (CurToken != TOK_EOF) {
        ExprAST *Expr = ParseExpression();

        if (!Expr)
            return nullptr;

        LastValue = Expr->codegen();

        if (!LastValue)
            return nullptr;

        if (Builder.GetInsertBlock()->getTerminator())
            break;
    }

    return LastValue;
}

int main() {
    MainModule = new Module("main", MainContext);

    FunctionType *FT = FunctionType::get(
        Type::getInt32Ty(MainContext),
        false
    );

    MainFunction = Function::Create(
        FT,
        Function::ExternalLinkage,
        "main",
        MainModule
    );

    BasicBlock *BB = BasicBlock::Create(MainContext, "entry", MainFunction);
    Builder.SetInsertPoint(BB);

    Value *Ret = Parse();

    if (!Ret)
        return 1;

    if (!Builder.GetInsertBlock()->getTerminator()) {
        Builder.CreateRet(
            ConstantInt::get(MainContext, APInt(32, 0, true))
        );
    }

    verifyFunction(*MainFunction);
    MainModule->print(outs(), nullptr);

    delete MainModule;

    return 0;
}