#pragma once

#include "diagnostics/diagnostic.hpp"
#include "lexer/token.hpp"
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace dlang {

enum class TypeKind { Void, Bool, Char, Int, Long, Float, Double, String, Unknown };
struct Type {
  TypeKind kind = TypeKind::Unknown;
  std::string name;
  bool operator==(const Type&) const = default;
};
struct Expr;
using ExprPtr = std::unique_ptr<Expr>;
struct Literal {
  TokenKind kind;
  std::string value;
};
struct NameExpr {
  std::string name;
};
struct UnaryExpr {
  TokenKind op;
  ExprPtr operand;
};
struct BinaryExpr {
  TokenKind op;
  ExprPtr left;
  ExprPtr right;
};
struct CallExpr {
  std::string callee;
  std::vector<ExprPtr> arguments;
};
struct Expr {
  SourceLocation location;
  std::variant<Literal, NameExpr, UnaryExpr, BinaryExpr, CallExpr> value;
};
using StmtPtr = std::unique_ptr<struct Stmt>;
struct BlockStmt {
  std::vector<StmtPtr> statements;
};
struct VarDeclStmt {
  Type type;
  std::string name;
  ExprPtr initializer;
};
struct ExprStmt {
  ExprPtr expression;
};
struct ReturnStmt {
  ExprPtr expression;
};
struct IfStmt {
  ExprPtr condition;
  StmtPtr thenBranch;
  StmtPtr elseBranch;
};
struct WhileStmt {
  ExprPtr condition;
  StmtPtr body;
};
struct ForStmt {
  StmtPtr initialization;
  ExprPtr condition;
  ExprPtr increment;
  StmtPtr body;
};
struct BreakStmt {};
struct ContinueStmt {};
struct Stmt {
  SourceLocation location;
  std::variant<BlockStmt, VarDeclStmt, ExprStmt, ReturnStmt, IfStmt, WhileStmt, ForStmt, BreakStmt,
               ContinueStmt>
      value;
};
struct Parameter {
  Type type;
  std::string name;
  SourceLocation location;
};
struct Function {
  Type returnType;
  std::string name;
  std::vector<Parameter> parameters;
  std::unique_ptr<Stmt> body;
  SourceLocation location;
};
struct Module {
  std::string name;
  std::vector<std::string> imports;
  std::vector<Function> functions;
};

} // namespace dlang
