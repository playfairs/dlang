#include "codegen/codegen.hpp"
#include "lexer/lexer.hpp"
#include "parser/parser.hpp"
#include "semantic/semantic.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace dlang {
static void printDiagnostics(const Diagnostics& diagnostics) {
  for (const auto& diagnostic : diagnostics)
    std::cerr << diagnostic.location.file << ':' << diagnostic.location.line << ':'
              << diagnostic.location.column << ": "
              << (diagnostic.severity == Severity::Error ? "error: " : "warning: ")
              << diagnostic.message << '\n';
}
static void help() {
  std::cout << "A D compiler written in C++\nUsage: dlang source.d [options]\n  -o <file>       "
               "output executable/object/IR\n  -c              emit object file\n  --emit-llvm     "
               "emit LLVM IR\n  --check         parse and semantically check only\n  -O0..-O3      "
               "  LLVM optimization level\n  --version       print compiler version\n  --help      "
               "    print this help\n";
}
} // namespace dlang
int main(int argc, char** argv) {
  using namespace dlang;
  std::string input, output;
  bool check = false, emitLLVM = false, objectOnly = false;
  unsigned optimization = 0;
  for (int i = 1; i < argc; ++i) {
    std::string argument = argv[i];
    if (argument == "--help" || argument == "-h") {
      help();
      return 0;
    }
    if (argument == "--version") {
      std::cout << "dlang " << DLANG_VERSION << '\n';
      return 0;
    }
    if (argument == "-o" && i + 1 < argc) {
      output = argv[++i];
      continue;
    }
    if (argument == "-c") {
      objectOnly = true;
      continue;
    }
    if (argument == "--emit-llvm") {
      emitLLVM = true;
      continue;
    }
    if (argument == "--check") {
      check = true;
      continue;
    }
    if (argument.size() == 3 && argument[0] == '-' && argument[1] == 'O' && argument[2] >= '0' &&
        argument[2] <= '3') {
      optimization = static_cast<unsigned>(argument[2] - '0');
      continue;
    }
    if (argument[0] == '-') {
      std::cerr << "dlang: unknown option " << argument << '\n';
      return 2;
    }
    input = argument;
  }
  if (input.empty()) {
    std::cerr << "dlang: no input files\n";
    return 2;
  }
  std::ifstream file(input);
  if (!file) {
    std::cerr << "dlang: cannot open " << input << '\n';
    return 1;
  }
  std::stringstream buffer;
  buffer << file.rdbuf();
  Diagnostics diagnostics;
  auto tokens = Lexer(input, buffer.str()).lex(diagnostics);
  auto module = Parser(tokens, diagnostics).parse();
  SemanticAnalyzer semantic(diagnostics);
  semantic.analyze(module);
  if (!diagnostics.empty()) {
    printDiagnostics(diagnostics);
    return 1;
  }
  if (check)
    return 0;
  std::filesystem::path inputPath(input);
  if (output.empty())
    output = (emitLLVM     ? inputPath.stem().string() + ".ll"
              : objectOnly ? inputPath.stem().string() + ".o"
                           : inputPath.stem().string());
  if (emitLLVM || objectOnly) {
    if (!CodeGenerator(module, semantic, optimization, diagnostics)
             .emit(output, emitLLVM, objectOnly)) {
      printDiagnostics(diagnostics);
      return 1;
    }
    return 0;
  }
  std::string object = output + ".o";
  if (!CodeGenerator(module, semantic, optimization, diagnostics).emit(object, false, true)) {
    printDiagnostics(diagnostics);
    return 1;
  }
  std::string command = "clang \"" + object + "\" -o \"" + output + "\"";
  int result = std::system(command.c_str());
  std::filesystem::remove(object);
  return result == 0 ? 0 : 1;
}
