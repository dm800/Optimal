#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Constants.h>
#include <llvm/Support/raw_ostream.h>

using namespace llvm;

int main() {
    LLVMContext ctx;

    Module mod("lab2_module", ctx);
    IRBuilder<> ir(ctx);

    Type* intType = Type::getInt32Ty(ctx);

    FunctionType* mainType = FunctionType::get(intType, false);

    Function* mainFunction = Function::Create(
        mainType,
        Function::ExternalLinkage,
        "main",
        mod
    );

    BasicBlock* entry = BasicBlock::Create(ctx, "entry", mainFunction);
    ir.SetInsertPoint(entry);

    Value* left = ConstantInt::get(intType, 353);
    Value* right = ConstantInt::get(intType, 48);

    Value* sum = ir.CreateAdd(left, right, "sum");

    ir.CreateRet(sum);

    mod.print(outs(), nullptr);

    return 0;
}