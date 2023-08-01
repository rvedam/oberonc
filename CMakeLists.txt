cmake_minimum_required(VERSION 3.0)
project(oberonc)

# update to using C++17
set(CMAKE_CXX_STANDARD 17)

list(APPEND CMAKE_MODULE_PATH
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake
   )

# find LLVM package
find_package(LLVM)

# grab all the LLVM libraries (will prune later on)
execute_process(COMMAND llvm-config --libs all
                OUTPUT_VARIABLE llvm_libraries)
execute_process(COMMAND llvm-config --cxxflags all
                OUTPUT_VARIABLE llvm_cxx_flags)

message("LLVM Include Directory: ${LLVM_INCLUDE_DIRS}")
message("LLVM Lib Directory: ${LLVM_LIBRARY_DIRS}")
message("LLVM Libraries: ${llvm_libraries}")

include_directories(
    ${PROJECT_BINARY_DIR}
    ${ANTLR4_INCLUDE_DIR}
    ${ANTLR4_INCLUDE_DIR_oberon_parser}
    )

set(HEADERS
    ArrayTypeSymbol.hpp
    ErrorReporter.hpp
    Expr.hpp
    FunctionSymbol.hpp
    Symbol.hpp
    SymbolTable.hpp
    Tokenizer.hpp
    TokenType.hpp
    OberonParser.hpp
    Token.hpp)

set(SOURCES
    ArrayTypeSymbol.cpp
    Symbol.cpp
    SymbolTable.cpp
    ErrorReporter.cpp
    main.cpp
    FunctionSymbol.cpp
    PointerTypeSymbol.cpp
    PointerTypeSymbol.hpp
    Tokenizer.cpp
    TokenType.cpp
    OberonParser.cpp
    Token.cpp)

add_executable(${PROJECT_NAME}
    ${HEADERS}
    ${SOURCES}
    )

target_include_directories(${PROJECT_NAME}
    PUBLIC
      ${LLVM_INCLUDE_DIRS}
      )

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${llvm_options}")

target_link_libraries(${PROJECT_NAME}
    PUBLIC
    LLVMCore
    )
