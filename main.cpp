#include <iostream>
#include "scanner.h"
#include "LogReporter.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "USAGE: ./oberonc <filename>" << std::endl;
        return -1;
    }
    auto filename = argv[1];
    utils::LogReporter reporter;
    lexer::scanner scanner(&reporter);
    scanner.read(filename);
    scanner.printTokens();
    return 0;
}