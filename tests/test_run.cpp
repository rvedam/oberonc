// Execution tests: compile an Oberon program, link it against the hosted
// runtime (liboberon_runtime.a), run it as an ordinary host process, and
// compare its standard output. They check what the generated code does at
// run time, not just that it compiles.
//
// Each program can be compiled in either of oberonc's two type-size modes
// (run()'s `int32` argument):
//
//   * Default: INTEGER and SET are 64 bits, REAL is a double.
//   * --int32: INTEGER and SET are 32 bits, REAL is a float. This matches
//     Niklaus Wirth's 32-bit RISC5 machine, the target of Project Oberon,
//     the operating system whose modules (project-oberon/*.Mod) we compile
//     into the bare-metal kernel. Those modules depend on 32-bit sizes in
//     their record layouts, disk format and arithmetic (e.g. overflow
//     wrap-around), so the kernel build uses this mode.
//
// In --int32 mode a test program must not turn an address into an INTEGER
// (SYSTEM.ADR, ORD of a pointer): these tests run as normal macOS or Linux
// processes, whose addresses lie above 4 GiB and would be truncated. Test
// address arithmetic in the default mode instead.

#include <catch2/catch_test_macros.hpp>
#include "lexer.hpp"
#include "parser.hpp"
#include "codegen.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <system_error>

// mkdtemp (POSIX): <stdlib.h> per the standard, but macOS only declares it
// in <unistd.h> under strict -std=c++20.
#include <stdlib.h>
#include <unistd.h>

#ifndef TEST_MODULE_DIR
#define TEST_MODULE_DIR "tests"
#endif
#ifndef OBERON_RUNTIME_LIB
#error "OBERON_RUNTIME_LIB must be defined by CMake"
#endif

namespace {

// A uniquely named temporary directory that is deleted, with its contents,
// when the object is destroyed.
//
// Interim wrapper: std::filesystem (C++17) has temp_directory_path() but no
// way to create a unique directory, and no RAII owner for one. Replace this
// class with the standard facility if a WG21 proposal for one is adopted.
class TempDir {
public:
    explicit TempDir(const std::string& prefix) {
        std::string tmpl = (std::filesystem::temp_directory_path() /
                            (prefix + "XXXXXX")).string();
        if (::mkdtemp(tmpl.data()) == nullptr)
            throw std::runtime_error("mkdtemp failed for " + tmpl);
        path_ = tmpl;
    }
    ~TempDir() {
        std::error_code ec;                       // never throw from a destructor
        std::filesystem::remove_all(path_, ec);
    }
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;
    TempDir(TempDir&&) = delete;
    TempDir& operator=(TempDir&&) = delete;

    const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

// This process's scratch directory, created on first use and deleted when
// the process exits. Each process gets its own: ctest runs every test case
// as a separate process, and they must not overwrite each other's files.
const std::filesystem::path& scratchDir() {
    static const TempDir dir("oberonc_run_tests_");
    return dir.path();
}

// Compiles and runs `src` (feeding it `input` on stdin); returns its stdout,
// or "COMPILE ERROR: …".
std::string run(const std::string& src, bool int32, const std::string& input = "") {
    static int counter = 0;
    std::string stem = (scratchDir() / ("prog" + std::to_string(counter++) +
                                        (int32 ? "_32" : "_64"))).string();
    try {
        Lexer  lex(src, "<test>");
        Parser parser(lex);
        Module mod = parser.parseModule();
        CodeGen cg(mod.name);
        cg.setModulePaths(std::array<std::string, 1>{TEST_MODULE_DIR});
        cg.setInt32(int32);
        cg.generate(mod);
        cg.writeObject(stem + ".o");
    } catch (const std::runtime_error& e) {
        return std::string("COMPILE ERROR: ") + e.what();
    }
    std::string link = "cc \"" + stem + ".o\" \"" OBERON_RUNTIME_LIB "\" -o \"" + stem + "\"";
    if (std::system(link.c_str()) != 0) return "LINK ERROR";

    std::string out;
    std::ofstream(stem + ".in") << input;
    FILE* p = popen(("\"" + stem + "\" < \"" + stem + ".in\"").c_str(), "r");
    if (!p) return "RUN ERROR";
    std::array<char, 256> buf{};
    while (size_t n = fread(buf.data(), 1, buf.size(), p)) out.append(buf.data(), n);
    int rc = pclose(p);
    if (rc != 0) out += "[exit " + std::to_string(rc) + "]";
    return out;
}

} // namespace

TEST_CASE("Run: DIV and MOD round toward negative infinity", "[run]") {
    const char* src = R"(MODULE T; IMPORT Out;
VAR a, b: INTEGER;
BEGIN a := -7; b := 2;
  Out.Int(a DIV b, 0); Out.Int(a MOD b, 3);
  a := 7; b := -2;
  Out.Int(a DIV b, 3); Out.Int(a MOD b, 3);
  Out.Int((-8) DIV 4, 3); Out.Int((-8) MOD 4, 3); Out.Ln
END T.)";
    for (bool w : {false, true})
        CHECK(run(src, w) == "-4  1 -4 -1 -2  0\n");
}

