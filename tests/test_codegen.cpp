#include <catch2/catch_test_macros.hpp>
#include "lexer.hpp"
#include "parser.hpp"
#include "codegen.hpp"

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
        cg.setModulePaths({modDir});
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
