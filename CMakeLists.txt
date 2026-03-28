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
    core support irreader passes nativecodegen mc asmprinter)

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
# Pass the runtime library path so the compiler can link user programs
target_compile_definitions(oberonc PRIVATE
    OBERON_RUNTIME_LIB="$<TARGET_FILE:oberon_runtime>")

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

# ---- Architecture selection for bare-metal targets --------------------------
set(KERNEL_ARCH "x86_64" CACHE STRING
    "Target architecture for bare-metal kernel (x86_64 or aarch64)")
set_property(CACHE KERNEL_ARCH PROPERTY STRINGS x86_64 aarch64)
message(STATUS "Bare-metal kernel architecture: ${KERNEL_ARCH}")

if(KERNEL_ARCH STREQUAL "x86_64")

    # Runtime library — host compiler is correct for x86_64
    add_library(oberon_runtime_bare STATIC
        runtime/oberon_runtime_bare.cpp
        runtime/platform/x86_64/hal.cpp)
    target_include_directories(oberon_runtime_bare PUBLIC runtime)
    target_compile_options(oberon_runtime_bare PRIVATE
        -ffreestanding -mno-red-zone -O2)

    # Boot stub — assembled by the host assembler
    add_library(boot_bare OBJECT runtime/boot/x86_64/boot.S)
    target_compile_options(boot_bare PRIVATE -ffreestanding)

    add_custom_target(kernel
        COMMAND llc-14 --march=x86-64 --filetype=obj
                ${CMAKE_SOURCE_DIR}/tests/Hello.ll -o ${CMAKE_BINARY_DIR}/hello.o
        COMMAND ld -m elf_x86_64
                -T ${CMAKE_SOURCE_DIR}/runtime/boot/x86_64/kernel.ld
                $<TARGET_OBJECTS:boot_bare>
                ${CMAKE_BINARY_DIR}/hello.o
                $<TARGET_FILE:oberon_runtime_bare>
                -o ${CMAKE_BINARY_DIR}/kernel_x86_64.elf
        DEPENDS boot_bare oberon_runtime_bare
        COMMENT "Linking x86-64 bare-metal kernel → kernel_x86_64.elf")

elseif(KERNEL_ARCH STREQUAL "aarch64")

    # Cross-compiler toolchain — override with -DAARCH64_CC= etc. if needed
    set(AARCH64_CC  "aarch64-linux-gnu-gcc"  CACHE STRING "AArch64 C compiler")
    set(AARCH64_CXX "aarch64-linux-gnu-g++"  CACHE STRING "AArch64 C++ compiler")
    set(AARCH64_LD  "aarch64-linux-gnu-ld"   CACHE STRING "AArch64 linker")

    # Runtime objects — must be built with the cross-compiler
    add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/oberon_runtime_bare.o
        COMMAND ${AARCH64_CXX} -std=c++20 -ffreestanding -O2
                -I${CMAKE_SOURCE_DIR}/runtime
                -c ${CMAKE_SOURCE_DIR}/runtime/oberon_runtime_bare.cpp
                -o ${CMAKE_BINARY_DIR}/oberon_runtime_bare.o
        DEPENDS ${CMAKE_SOURCE_DIR}/runtime/oberon_runtime_bare.cpp
        COMMENT "Cross-compiling AArch64 bare runtime")

    add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/hal.o
        COMMAND ${AARCH64_CXX} -std=c++20 -ffreestanding -O2
                -I${CMAKE_SOURCE_DIR}/runtime
                -c ${CMAKE_SOURCE_DIR}/runtime/platform/aarch64/hal.cpp
                -o ${CMAKE_BINARY_DIR}/hal.o
        DEPENDS ${CMAKE_SOURCE_DIR}/runtime/platform/aarch64/hal.cpp
        COMMENT "Cross-compiling AArch64 HAL")

    add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/liboberon_runtime_bare.a
        COMMAND ${CMAKE_AR} rcs
                ${CMAKE_BINARY_DIR}/liboberon_runtime_bare.a
                ${CMAKE_BINARY_DIR}/oberon_runtime_bare.o
                ${CMAKE_BINARY_DIR}/hal.o
        DEPENDS ${CMAKE_BINARY_DIR}/oberon_runtime_bare.o
                ${CMAKE_BINARY_DIR}/hal.o
        COMMENT "Archiving AArch64 runtime library")

    # Boot stub — must be assembled by the cross-assembler
    add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/boot.o
        COMMAND ${AARCH64_CC} -ffreestanding
                -c ${CMAKE_SOURCE_DIR}/runtime/boot/aarch64/boot.S
                -o ${CMAKE_BINARY_DIR}/boot.o
        DEPENDS ${CMAKE_SOURCE_DIR}/runtime/boot/aarch64/boot.S
        COMMENT "Cross-assembling AArch64 boot stub")

    add_custom_target(kernel
        COMMAND llc --march=aarch64 --filetype=obj
                ${CMAKE_SOURCE_DIR}/tests/Hello.ll -o ${CMAKE_BINARY_DIR}/hello.o
        COMMAND ${AARCH64_LD}
                -T ${CMAKE_SOURCE_DIR}/runtime/boot/aarch64/kernel.ld
                ${CMAKE_BINARY_DIR}/boot.o
                ${CMAKE_BINARY_DIR}/hello.o
                ${CMAKE_BINARY_DIR}/liboberon_runtime_bare.a
                -o ${CMAKE_BINARY_DIR}/kernel_aarch64.elf
        DEPENDS ${CMAKE_BINARY_DIR}/boot.o
                ${CMAKE_BINARY_DIR}/liboberon_runtime_bare.a
        COMMENT "Linking AArch64 bare-metal kernel → kernel_aarch64.elf")

else()
    message(FATAL_ERROR
        "Unknown KERNEL_ARCH '${KERNEL_ARCH}'. Valid values: x86_64, aarch64")
endif()
