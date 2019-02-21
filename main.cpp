#include <iostream>
#include <fstream>
#include "lexer_enums.h"
#include "scanner.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "USAGE: ./oberonc <filename>" << std::endl;
        return -1;
    }
    auto filename = argv[1];
    lexer::scanner scanner;
    scanner.read(filename);
    auto tokens = scanner.tokens();
    std::cout << "[";
    for(auto token : tokens) {
        std::cout << "(" << token.type() << ", " << token.value() << ")";
    }
    std::cout << "]" << std::endl;
    return 0;
}