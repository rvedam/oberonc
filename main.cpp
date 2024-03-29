#include <iostream>
#include <string>
#include <vector>
#include <llvm/IR/LLVMContext.h>

#include "ErrorReporter.hpp"
#include "Tokenizer.hpp"
#include "OberonParser.hpp"


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
    Tokenizer tokenizer(filename);
    tokenizer.scan();

    OberonParser parser(&reporter);
    parser.parseModule(&tokenizer);
    // TODO: Need to move this into its own class
    llvm::LLVMContext ctx;
  }

  // report any errors found in code.
  reporter.printErrors();
  return 0;
}
