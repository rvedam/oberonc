#include "lexer.hpp"
#include "parser.hpp"
#include "codegen.hpp"

#include <array>
#include <fstream>
#include <iostream>
#include <sstream>
#include <filesystem>
#include <cstdlib>
#include <string>
#include <string_view>

static std::string readFile(const std::string& path) {
    std::ifstream f(path);
    if (!f) {
        std::cerr << "error: cannot open '" << path << "'\n";
        std::exit(1);
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// Derive a stem path (no extension) from the input file
static std::string stemPath(std::string_view input) {
    std::filesystem::path p(input);
    return (p.parent_path() / p.stem()).string();
}

// Invoke system linker: cc <objFiles...> <runtimeLib> -o <exeOut>
static void link(const std::vector<std::string>& objFiles, const std::string& exeOut) {
#ifndef OBERON_RUNTIME_LIB
#error "OBERON_RUNTIME_LIB must be defined by CMake"
#endif
    std::string cmd = "cc";
    for (auto& f : objFiles) cmd += " \"" + f + "\"";
    cmd += " \"" OBERON_RUNTIME_LIB "\" -o \"" + exeOut + "\"";
    int rc = std::system(cmd.c_str());
    if (rc != 0)
        throw std::runtime_error("linker invocation failed (exit " + std::to_string(rc) + ")");
}

// Compile a single Oberon source file to a native object, returning the obj path
static std::string compileToObject(const std::string& modSrcPath) {
    std::string src = [&] {
        std::ifstream f(modSrcPath);
        if (!f) throw std::runtime_error("cannot open '" + modSrcPath + "'");
        std::ostringstream ss; ss << f.rdbuf(); return ss.str();
    }();

    std::filesystem::path p(modSrcPath);
    std::string stem = (p.parent_path() / p.stem()).string();

    Lexer  lex(src, modSrcPath);
    Parser parser(lex);
    Module mod = parser.parseModule();

    CodeGen cg(mod.name);
    std::string dir = p.parent_path().string();
    if (dir.empty()) dir = ".";
    cg.setModulePaths(std::array<std::string, 1>{dir});
    cg.setModuleInitOnly(true); // don't emit oberon_main for imported modules
    cg.generate(mod);

    std::string objPath = stem + ".o";
    cg.writeObject(objPath);
    return objPath;
}

static void usage() {
    std::cerr <<
        "usage: oberonc [--emit-llvm] [--init-only] [--module-path <dir>] [-o output] <file.Mod>\n"
        "  --emit-llvm        emit LLVM IR (.ll) instead of linking an executable\n"
        "  --init-only        emit ModuleName_init() instead of oberon_main()\n"
        "                     (use when compiling library/dependency modules)\n"
        "  --module-path dir  add a directory to the module search path (repeatable)\n"
        "  -o output          output file path (executable, or .ll with --emit-llvm)\n";
}

int main(int argc, char* argv[]) {
    bool emitLLVM   = false;
    bool initOnly   = false;
    std::string outputArg;
    std::string srcPath;
    std::vector<std::string> extraModulePaths;

    for (int i = 1; i < argc; ++i) {
        std::string_view a = argv[i];
        if (a == "--emit-llvm") {
            emitLLVM = true;
        } else if (a == "--init-only") {
            initOnly = true;
        } else if (a == "--module-path") {
            if (++i >= argc) { usage(); return 1; }
            extraModulePaths.push_back(argv[i]);
        } else if (a == "-o") {
            if (++i >= argc) { usage(); return 1; }
            outputArg = argv[i];
        } else if (a[0] == '-') {
            std::cerr << "error: unknown flag '" << a << "'\n";
            usage();
            return 1;
        } else {
            if (!srcPath.empty()) { usage(); return 1; }
            srcPath = a;
        }
    }

    if (srcPath.empty()) { usage(); return 1; }

    std::string source = readFile(srcPath);
    std::string stem   = stemPath(srcPath);

    try {
        Lexer  lexer(source, srcPath);
        Parser parser(lexer);
        Module mod = parser.parseModule();

        CodeGen cg(mod.name);
        // Source file's directory is always searched first; --module-path dirs follow
        std::string srcDir = std::filesystem::path(srcPath).parent_path().string();
        if (srcDir.empty()) srcDir = ".";
        std::vector<std::string> modulePaths = {srcDir};
        modulePaths.insert(modulePaths.end(), extraModulePaths.begin(), extraModulePaths.end());
        cg.setModulePaths(modulePaths);
        if (initOnly) cg.setModuleInitOnly(true);
        cg.generate(mod);

        if (emitLLVM) {
            std::string out = outputArg.empty() ? stem + ".ll" : outputArg;
            cg.writeIR(out);
            std::cout << "OK: '" << srcPath << "' → '" << out << "'\n";
        } else {
            // Emit object file then link (including any imported user modules)
            std::string objPath = stem + ".o";
            std::string exePath = outputArg.empty() ? stem : outputArg;
            cg.writeObject(objPath);

            // Compile imported user modules to object files
            std::vector<std::string> allObjs = {objPath};
            std::vector<std::string> tempObjs;
            for (auto& [alias, modName] : cg.loadedUserModules()) {
                // Find the source path (same logic as loadModuleInterface)
                std::string modSrcPath = srcDir + "/" + modName + ".Mod";
                if (std::filesystem::exists(modSrcPath)) {
                    std::string modObj = compileToObject(modSrcPath);
                    allObjs.push_back(modObj);
                    tempObjs.push_back(modObj);
                }
            }

            link(allObjs, exePath);

            std::filesystem::remove(objPath);
            for (auto& t : tempObjs) std::filesystem::remove(t);
            std::cout << "OK: '" << srcPath << "' → '" << exePath << "'\n";
        }

    } catch (const ParseError& e) {
        std::cerr << "parse error: " << e.what() << "\n";
        return 1;
    } catch (const std::runtime_error& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
