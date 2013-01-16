#
# Check if ObjectiveC and ObjectiveC++ compilers work
#
include(CMakeTestOBJCCompiler)
#include(CMakeTestOBJCXXCompiler)

#
# Check if the same compile family is used for both C and CXX
#
if (NOT (CMAKE_C_COMPILER_ID STREQUAL CMAKE_CXX_COMPILER_ID))
    message(WARNING "CMAKE_C_COMPILER_ID (${CMAKE_C_COMPILER_ID}) is different "
                    "from CMAKE_CXX_COMPILER_ID (${CMAKE_CXX_COMPILER_ID})."
                    "The final binary may be unusable.")
endif()

# We support building with Clang and gcc. First check 
# what we're using for build.
#
if (CMAKE_C_COMPILER_ID STREQUAL Clang)
    set(CMAKE_COMPILER_IS_CLANG  ON)
    set(CMAKE_COMPILER_IS_GNUCC  OFF)
    set(CMAKE_COMPILER_IS_GNUCXX OFF)
endif()

# Check GCC version:
# GCC older than 4.6 is not supported.
if (CMAKE_COMPILER_IS_GNUCC)
    execute_process(COMMAND ${CMAKE_C_COMPILER} -dumpversion
        OUTPUT_VARIABLE CC_VERSION)
    if (CC_VERSION VERSION_GREATER 4.6 OR CC_VERSION VERSION_EQUAL 4.6)
        message(STATUS
            "${CMAKE_C_COMPILER} version >= 4.6 -- ${CC_VERSION}")
    else()
        message (FATAL_ERROR
            "${CMAKE_C_COMPILER} version should be >= 4.6 -- ${CC_VERSION}")
    endif()
endif()

#
# Perform build type specific configuration.
#
if (CMAKE_COMPILER_IS_GNUCC)
    set (CC_DEBUG_OPT "-ggdb")
else()
    set (CC_DEBUG_OPT "-g")
endif()

set (CMAKE_C_FLAGS_DEBUG
    "${CMAKE_C_FLAGS_DEBUG} ${CC_DEBUG_OPT} -O0")
set (CMAKE_C_FLAGS_RELWITHDEBUGINFO
    "${CMAKE_C_FLAGS_RELWITHDEBUGINFO} ${CC_DEBUG_OPT} -O2")
set (CMAKE_CXX_FLAGS_DEBUG
    "${CMAKE_CXX_FLAGS_DEBUG} ${CC_DEBUG_OPT} -O0")
set (CMAKE_CXX_FLAGS_RELWITHDEBUGINFO
    "${CMAKE_CXX_FLAGS_RELWITHDEBUGINFO} ${CC_DEBUG_OPT} -O2")

unset(CC_DEBUG_OPT)

#
# Set flags for all include files: those maintained by us and
# coming from third parties.
# We must set -fno-omit-frame-pointer here, since we rely
# on frame pointer when getting a backtrace, and it must
# be used consistently across all object files.
# The same reasoning applies to -fno-stack-protector switch.
# Since we began using luajit, which uses gcc stack unwind
# internally, we also need to make sure all code is compiled
# with unwind info.
#

add_compile_flags("C;CXX"
    "-fno-omit-frame-pointer"
    "-fno-stack-protector"
    "-fexceptions"
    "-funwind-tables")

if (NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
# Remove VALGRIND code and assertions in *any* type of release build.
    add_definitions("-DNDEBUG" "-DNVALGRIND")
endif()

#
# Enable @try/@catch/@finaly syntax in Objective C code.
#
add_compile_flags("OBJC;OBJCXX"
    "-fobjc-exceptions")

#
# Invoke C++ constructors/destructors in the Objective C class instances.
#
add_compile_flags("OBJCXX" "-fobjc-call-cxx-cdtors")

#
# Assume that all Objective-C message dispatches (e.g., [receiver message:arg])
# ensure that the receiver is not nil. This allows for more efficient entry
# points in the runtime to be used
#
if (CMAKE_COMPILER_IS_GNUCC)
    add_compile_flags("OBJC;OBJCXX"
        "-fno-nil-receivers")
endif()

if (CMAKE_COMPILER_IS_CLANG)
    add_compile_flags("OBJC"
        "-fobjc-nonfragile-abi"
        "-fno-objc-legacy-dispatch")
endif()

if (CMAKE_COMPILER_IS_CLANGXX)
    add_compile_flags("OBJCXX"
        "-fobjc-nonfragile-abi"
        "-fno-objc-legacy-dispatch")
elseif(CMAKE_COMPILER_IS_GNUCXX)
    # Suppress deprecated warnings in objc/runtime-deprecated.h
    add_compile_flags("OBJCXX"
        " -Wno-deprecated-declarations")
endif()

macro(enable_tnt_compile_flags)
    # Tarantool code is written in GNU C dialect.
    # Additionally, compile it with more strict flags than the rest
    # of the code.

    add_compile_flags("C;OBJC" "-std=gnu99")
    add_compile_flags("CXX;OBJCXX" "-std=gnu++11 -fno-rtti")

    add_compile_flags("C;CXX"
        "-Wall"
        "-Wextra"
        "-Wno-sign-compare"
        "-Wno-strict-aliasing"
    )

    # Only add -Werror if it's a debug build, done by developers using GCC.
    # Community builds should not cause extra trouble.
    if (${CMAKE_BUILD_TYPE} STREQUAL "Debug" AND CMAKE_COMPILER_IS_GNUCC)
        add_compile_flags("C;CXX" "-Werror")
    endif()
endmacro(enable_tnt_compile_flags)

#
# GCC started to warn for unused result starting from 4.2, and
# this is when it introduced -Wno-unused-result
# GCC can also be built on top of llvm runtime (on mac).
#
check_c_compiler_flag("-Wno-unused-result" CC_HAS_WNO_UNUSED_RESULT)
check_c_compiler_flag("-Wno-unused-value" CC_HAS_WNO_UNUSED_VALUE)
check_c_compiler_flag("-fno-strict-aliasing" CC_HAS_FNO_STRICT_ALIASING)
check_c_compiler_flag("-Wno-comment" CC_HAS_WNO_COMMENT)
check_c_compiler_flag("-Wno-parentheses" CC_HAS_WNO_PARENTHESES)

#
# Check for SSE2 intrinsics
#
if (CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_CLANG )
    set(CMAKE_REQUIRED_FLAGS "-msse2")
    check_c_source_runs("
    #include <immintrin.h>

    int main()
    {
    __m128i a = _mm_setzero_si128();
    return 0;
    }"
    CC_HAS_SSE2_INTRINSICS)
endif()

#
# Check for AVX intrinsics
#
if (CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_CLANG )
    set(CMAKE_REQUIRED_FLAGS "-mavx")
    check_c_source_runs("
    #include <immintrin.h>

    int main()
    {
    __m256i a = _mm256_setzero_si256();
    return 0;
    }"
    CC_HAS_AVX_INTRINSICS)
endif()