TEST_CASE("Run: --int32 INTEGER wraps at 32 bits and literals are 32-bit", "[run][int32]") {
    const char* src = R"(MODULE T; IMPORT Out;
CONST Mark = 9B1EA38DH;
VAR x: INTEGER; a: ARRAY 2 OF INTEGER;
BEGIN x := 7FFFFFFFH; INC(x); Out.Int(x, 0);
  a[0] := Mark; Out.Int(ORD(a[0] = Mark), 2); Out.Int(ORD(Mark < 0), 2); Out.Ln
END T.)";
    CHECK(run(src, true) == "-2147483648 1 1\n");
}

TEST_CASE("Run: BYTE, CHAR and BOOLEAN widen without sign extension", "[run]") {
    const char* src = R"(MODULE T; IMPORT Out;
VAR b, c: BYTE; ch: CHAR; t: BOOLEAN;
BEGIN b := 200; c := 100; Out.Int(b + 1, 0); Out.Int(b * c, 6);
  t := TRUE; Out.Int(ORD(t), 2);
  ch := 0C8X; Out.Int(ORD(ch), 4); Out.Int(ORD(ch > "a"), 2);
  IF b >= 80H THEN Out.String(" hi") END; Out.Ln
END T.)";
    for (bool w : {false, true})
        CHECK(run(src, w) == "201 20000 1 200 1 hi\n");
}

TEST_CASE("Run: SET operations, including complement and typed VAL", "[run]") {
    const char* src = R"(MODULE T; IMPORT Out, SYSTEM;
VAR s, u: SET; i: INTEGER;
BEGIN s := {1, 3}; u := SYSTEM.VAL(SET, 4) + s; Out.Int(ORD(u), 0);
  Out.Int(ORD(s / {1}), 3); Out.Int(ORD(s - {3}), 3); Out.Int(ORD(s * {3, 5}), 3);
  INCL(s, 4); EXCL(s, 1); Out.Int(ORD(s), 3);
  i := 2; s := {i .. 4}; Out.Int(ORD(s), 3);
  IF 3 IN s THEN Out.String(" in") END;
  u := {0 .. 31}; IF (-u) * {0 .. 31} = {} THEN Out.String(" ok") END; Out.Ln
END T.)";
    for (bool w : {false, true})
        CHECK(run(src, w) == "14  8  2  8 24 28 in ok\n");
}

TEST_CASE("Run: --int32 LSL/ASR/ROR use 32-bit words and counts mod 32", "[run][int32]") {
    const char* src = R"(MODULE T; IMPORT Out;
VAR x, k: INTEGER;
BEGIN x := 1; Out.Int(LSL(x, 31), 0); Out.Int(ASR(-8, 1), 3);
  Out.Int(ROR(x, 1), 12); k := 24; Out.Int(ASR(256, -k), 2);
  Out.Int(LSL(255, -k), 6); Out.Ln
END T.)";
    CHECK(run(src, true) == "-2147483648 -4 -2147483648 1 65280\n");
}

