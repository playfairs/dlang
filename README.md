A D compiler written in C++

`dlang` is a production-oriented D compiler under active development. It uses a separated frontend pipeline (source management, lexer, parser, semantic analysis) and LLVM for optimization and native code generation.

## Status

The current compiler supports modules, imports as parsed declarations, integer/floating-point/boolean/character/string literals, variables, functions, calls, arithmetic and comparisons, blocks, `if`, `while`, `for`, `break`, `continue`, `return`, and LLVM IR/native executable generation. Floating-point arithmetic is lowered to LLVM floating-point instructions, and loop control is lowered to explicit control-flow targets. Unsupported D features are diagnosed rather than silently accepted.

## Build

```sh
nix develop
meson setup build
meson compile -C build
```

Format with `nix fmt`. Run checks with `meson test -C build`.

## Usage

```sh
./build/dlang examples/hello.d -o hello
./hello
./build/dlang examples/hello.d --emit-llvm
./build/dlang examples/hello.d --check
```

`-c` emits an object file. `-O0` through `-O3` select LLVM optimization levels. `--version` reads the authoritative root `VERSION` file.

## Architecture

The driver parses options and coordinates source loading, lexing, recursive-descent parsing, scoped semantic analysis, and LLVM code generation. Types and symbols are explicit frontend objects; LLVM is used only after semantic validation.

## Limitations

This is an early compiler slice. Aggregate types, imports across files, runtime/string support, and the wider D type system are not yet implemented. The supported subset is intentionally diagnosed when a feature is outside the implemented pipeline.
