#pragma once

#include <string>
#include <vector>

namespace dlang {

enum class Severity { Error, Warning };

struct SourceLocation {
  std::string file;
  unsigned line = 1;
  unsigned column = 1;
};

struct Diagnostic {
  Severity severity;
  SourceLocation location;
  std::string message;
};

using Diagnostics = std::vector<Diagnostic>;

} // namespace dlang