TEST_CASE("Run: & and OR short-circuit", "[run]") {
    const char* src = R"(MODULE T; IMPORT Out;
TYPE P = POINTER TO R; R = RECORD x: INTEGER END;
VAR p: P;
BEGIN p := NIL;
  IF (p # NIL) & (p.x = 1) THEN Out.String("bad") ELSE Out.String("and") END;
  IF (p = NIL) OR (p.x = 1) THEN Out.String(" or") END; Out.Ln
END T.)";
    for (bool w : {false, true})
        CHECK(run(src, w) == "and or\n");
}

TEST_CASE("Run: pointer and procedure value parameters pass the value", "[run]") {
    const char* src = R"(MODULE T; IMPORT Out;
TYPE P = POINTER TO R; R = RECORD x: INTEGER END;
  Proc = PROCEDURE (q: P);
VAR p: P; f: Proc;
PROCEDURE Set(q: P); BEGIN q.x := 5 END Set;
PROCEDURE Call(g: Proc; q: P); BEGIN g(q) END Call;
BEGIN NEW(p); p.x := 0; Set(p); Out.Int(p.x, 0);
  f := Set; p.x := 0; Call(f, p); Out.Int(p.x, 2); Out.Ln
END T.)";
    for (bool w : {false, true})
        CHECK(run(src, w) == "5 5\n");
}

TEST_CASE("Run: string comparison, assignment and literal LEN", "[run]") {
    const char* src = R"(MODULE T; IMPORT Out;
VAR a, b: ARRAY 16 OF CHAR;
PROCEDURE Len(s: ARRAY OF CHAR): INTEGER; BEGIN RETURN LEN(s) END Len;
PROCEDURE Copy(s: ARRAY OF CHAR; VAR d: ARRAY OF CHAR); BEGIN d := s END Copy;
BEGIN a := "Oberon"; b := "System";
  IF a < b THEN Out.String("lt") END;
  b := a; IF a = b THEN Out.String(" eq") END;
  IF a # "Oberon10" THEN Out.String(" ne") END;
  Copy("Fonts", b); Out.String(" "); Out.String(b);
  Out.Int(Len("abc"), 2); Out.Ln
END T.)";
    for (bool w : {false, true})
        CHECK(run(src, w) == "lt eq ne Fonts 4\n");
}

TEST_CASE("Run: IS and type CASE use dynamic types", "[run][rtti]") {
    const char* src = R"(MODULE T; IMPORT Out;
TYPE Msg = RECORD END;
  AMsg = RECORD (Msg) a: INTEGER END;
  BMsg = RECORD (Msg) b: INTEGER END;
  CMsg = RECORD (BMsg) c: INTEGER END;
  Obj = POINTER TO ObjDesc; ObjDesc = RECORD END;
  Sub = POINTER TO SubDesc; SubDesc = RECORD (ObjDesc) v: INTEGER END;
VAR am: AMsg; cm: CMsg; o: Obj; s: Sub;
PROCEDURE Handle(VAR M: Msg);
BEGIN
  IF M IS AMsg THEN Out.String("A") ELSIF M IS BMsg THEN Out.String("B") ELSE Out.String("?") END;
  CASE M OF CMsg: Out.Int(M.c, 2) | AMsg: Out.Int(M.a, 2) END
END Handle;
PROCEDURE Forward(VAR M: Msg); BEGIN Handle(M) END Forward;
BEGIN am.a := 1; cm.c := 3;
  Handle(am); Out.String(" "); Forward(cm);
  NEW(s); s.v := 7; o := s;
  IF o IS Sub THEN Out.String(" sub") END;
  NEW(o); IF ~(o IS Sub) THEN Out.String(" base") END;
  o := NIL; IF ~(o IS Sub) THEN Out.String(" nil") END; Out.Ln
END T.)";
    for (bool w : {false, true})
        CHECK(run(src, w) == "A 1 B 3 sub base nil\n");
}

