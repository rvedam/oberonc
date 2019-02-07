#include <iostream>
#include <fstream>
#include "lexer.h"
#include "token_stream.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "USAGE: ./oberonc <filename>" << std::endl;
        return -1;
    }
    auto filename = argv[1];
    std::ifstream ifs;
    ifs.open(filename, std::ios::in);
    if (!ifs.is_open()) {
        std::cout << "Filename not open" << std::endl;
        return -1;
    }

    std::string line;
    while(std::getline(ifs, line))
    {

    }
    ifs.close();
    return 0;
}