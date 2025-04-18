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
    bool found = false;
    for (auto &BB : F) {
      for (auto &I : BB) {
        auto *callInst = dyn_cast<CallInst>(&I);

        if (!callInst)
          continue;

        for (unsigned i = 0; i < callInst->arg_size(); ++i) {
          Value *arg = callInst->getArgOperand(i);
          if (auto *cint = dyn_cast<ConstantInt>(arg)) {
            if (cint->getZExtValue() == 43) { // supported_versions
              errs() << "[+] Found call with arg = 43 in function: " 
                     << F.getName() << "\n  ";
              I.print(errs());
              errs() << "\n";
              found = true;
              break;
            }

            if (cint->getZExtValue() == 771) { // legacy_version
              errs() << "[+] Found call with arg = 771 in function: " 
                     << F.getName() << "\n  ";
              I.print(errs());
              errs() << "\n";
              found = true;
              break;
            }
          }
        }
      }
    }
    return found; 
  }
};

} // namespace

char Hello::ID = 0;

// Register for opt
static RegisterPass<Hello> X("hello", "Hello World Pass");

// Register for clang
static RegisterStandardPasses Y(PassManagerBuilder::EP_EarlyAsPossible,
  [](const PassManagerBuilder &Builder, legacy::PassManagerBase &PM) {
    PM.add(new Hello());
  });
