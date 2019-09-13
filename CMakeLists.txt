cmake_minimum_required(VERSION 3.0)
project(oberonc)

set(CMAKE_CXX_STANDARD 14)

# find LLVM package
find_package(LLVM)

message("LLVM Include Directory: ${LLVM_INCLUDE_DIRS}")
message("LLVM Lib Directory: ${LLVM_LIBRARY_DIRS}")

set(HEADERS
    lexer_enums.h
    token.h
    scanner.h
    expr_ast.h
    integer_expr_ast.h
    comment_expr_ast.h
    double_expr_ast.h
    module_expr_ast.h
    LogReporter.h
    parser.h
    string_expr_ast.h
    )

set(SOURCES
    main.cpp
    token.t
    scanner.cpp
    integer_expr_ast.cpp
    comment_expr_ast.cpp
    double_expr_ast.cpp
    lexer_enums.cpp
    module_expr_ast.cpp
    LogReporter.cpp
    parser.cpp
    string_expr_ast.cpp
    )

include_directories(${LLVM_INCLUDE_DIRS})

add_executable(${PROJECT_NAME}
    ${HEADERS}
    ${SOURCES}
    )

target_link_libraries(${PROJECT_NAME} ${LLVM_LIBRARIES})