TEST_CASE("Run: FOR loops on --int32 control variables", "[run][int32]") {
    const char* src = R"(MODULE T; IMPORT Out;
VAR i, s: INTEGER;
BEGIN s := 0; FOR i := 10 TO 0 BY -2 DO s := s + i END; Out.Int(s, 0);
  FOR i := 1 TO 3 DO Out.Int(i, 2) END; Out.Ln
END T.)";
    for (bool w : {false, true})
        CHECK(run(src, w) == "30 1 2 3\n");
}

TEST_CASE("Run: In.Int/In.Real write only their (--int32-sized) variable", "[run][int32]") {
    const char* src = R"(MODULE T; IMPORT In, Out;
VAR a, b: INTEGER; x: REAL; c: INTEGER;
BEGIN b := 7; c := 9; In.Int(a); In.Real(x);
  Out.Int(a, 0); Out.Int(b, 2); Out.Int(FLOOR(x * 2.0), 2); Out.Int(c, 2); Out.Ln
END T.)";
    for (bool w : {false, true})
        CHECK(run(src, w, "-42 2.5\n") == "-42 7 5 9\n");
}

TEST_CASE("Run: LEN of a named string constant includes its 0X", "[run]") {
    const char* src = R"(MODULE T; IMPORT Out;
CONST msg = "hello";
PROCEDURE Count(s: ARRAY OF CHAR): INTEGER;
  VAR i: INTEGER;
BEGIN i := 0; WHILE (i < LEN(s)) & (s[i] # 0X) DO INC(i) END; RETURN i
END Count;
BEGIN Out.Int(LEN(msg), 0); Out.Int(Count(msg), 2); Out.Ln
END T.)";
    for (bool w : {false, true})
        CHECK(run(src, w) == "6 5\n");
}

TEST_CASE("Run: a type-guarded VAR record actual keeps its address and dynamic type", "[run][rtti]") {
    const char* src = R"(MODULE T; IMPORT Out;
TYPE Msg = RECORD END;
  AMsg = RECORD (Msg) a: INTEGER END;
  BMsg = RECORD (AMsg) b: INTEGER END;
VAR bm: BMsg;
PROCEDURE SetA(VAR M: AMsg);
BEGIN M.a := 5; IF M IS BMsg THEN Out.String("B") END
END SetA;
PROCEDURE Handle(VAR M: Msg);
BEGIN SetA(M(AMsg))
END Handle;
BEGIN bm.a := 0; Handle(bm); Out.Int(bm.a, 2); Out.Ln
END T.)";
    for (bool w : {false, true})
        CHECK(run(src, w) == "B 5\n");
}

TEST_CASE("Run: string comparison stops at the array length", "[run]") {
    const char* src = R"(MODULE T; IMPORT Out;
TYPE Rec = RECORD a: ARRAY 4 OF CHAR; x: CHAR; b: ARRAY 4 OF CHAR; y: CHAR END;
VAR r: Rec;
BEGIN r.a[0] := "a"; r.a[1] := "b"; r.a[2] := "c"; r.a[3] := "d"; r.x := "X";
  r.b[0] := "a"; r.b[1] := "b"; r.b[2] := "c"; r.b[3] := "d"; r.y := "Y";
  IF r.a = r.b THEN Out.String("eq") ELSE Out.String("ne") END; Out.Ln
END T.)";
    for (bool w : {false, true})
        CHECK(run(src, w) == "eq\n");
}

TEST_CASE("Run: a type CASE with no matching arm executes nothing", "[run][rtti]") {
    const char* src = R"(MODULE T; IMPORT Out;
TYPE Msg = RECORD END; AMsg = RECORD (Msg) END; BMsg = RECORD (Msg) END;
VAR bm: BMsg;
PROCEDURE Handle(VAR M: Msg);
BEGIN CASE M OF AMsg: Out.String("A") END; Out.String("done")
END Handle;
BEGIN Handle(bm); Out.Ln
END T.)";
    for (bool w : {false, true})
        CHECK(run(src, w) == "done\n");
}

