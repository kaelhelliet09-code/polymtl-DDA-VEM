set(CMAKE_SYSTEM_NAME               Generic)
set(CMAKE_SYSTEM_PROCESSOR          arm)

set(CMAKE_C_COMPILER_ID GNU)
set(CMAKE_CXX_COMPILER_ID GNU)

# STM32Cube installs Ninja beside its other versioned bundles, but does not
# necessarily add it to a fresh shell's PATH. The toolchain file is evaluated
# early enough to provide the generator program before compiler checks begin.
if(CMAKE_HOST_WIN32 AND NOT CMAKE_MAKE_PROGRAM AND
   DEFINED ENV{LOCALAPPDATA})
    file(GLOB _STM32_NINJA_PROGRAMS
        "$ENV{LOCALAPPDATA}/stm32cube/bundles/ninja/*/bin/ninja.exe")
    list(SORT _STM32_NINJA_PROGRAMS COMPARE NATURAL ORDER DESCENDING)
    if(_STM32_NINJA_PROGRAMS)
        list(GET _STM32_NINJA_PROGRAMS 0 _STM32_NINJA_PROGRAM)
        set(CMAKE_MAKE_PROGRAM "${_STM32_NINJA_PROGRAM}" CACHE FILEPATH
            "Build program used by the selected CMake generator")
    endif()
endif()

# Prefer an explicit toolchain directory, then PATH, then an installed
# STM32Cube bundle. This keeps CLI builds reproducible without embedding one
# developer's absolute installation path in the repository.
set(STM32_GNU_TOOLCHAIN_BIN "" CACHE PATH
    "Directory containing arm-none-eabi-gcc and related tools")

if(STM32_GNU_TOOLCHAIN_BIN)
    set(_ARM_TOOLCHAIN_BIN "${STM32_GNU_TOOLCHAIN_BIN}")
else()
    find_program(_ARM_GCC_ON_PATH NAMES arm-none-eabi-gcc)
    if(_ARM_GCC_ON_PATH)
        get_filename_component(_ARM_TOOLCHAIN_BIN
            "${_ARM_GCC_ON_PATH}" DIRECTORY)
    elseif(CMAKE_HOST_WIN32 AND DEFINED ENV{LOCALAPPDATA})
        file(GLOB _STM32_GNU_BUNDLE_BINS LIST_DIRECTORIES true
            "$ENV{LOCALAPPDATA}/stm32cube/bundles/gnu-tools-for-stm32/*/bin")
        list(SORT _STM32_GNU_BUNDLE_BINS COMPARE NATURAL ORDER DESCENDING)
        foreach(_CANDIDATE_BIN IN LISTS _STM32_GNU_BUNDLE_BINS)
            if(EXISTS "${_CANDIDATE_BIN}/arm-none-eabi-gcc.exe")
                set(_ARM_TOOLCHAIN_BIN "${_CANDIDATE_BIN}")
                break()
            endif()
        endforeach()
    endif()
endif()

if(NOT _ARM_TOOLCHAIN_BIN)
    message(FATAL_ERROR
        "GNU Arm Embedded toolchain not found. Add it to PATH or configure "
        "-DSTM32_GNU_TOOLCHAIN_BIN=<toolchain-bin-directory>.")
endif()

if(CMAKE_HOST_WIN32)
    set(_ARM_TOOL_SUFFIX ".exe")
else()
    set(_ARM_TOOL_SUFFIX "")
endif()

set(TOOLCHAIN_PREFIX "${_ARM_TOOLCHAIN_BIN}/arm-none-eabi-")

set(CMAKE_C_COMPILER                "${TOOLCHAIN_PREFIX}gcc${_ARM_TOOL_SUFFIX}")
set(CMAKE_ASM_COMPILER              ${CMAKE_C_COMPILER})
set(CMAKE_CXX_COMPILER              "${TOOLCHAIN_PREFIX}g++${_ARM_TOOL_SUFFIX}")
set(CMAKE_LINKER                    "${TOOLCHAIN_PREFIX}g++${_ARM_TOOL_SUFFIX}")
set(CMAKE_OBJCOPY                   "${TOOLCHAIN_PREFIX}objcopy${_ARM_TOOL_SUFFIX}")
set(CMAKE_SIZE                      "${TOOLCHAIN_PREFIX}size${_ARM_TOOL_SUFFIX}")

set(CMAKE_EXECUTABLE_SUFFIX_ASM     ".elf")
set(CMAKE_EXECUTABLE_SUFFIX_C       ".elf")
set(CMAKE_EXECUTABLE_SUFFIX_CXX     ".elf")

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# MCU specific flags
set(TARGET_FLAGS "-mcpu=cortex-m0plus ")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${TARGET_FLAGS}")
set(CMAKE_ASM_FLAGS "${CMAKE_C_FLAGS} -x assembler-with-cpp -MMD -MP")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -fdata-sections -ffunction-sections -fstack-usage")

# The cyclomatic-complexity parameter must be defined for the Cyclomatic complexity feature in STM32CubeIDE to work.
# However, most GCC toolchains do not support this option, which causes a compilation error; for this reason, the feature is disabled by default.
# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fcyclomatic-complexity")

set(CMAKE_C_FLAGS_DEBUG "-O0 -g3")
set(CMAKE_C_FLAGS_RELEASE "-Os -g0")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g3")
set(CMAKE_CXX_FLAGS_RELEASE "-Os -g0")

set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -fno-rtti -fno-exceptions -fno-threadsafe-statics")

set(CMAKE_EXE_LINKER_FLAGS "${TARGET_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -T \"${CMAKE_SOURCE_DIR}/linker/STM32G0B1_DDA.ld\"")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --specs=nano.specs")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-Map=${CMAKE_PROJECT_NAME}.map -Wl,--gc-sections")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--print-memory-usage")
set(TOOLCHAIN_LINK_LIBRARIES "m")
