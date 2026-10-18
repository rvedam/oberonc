cmake_minimum_required(VERSION 3.16)
project(oberonc C CXX ASM)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(-Wall -Wextra -Wpedantic)

# ---- LLVM (18 on Homebrew macOS, 14 on Debian/Ubuntu) ----------------------
# Probe known install prefixes so the user doesn't have to pass -DLLVM_DIR.
if(NOT LLVM_DIR)
    foreach(ver 18 14)
        foreach(probe
                "/opt/homebrew/opt/llvm@${ver}/lib/cmake/llvm"
                "/usr/lib/llvm-${ver}/lib/cmake/llvm")
            if(EXISTS "${probe}/LLVMConfig.cmake")
                set(LLVM_DIR "${probe}" CACHE PATH "LLVM cmake config dir" FORCE)
                break()
            endif()
        endforeach()
        if(LLVM_DIR)
            break()
        endif()
    endforeach()
endif()
find_package(LLVM REQUIRED CONFIG)
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

# ---- Optional: Valgrind memcheck ------------------------------------------
option(ENABLE_VALGRIND "Add valgrind_tests target for memcheck" OFF)
if(ENABLE_VALGRIND)
    find_program(VALGRIND valgrind REQUIRED)
    set(VALGRIND_BASE
        ${VALGRIND}
        --error-exitcode=1
        --track-origins=yes
        --num-callers=20)
    # Parser/lexer suite: no LLVM globals — check all leak kinds
    set(VALGRIND_PARSER  ${VALGRIND_BASE} --leak-check=full)
    # Codegen suite: LLVM has benign "still reachable" singletons —
    # only report definite/indirect leaks to avoid false positives
    set(VALGRIND_CODEGEN ${VALGRIND_BASE} --leak-check=full
        --show-leak-kinds=definite,indirect)

    add_custom_target(valgrind_tests
        COMMAND ${VALGRIND_PARSER}  $<TARGET_FILE:oberonc_tests>
        COMMAND ${VALGRIND_CODEGEN} $<TARGET_FILE:oberonc_codegen_tests>
        DEPENDS oberonc_tests oberonc_codegen_tests
        COMMENT "Running test suites under Valgrind memcheck"
        USES_TERMINAL)
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

# ---- Test executable (codegen — requires LLVM) ----------------------------
add_executable(oberonc_codegen_tests
    tests/test_codegen.cpp
    tests/test_run.cpp
)
target_link_libraries(oberonc_codegen_tests PRIVATE oberonc_codegen Catch2::Catch2WithMain)
add_dependencies(oberonc_codegen_tests oberon_runtime)
# Inject the absolute path to the tests/ directory so test fixtures are findable
# regardless of working directory; the run tests link against the runtime.
target_compile_definitions(oberonc_codegen_tests PRIVATE
    TEST_MODULE_DIR="${CMAKE_SOURCE_DIR}/tests"
    OBERON_RUNTIME_LIB="$<TARGET_FILE:oberon_runtime>")

include(CTest)
include(Catch)
catch_discover_tests(oberonc_tests)
catch_discover_tests(oberonc_codegen_tests)

# ---- Generate Hello.ll from Hello.Mod (needed by all kernel targets) --------
add_custom_command(
    OUTPUT ${CMAKE_SOURCE_DIR}/tests/Hello.ll
    COMMAND $<TARGET_FILE:oberonc> --emit-llvm
            ${CMAKE_SOURCE_DIR}/tests/Hello.Mod
            -o ${CMAKE_SOURCE_DIR}/tests/Hello.ll
    DEPENDS oberonc ${CMAKE_SOURCE_DIR}/tests/Hello.Mod
    COMMENT "Generating LLVM IR: Hello.Mod → tests/Hello.ll")

add_custom_target(generate_hello_ll
    DEPENDS ${CMAKE_SOURCE_DIR}/tests/Hello.ll)

# ---- Architecture selection for bare-metal targets --------------------------
set(KERNEL_ARCH "x86_64" CACHE STRING
    "Target architecture for bare-metal kernel (x86_64, aarch64, apple-aarch64, apple-x86_64)")
set_property(CACHE KERNEL_ARCH PROPERTY STRINGS x86_64 aarch64 apple-aarch64 apple-x86_64)
message(STATUS "Bare-metal kernel architecture: ${KERNEL_ARCH}")

