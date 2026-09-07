#pragma once

#include "diagnostics/diagnostic.hpp"
#include <string>

namespace dlang {

enum class TokenKind {
  Eof,
  Identifier,
  IntLiteral,
  FloatLiteral,
  StringLiteral,
  CharLiteral,
  Module,
  Import,
  Struct,
  String,
  If,
  Else,
  While,
  For,
  Break,
  Continue,
  Return,
  True,
  False,
  Void,
  Bool,
  Char,
  Int,
  Long,
  Float,
  Double,
  Plus,
  Minus,
  Star,
  Slash,
  Percent,
  Bang,
  AmpAmp,
  PipePipe,
  Equal,
  EqualEqual,
  BangEqual,
  Less,
  LessEqual,
  Greater,
  GreaterEqual,
  LeftParen,
  RightParen,
  LeftBrace,
  RightBrace,
  LeftBracket,
  RightBracket,
  Comma,
  Semicolon,
  Colon,
  Dot,
  Arrow
};

struct Token {
  TokenKind kind;
  std::string lexeme;
  SourceLocation location;
};

const char* tokenKindName(TokenKind kind);

} // namespace dlang
