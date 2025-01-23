#include "backend.h"

#include <stdlib.h>
#include "llvm/IR/Type.h"
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>


struct codegen_state_t {
    llvm::LLVMContext context;
    llvm::Module      module;
    llvm::IRBuilder<> builder;
};

void init_codegen(void) {
    LLVMInitializeX86TargetInfo();
    LLVMInitializeX86Target();
    LLVMInitializeX86TargetMC();
    LLVMInitializeX86AsmParser();
    LLVMInitializeX86AsmPrinter();
}

void generate_code(void) {
    init_codegen();

    // Create an LLVM context
    llvm::LLVMContext context;

    // Create a module
    std::unique_ptr<llvm::Module> module = std::make_unique<llvm::Module>("my_module", context);

    // Create an IR builder
    llvm::IRBuilder<> builder(context);

    // Define a function
    llvm::FunctionType *funcType = llvm::FunctionType::get(builder.getInt32Ty(), false);
    llvm::Function *mainFunc = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, "main", module.get());

    // Create a basic block
    llvm::BasicBlock *entry = llvm::BasicBlock::Create(context, "entry", mainFunc);
    builder.SetInsertPoint(entry);

    // Generate IR instructions
    llvm::Value *retVal = builder.getInt32(42);
    builder.CreateRet(retVal);

    // Verify the generated IR
    verifyFunction(*mainFunc);

    // Emit the code to an object file

    auto TargetTriple = llvm::sys::getDefaultTargetTriple();
    std::string Error;
    auto target = llvm::TargetRegistry::lookupTarget(TargetTriple, Error);

    // Print an error and exit if we couldn't find the requested target.
    // This generally occurs if we've forgotten to initialise the
    // TargetRegistry or we have a bogus target triple.
    if (!target) {
        llvm::errs() << Error;
      return;
    }

    auto CPU = "generic";
    auto Features = "";

    llvm::TargetOptions opt;
    auto TargetMachine = target->createTargetMachine(TargetTriple, CPU, Features, opt, llvm::Reloc::PIC_);

    module->setDataLayout(TargetMachine->createDataLayout());
    module->setTargetTriple(TargetTriple);

    std::error_code EC;

    llvm::raw_fd_ostream dest("output.o", EC, llvm::sys::fs::OF_None);
    if (EC) {
        llvm::errs() << "Could not open file: " << EC.message();
        return;
    }

    dest.flush();
}
