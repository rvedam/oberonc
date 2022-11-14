#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include "CstVisitor.hpp"
#include "llvm/IR/IRBuilder.h"

#include "antlr4-runtime.h"
#include "OberonLexer.h"
#include "OberonParser.h"
#include "CstVisitor.hpp"
#include "ErrorReporter.hpp"

int main(int argc, char** argv) 
{
  std::vector<std::string> filenames;
  auto debugMode = false;
  // read the stream
  if(argc < 2) {
    std::cout << "USAGE: ./oberonc [-d] <filenames>..." << std::endl;
    return -1;
  }

  auto argIdx = 1;
  if(std::string{argv[1]} == "-d") 
  {
      argIdx++;
      debugMode = true;
  }

  // read in all the filenames provided by stdin
  ErrorReporter reporter;
  for(;argIdx < argc; ++argIdx)
      filenames.emplace_back(std::string{argv[argIdx]});

  // call the lexer
  for(auto filename : filenames) {
    std::ifstream  ifs(filename);
    if(!ifs.good()) {
      std::cout << "file: " << filename << "does not exist\n";
      return -1;
    }
    std::ostringstream oss;
    oss << ifs.rdbuf();

    antlr4::ANTLRInputStream is(oss.str());
    oberon::OberonLexer lexer(&is);
    lexer.removeErrorListeners();
    antlr4::CommonTokenStream tokens(&lexer);
    oberon::OberonParser parser(&tokens);
    CstVisitor visitor(&reporter);
    visitor.visitModule(parser.module());
  }

  return 0;
}
