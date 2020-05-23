cmake_minimum_required(VERSION 3.0)
project(oberonc)

# update to using C++17
set(CMAKE_CXX_STANDARD 17)

list(APPEND CMAKE_MODULE_PATH
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake
   )

# find LLVM package
find_package(LLVM)

add_definitions(-DANTLR4CPP_STATIC)
set(ANTLR4_JAR_LOCATION /usr/local/antlr-4.8-complete.jar)
find_package(antlr4-generator REQUIRED)
find_package(antlr4-runtime REQUIRED)
set(ANTLR4_CPP_GENERATED_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/antlr4_generated_src")

antlr4_generate(oberon_lexer 
    OberonLexer.g4 
    LEXER 
    FALSE
    TRUE
    "oberon")

message("ANTLR Lexer generated include dir: ${ANTLR4_INCLUDE_DIR_oberon_lexer}")

antlr4_generate(oberon_parser 
    Oberon.g4 
    PARSER 
    FALSE
    TRUE
    "oberon")

if(${ANTLR_FOUND})
  message("ANTLR Executable Path: ${ANTLR_EXECUTABLE}")
  message("ANTLR Include Directory: ${ANTLR4_INCLUDE_DIRS}")
  message("ANTLR Shared Library Directory: ${ANTLR4_SHARED_LIBRARIES}")
  message("ANTLR Static Library Directory: ${ANTLR4_STATIC_LIBRARIES}")
endif()

message("LLVM Include Directory: ${LLVM_INCLUDE_DIRS}")
message("LLVM Lib Directory: ${LLVM_LIBRARY_DIRS}")

include_directories(
    ${PROJECT_BINARY_DIR}
    ${ANTLR4_INCLUDE_DIR}
    ${ANTLR4_INCLUDE_DIR_oberon_lexer}
    ${ANTLR4_INCLUDE_DIR_oberon_parser}
    )

set(HEADERS
    )

set(SOURCES
    main.cpp
    )

add_executable(${PROJECT_NAME}
    ${HEADERS}
    ${SOURCES}
    ${ANTLR4_SRC_FILES_oberon_lexer}
    ${ANTLR4_SRC_FILES_oberon_parser}
    )

add_dependencies(${PROJECT_NAME} antlr4_static)

target_include_directories(${PROJECT_NAME}
    PUBLIC
      ${LLVM_INCLUDE_DIRS}
      ${ANTLR4_INCLUDE_DIR}
      ${ANTLR4_INCLUDE_DIR_oberon_lexer}
      ${ANTLR4_INCLUDE_DIR_oberon_parser}
      )

target_link_libraries(${PROJECT_NAME} 
    PRIVATE
    ${LLVM_LIBRARIES}
    antlr4_static
    )