if(KERNEL_ARCH STREQUAL "x86_64")

    # Runtime library — host compiler is correct for x86_64
    add_library(oberon_runtime_bare STATIC
        runtime/oberon_runtime_bare.cpp
        runtime/platform/x86_64/hal.cpp
        runtime/framebuffer.cpp)
    target_include_directories(oberon_runtime_bare PUBLIC runtime)
    target_compile_options(oberon_runtime_bare PRIVATE
        -ffreestanding -mno-red-zone -O2 -Wall -Wextra -Wpedantic)

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
    set(AARCH64_CXXFLAGS -std=c++20 -ffreestanding -O2 -Wall -Wextra -Wpedantic)
    set(AARCH64_HEADERS
        ${CMAKE_SOURCE_DIR}/runtime/compat_stdint.hpp
        ${CMAKE_SOURCE_DIR}/runtime/platform/hal.hpp)
    add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/oberon_runtime_bare.o
        COMMAND ${AARCH64_CXX} ${AARCH64_CXXFLAGS}
                -I${CMAKE_SOURCE_DIR}/runtime
                -c ${CMAKE_SOURCE_DIR}/runtime/oberon_runtime_bare.cpp
                -o ${CMAKE_BINARY_DIR}/oberon_runtime_bare.o
        DEPENDS ${CMAKE_SOURCE_DIR}/runtime/oberon_runtime_bare.cpp ${AARCH64_HEADERS}
        COMMENT "Cross-compiling AArch64 bare runtime")

    add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/hal.o
        COMMAND ${AARCH64_CXX} ${AARCH64_CXXFLAGS}
                -I${CMAKE_SOURCE_DIR}/runtime
                -c ${CMAKE_SOURCE_DIR}/runtime/platform/aarch64/hal.cpp
                -o ${CMAKE_BINARY_DIR}/hal.o
        DEPENDS ${CMAKE_SOURCE_DIR}/runtime/platform/aarch64/hal.cpp ${AARCH64_HEADERS}
        COMMENT "Cross-compiling AArch64 HAL")

    add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/framebuffer.o
        COMMAND ${AARCH64_CXX} ${AARCH64_CXXFLAGS}
                -I${CMAKE_SOURCE_DIR}/runtime
                -c ${CMAKE_SOURCE_DIR}/runtime/framebuffer.cpp
                -o ${CMAKE_BINARY_DIR}/framebuffer.o
        DEPENDS ${CMAKE_SOURCE_DIR}/runtime/framebuffer.cpp ${AARCH64_HEADERS}
        COMMENT "Cross-compiling AArch64 shared framebuffer")

    add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/liboberon_runtime_bare.a
        COMMAND ${CMAKE_AR} rcs
                ${CMAKE_BINARY_DIR}/liboberon_runtime_bare.a
                ${CMAKE_BINARY_DIR}/oberon_runtime_bare.o
                ${CMAKE_BINARY_DIR}/hal.o
                ${CMAKE_BINARY_DIR}/framebuffer.o
        DEPENDS ${CMAKE_BINARY_DIR}/oberon_runtime_bare.o
                ${CMAKE_BINARY_DIR}/hal.o
                ${CMAKE_BINARY_DIR}/framebuffer.o
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

