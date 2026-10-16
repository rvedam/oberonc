// Execution tests: compile an Oberon program, link it against the hosted
// runtime, run it, and compare its standard output.
//
// These cover run-time semantics (as opposed to "does it compile"), in both
// the default 64-bit INTEGER mode and the Project Oberon --int32 mode.
// Programs must not convert addresses to INTEGER in --int32 mode: host
// addresses do not fit in 32 bits.

#include <catch2/catch_test_macros.hpp>
#include "lexer.hpp"
#include "parser.hpp"
#include "codegen.hpp"

#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#ifndef TEST_MODULE_DIR
#define TEST_MODULE_DIR "tests"
#endif
#ifndef OBERON_RUNTIME_LIB
#error "OBERON_RUNTIME_LIB must be defined by CMake"
#endif

namespace {

// Compiles and runs `src` (feeding it `input` on stdin); returns its stdout,
// or "COMPILE ERROR: …".
std::string run(const std::string& src, bool int32, const std::string& input = "") {
    namespace fs = std::filesystem;
    static int counter = 0;
    fs::path dir = fs::temp_directory_path() / "oberonc_run_tests";
    fs::create_directories(dir);
    std::string stem = (dir / ("prog" + std::to_string(counter++) +
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
