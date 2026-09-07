#pragma once

#include "lexer/token.hpp"
#include <string>
#include <vector>

namespace dlang {

class Lexer {
public:
  Lexer(std::string file, std::string source)
      : file_(std::move(file)), source_(std::move(source)) {}
  std::vector<Token> lex(Diagnostics& diagnostics);

private:
  char peek(unsigned lookahead = 0) const;
  char advance();
  bool match(char expected);
  void add(TokenKind kind, unsigned start, SourceLocation location);
  void diagnostic(Diagnostics& diagnostics, SourceLocation location, std::string message);
  std::string file_, source_;
  unsigned index_ = 0, line_ = 1, column_ = 1;
};

} // namespace dlang