elseif(KERNEL_ARCH MATCHES "^apple-(aarch64|x86_64)$")

    # Apple Silicon (or Intel Mac) host, cross-compiling with Homebrew LLVM 18:
    # clang --target=<arch>-none-elf for the runtime and boot stub, llc for
    # Oberon IR, ld.lld for ELF output. The kernels run under QEMU:
    #   apple-aarch64: qemu-system-aarch64 -M virt (PL011 UART, ramfb display)
    #   apple-x86_64:  qemu-system-x86_64 PVH boot (COM1 UART, Bochs VBE display)
    set(BARE_ARCH "${CMAKE_MATCH_1}")

    set(LLVM18_BIN "/opt/homebrew/opt/llvm@18/bin"
        CACHE PATH "LLVM 18 bin directory (Homebrew)")
    set(BARE_CXX "${LLVM18_BIN}/clang++" CACHE STRING "Bare-metal C++ compiler")
    set(BARE_CC  "${LLVM18_BIN}/clang"   CACHE STRING "Bare-metal C compiler / assembler")
    set(BARE_LD  "${LLVM18_BIN}/ld.lld"  CACHE STRING "ELF linker")
    set(BARE_AR  "${LLVM18_BIN}/llvm-ar" CACHE STRING "Archive tool")
    set(BARE_LLC "${LLVM18_BIN}/llc"     CACHE STRING "LLVM static compiler")

    if(BARE_ARCH STREQUAL "aarch64")
        set(BARE_TRIPLE    "aarch64-none-elf")
        set(BARE_LLC_MARCH "aarch64")
        set(BARE_ARCH_FLAGS "")
        set(QEMU_NAME      "qemu-system-aarch64")
        set(QEMU_MACHINE   -M virt -cpu cortex-a57 -device ramfb)
    else()
        set(BARE_TRIPLE    "x86_64-none-elf")
        set(BARE_LLC_MARCH "x86-64")
        # No red zone: exception handlers run on the interrupted stack.
        set(BARE_ARCH_FLAGS -mno-red-zone)
        set(QEMU_NAME      "qemu-system-x86_64")
        set(QEMU_MACHINE   -vga std)
    endif()
    set(BARE_PLATFORM "${CMAKE_SOURCE_DIR}/runtime/platform/${BARE_ARCH}")
    set(BARE_BOOT     "${CMAKE_SOURCE_DIR}/runtime/boot/${BARE_ARCH}")

    # -nostdinc++ prevents Homebrew clang from picking up macOS SDK libc++
    # headers (which require OS thread APIs) when targeting bare-metal ELF.
    # -DOBERON_BUILD_ON_APPLE switches compat_stdint.hpp to C-style headers.
    set(BARE_CXXFLAGS
        --target=${BARE_TRIPLE}
        -std=c++20 -ffreestanding -nostdinc++ -O2 ${BARE_ARCH_FLAGS}
        -Wall -Wextra -Wpedantic
        -DOBERON_BUILD_ON_APPLE
        -I${CMAKE_SOURCE_DIR}/runtime)
    # Rewritten only when the target triple changes, so switching KERNEL_ARCH
    # in an existing build tree rebuilds every object instead of reusing the
    # other architecture's.
    set(BARE_ARCH_STAMP "${CMAKE_BINARY_DIR}/bare_arch.stamp")
    file(CONFIGURE OUTPUT "${BARE_ARCH_STAMP}" CONTENT "${BARE_TRIPLE}\n")
    set(BARE_HEADERS
        ${CMAKE_SOURCE_DIR}/runtime/compat_stdint.hpp
        ${CMAKE_SOURCE_DIR}/runtime/platform/hal.hpp
        ${BARE_ARCH_STAMP})

    # Runtime library objects: <name>.o from runtime/<path>
    set(BARE_RUNTIME_OBJS "")
    foreach(pair
            "oberon_runtime_bare:runtime/oberon_runtime_bare.cpp"
            "hal:runtime/platform/${BARE_ARCH}/hal.cpp"
            "framebuffer:runtime/framebuffer.cpp"
            "po_devices:runtime/po_devices.cpp")
        string(REPLACE ":" ";" pair "${pair}")
        list(GET pair 0 obj_name)
        list(GET pair 1 src)
        add_custom_command(
            OUTPUT ${CMAKE_BINARY_DIR}/${obj_name}.o
            COMMAND ${BARE_CXX} ${BARE_CXXFLAGS}
                    -c ${CMAKE_SOURCE_DIR}/${src}
                    -o ${CMAKE_BINARY_DIR}/${obj_name}.o
            DEPENDS ${CMAKE_SOURCE_DIR}/${src} ${BARE_HEADERS}
            COMMENT "Compiling ${src} (${BARE_TRIPLE})")
        list(APPEND BARE_RUNTIME_OBJS ${CMAKE_BINARY_DIR}/${obj_name}.o)
    endforeach()

    # Project Oberon file-system image for the RAM disk (po_devices.cpp).
    # Archive members are only linked when referenced, so the Hello-World
    # kernel does not pull in the disk image.
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    set(PO_DISK_IMAGE "${CMAKE_BINARY_DIR}/po_disk.img")
    add_custom_command(
        OUTPUT ${PO_DISK_IMAGE}
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/mkpodisk.py
                ${PO_DISK_IMAGE} 1024
                Oberon10.Scn.Fnt=${CMAKE_SOURCE_DIR}/project-oberon/Oberon10.Scn.Fnt
                System.Tool=${CMAKE_SOURCE_DIR}/project-oberon/System.Tool.txt
        DEPENDS ${CMAKE_SOURCE_DIR}/tools/mkpodisk.py
                ${CMAKE_SOURCE_DIR}/project-oberon/Oberon10.Scn.Fnt
                ${CMAKE_SOURCE_DIR}/project-oberon/System.Tool.txt
        COMMENT "Building Project Oberon RAM-disk image")

    add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/po_disk_image.o
        COMMAND ${BARE_CC} --target=${BARE_TRIPLE}
                "-DPO_DISK_IMAGE=\"${PO_DISK_IMAGE}\""
                -c ${CMAKE_SOURCE_DIR}/runtime/po_disk_image.S
                -o ${CMAKE_BINARY_DIR}/po_disk_image.o
        DEPENDS ${CMAKE_SOURCE_DIR}/runtime/po_disk_image.S ${PO_DISK_IMAGE} ${BARE_ARCH_STAMP}
        COMMENT "Embedding Project Oberon RAM-disk image"
        VERBATIM)
    list(APPEND BARE_RUNTIME_OBJS ${CMAKE_BINARY_DIR}/po_disk_image.o)

    set(BARE_RUNTIME_LIB ${CMAKE_BINARY_DIR}/liboberon_runtime_bare.a)
    add_custom_command(
        OUTPUT ${BARE_RUNTIME_LIB}
        COMMAND ${CMAKE_COMMAND} -E rm -f ${BARE_RUNTIME_LIB}
        COMMAND ${BARE_AR} rcs ${BARE_RUNTIME_LIB} ${BARE_RUNTIME_OBJS}
        DEPENDS ${BARE_RUNTIME_OBJS}
        COMMENT "Archiving ${BARE_ARCH} runtime library")

    add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/boot.o
        COMMAND ${BARE_CC} --target=${BARE_TRIPLE} -ffreestanding
                -c ${BARE_BOOT}/boot.S
                -o ${CMAKE_BINARY_DIR}/boot.o
        DEPENDS ${BARE_BOOT}/boot.S ${BARE_ARCH_STAMP}
        COMMENT "Assembling ${BARE_ARCH} boot stub")

    # Boot stub + runtime archive only (used by build_kernel.sh).
    add_custom_target(kernel_runtime
        DEPENDS ${CMAKE_BINARY_DIR}/boot.o ${BARE_RUNTIME_LIB})

    set(BARE_LLC_FLAGS --march=${BARE_LLC_MARCH} -mtriple=${BARE_TRIPLE} --filetype=obj)
    set(BARE_LINK ${BARE_LD} -T ${BARE_BOOT}/kernel.ld ${CMAKE_BINARY_DIR}/boot.o)

    add_custom_target(kernel
        COMMAND ${BARE_LLC} ${BARE_LLC_FLAGS}
                ${CMAKE_SOURCE_DIR}/tests/Hello.ll
                -o ${CMAKE_BINARY_DIR}/hello.o
        COMMAND ${BARE_LINK}
                ${CMAKE_BINARY_DIR}/hello.o
                ${BARE_RUNTIME_LIB}
                -o ${CMAKE_BINARY_DIR}/kernel_${BARE_ARCH}.elf
        DEPENDS generate_hello_ll
                ${CMAKE_BINARY_DIR}/boot.o
                ${BARE_RUNTIME_LIB}
        COMMENT "Linking ${BARE_ARCH} bare-metal kernel → kernel_${BARE_ARCH}.elf")

    # Cached per architecture, so switching KERNEL_ARCH picks the right QEMU.
    find_program(QEMU_BIN_${BARE_ARCH} ${QEMU_NAME})
    set(QEMU_BIN "${QEMU_BIN_${BARE_ARCH}}")
    set(KERNEL_QEMU_DISPLAY "none" CACHE STRING
        "QEMU -display backend for the run_* targets (none, cocoa, ...)")
    if(QEMU_BIN)
        add_custom_target(run_kernel
            COMMAND ${QEMU_BIN} ${QEMU_MACHINE}
                    -kernel ${CMAKE_BINARY_DIR}/kernel_${BARE_ARCH}.elf
                    -serial stdio -display none -no-reboot
            DEPENDS kernel
            COMMENT "Running ${BARE_ARCH} kernel under QEMU"
            USES_TERMINAL)
        message(STATUS "QEMU found: run_kernel target available")
    else()
        message(STATUS "QEMU not found: install ${QEMU_NAME} to use run_kernel")
    endif()

    # ---- Project Oberon: all 14 modules → bootable kernel ------------------
    #
    # Compilation model:
    #   - Dependency modules (all except System) use --init-only, emitting
    #     ModuleName_init() as their public entry point.
    #   - System.Mod is the top-level: it emits oberon_main(), which calls
    #     every imported module's _init() in dependency order, then runs
    #     System's own BEGIN…END body.
    #   - All modules are compiled with --int32 (Project Oberon's 32-bit
    #     INTEGER/SET/REAL), which their record layouts and disk format need.
    # Boot sequence: _start → hal_init → oberon_main → (System body enters
    # Oberon.Loop, which never returns).
    #
    # External symbols: Oberon_NEW, Oberon_Trap and the HAL_* functions
    # (framebuffer, heap bounds, timer, mouse/keyboard, RAM-disk sectors), all
    # provided by liboberon_runtime_bare.a.

    set(PO_DIR   "${CMAKE_SOURCE_DIR}/project-oberon")
    set(PO_BUILD "${CMAKE_BINARY_DIR}/project-oberon")
    file(MAKE_DIRECTORY "${PO_BUILD}")
    # Every module's IR depends on the interfaces of the modules it imports.
    file(GLOB PO_SOURCES "${PO_DIR}/*.Mod")

    set(PO_DEPS
        Kernel Display Input FileDir Files Modules Fonts Viewers
        Texts Oberon MenuViewers TextFrames Edit)

    set(PO_OBJ_FILES "")
    foreach(mod IN LISTS PO_DEPS ITEMS System)
        set(ll_out  "${PO_BUILD}/${mod}.ll")
        set(obj_out "${PO_BUILD}/${mod}.o")
        if(mod STREQUAL "System")
            set(mode_flags "")            # top level: emits oberon_main()
        else()
            set(mode_flags --init-only)
        endif()
        add_custom_command(
            OUTPUT  "${ll_out}"
            COMMAND $<TARGET_FILE:oberonc> --emit-llvm ${mode_flags} --int32
                    --module-path "${PO_DIR}"
                    -o "${ll_out}"
                    "${PO_DIR}/${mod}.Mod"
            DEPENDS oberonc ${PO_SOURCES}
            COMMENT "PO: ${mod}.Mod → ${mod}.ll"
            VERBATIM)
        add_custom_command(
            OUTPUT  "${obj_out}"
            COMMAND "${BARE_LLC}" ${BARE_LLC_FLAGS} "${ll_out}" -o "${obj_out}"
            DEPENDS "${ll_out}" "${BARE_ARCH_STAMP}"
            COMMENT "PO: ${mod}.ll → ${mod}.o"
            VERBATIM)
        list(APPEND PO_OBJ_FILES "${obj_out}")
    endforeach()

    set(PO_KERNEL "${CMAKE_BINARY_DIR}/project_oberon_${BARE_ARCH}.elf")
    add_custom_target(project_oberon_kernel
        COMMAND ${BARE_LINK} ${PO_OBJ_FILES} "${BARE_RUNTIME_LIB}" -o "${PO_KERNEL}"
        DEPENDS "${CMAKE_BINARY_DIR}/boot.o" "${BARE_RUNTIME_LIB}" ${PO_OBJ_FILES}
        COMMENT "PO: linking Project Oberon kernel → project_oberon_${BARE_ARCH}.elf"
        VERBATIM)

    if(QEMU_BIN)
        add_custom_target(run_project_oberon
            COMMAND "${QEMU_BIN}" ${QEMU_MACHINE}
                    -kernel "${PO_KERNEL}"
                    -serial stdio -display ${KERNEL_QEMU_DISPLAY} -no-reboot
            DEPENDS project_oberon_kernel
            COMMENT "Running Project Oberon kernel under QEMU"
            USES_TERMINAL)
        message(STATUS "run_project_oberon target available")
    endif()
else()
    message(FATAL_ERROR
        "Unknown KERNEL_ARCH '${KERNEL_ARCH}'. Valid values: x86_64, aarch64, apple-aarch64, apple-x86_64")
endif()
