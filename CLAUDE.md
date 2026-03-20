# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Commands

```bash
# Configure (first time or after CMakeLists.txt changes)
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# Build everything (compiler + tests)
cmake --build build -j$(nproc)

# Run the compiler on an Oberon source file
./build/oberonc tests/Hello.Mod

# Run all tests
./build/oberonc_tests

# Run a specific test by name
./build/oberonc_tests "Lexer: nested comments are handled"

# Run a test tag group
./build/oberonc_tests "[lexer]"
./build/oberonc_tests "[parser]"

# Build with AddressSanitizer
cmake -B build -DENABLE_ASAN=ON && cmake --build build -j$(nproc)
```

Catch2 v3 is fetched automatically by CMake via `FetchContent` on first configure (requires internet access).

## Architecture

The project implements a **recursive-descent LL(1) parser** for the **Oberon-07** language (Niklaus Wirth, 2016 revision). The pipeline is: source text → Lexer → token stream → Parser → AST.

### Source files (`src/`)

| File | Role |
|---|---|
| `token.hpp` | `TokenKind` enum + `Token` struct + `SourceLoc`. `tokenKindName()` is defined in `lexer.cpp`. |
| `lexer.hpp/cpp` | Hand-written lexer. Produces one `Token` per `next()` call. Handles nested `(* ... *)` comments, decimal and hex integers, real literals with scale factors, string literals (`"..."` and `0DX`-style char constants). |
| `ast.hpp` | All AST node types. Uses `std::unique_ptr` for ownership, `std::variant<FieldSelector, IndexSelector, DerefSelector, TypeGuardSelector>` for selectors. |
| `parser.hpp/cpp` | Recursive descent parser. One `parse*()` method per grammar production. Maintains a single lookahead token (`cur_`). Throws `ParseError` on syntax errors. |
| `main.cpp` | CLI entry point: reads a file, runs Lexer+Parser, prints a summary. |

### Key design decisions

**qualident vs field selector ambiguity**: Per the Oberon-07 grammar, `r.x` is parsed as `qualident(module="r", ident="x")` — a two-part qualified name with no selectors. Whether `r` refers to an imported module or a record variable is resolved in semantic analysis, not during parsing. Only `p^.x` (pointer deref then field access) produces an unambiguous `FieldSelector`.

**Type guard selector `(T)`**: The grammar allows `selector = "(" qualident ")"` inside a designator. To keep LL(1) without extra lookahead, `(` is *not* consumed inside `parseDesignator`. The caller (`parseFactor` / `parseStatement`) handles `(` as `ActualParameters`. The semantic phase distinguishes type guards from procedure calls.

**`std::format` unavailability**: GCC 12 does not ship `<format>`. A variadic `fmt()` helper using `std::ostringstream` is defined at the top of `parser.cpp` instead.

### Grammar reference

The full Oberon-07 EBNF is in the Appendix of the official language report: `Oberon07.Report.pdf` (Niklaus Wirth, ETH Zurich). The key productions the parser implements are: `module`, `DeclarationSequence`, `ProcedureDeclaration`, `statement`, `expression`, `designator`, `type`.
