cmake_minimum_required(VERSION 3.0)
project(oberonc)

set(CMAKE_CXX_STANDARD 17)

# find LLVM package
find_package(LLVM)

message("LLVM Include Directory: ${LLVM_INCLUDE_DIRS}")
message("LLVM Lib Directory: ${LLVM_LIBRARY_DIRS}")

set(HEADERS
    lexer.h
    token.h
    token_stream.h
    )

set(SOURCES
    main.cpp
    token.cpp
    token_stream.cpp
    )

include_directories(${LLVM_INCLUDE_DIRS})

add_executable(${PROJECT_NAME}
    ${HEADERS}
    ${SOURCES}
    )

target_link_libraries(${PROJECT_NAME} ${LLVM_LIBRARIES})