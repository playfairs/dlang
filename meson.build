project('dlang', ['c', 'cpp'], version: run_command('cat', files('VERSION'), check: true).stdout().strip(), license: 'Unlicense',
  default_options: ['cpp_std=c++20', 'warning_level=3'])
add_project_arguments('-DDLANG_VERSION="' + meson.project_version() + '"', language: 'cpp')

llvm = dependency('llvm', modules: ['core', 'irreader', 'passes', 'target', 'support'])
inc = include_directories('src')
sources = files(
  'src/lexer/token.cpp',
  'src/lexer/lexer.cpp',
  'src/parser/parser.cpp',
  'src/semantic/semantic.cpp',
  'src/codegen/codegen.cpp',
  'src/driver/main.cpp',
)
dlang = executable('dlang', sources, include_directories: inc, dependencies: llvm,
  install: true)

test('dlang check', dlang, args: ['--check', meson.project_source_root() / 'examples/hello.d'])
test('dlang floating point', find_program('tests/floating.sh'),
  args: [dlang, meson.project_source_root() / 'examples/floating.d'])
test('dlang for loop', find_program('tests/for.sh'),
  args: [dlang, meson.project_source_root() / 'tests/for.d'])
test('dlang malformed input', find_program('tests/diagnostics.sh'),
  args: [dlang, meson.project_source_root() / 'tests/malformed.d'])
test('dlang struct members', find_program('tests/struct.sh'),
  args: [dlang, meson.project_source_root() / 'tests/struct.d'])
