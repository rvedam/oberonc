#include <catch2/catch_test_macros.hpp>
#include "lexer.hpp"
#include "parser.hpp"
#include "codegen.hpp"

#include <array>
#include <string>
#include <fstream>
#include <sstream>

// The directory that holds TypeExporter.Mod and other test fixture modules.
// Injected by CMake so the tests work regardless of working directory.
#ifndef TEST_MODULE_DIR
#define TEST_MODULE_DIR "tests"
#endif

// -----------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------

// Compile an Oberon source string, searching modDir for imports.
// Returns "" on success, or the exception message on failure.
static std::string tryCompile(const std::string& src,
                               const std::string& modDir = TEST_MODULE_DIR) {
    try {
        Lexer  lex(src, "<test>");
        Parser parser(lex);
        Module mod = parser.parseModule();
        CodeGen cg(mod.name);
        cg.setModulePaths(std::array<std::string, 1>{modDir});
        cg.generate(mod);
        return "";
    } catch (const std::runtime_error& e) {
        // ParseError derives from std::runtime_error; catches both.
        return e.what();
    }
}

static std::string readFile(const std::string& path) {
    std::ifstream f(path);
    if (!f) return "";
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// -----------------------------------------------------------------------
// Sanity: the test fixture module itself must compile cleanly.
// -----------------------------------------------------------------------
TEST_CASE("TypeExporter fixture compiles", "[codegen][import]") {
    std::string src = readFile(std::string(TEST_MODULE_DIR) + "/TypeExporter.Mod");
    REQUIRE_FALSE(src.empty()); // fixture file must exist
    auto err = tryCompile(src);
    CHECK(err.empty());
}

// -----------------------------------------------------------------------
// loadModuleInterface: exported types must be available in the importing
// module's type namespace.
//
// All tests below use TypeExporter.Mod (see tests/TypeExporter.Mod):
//   TYPE Point*     = RECORD x*, y*: INTEGER END;
//        PointPtr*  = POINTER TO Point;
//        Point3D*   = RECORD (Point) z*: INTEGER END;
//        (* PrivateHelper — not exported *)
// -----------------------------------------------------------------------

TEST_CASE("loadModuleInterface: exported record type usable in VAR declaration",
          "[codegen][import]") {
    // VAR r: TypeExporter.Point  must resolve the imported record type.
    auto err = tryCompile(R"(
MODULE Importer;
  IMPORT TypeExporter;
  VAR r: TypeExporter.Point;
BEGIN
END Importer.
)");
    CHECK(err.empty());
}

TEST_CASE("loadModuleInterface: exported pointer type usable in VAR declaration",
          "[codegen][import]") {
    // VAR p: TypeExporter.PointPtr  must resolve the imported pointer type.
    auto err = tryCompile(R"(
MODULE Importer;
  IMPORT TypeExporter;
  VAR p: TypeExporter.PointPtr;
BEGIN
END Importer.
)");
    CHECK(err.empty());
}

TEST_CASE("loadModuleInterface: imported type usable as procedure parameter type",
          "[codegen][import]") {
    auto err = tryCompile(R"(
MODULE Importer;
  IMPORT TypeExporter;
  PROCEDURE UsePoint(p: TypeExporter.PointPtr);
  BEGIN
  END UsePoint;
BEGIN
END Importer.
)");
    CHECK(err.empty());
}

TEST_CASE("loadModuleInterface: imported type usable as procedure return type",
          "[codegen][import]") {
    auto err = tryCompile(R"(
MODULE Importer;
  IMPORT TypeExporter;
  PROCEDURE MakePoint(): TypeExporter.PointPtr;
  BEGIN
    RETURN NIL
  END MakePoint;
BEGIN
END Importer.
)");
    CHECK(err.empty());
}

TEST_CASE("loadModuleInterface: imported type usable as record extension base",
          "[codegen][import]") {
    // RECORD (TypeExporter.Point) needs the base type to be resolvable.
    auto err = tryCompile(R"(
MODULE Importer;
  IMPORT TypeExporter;
  TYPE Extended* = RECORD (TypeExporter.Point)
    z*: INTEGER
  END;
BEGIN
END Importer.
)");
    CHECK(err.empty());
}

TEST_CASE("loadModuleInterface: import alias resolves types correctly",
          "[codegen][import]") {
    // IMPORT TE := TypeExporter — alias "TE" is used to reference the types.
    auto err = tryCompile(R"(
MODULE Importer;
  IMPORT TE := TypeExporter;
  VAR p: TE.PointPtr;
BEGIN
END Importer.
)");
    CHECK(err.empty());
}

TEST_CASE("loadModuleInterface: non-exported type is NOT importable",
          "[codegen][import]") {
    // PrivateHelper is defined in TypeExporter but not exported (no *).
    // Trying to use it in an importing module must fail.
    auto err = tryCompile(R"(
MODULE Importer;
  IMPORT TypeExporter;
  VAR h: TypeExporter.PrivateHelper;
BEGIN
END Importer.
)");
    CHECK_FALSE(err.empty());
    CHECK(err.find("unknown type") != std::string::npos);
}

TEST_CASE("loadModuleInterface: missing import module compiles without crash",
          "[codegen][import]") {
    // Importing a module whose .Mod file doesn't exist should not crash —
    // loadModuleInterface silently skips missing modules.
    // Type resolution errors will surface only if the missing types are used.
    auto err = tryCompile(R"(
MODULE Importer;
  IMPORT NonExistentModule;
  VAR x: INTEGER;
BEGIN
END Importer.
)");
    // Should succeed — x is INTEGER (local), NonExistentModule types are unused
    CHECK(err.empty());
}

// -----------------------------------------------------------------------
// CHAR literals: single-char strings and hex char constants must be
// emitted as i8 values, not as global string pointers.
//
// The critical case is CONST declarations: "versionkey = 1X" and
// "sentinel = "O"" were previously registered as TypeKind::String
// (global ptr), causing icmp i8 vs ptr verifier failures when
// compared against a CHAR variable.
// -----------------------------------------------------------------------

TEST_CASE("Char literal: hex char constants 0AX-0EX in direct comparison",
          "[codegen][char]") {
    auto err = tryCompile(R"(
MODULE CharTest;
  VAR ch: CHAR;
BEGIN
  IF ch = 0AX THEN END;
  IF ch = 0BX THEN END;
  IF ch = 0CX THEN END;
  IF ch = 0DX THEN END;
  IF ch = 0EX THEN END;
END CharTest.
)");
    CHECK(err.empty());
}

TEST_CASE("Char literal: single-char strings A-E in direct comparison",
          "[codegen][char]") {
    auto err = tryCompile(R"(
MODULE CharTest;
  VAR ch: CHAR;
BEGIN
  IF ch = "A" THEN END;
  IF ch = "B" THEN END;
  IF ch = "C" THEN END;
  IF ch = "D" THEN END;
  IF ch = "E" THEN END;
END CharTest.
)");
    CHECK(err.empty());
}

TEST_CASE("Char literal: hex char CONST used in comparison (was i8 vs ptr bug)",
          "[codegen][char]") {
    // CONST with hex char must be registered as CHAR (i8), not String (ptr).
    // Before fix: genConstDecls stored 1X as a GlobalVariable (String),
    // producing "icmp eq i8 %ch, ptr @.str" which fails LLVM verification.
    auto err = tryCompile(R"(
MODULE CharTest;
  CONST versionkey = 1X; nl = 0AX; cr = 0DX; eof = 0X;
  VAR ch: CHAR;
BEGIN
  IF ch = versionkey THEN END;
  IF ch # nl THEN END;
  IF ch = cr THEN END;
  IF ch = eof THEN END;
END CharTest.
)");
    CHECK(err.empty());
}

TEST_CASE("Char literal: single-char string CONST used in comparison (was i8 vs ptr bug)",
          "[codegen][char]") {
    // CONST with single-char string must also be registered as CHAR (i8).
    auto err = tryCompile(R"(
MODULE CharTest;
  CONST sentinel = "O"; dot = "."; space = " ";
  VAR ch: CHAR;
BEGIN
  IF ch = sentinel THEN END;
  IF ch # dot THEN END;
  IF ch = space THEN END;
END CharTest.
)");
    CHECK(err.empty());
}

TEST_CASE("Char literal: assigned to CHAR variable",
          "[codegen][char]") {
    auto err = tryCompile(R"(
MODULE CharTest;
  CONST cr = 0DX;
  VAR ch: CHAR;
BEGIN
  ch := "A";
  ch := 0AX;
  ch := cr;
  ch := 0X;
END CharTest.
)");
    CHECK(err.empty());
}

TEST_CASE("Char literal: passed as CHAR procedure argument",
          "[codegen][char]") {
    // Char literal or const passed to a CHAR parameter must emit i8, not ptr.
    auto err = tryCompile(R"(
MODULE CharTest;
  CONST nl = 0AX;
  PROCEDURE Eat(c: CHAR);
  BEGIN
  END Eat;
BEGIN
  Eat("A");
  Eat(0DX);
  Eat(nl);
  Eat("E");
END CharTest.
)");
    CHECK(err.empty());
}
