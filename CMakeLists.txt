cmake_minimum_required(VERSION 3.13)
project(oberonc)

set(CMAKE_CXX_STANDARD 17)

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

add_executable(${PROJECT_NAME}
        ${HEADERS}
        ${SOURCES}
        )