TEST_CASE("Run: SYSTEM.PUT/GET/BIT/COPY access memory through ADR addresses", "[run][system]") {
    // Default mode only: ADR yields a host address (see the top of the file).
    const char* src = R"(MODULE T; IMPORT Out, SYSTEM;
VAR x, y, i: INTEGER; ch: CHAR; t: BOOLEAN; a, d: ARRAY 4 OF INTEGER;
BEGIN x := 0; SYSTEM.PUT(SYSTEM.ADR(x), 1234); SYSTEM.GET(SYSTEM.ADR(x), y); Out.Int(y, 0);
  SYSTEM.PUT(SYSTEM.ADR(ch), "Q"); Out.Char(" "); Out.Char(ch);
  x := -1; t := FALSE; SYSTEM.PUT(SYSTEM.ADR(x), t); Out.Int(x, 5);
  x := 5; Out.Int(ORD(SYSTEM.BIT(SYSTEM.ADR(x), 0)), 2);
  Out.Int(ORD(SYSTEM.BIT(SYSTEM.ADR(x), 1)), 2); Out.Int(ORD(SYSTEM.BIT(SYSTEM.ADR(x), 2)), 2);
  FOR i := 0 TO 3 DO a[i] := (i + 1) * 10; d[i] := 0 END;
  SYSTEM.COPY(SYSTEM.ADR(a), SYSTEM.ADR(d), 3);
  Out.Int(d[0] + d[1] + d[2], 3); Out.Int(d[3], 2); Out.Ln
END T.)";
    // PUT of a BOOLEAN stores one byte (little-endian hosts): -1 becomes -256.
    CHECK(run(src, false) == "1234 Q -256 1 0 1 60 0\n");
}

TEST_CASE("Run: SYSTEM.ADR of a procedure and of a $hex$ byte string", "[run][system]") {
    const char* src = R"(MODULE T; IMPORT Out, SYSTEM;
TYPE Proc = PROCEDURE;
VAR p: Proc; a: INTEGER; c: CHAR;
PROCEDURE Hi; BEGIN Out.String("hi") END Hi;
BEGIN p := SYSTEM.VAL(Proc, SYSTEM.ADR(Hi)); p;
  a := SYSTEM.ADR($4F4B00$); Out.Char(" ");
  SYSTEM.GET(a, c); WHILE c # 0X DO Out.Char(c); INC(a); SYSTEM.GET(a, c) END; Out.Ln
END T.)";
    CHECK(run(src, false) == "hi OK\n");
}

TEST_CASE("Run: SYSTEM.SIZE, SYSTEM.VAL bit patterns and SYSTEM.UML", "[run][system]") {
    const char* src = R"(MODULE T; IMPORT Out, SYSTEM;
TYPE R = RECORD a: INTEGER; c: CHAR END; S = RECORD c: CHAR; a: INTEGER END;
VAR x: REAL; i: INTEGER;
BEGIN Out.Int(SYSTEM.SIZE(CHAR), 0); Out.Int(SYSTEM.SIZE(INTEGER), 2);
  Out.Int(SYSTEM.SIZE(R), 3); Out.Int(SYSTEM.SIZE(S), 3);
  x := 1.0; i := SYSTEM.VAL(INTEGER, x); Out.Int(i, 20);
  x := SYSTEM.VAL(REAL, i); Out.Int(FLOOR(x * 4.0), 2);
  Out.Int(SYSTEM.UML(10000H, 30000H), 2); Out.Ln
END T.)";
    // Record sizes include the target's alignment padding (NEW allocates
    // SIZE bytes, so they must match the layout the code is emitted with).
    CHECK(run(src, false) == "1 8 16 16 4607182418800017408 4 3\n");
    CHECK(run(src, true)  == "1 4  8  8          1065353216 4 3\n");
}

TEST_CASE("Run: REPEAT runs its body at least once; WHILE with ELSIF", "[run]") {
    const char* src = R"(MODULE T; IMPORT Out;
VAR i, n: INTEGER;
BEGIN i := 0; REPEAT INC(i) UNTIL i >= 5; Out.Int(i, 0);
  n := 0; REPEAT INC(n) UNTIL TRUE; Out.Int(n, 2);
  i := 0; n := 0;
  REPEAT INC(i); IF ODD(i) THEN INC(n) END UNTIL (i = 9) OR (n > 10); Out.Int(n, 2);
  i := 0; n := 0;
  WHILE i < 3 DO INC(i) ELSIF n < 2 DO INC(n) END; Out.Int(i * 10 + n, 3); Out.Ln
END T.)";
    for (bool w : {false, true})
        CHECK(run(src, w) == "5 1 5 32\n");
}

