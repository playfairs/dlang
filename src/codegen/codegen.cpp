#include "codegen/codegen.hpp"
#include <fstream>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/Transforms/Scalar.h>
#include <utility>
#include <vector>

namespace dlang {
using namespace llvm;
static llvm::Type* llvmType(LLVMContext& context, dlang::Type type) {
  switch (type.kind) {
  case TypeKind::Void:
    return llvm::Type::getVoidTy(context);
  case TypeKind::Bool:
    return llvm::Type::getInt1Ty(context);
  case TypeKind::Char:
    return llvm::Type::getInt8Ty(context);
  case TypeKind::Long:
    return llvm::Type::getInt64Ty(context);
  case TypeKind::Float:
    return llvm::Type::getFloatTy(context);
  case TypeKind::Double:
    return llvm::Type::getDoubleTy(context);
  default:
    return llvm::Type::getInt32Ty(context);
  }
}
class FunctionEmitter {
public:
  FunctionEmitter(LLVMContext& context, llvm::Module& module, const dlang::Function& function)
      : builder_(context), module_(module), function_(function) {}
  void emit() {
    std::vector<llvm::Type*> parameters;
    for (const auto& parameter : function_.parameters)
      parameters.push_back(llvmType(module_.getContext(), parameter.type));
    auto* llvmFunction = llvm::Function::Create(
        llvm::FunctionType::get(llvmType(module_.getContext(), function_.returnType), parameters,
                                false),
        llvm::Function::ExternalLinkage, function_.name, &module_);
    current_ = llvmFunction;
    auto* entry = BasicBlock::Create(module_.getContext(), "entry", llvmFunction);
    builder_.SetInsertPoint(entry);
    unsigned index = 0;
    for (auto& argument : llvmFunction->args()) {
      const auto& parameter = function_.parameters[index++];
      auto* slot = builder_.CreateAlloca(llvmType(module_.getContext(), parameter.type), nullptr,
                                         parameter.name);
      builder_.CreateStore(&argument, slot);
      values_[parameter.name] = slot;
    }
    statement(*function_.body);
    if (!builder_.GetInsertBlock()->getTerminator())
      builder_.CreateRet(function_.returnType.kind == TypeKind::Void
                             ? nullptr
                             : zeroValue(llvmType(module_.getContext(), function_.returnType)));
  }

private:
  Value* zeroValue(llvm::Type* type) {
    if (type->isFloatingPointTy())
      return ConstantFP::get(type, 0.0);
    return ConstantInt::get(type, 0);
  }
  Value* convert(Value* value, llvm::Type* target) {
    llvm::Type* source = value->getType();
    if (source == target)
      return value;
    if (source->isFloatingPointTy() && target->isFloatingPointTy())
      return source->getPrimitiveSizeInBits() < target->getPrimitiveSizeInBits()
                 ? builder_.CreateFPExt(value, target)
                 : builder_.CreateFPTrunc(value, target);
    if (source->isIntegerTy() && target->isFloatingPointTy())
      return builder_.CreateSIToFP(value, target);
    return value;
  }
  llvm::Type* commonFloatingType(Value* left, Value* right) {
    if (!left->getType()->isFloatingPointTy() && !right->getType()->isFloatingPointTy())
      return nullptr;
    if (left->getType()->isDoubleTy() || right->getType()->isDoubleTy())
      return llvm::Type::getDoubleTy(module_.getContext());
    return llvm::Type::getFloatTy(module_.getContext());
  }
  Value* expression(const Expr& expressionNode) {
    return std::visit(
        [&](const auto& value) -> Value* {
          using ValueType = std::decay_t<decltype(value)>;
          if constexpr (std::is_same_v<ValueType, Literal>) {
            if (value.kind == TokenKind::True || value.kind == TokenKind::False)
              return ConstantInt::get(llvm::Type::getInt1Ty(module_.getContext()),
                                      value.kind == TokenKind::True);
            if (value.kind == TokenKind::FloatLiteral)
              return ConstantFP::get(llvm::Type::getDoubleTy(module_.getContext()),
                                     std::stod(value.value));
            return ConstantInt::get(llvm::Type::getInt32Ty(module_.getContext()),
                                    std::stoll(value.value));
          }
          if constexpr (std::is_same_v<ValueType, NameExpr>)
            return builder_.CreateLoad(values_[value.name]->getAllocatedType(), values_[value.name],
                                       value.name);
          if constexpr (std::is_same_v<ValueType, UnaryExpr>) {
            Value* operand = expression(*value.operand);
            if (value.op == TokenKind::Bang)
              return builder_.CreateNot(operand);
            if (value.op == TokenKind::Minus)
              return builder_.CreateNeg(operand);
            return operand;
          }
          if constexpr (std::is_same_v<ValueType, BinaryExpr>) {
            if (value.op == TokenKind::Equal) {
              auto* name = std::get_if<NameExpr>(&value.left->value);
              Value* right =
                  convert(expression(*value.right), values_[name->name]->getAllocatedType());
              builder_.CreateStore(right, values_[name->name]);
              return right;
            }
            Value *left = expression(*value.left), *right = expression(*value.right);
            if (llvm::Type* floatingType = commonFloatingType(left, right)) {
              left = convert(left, floatingType);
              right = convert(right, floatingType);
            }
            switch (value.op) {
            case TokenKind::Plus:
              return left->getType()->isFloatingPointTy() ? builder_.CreateFAdd(left, right)
                                                          : builder_.CreateAdd(left, right);
            case TokenKind::Minus:
              return left->getType()->isFloatingPointTy() ? builder_.CreateFSub(left, right)
                                                          : builder_.CreateSub(left, right);
            case TokenKind::Star:
              return left->getType()->isFloatingPointTy() ? builder_.CreateFMul(left, right)
                                                          : builder_.CreateMul(left, right);
            case TokenKind::Slash:
              return left->getType()->isFloatingPointTy() ? builder_.CreateFDiv(left, right)
                                                          : builder_.CreateSDiv(left, right);
            case TokenKind::Percent:
              return left->getType()->isFloatingPointTy() ? builder_.CreateFRem(left, right)
                                                          : builder_.CreateSRem(left, right);
            case TokenKind::EqualEqual:
              return left->getType()->isFloatingPointTy() ? builder_.CreateFCmpOEQ(left, right)
                                                          : builder_.CreateICmpEQ(left, right);
            case TokenKind::BangEqual:
              return left->getType()->isFloatingPointTy() ? builder_.CreateFCmpONE(left, right)
                                                          : builder_.CreateICmpNE(left, right);
            case TokenKind::Less:
              return left->getType()->isFloatingPointTy() ? builder_.CreateFCmpOLT(left, right)
                                                          : builder_.CreateICmpSLT(left, right);
            case TokenKind::LessEqual:
              return left->getType()->isFloatingPointTy() ? builder_.CreateFCmpOLE(left, right)
                                                          : builder_.CreateICmpSLE(left, right);
            case TokenKind::Greater:
              return left->getType()->isFloatingPointTy() ? builder_.CreateFCmpOGT(left, right)
                                                          : builder_.CreateICmpSGT(left, right);
            case TokenKind::GreaterEqual:
              return left->getType()->isFloatingPointTy() ? builder_.CreateFCmpOGE(left, right)
                                                          : builder_.CreateICmpSGE(left, right);
            case TokenKind::AmpAmp:
              return builder_.CreateAnd(left, right);
            case TokenKind::PipePipe:
              return builder_.CreateOr(left, right);
            default:
              return left;
            }
          }
          if constexpr (std::is_same_v<ValueType, CallExpr>) {
            auto* callee = module_.getFunction(value.callee);
            std::vector<Value*> args;
            for (size_t index = 0; index < value.arguments.size(); ++index)
              args.push_back(convert(expression(*value.arguments[index]),
                                     callee->getFunctionType()->getParamType(index)));
            return builder_.CreateCall(callee, args);
          }
        },
        expressionNode.value);
  }
  void statement(const Stmt& statementNode) {
    std::visit(
        [&](const auto& value) {
          using ValueType = std::decay_t<decltype(value)>;
          if constexpr (std::is_same_v<ValueType, BlockStmt>) {
            for (const auto& child : value.statements) {
              statement(*child);
              if (builder_.GetInsertBlock()->getTerminator())
                break;
            }
          } else if constexpr (std::is_same_v<ValueType, VarDeclStmt>) {
            auto* slot = builder_.CreateAlloca(llvmType(module_.getContext(), value.type), nullptr,
                                               value.name);
            values_[value.name] = slot;
            if (value.initializer)
              builder_.CreateStore(
                  convert(expression(*value.initializer), slot->getAllocatedType()), slot);
          } else if constexpr (std::is_same_v<ValueType, ExprStmt>)
            expression(*value.expression);
          else if constexpr (std::is_same_v<ValueType, ReturnStmt>)
            builder_.CreateRet(value.expression
                                   ? convert(expression(*value.expression),
                                             llvmType(module_.getContext(), function_.returnType))
                                   : nullptr);
          else if constexpr (std::is_same_v<ValueType, IfStmt>) {
            auto *thenBlock = BasicBlock::Create(module_.getContext(), "if.then", current_),
                 *merge = BasicBlock::Create(module_.getContext(), "if.end", current_);
            BasicBlock* elseBlock =
                value.elseBranch ? BasicBlock::Create(module_.getContext(), "if.else", current_)
                                 : merge;
            builder_.CreateCondBr(expression(*value.condition), thenBlock, elseBlock);
            builder_.SetInsertPoint(thenBlock);
            statement(*value.thenBranch);
            if (!builder_.GetInsertBlock()->getTerminator())
              builder_.CreateBr(merge);
            if (value.elseBranch) {
              builder_.SetInsertPoint(elseBlock);
              statement(*value.elseBranch);
              if (!builder_.GetInsertBlock()->getTerminator())
                builder_.CreateBr(merge);
            }
            builder_.SetInsertPoint(merge);
          } else if constexpr (std::is_same_v<ValueType, WhileStmt>) {
            auto *condition = BasicBlock::Create(module_.getContext(), "while.cond", current_),
                 *body = BasicBlock::Create(module_.getContext(), "while.body", current_),
                 *end = BasicBlock::Create(module_.getContext(), "while.end", current_);
            builder_.CreateBr(condition);
            builder_.SetInsertPoint(condition);
            builder_.CreateCondBr(expression(*value.condition), body, end);
            loops_.push_back({condition, end});
            builder_.SetInsertPoint(body);
            statement(*value.body);
            loops_.pop_back();
            if (!builder_.GetInsertBlock()->getTerminator())
              builder_.CreateBr(condition);
            builder_.SetInsertPoint(end);
          } else if constexpr (std::is_same_v<ValueType, ForStmt>) {
            if (value.initialization)
              statement(*value.initialization);
            auto *condition = BasicBlock::Create(module_.getContext(), "for.cond", current_),
                 *body = BasicBlock::Create(module_.getContext(), "for.body", current_),
                 *increment = BasicBlock::Create(module_.getContext(), "for.increment", current_),
                 *end = BasicBlock::Create(module_.getContext(), "for.end", current_);
            builder_.CreateBr(condition);
            builder_.SetInsertPoint(condition);
            Value* conditionValue = value.condition ? expression(*value.condition)
                                                    : ConstantInt::getTrue(module_.getContext());
            builder_.CreateCondBr(conditionValue, body, end);
            loops_.push_back({increment, end});
            builder_.SetInsertPoint(body);
            statement(*value.body);
            if (!builder_.GetInsertBlock()->getTerminator())
              builder_.CreateBr(increment);
            builder_.SetInsertPoint(increment);
            if (value.increment)
              expression(*value.increment);
            if (!builder_.GetInsertBlock()->getTerminator())
              builder_.CreateBr(condition);
            loops_.pop_back();
            builder_.SetInsertPoint(end);
          } else if constexpr (std::is_same_v<ValueType, BreakStmt>) {
            if (!loops_.empty())
              builder_.CreateBr(loops_.back().breakTarget);
          } else if constexpr (std::is_same_v<ValueType, ContinueStmt>) {
            if (!loops_.empty())
              builder_.CreateBr(loops_.back().continueTarget);
          }
        },
        statementNode.value);
  }
  IRBuilder<> builder_;
  llvm::Module& module_;
  const dlang::Function& function_;
  llvm::Function* current_ = nullptr;
  std::unordered_map<std::string, AllocaInst*> values_;
  struct LoopTargets {
    BasicBlock* continueTarget;
    BasicBlock* breakTarget;
  };
  std::vector<LoopTargets> loops_;
};
CodeGenerator::CodeGenerator(const Module& ast, const SemanticAnalyzer& semantic,
                             unsigned optimization, Diagnostics& diagnostics)
    : ast_(ast), optimization_(optimization), diagnostics_(diagnostics) {
  (void)semantic;
}
bool CodeGenerator::emit(const std::string& output, bool emitLLVM, bool objectOnly) {
  (void)objectOnly;
  InitializeNativeTarget();
  InitializeNativeTargetAsmPrinter();
  LLVMContext context;
  auto module = std::make_unique<llvm::Module>(ast_.name.empty() ? "dlang" : ast_.name, context);
  for (const auto& function : ast_.functions)
    FunctionEmitter(context, *module, function).emit();
  if (verifyModule(*module, &errs())) {
    diagnostics_.push_back({Severity::Error, {"<llvm>", 1, 1}, "LLVM module verification failed"});
    return false;
  }
  if (optimization_) {
    OptimizationLevel level = optimization_ == 1   ? OptimizationLevel::O1
                              : optimization_ == 2 ? OptimizationLevel::O2
                                                   : OptimizationLevel::O3;
    PassBuilder passBuilder;
    LoopAnalysisManager loopAnalysis;
    FunctionAnalysisManager functionAnalysis;
    CGSCCAnalysisManager cgsccAnalysis;
    ModuleAnalysisManager moduleAnalysis;
    passBuilder.registerModuleAnalyses(moduleAnalysis);
    passBuilder.registerCGSCCAnalyses(cgsccAnalysis);
    passBuilder.registerFunctionAnalyses(functionAnalysis);
    passBuilder.registerLoopAnalyses(loopAnalysis);
    passBuilder.crossRegisterProxies(loopAnalysis, functionAnalysis, cgsccAnalysis, moduleAnalysis);
    auto passes = passBuilder.buildPerModuleDefaultPipeline(level);
    passes.run(*module, moduleAnalysis);
  }
  if (emitLLVM) {
    std::error_code error;
    raw_fd_ostream stream(output, error);
    module->print(stream, nullptr);
    return !error;
  }
  std::string triple = sys::getDefaultTargetTriple();
  llvm::Triple targetTriple(triple);
  module->setTargetTriple(targetTriple);
  std::string error;
  auto target = TargetRegistry::lookupTarget(triple, error);
  if (!target) {
    diagnostics_.push_back({Severity::Error, {"<llvm>", 1, 1}, error});
    return false;
  }
  auto machine = std::unique_ptr<TargetMachine>(
      target->createTargetMachine(targetTriple, "generic", "", TargetOptions{}, Reloc::PIC_));
  module->setDataLayout(machine->createDataLayout());
  std::error_code fileError;
  raw_fd_ostream stream(output, fileError);
  if (fileError) {
    diagnostics_.push_back({Severity::Error, {output, 1, 1}, fileError.message()});
    return false;
  }
  legacy::PassManager pass;
  if (machine->addPassesToEmitFile(pass, stream, nullptr, CodeGenFileType::ObjectFile))
    return false;
  pass.run(*module);
  stream.flush();
  return true;
}
} // namespace dlang
