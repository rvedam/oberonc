#include "lexer.hpp"
#include "parser.hpp"
#include "codegen.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <filesystem>

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

// Derive output path: replace .Mod extension (or append) with .ll
static std::string outputPath(const std::string& input) {
    std::filesystem::path p(input);
    return (p.parent_path() / p.stem()).string() + ".ll";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "usage: oberonc <file.Mod> [output.ll]\n";
        return 1;
    }

    std::string srcPath = argv[1];
    std::string source  = readFile(srcPath);

    try {
        // ---- Parse ----
        Lexer  lexer(source, srcPath);
        Parser parser(lexer);
        Module mod = parser.parseModule();

        // ---- Code generation ----
        CodeGen cg(mod.name);
        cg.generate(mod);

        // ---- Emit LLVM IR ----
        std::string outPath = (argc >= 3) ? argv[2] : outputPath(srcPath);
        cg.writeIR(outPath);

        std::cout << "OK: '" << srcPath << "' → '" << outPath << "'\n";

    } catch (const ParseError& e) {
        std::cerr << "parse error: " << e.what() << "\n";
        return 1;
    } catch (const std::runtime_error& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
