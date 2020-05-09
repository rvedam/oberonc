cmake_minimum_required(VERSION 3.0)
project(oberonc)

# update to using C++17
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_MODULE_PATH
    /usr/local/lib/cmake/antlr4
   )

# find LLVM package
find_package(LLVM)

add_definitions(-DANTLR4CPP_STATIC)
include(ExternalAntlr4Cpp)
set(ANTLR_EXECUTABLE /usr/local/antlr-4.8-complete.jar)
find_package(ANTLR REQUIRED)

if(${ANTLR_FOUND})
  message("ANTLR Executable Path: ${ANTLR_EXECUTABLE}")
  message("ANTLR Include Directory: ${ANTLR4_INCLUDE_DIRS}")
  message("ANTLR Shared Library Directory: ${ANTLR4_SHARED_LIBRARIES}")
  message("ANTLR Static Library Directory: ${ANTLR4_STATIC_LIBRARIES}")
endif()

message("LLVM Include Directory: ${LLVM_INCLUDE_DIRS}")
message("LLVM Lib Directory: ${LLVM_LIBRARY_DIRS}")

antlr_target(Oberon oberon.g4)

set(HEADERS
    )

set(SOURCES
    main.cpp
    )

include_directories(${LLVM_INCLUDE_DIRS})

add_executable(${PROJECT_NAME}
    ${HEADERS}
    ${SOURCES}
    )

target_link_libraries(${PROJECT_NAME} ${LLVM_LIBRARIES})
