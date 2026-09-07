#pragma once

#include "ast/ast.hpp"
#include <vector>

namespace dlang {

class Parser {
public:
  Parser(const std::vector<Token>& tokens, Diagnostics& diagnostics)
      : tokens_(tokens), diagnostics_(diagnostics) {}
  Module parse();

private:
  const Token& current() const;
  const Token& advance();
  bool check(TokenKind kind) const;
  bool match(TokenKind kind);
  const Token& expect(TokenKind kind, const char* message);
  void error(const Token& token, std::string message);
  Type parseType();
  Function parseFunction();
  std::unique_ptr<Stmt> parseStatement();
  std::unique_ptr<Stmt> parseBlock();
  ExprPtr parseExpression();
  ExprPtr parseAssignment();
  ExprPtr parseBinary(int minimumPrecedence);
  ExprPtr parseUnary();
  ExprPtr parsePrimary();
  int precedence(TokenKind kind) const;
  const std::vector<Token>& tokens_;
  Diagnostics& diagnostics_;
  size_t index_ = 0;
};

} // namespace dlang
