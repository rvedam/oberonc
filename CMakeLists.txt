cmake_minimum_required(VERSION 3.0)
project(oberonc)

# update to using C++17
set(CMAKE_CXX_STANDARD 17)

list(APPEND CMAKE_MODULE_PATH
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake
   )
 
list(APPEND CMAKE_PREFIX_PATH
  "/usr/local/lib/cmake/antlr4"
  )

# find LLVM package
find_package(LLVM)

# set up the ANTLR4 library as a static linkage with access to the 
# ANTLR4 JAR (currently documented as version 4.8)
add_definitions(-DANTLR4CPP_STATIC)
if(APPLE)
  set(ANTLR4_JAR_LOCATION "/usr/local/antlr-4.8-complete.jar")
else()
  set(ANTLR4_JAR_LOCATION "/usr/local/lib/antlr-4.8-complete.jar")
endif()

# set up the required packages to grab the C++ antlr generator and
# antlr runtime dependencies.
find_package(antlr4-generator REQUIRED)
find_package(antlr4-runtime REQUIRED)

# setup the rules to read the appropriate grammar file. The effect of this is
# the creation of a lexer and parser under the oberon namespace. For further
# information, please consult the ANTLR4 C++ documentation to understand the
# parameters passed to antlr4_generate.
antlr4_generate(oberon_parser
    ${CMAKE_CURRENT_SOURCE_DIR}/Oberon.g4 
    BOTH
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
    ${ANTLR4_INCLUDE_DIR_oberon_parser}
    )

set(HEADERS
    CstVisitor.hpp
    Expr.hpp
    SymbolTable.hpp
    )

set(SOURCES
    CstVisitor.cpp
    SymbolTable.cpp
    main.cpp
    )

message("ANTLR4 source files: ${ANTLR4_SRC_FILES_oberon_parser}")


if(APPLE)
  list(REMOVE_ITEM ${ANTLR4_SRC_FILES_oberon_parser} "${ANTLR4_INCLUDE_DIR_oberon_parser}/Oberon.cpp")
endif()

add_executable(${PROJECT_NAME}
    ${HEADERS}
    ${ANTLR4_SRC_FILES_oberon_parser}
    ${SOURCES}
    )

add_dependencies(${PROJECT_NAME} antlr4_static)

target_include_directories(${PROJECT_NAME}
    PUBLIC
      ${LLVM_INCLUDE_DIRS}
      ${ANTLR4_INCLUDE_DIR}
      ${ANTLR4_INCLUDE_DIR_oberon_parser}
      )

target_link_libraries(${PROJECT_NAME} 
    PUBLIC
    ${LLVM_LIBRARIES}
    LLVMSupport
    PRIVATE
    antlr4_static
    )