TEST_CASE("Run: integer and CHAR CASE with label lists, ranges and constants", "[run]") {
    // Oberon-07 labels are integers, strings or qualidents, so negative
    // labels must be named constants.
    const char* src = R"(MODULE T; IMPORT Out;
CONST Max = 100; M3 = -3; M1 = -1;
VAR i: INTEGER;
PROCEDURE Kind(i: INTEGER): INTEGER;
  VAR k: INTEGER;
BEGIN
  CASE i OF
    0: k := 0
  | 1, 3, 5: k := 1
  | 6 .. 9, 20: k := 2
  | M3 .. M1: k := 3
  | Max: k := 4
  END;
  RETURN k
END Kind;
PROCEDURE Class(c: CHAR): INTEGER;
  VAR k: INTEGER;
BEGIN
  CASE c OF
    "a" .. "z": k := 1
  | "A" .. "Z": k := 2
  | "0" .. "9", "_": k := 3
  | 0X: k := 4
  END;
  RETURN k
END Class;
BEGIN
  Out.Int(Kind(0), 0); Out.Int(Kind(3), 2); Out.Int(Kind(6), 2); Out.Int(Kind(9), 2);
  Out.Int(Kind(20), 2); Out.Int(Kind(-3), 2); Out.Int(Kind(-1), 2); Out.Int(Kind(Max), 2);
  Out.Int(Class("q"), 2); Out.Int(Class("Q"), 2); Out.Int(Class("7"), 2);
  Out.Int(Class("_"), 2); Out.Int(Class(0X), 2); Out.Ln
END T.)";
    for (bool w : {false, true})
        CHECK(run(src, w) == "0 1 2 2 2 3 3 4 1 2 3 3 4\n");
}

TEST_CASE("Run: constant expressions in CONST declarations and array lengths", "[run]") {
    const char* src = R"(MODULE T; IMPORT Out;
CONST A = 10; B = A * 3 + 2; C = (-7) DIV 2; D = (-7) MOD 2; E = B - A DIV 3;
  F = -7 DIV 2; G = 7 MOD (-2);
  N = (A + 2) DIV 4; H = 0FFH; Neg = -H;
VAR arr: ARRAY N * 2 OF INTEGER; m: ARRAY A, N OF CHAR;
BEGIN Out.Int(B, 0); Out.Int(C, 3); Out.Int(D, 2); Out.Int(E, 3);
  Out.Int(LEN(arr), 2); Out.Int(LEN(m), 3); Out.Int(LEN(m[0]), 2);
  Out.Int(H, 4); Out.Int(Neg, 5); Out.Int(F, 3); Out.Int(G, 3); Out.Ln
END T.)";
    // DIV and MOD fold with floor division, as at run time; a leading minus
    // applies to the whole term, so F = -(7 DIV 2).
    for (bool w : {false, true})
        CHECK(run(src, w) == "32 -4 1 29 6 10 3 255 -255 -3 -1\n");
}

TEST_CASE("Run: ORD of a pointer is its address", "[run]") {
    // Default mode only (see the top of the file). The Bug 4 path: ORD(ptr)
    // passed where an INTEGER is expected.
    const char* src = R"(MODULE T; IMPORT Out, SYSTEM;
TYPE P = POINTER TO R; R = RECORD x: INTEGER END;
VAR p, q: P;
PROCEDURE Show(a: INTEGER); BEGIN IF a # 0 THEN Out.String("addr") END END Show;
BEGIN NEW(p); q := p; Show(ORD(p));
  IF ORD(p) = ORD(q) THEN Out.String(" same") END;
  IF ORD(p) = SYSTEM.ADR(p.x) - 0 THEN Out.String(" field0") END;
  p := NIL; Out.Int(ORD(p), 2); Out.Ln
END T.)";
    CHECK(run(src, false) == "addr same field0 0\n");
}
