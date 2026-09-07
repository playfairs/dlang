#pragma once

#include "ast/ast.hpp"
#include "semantic/semantic.hpp"
#include <memory>
#include <string>

namespace dlang {

class CodeGenerator {
public:
  CodeGenerator(const Module& ast, const SemanticAnalyzer& semantic, unsigned optimization,
                Diagnostics& diagnostics);
  bool emit(const std::string& output, bool emitLLVM, bool objectOnly);

private:
  const Module& ast_;
  unsigned optimization_;
  Diagnostics& diagnostics_;
};

} // namespace dlang
