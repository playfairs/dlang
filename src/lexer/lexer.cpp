#include "lexer/lexer.hpp"
#include <cctype>
#include <unordered_map>

namespace dlang {
char Lexer::peek(unsigned lookahead) const {
  return index_ + lookahead < source_.size() ? source_[index_ + lookahead] : '\0';
}
char Lexer::advance() {
  char c = peek();
  if (c) {
    ++index_;
    if (c == '\n') {
      ++line_;
      column_ = 1;
    } else
      ++column_;
  }
  return c;
}
bool Lexer::match(char expected) {
  if (peek() != expected)
    return false;
  advance();
  return true;
}
void Lexer::add(TokenKind, unsigned, SourceLocation) {}
void Lexer::diagnostic(Diagnostics& diagnostics, SourceLocation location, std::string message) {
  diagnostics.push_back({Severity::Error, std::move(location), std::move(message)});
}
std::vector<Token> Lexer::lex(Diagnostics& diagnostics) {
  std::vector<Token> tokens;
  auto keywords = std::unordered_map<std::string, TokenKind>{
      {"module", TokenKind::Module},     {"import", TokenKind::Import},
      {"struct", TokenKind::Struct},     {"string", TokenKind::String},
      {"if", TokenKind::If},             {"else", TokenKind::Else},
      {"while", TokenKind::While},       {"for", TokenKind::For},
      {"break", TokenKind::Break},       {"continue", TokenKind::Continue},
      {"return", TokenKind::Return},     {"true", TokenKind::True},
      {"false", TokenKind::False},       {"void", TokenKind::Void},
      {"bool", TokenKind::Bool},         {"char", TokenKind::Char},
      {"int", TokenKind::Int},           {"long", TokenKind::Long},
      {"float", TokenKind::Float},       {"double", TokenKind::Double}};
  while (peek()) {
    if (std::isspace(static_cast<unsigned char>(peek()))) {
      advance();
      continue;
    }
    SourceLocation location{file_, line_, column_};
    unsigned start = index_;
    char c = advance();
    if (c == '/' && peek() == '/') {
      while (peek() && peek() != '\n')
        advance();
      continue;
    }
    if (c == '/' && peek() == '*') {
      advance();
      bool closed = false;
      while (peek()) {
        if (advance() == '*' && match('/')) {
          closed = true;
          break;
        }
      }
      if (!closed)
        diagnostic(diagnostics, location, "unterminated block comment");
      continue;
    }
    if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
      while (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_')
        advance();
      auto text = source_.substr(start, index_ - start);
      auto it = keywords.find(text);
      tokens.push_back({it == keywords.end() ? TokenKind::Identifier : it->second, text, location});
      continue;
    }
    if (std::isdigit(static_cast<unsigned char>(c))) {
      bool floating = false;
      while (std::isdigit(static_cast<unsigned char>(peek())))
        advance();
      if (peek() == '.' && std::isdigit(static_cast<unsigned char>(peek(1)))) {
        floating = true;
        advance();
        while (std::isdigit(static_cast<unsigned char>(peek())))
          advance();
      }
      tokens.push_back({floating ? TokenKind::FloatLiteral : TokenKind::IntLiteral,
                        source_.substr(start, index_ - start), location});
      continue;
    }
    if (c == '"' || c == '\'') {
      char quote = c;
      bool closed = false;
      while (peek()) {
        char value = advance();
        if (value == '\\' && peek())
          advance();
        else if (value == quote) {
          closed = true;
          break;
        }
      }
      if (!closed)
        diagnostic(diagnostics, location, "unterminated literal");
      tokens.push_back({quote == '"' ? TokenKind::StringLiteral : TokenKind::CharLiteral,
                        source_.substr(start, index_ - start), location});
      continue;
    }
    TokenKind kind;
    bool recognized = true;
    switch (c) {
    case '+':
      kind = TokenKind::Plus;
      break;
    case '-':
      kind = match('>') ? TokenKind::Arrow : TokenKind::Minus;
      break;
    case '*':
      kind = TokenKind::Star;
      break;
    case '/':
      kind = TokenKind::Slash;
      break;
    case '%':
      kind = TokenKind::Percent;
      break;
    case '!':
      kind = match('=') ? TokenKind::BangEqual : TokenKind::Bang;
      break;
    case '=':
      kind = match('=') ? TokenKind::EqualEqual : TokenKind::Equal;
      break;
    case '<':
      kind = match('=') ? TokenKind::LessEqual : TokenKind::Less;
      break;
    case '>':
      kind = match('=') ? TokenKind::GreaterEqual : TokenKind::Greater;
      break;
    case '&':
      if (match('&'))
        kind = TokenKind::AmpAmp;
      else
        recognized = false;
      break;
    case '|':
      if (match('|'))
        kind = TokenKind::PipePipe;
      else
        recognized = false;
      break;
    case '(':
      kind = TokenKind::LeftParen;
      break;
    case ')':
      kind = TokenKind::RightParen;
      break;
    case '{':
      kind = TokenKind::LeftBrace;
      break;
    case '}':
      kind = TokenKind::RightBrace;
      break;
    case '[':
      kind = TokenKind::LeftBracket;
      break;
    case ']':
      kind = TokenKind::RightBracket;
      break;
    case ',':
      kind = TokenKind::Comma;
      break;
    case ';':
      kind = TokenKind::Semicolon;
      break;
    case ':':
      kind = TokenKind::Colon;
      break;
    case '.':
      kind = TokenKind::Dot;
      break;
    default:
      recognized = false;
      break;
    }
    if (recognized)
      tokens.push_back({kind, source_.substr(start, index_ - start), location});
    else
      diagnostic(diagnostics, location, "unexpected character '" + std::string(1, c) + "'");
  }
  tokens.push_back({TokenKind::Eof, "", {file_, line_, column_}});
  return tokens;
}
} // namespace dlang
