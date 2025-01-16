#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/ValueSymbolTable.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/DerivedTypes.h"
#include <unordered_set>
using namespace llvm;
namespace {
	struct Hello : public FunctionPass {
		static char ID;
		std::unordered_set<Value*> mutated_values;
		Hello() : FunctionPass(ID) {
		}
		bool runOnFunction(Function &F) override {
			StringRef FunctionName = F.getName();

            // Identify session functions based on session structs
			if (!F.arg_empty()) {
				Type* firstParamType = F.arg_begin()->getType();
				if (!firstParamType->isPointerTy()) {
					return false;
				}
				PointerType* ptrType = cast<PointerType>(firstParamType);
				Type* pointeeType = ptrType->getElementType();
                // Session structs: mbedtls, openssl/libressl, guntls, matrixssl, wolfssl
				// if (!pointeeType->isStructTy() || pointeeType->getStructName() != "struct.mbedtls_ssl_context") {
				if (!pointeeType->isStructTy() || pointeeType->getStructName() != "struct.ssl") {
				// if (!pointeeType->isStructTy() || pointeeType->getStructName() != "struct.gnutls_session_int") {
				// if (!pointeeType->isStructTy() || pointeeType->getStructName() != "struct.ssl_st") {
				// if (!pointeeType->isStructTy() || pointeeType->getStructName() != "struct.WOLFSSL") {
					return false;
				}
				errs() << "[+]Function Name: " << FunctionName << "\n";
				// errs() << "[%]Parameter Type: " << *firstParamType << "\n";
			} else {
				return false;
			}

			LLVMContext &Ctx = F.getContext();

            // Assign ID to mutation point
			GlobalVariable *globalPointID = F.getParent()->getGlobalVariable("globalPointID");
			if (!globalPointID) {
				globalPointID = new GlobalVariable(*F.getParent(), Type::getInt32Ty(Ctx), false, GlobalValue::CommonLinkage, ConstantInt::get(Type::getInt32Ty(Ctx), 0), "globalPointID");
			}

			for (auto &BB : F) {
				for (auto &I : BB) {

                    // Instrument load instructions
					if (auto *op = dyn_cast<LoadInst>(&I)) {
						Type *loadedType = op->getType();
						if (loadedType->isIntegerTy(32)) {
							IRBuilder<> builder(op);
							Instruction *instruction = op->getNextNode();
							Value *oldPointID = builder.CreateAtomicRMW(AtomicRMWInst::Add, globalPointID, builder.getInt32(1), AtomicOrdering::SequentiallyConsistent);
							builder.SetInsertPoint(&BB, ++builder.GetInsertPoint());
							Value *arg = op->getOperand(0);
							if (mutated_values.count(arg) > 0) {
								// Value already mutated, skip it
								continue;
							}
							mutated_values.insert(arg);
							std::string instrStr;
							raw_string_ostream rso(instrStr);
							I.print(rso);
							// errs() << "[-]Find i32 load with id " << oldPointID << ", with value " << arg << "\n";
							Value *args[] = {
								dyn_cast_or_null<Value>(op),
								              oldPointID,
								              builder.CreateGlobalStringPtr(FunctionName),
								              builder.CreateGlobalStringPtr(rso.str())
							}
							;
							std::vector<Type *> paramTypes = {
								Type::getInt32Ty(Ctx),
								              Type::getInt32Ty(Ctx),
								              Type::getInt8PtrTy(Ctx),
								              Type::getInt8PtrTy(Ctx),
							}
							;
							Type *retType = Type::getInt32Ty(Ctx);
							FunctionType *mutateFuncType = FunctionType::get(retType, paramTypes, false);
							FunctionCallee mutateFunc;

                            // Instrument switch instructions
							if (StringRef(instruction->getOpcodeName()).equals("switch")) {
								// errs() << "[-]Find switch" << "\n";
								mutateFunc = F.getParent()->getOrInsertFunction("mutate_switch", mutateFuncType);
							} else {
								mutateFunc = F.getParent()->getOrInsertFunction("mutate_int", mutateFuncType);
							}
							// mutateFunc = F.getParent()->getOrInsertFunction("mutate_int", mutateFuncType);
							Value *mutatedValue = builder.CreateCall(mutateFunc, args);
							CallInst *CI = llvm::dyn_cast<llvm::CallInst>(mutatedValue);
							CI->setArgOperand(0, op);
							op->replaceAllUsesWith(mutatedValue);
							CI->setArgOperand(0, op);
						} else if (loadedType->isIntegerTy(64)) {
							IRBuilder<> builder(op);
							Value *oldPointID = builder.CreateAtomicRMW(AtomicRMWInst::Add, globalPointID, builder.getInt32(1), AtomicOrdering::SequentiallyConsistent);
							builder.SetInsertPoint(&BB, ++builder.GetInsertPoint());
							Value *arg = op->getOperand(0);
							if (mutated_values.count(arg) > 0) {
								// Value already mutated, skip it
								continue;
							}
							mutated_values.insert(arg);
							std::string instrStr;
							raw_string_ostream rso(instrStr);
							I.print(rso);
							// errs() << "[-]Find i64 load with id " << oldPointID << ", with value " << arg << "\n";
							Value *args[] = {
								dyn_cast_or_null<Value>(op),
								              oldPointID,
								              builder.CreateGlobalStringPtr(FunctionName),
								              builder.CreateGlobalStringPtr(rso.str())
							}
							;
							std::vector<Type *> paramTypes = {
								Type::getInt64Ty(Ctx),
								              Type::getInt32Ty(Ctx),
								              Type::getInt8PtrTy(Ctx),
								              Type::getInt8PtrTy(Ctx),
							}
							;
							Type *retType = Type::getInt64Ty(Ctx);
							FunctionType *mutateFuncType = FunctionType::get(retType, paramTypes, false);
							FunctionCallee mutateFunc = F.getParent()->getOrInsertFunction("mutate_int64", mutateFuncType);
							Value *mutatedValue = builder.CreateCall(mutateFunc, args);
							CallInst *CI = llvm::dyn_cast<llvm::CallInst>(mutatedValue);
							CI->setArgOperand(0, op);
							op->replaceAllUsesWith(mutatedValue);
							CI->setArgOperand(0, op);
						} else if (loadedType->isIntegerTy(8)) {
							IRBuilder<> builder(op);
							Value *oldPointID = builder.CreateAtomicRMW(AtomicRMWInst::Add, globalPointID, builder.getInt32(1), AtomicOrdering::SequentiallyConsistent);
							builder.SetInsertPoint(&BB, ++builder.GetInsertPoint());
							Value *arg = op->getOperand(0);
							if (mutated_values.count(arg) > 0) {
								// Value already mutated, skip it
								continue;
							}
							mutated_values.insert(arg);
							std::string instrStr;
							raw_string_ostream rso(instrStr);
							I.print(rso);
							// errs() << "[-]Find i8 load with id " << oldPointID << ", with value " << arg << "\n";
							Value *args[] = {
								dyn_cast_or_null<Value>(op),
								              oldPointID,
								              builder.CreateGlobalStringPtr(FunctionName),
								              builder.CreateGlobalStringPtr(rso.str())
							}
							;
							std::vector<Type *> paramTypes = {
								Type::getInt8Ty(Ctx),
								              Type::getInt32Ty(Ctx),
								              Type::getInt8PtrTy(Ctx),
								              Type::getInt8PtrTy(Ctx),
							}
							;
							Type *retType = Type::getInt8Ty(Ctx);
							FunctionType *mutateFuncType = FunctionType::get(retType, paramTypes, false);
							FunctionCallee mutateFunc = F.getParent()->getOrInsertFunction("mutate_int8", mutateFuncType);
							Value *mutatedValue = builder.CreateCall(mutateFunc, args);
							CallInst *CI = llvm::dyn_cast<llvm::CallInst>(mutatedValue);
							CI->setArgOperand(0, op);
							op->replaceAllUsesWith(mutatedValue);
							CI->setArgOperand(0, op);
						} else {
							continue;
						}
					}

                    // Instrument store instructions
					if (auto *op = dyn_cast<StoreInst>(&I)) {
						Type *storedType = op->getValueOperand()->getType();
						if (!storedType->isIntegerTy(32)) {
							continue;
						}
						IRBuilder<> builder(op);
						Value *oldPointID = builder.CreateAtomicRMW(AtomicRMWInst::Add, globalPointID, builder.getInt32(1), AtomicOrdering::SequentiallyConsistent);
						Value *originalValue = op->getValueOperand();
						Value *arg = op->getOperand(1);
						if (mutated_values.count(arg) > 0) {
							// Value already mutated, skip it
							continue;
						}
						mutated_values.insert(arg);
						std::string instrStr;
						raw_string_ostream rso(instrStr);
						I.print(rso);
						// errs() << "[-]Find constant store with id " << oldPointID << ", with value " << arg << "\n";
						Value *args[] = {
							originalValue,
							            oldPointID,
							            builder.CreateGlobalStringPtr(FunctionName),
							            builder.CreateGlobalStringPtr(rso.str())
						}
						;
						std::vector<Type *> paramTypes = {
							Type::getInt32Ty(Ctx),
							            Type::getInt32Ty(Ctx),
							            Type::getInt8PtrTy(Ctx),
							            Type::getInt8PtrTy(Ctx),
						}
						;
						Type *retType = Type::getInt32Ty(Ctx);
						FunctionType *mutateFuncType = FunctionType::get(retType, paramTypes, false);
						FunctionCallee mutateFunc = F.getParent()->getOrInsertFunction("mutate_int", mutateFuncType);
						Value *mutatedValue = builder.CreateCall(mutateFunc, args);
						op->setOperand(0, mutatedValue);
					}

                    // Instrument cmp instructions
					if (auto *cmp = dyn_cast<ICmpInst>(&I)) {
						if (!cmp || !cmp->getType() || !cmp->getType()->isIntegerTy(1)) {
							continue;
						}
						// Value *operand0 = cmp->getOperand(0);
						// Value *operand1 = cmp->getOperand(1);
						// if (!operand0->getType()->isIntegerTy() || !operand1->getType()->isIntegerTy()) {
						//   continue;
						// }
						Value *arg = cmp->getOperand(0);
						if (mutated_values.count(arg) > 0) {
							// Value already mutated, skip it
							continue;
						}
						mutated_values.insert(arg);
						IRBuilder<> builder(cmp);
						Value *oldPointID = builder.CreateAtomicRMW(AtomicRMWInst::Add, globalPointID, builder.getInt32(1), AtomicOrdering::SequentiallyConsistent);
						builder.SetInsertPoint(&BB, ++builder.GetInsertPoint());
						// errs() << "[-]Find cmp with id " << oldPointID << "\n";
						std::string instrStr;
						raw_string_ostream rso(instrStr);
						I.print(rso);
						Value *args[] = {
							dyn_cast_or_null<Value>(cmp),
							            oldPointID,
							            builder.CreateGlobalStringPtr(FunctionName),
							            builder.CreateGlobalStringPtr(rso.str())
						}
						;
						std::vector<Type *> paramTypes = {
							Type::getInt1Ty(Ctx),
							            Type::getInt32Ty(Ctx),
							            Type::getInt8PtrTy(Ctx),
							            Type::getInt8PtrTy(Ctx),
						}
						;
						Type *retType = Type::getInt1Ty(Ctx);
						FunctionType *mutateFuncType = FunctionType::get(retType, paramTypes, false);
						FunctionCallee mutateFunc = F.getParent()->getOrInsertFunction("mutate_cmp", mutateFuncType);
						Value *mutatedValue = builder.CreateCall(mutateFunc, args);
						CallInst *CI = llvm::dyn_cast<llvm::CallInst>(mutatedValue);
						// CI->setArgOperand(0, cmp);
						cmp->replaceAllUsesWith(mutatedValue);
						CI->setArgOperand(0, cmp);
					}
				}
			}
			return true;
		}
	}
	;
}
char Hello::ID = 0;
// Register for opt
static RegisterPass<Hello> X("hello", "Hello World Pass");
// Register for clang
static RegisterStandardPasses Y(PassManagerBuilder::EP_EarlyAsPossible,
  [](const PassManagerBuilder &Builder, legacy::PassManagerBase &PM) {
	PM.add(new Hello());
}
);