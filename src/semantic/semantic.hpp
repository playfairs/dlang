#pragma once

#include "ast/ast.hpp"
#include <unordered_map>

namespace dlang {

struct FunctionInfo {
  Type returnType;
  std::vector<Type> parameters;
};

class SemanticAnalyzer {
public:
  explicit SemanticAnalyzer(Diagnostics& diagnostics) : diagnostics_(diagnostics) {}
  bool analyze(const Module& module);
  const std::unordered_map<std::string, FunctionInfo>& functions() const { return functions_; }

private:
  struct Scope {
    std::unordered_map<std::string, Type> values;
    Scope* parent = nullptr;
  };
  Type expressionType(const Expr& expression, Scope& scope);
  bool statement(const Stmt& statement, Scope& scope, const Type& returnType, bool inLoop);
  Type lookup(const std::string& name, Scope& scope);
  Type memberType(const MemberExpr& member, Scope& scope, const SourceLocation& location);
  void error(const SourceLocation& location, std::string message);
  Diagnostics& diagnostics_;
  std::unordered_map<std::string, const StructDecl*> structs_;
  std::unordered_map<std::string, FunctionInfo> functions_;
};

} // namespace dlang
