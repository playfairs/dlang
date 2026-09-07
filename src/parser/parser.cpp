#include "parser/parser.hpp"
#include <cstdlib>
#include <utility>

namespace dlang {
const Token& Parser::current() const { return tokens_[index_]; }
const Token& Parser::advance() {
  if (!check(TokenKind::Eof))
    ++index_;
  return tokens_[index_ - 1];
}
bool Parser::check(TokenKind kind) const { return current().kind == kind; }
bool Parser::match(TokenKind kind) {
  if (!check(kind))
    return false;
  advance();
  return true;
}
const Token& Parser::expect(TokenKind kind, const char* message) {
  if (!check(kind))
    error(current(), message);
  return advance();
}
void Parser::error(const Token& token, std::string message) {
  diagnostics_.push_back({Severity::Error, token.location, std::move(message)});
}
Type Parser::parseType() {
  Type type;
  const Token& token = current();
  switch (token.kind) {
  case TokenKind::Void:
    type.kind = TypeKind::Void;
    break;
  case TokenKind::Bool:
    type.kind = TypeKind::Bool;
    break;
  case TokenKind::Char:
    type.kind = TypeKind::Char;
    break;
  case TokenKind::Int:
    type.kind = TypeKind::Int;
    break;
  case TokenKind::Long:
    type.kind = TypeKind::Long;
    break;
  case TokenKind::Float:
    type.kind = TypeKind::Float;
    break;
  case TokenKind::Double:
    type.kind = TypeKind::Double;
    break;
  default:
    error(token, "expected a type");
    return type;
  }
  type.name = token.lexeme;
  advance();
  return type;
}
Module Parser::parse() {
  Module module;
  if (match(TokenKind::Module)) {
    if (check(TokenKind::Identifier))
      module.name = advance().lexeme;
    else
      error(current(), "expected module name");
    expect(TokenKind::Semicolon, "expected ';' after module declaration");
  }
  while (match(TokenKind::Import)) {
    if (check(TokenKind::Identifier))
      module.imports.push_back(advance().lexeme);
    else
      error(current(), "expected import name");
    expect(TokenKind::Semicolon, "expected ';' after import");
  }
  while (!check(TokenKind::Eof)) {
    module.functions.push_back(parseFunction());
  }
  return module;
}
Function Parser::parseFunction() {
  Function function;
  function.location = current().location;
  function.returnType = parseType();
  if (check(TokenKind::Identifier))
    function.name = advance().lexeme;
  else
    error(current(), "expected function name");
  expect(TokenKind::LeftParen, "expected '(' after function name");
  if (!check(TokenKind::RightParen)) {
    do {
      Parameter parameter;
      parameter.location = current().location;
      parameter.type = parseType();
      if (check(TokenKind::Identifier))
        parameter.name = advance().lexeme;
      else
        error(current(), "expected parameter name");
      function.parameters.push_back(std::move(parameter));
    } while (match(TokenKind::Comma));
  }
  expect(TokenKind::RightParen, "expected ')' after parameters");
  function.body = parseBlock();
  return function;
}
std::unique_ptr<Stmt> Parser::parseBlock() {
  auto statement = std::make_unique<Stmt>();
  statement->location = current().location;
  BlockStmt block;
  expect(TokenKind::LeftBrace, "expected '{'");
  while (!check(TokenKind::RightBrace) && !check(TokenKind::Eof))
    block.statements.push_back(parseStatement());
  expect(TokenKind::RightBrace, "expected '}'");
  statement->value = std::move(block);
  return statement;
}
std::unique_ptr<Stmt> Parser::parseStatement() {
  auto statement = std::make_unique<Stmt>();
  statement->location = current().location;
  if (check(TokenKind::LeftBrace))
    return parseBlock();
  if (match(TokenKind::Return)) {
    ReturnStmt result;
    if (!check(TokenKind::Semicolon))
      result.expression = parseExpression();
    expect(TokenKind::Semicolon, "expected ';' after return");
    statement->value = std::move(result);
    return statement;
  }
  if (match(TokenKind::If)) {
    IfStmt result;
    expect(TokenKind::LeftParen, "expected '(' after if");
    result.condition = parseExpression();
    expect(TokenKind::RightParen, "expected ')' after condition");
    result.thenBranch = parseStatement();
    if (match(TokenKind::Else))
      result.elseBranch = parseStatement();
    statement->value = std::move(result);
    return statement;
  }
  if (match(TokenKind::While)) {
    WhileStmt result;
    expect(TokenKind::LeftParen, "expected '(' after while");
    result.condition = parseExpression();
    expect(TokenKind::RightParen, "expected ')' after condition");
    result.body = parseStatement();
    statement->value = std::move(result);
    return statement;
  }
  if (match(TokenKind::Break)) {
    expect(TokenKind::Semicolon, "expected ';' after break");
    statement->value = BreakStmt{};
    return statement;
  }
  if (match(TokenKind::Continue)) {
    expect(TokenKind::Semicolon, "expected ';' after continue");
    statement->value = ContinueStmt{};
    return statement;
  }
  if (check(TokenKind::Int) || check(TokenKind::Long) || check(TokenKind::Bool) ||
      check(TokenKind::Char)) {
    VarDeclStmt result;
    result.type = parseType();
    if (check(TokenKind::Identifier))
      result.name = advance().lexeme;
    else
      error(current(), "expected variable name");
    if (match(TokenKind::Equal))
      result.initializer = parseExpression();
    expect(TokenKind::Semicolon, "expected ';' after variable declaration");
    statement->value = std::move(result);
    return statement;
  }
  ExprStmt result;
  result.expression = parseExpression();
  expect(TokenKind::Semicolon, "expected ';' after expression");
  statement->value = std::move(result);
  return statement;
}
ExprPtr Parser::parseExpression() { return parseAssignment(); }
ExprPtr Parser::parseAssignment() {
  auto left = parseBinary(1);
  if (match(TokenKind::Equal)) {
    auto right = parseAssignment();
    auto node = std::make_unique<Expr>();
    node->location = left->location;
    node->value = BinaryExpr{TokenKind::Equal, std::move(left), std::move(right)};
    return node;
  }
  return left;
}
int Parser::precedence(TokenKind kind) const {
  switch (kind) {
  case TokenKind::PipePipe:
    return 2;
  case TokenKind::AmpAmp:
    return 3;
  case TokenKind::EqualEqual:
  case TokenKind::BangEqual:
    return 4;
  case TokenKind::Less:
  case TokenKind::LessEqual:
  case TokenKind::Greater:
  case TokenKind::GreaterEqual:
    return 5;
  case TokenKind::Plus:
  case TokenKind::Minus:
    return 6;
  case TokenKind::Star:
  case TokenKind::Slash:
  case TokenKind::Percent:
    return 7;
  default:
    return 0;
  }
}
ExprPtr Parser::parseBinary(int minimumPrecedence) {
  auto left = parseUnary();
  while (precedence(current().kind) >= minimumPrecedence) {
    Token op = advance();
    auto right = parseBinary(precedence(op.kind) + 1);
    auto node = std::make_unique<Expr>();
    node->location = left->location;
    node->value = BinaryExpr{op.kind, std::move(left), std::move(right)};
    left = std::move(node);
  }
  return left;
}
ExprPtr Parser::parseUnary() {
  if (check(TokenKind::Bang) || check(TokenKind::Minus) || check(TokenKind::Plus)) {
    Token op = advance();
    auto node = std::make_unique<Expr>();
    node->location = op.location;
    node->value = UnaryExpr{op.kind, parseUnary()};
    return node;
  }
  return parsePrimary();
}
ExprPtr Parser::parsePrimary() {
  Token token = advance();
  auto node = std::make_unique<Expr>();
  node->location = token.location;
  if (token.kind == TokenKind::IntLiteral || token.kind == TokenKind::FloatLiteral ||
      token.kind == TokenKind::StringLiteral || token.kind == TokenKind::CharLiteral ||
      token.kind == TokenKind::True || token.kind == TokenKind::False) {
    node->value = Literal{token.kind, token.lexeme};
    return node;
  }
  if (token.kind == TokenKind::Identifier) {
    if (match(TokenKind::LeftParen)) {
      CallExpr call;
      call.callee = token.lexeme;
      if (!check(TokenKind::RightParen)) {
        do {
          call.arguments.push_back(parseExpression());
        } while (match(TokenKind::Comma));
      }
      expect(TokenKind::RightParen, "expected ')' after arguments");
      node->value = std::move(call);
    } else
      node->value = NameExpr{token.lexeme};
    return node;
  }
  if (token.kind == TokenKind::LeftParen) {
    auto expression = parseExpression();
    expect(TokenKind::RightParen, "expected ')' after expression");
    return expression;
  }
  error(token, "expected expression");
  node->value = Literal{TokenKind::IntLiteral, "0"};
  return node;
}
} // namespace dlang
