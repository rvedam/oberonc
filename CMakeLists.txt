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
    )

set(SOURCES
    CstVisitor.cpp
    main.cpp
    )

message("ANTLR4 source files: ${ANTLR4_SRC_FILES_oberon_parser}")

set(GENERATED_SRCS
    ${ANTLR4_SRC_FILES_oberon_parser})

list(REMOVE_ITEM ${GENERATED_SRCS} "${ANTLR4_INCLUDE_DIR_oberon_parser}/Oberon.cpp")

add_executable(${PROJECT_NAME}
    ${HEADERS}
    ${SOURCES}
    ${GENERATED_SRCS}
    )

add_dependencies(${PROJECT_NAME} antlr4_static)

target_include_directories(${PROJECT_NAME}
    PUBLIC
      ${LLVM_INCLUDE_DIRS}
      ${ANTLR4_INCLUDE_DIR}
      ${GENERATED_SRCS}
      )

target_link_libraries(${PROJECT_NAME} 
    PRIVATE
    ${LLVM_LIBRARIES}
    antlr4_static
    )
