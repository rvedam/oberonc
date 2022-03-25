#include <iostream>
#include <string>
#include <sstream>
#include "CstVisitor.hpp"
#include "llvm/IR/IRBuilder.h"

#include "antlr4-runtime.h"
#include "OberonLexer.h"
#include "OberonParser.h"
#include "CstVisitor.hpp"

int main(int argc, char** argv) 
{
  std::string filename;
  auto debugMode = false;
  // read the stream
  if(argc < 2) {
    std::cout << "USAGE: ./oberonc [-d] <filenames>..." << std::endl;
    return -1;
  }

  if(std::string{argv[1]} == "-d") 
  {
    filename = std::string{argv[2]};
    debugMode = true;
  }
  else 
    filename = std::string{argv[1]};
  
  return 0;
}
