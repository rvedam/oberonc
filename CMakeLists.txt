cmake_minimum_required(VERSION 3.16)
project(oberonc C CXX ASM)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(-Wall -Wextra -Wpedantic)

# ---- LLVM 14 ---------------------------------------------------------------
find_package(LLVM 14 REQUIRED CONFIG
    HINTS /usr/lib/llvm-14/lib/cmake/llvm)
message(STATUS "LLVM version: ${LLVM_PACKAGE_VERSION}")
message(STATUS "LLVM include dir: ${LLVM_INCLUDE_DIRS}")
llvm_map_components_to_libnames(LLVM_LIBS
    core support irreader passes nativecodegen)

# LLVM headers use C++14 feature macros; suppress the warning about exceptions
# being disabled (we keep exceptions enabled in our own code).
add_compile_options(-Wno-unused-parameter)

# ---- Parser/lexer library (no LLVM dependency) ----------------------------
set(LIB_SOURCES
    src/lexer.cpp
    src/parser.cpp
)
add_library(oberonc_lib STATIC ${LIB_SOURCES})
target_include_directories(oberonc_lib PUBLIC src)

# ---- Code-generator library (depends on LLVM) -----------------------------
set(CODEGEN_SOURCES
    src/codegen.cpp
    src/codegen_gen.cpp
)
add_library(oberonc_codegen STATIC ${CODEGEN_SOURCES})
target_include_directories(oberonc_codegen PUBLIC src ${LLVM_INCLUDE_DIRS})
target_compile_definitions(oberonc_codegen PRIVATE ${LLVM_DEFINITIONS})
target_link_libraries(oberonc_codegen PUBLIC oberonc_lib ${LLVM_LIBS})

# ---- Oberon runtime (C++20, extern "C" symbols) ---------------------------
add_library(oberon_runtime STATIC runtime/oberon_runtime.cpp)

# ---- Compiler driver -------------------------------------------------------
add_executable(oberonc src/main.cpp)
target_link_libraries(oberonc PRIVATE oberonc_codegen oberon_runtime)

# ---- Optional: AddressSanitizer -------------------------------------------
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
if(ENABLE_ASAN)
    target_compile_options(oberonc_lib    PUBLIC -fsanitize=address,undefined)
    target_compile_options(oberonc_codegen PUBLIC -fsanitize=address,undefined)
    target_link_options(oberonc           PRIVATE -fsanitize=address,undefined)
endif()

# ---- Catch2 via FetchContent -----------------------------------------------
include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.7.1
)
FetchContent_MakeAvailable(Catch2)

# ---- Test executable (parser/lexer only — no LLVM) ------------------------
add_executable(oberonc_tests
    tests/test_lexer.cpp
    tests/test_parser.cpp
)
target_link_libraries(oberonc_tests PRIVATE oberonc_lib Catch2::Catch2WithMain)

include(CTest)
include(Catch)
catch_discover_tests(oberonc_tests)

# ---- Freestanding (bare-metal) runtime libraries --------------------------

add_library(oberon_runtime_x86_64 STATIC
    runtime/oberon_runtime_bare.cpp
    runtime/platform/x86_64/hal.cpp)
target_include_directories(oberon_runtime_x86_64 PUBLIC runtime)
target_compile_options(oberon_runtime_x86_64 PRIVATE
    -ffreestanding -mno-red-zone -O2)

add_library(oberon_runtime_aarch64 STATIC
    runtime/oberon_runtime_bare.cpp
    runtime/platform/aarch64/hal.cpp)
target_include_directories(oberon_runtime_aarch64 PUBLIC runtime)
target_compile_options(oberon_runtime_aarch64 PRIVATE -ffreestanding -O2)

# ---- Boot object files (assembled by the C compiler) ----------------------

add_library(boot_x86_64  OBJECT runtime/boot/x86_64/boot.S)
target_compile_options(boot_x86_64  PRIVATE -ffreestanding)

add_library(boot_aarch64 OBJECT runtime/boot/aarch64/boot.S)
target_compile_options(boot_aarch64 PRIVATE -ffreestanding)

# ---- Bare-metal kernel link targets ---------------------------------------
# These use add_custom_target so CMake does not inject CRT objects.

add_custom_target(kernel_x86_64
    COMMAND llc-14 --march=x86-64 --filetype=obj
            ${CMAKE_SOURCE_DIR}/tests/Hello.ll -o ${CMAKE_BINARY_DIR}/hello_x86.o
    COMMAND ld -m elf_x86_64
            -T ${CMAKE_SOURCE_DIR}/runtime/boot/x86_64/kernel.ld
            $<TARGET_OBJECTS:boot_x86_64>
            ${CMAKE_BINARY_DIR}/hello_x86.o
            $<TARGET_FILE:oberon_runtime_x86_64>
            -o ${CMAKE_BINARY_DIR}/kernel_x86_64.elf
    DEPENDS boot_x86_64 oberon_runtime_x86_64
    COMMENT "Linking x86-64 bare-metal kernel")

add_custom_target(kernel_aarch64
    COMMAND llc --march=aarch64 --filetype=obj
            ${CMAKE_SOURCE_DIR}/tests/Hello.ll -o ${CMAKE_BINARY_DIR}/hello_aa64.o
    COMMAND aarch64-linux-gnu-ld
            -T ${CMAKE_SOURCE_DIR}/runtime/boot/aarch64/kernel.ld
            $<TARGET_OBJECTS:boot_aarch64>
            ${CMAKE_BINARY_DIR}/hello_aa64.o
            $<TARGET_FILE:oberon_runtime_aarch64>
            -o ${CMAKE_BINARY_DIR}/kernel_aarch64.elf
    DEPENDS boot_aarch64 oberon_runtime_aarch64
    COMMENT "Linking AArch64 bare-metal kernel")
