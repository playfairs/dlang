#include "lexer/token.hpp"

namespace dlang {
const char* tokenKindName(TokenKind kind) {
  switch (kind) {
  case TokenKind::Eof:
    return "end of file";
  case TokenKind::Identifier:
    return "identifier";
  case TokenKind::IntLiteral:
    return "integer literal";
  case TokenKind::FloatLiteral:
    return "floating literal";
  case TokenKind::StringLiteral:
    return "string literal";
  case TokenKind::CharLiteral:
    return "character literal";
  case TokenKind::Module:
    return "module";
  case TokenKind::Import:
    return "import";
  case TokenKind::Struct:
    return "struct";
  case TokenKind::String:
    return "string";
  case TokenKind::If:
    return "if";
  case TokenKind::Else:
    return "else";
  case TokenKind::While:
    return "while";
  case TokenKind::For:
    return "for";
  case TokenKind::Break:
    return "break";
  case TokenKind::Continue:
    return "continue";
  case TokenKind::Return:
    return "return";
  case TokenKind::True:
    return "true";
  case TokenKind::False:
    return "false";
  case TokenKind::Void:
    return "void";
  case TokenKind::Bool:
    return "bool";
  case TokenKind::Char:
    return "char";
  case TokenKind::Int:
    return "int";
  case TokenKind::Long:
    return "long";
  case TokenKind::Float:
    return "float";
  case TokenKind::Double:
    return "double";
  default:
    return "operator or punctuation";
  }
}
} // namespace dlang
