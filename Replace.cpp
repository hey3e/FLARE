#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

using namespace llvm;

namespace {

struct Hello : public FunctionPass {
  static char ID;
  Hello() : FunctionPass(ID) {}

  bool runOnFunction(Function &F) override {

    //if (!F.getName().startswith("tls_construct"))

      //return false;

    bool modified = false;
    Module *M = F.getParent();
    LLVMContext &Ctx = F.getContext();

    GlobalVariable *globalPointID = M->getGlobalVariable("globalPointID");

    if (!globalPointID) {
      globalPointID = new GlobalVariable(
          *M, Type::getInt32Ty(Ctx), false, GlobalValue::CommonLinkage,
          ConstantInt::get(Type::getInt32Ty(Ctx), 0),
          "globalPointID");
    }

    Type *int32Ty = Type::getInt32Ty(Ctx);
    Type *int8PtrTy = Type::getInt8PtrTy(Ctx);
    std::vector<Type*> replaceArgTys = {int32Ty, int32Ty, int8PtrTy, int8PtrTy};
    FunctionType *replaceFuncTy = FunctionType::get(int32Ty, replaceArgTys, false);
    FunctionCallee replaceFunc = M->getOrInsertFunction("replace", replaceFuncTy);

    for (auto &BB : F) {
      for (auto &I : BB) {
        auto *callInst = dyn_cast<CallInst>(&I);
        if (!callInst)
          continue;
        
        Function *callee = callInst->getCalledFunction();
        if (!callee)
          continue;

        // call.cpp得到的函数
        if (callee->getName() == "WPACKET_put_bytes__") {
          if (callInst->arg_size() < 2)
            continue;
          Value *secondArg = callInst->getArgOperand(1);

          if (!secondArg->getType()->isIntegerTy(32))
            continue;
          
          IRBuilder<> builder(callInst);

          Value *oldPointID = builder.CreateAtomicRMW(
              AtomicRMWInst::Add,
              globalPointID,
              builder.getInt32(1),
              AtomicOrdering::SequentiallyConsistent);

          Value *fnNameVal = builder.CreateGlobalStringPtr(F.getName());
          std::string instStr;
          {
            raw_string_ostream rso(instStr);
            I.print(rso);
          }

          Value *instStrVal = builder.CreateGlobalStringPtr(instStr);

          std::vector<Value*> rArgs;
          rArgs.push_back(secondArg);   // oldVal
          rArgs.push_back(oldPointID);  // siteID
          rArgs.push_back(fnNameVal);   // fnName
          rArgs.push_back(instStrVal);  // instStr

          CallInst *repCall = builder.CreateCall(replaceFunc, rArgs);

          callInst->setArgOperand(1, repCall);

          errs() << "[InstrWpacketReplace] Instrumented call in " << F.getName()
                 << ": " << callee->getName() << "\n";

          modified = true;
        }
      }
    }

    return modified;
  }
};

} // end anonymous namespace

char Hello::ID = 0;

// Register for opt
static RegisterPass<Hello> X("hello", "Hello World Pass");

// Register for clang
static RegisterStandardPasses Y(PassManagerBuilder::EP_EarlyAsPossible,
  [](const PassManagerBuilder &Builder, legacy::PassManagerBase &PM) {
    PM.add(new Hello());
  });
