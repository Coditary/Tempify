cmake_minimum_required(VERSION 3.31)

project(Tempify VERSION 0.1.2 LANGUAGES CXX)

include(CTest)
include(FetchContent)
include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(TEMPIFY_ENABLE_COVERAGE "Enable GCC/GCov coverage instrumentation" OFF)
option(TEMPIFY_BUILD_FUZZERS "Build Tempify libFuzzer targets" OFF)
set(TEMPIFY_MSAN_LIBCXX_PREFIX ""
    CACHE PATH "Install prefix of a MemorySanitizer-instrumented libc++ (required for reliable MSan runs)")

if(TEMPIFY_MSAN_LIBCXX_PREFIX)
    set(PREBYTE_MSAN_LIBCXX_PREFIX "${TEMPIFY_MSAN_LIBCXX_PREFIX}" CACHE PATH "MSan libc++ prefix" FORCE)
endif()

if(MSVC AND NOT DEFINED CMAKE_MSVC_RUNTIME_LIBRARY)
    if(DEFINED VCPKG_TARGET_TRIPLET AND VCPKG_TARGET_TRIPLET MATCHES "windows-static")
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
endif()

find_package(CLI11 CONFIG REQUIRED)

set(TEMPIFY_PREBYTE_VERSION "v1.1.1" CACHE STRING "Pinned Prebyte git tag")
set(TEMPIFY_PREBYTE_SOURCE_DIR "" CACHE PATH "Optional local Prebyte source root")

set(PREBYTE_BUILD_TESTS OFF CACHE BOOL "Build Prebyte tests" FORCE)
set(PREBYTE_BUILD_BENCHMARKS OFF CACHE BOOL "Build Prebyte benchmarks" FORCE)
set(PREBYTE_INSTALL_SELF OFF CACHE BOOL "Install Prebyte artifacts" FORCE)

set(TEMPIFY_PREBYTE_BINARY_DIR "${CMAKE_BINARY_DIR}/_deps/prebyte-build")

if(TEMPIFY_PREBYTE_SOURCE_DIR)
    get_filename_component(TEMPIFY_PREBYTE_ROOT "${TEMPIFY_PREBYTE_SOURCE_DIR}" ABSOLUTE BASE_DIR "${CMAKE_SOURCE_DIR}")
    if(NOT EXISTS "${TEMPIFY_PREBYTE_ROOT}/CMakeLists.txt")
        message(FATAL_ERROR
            "TEMPIFY_PREBYTE_SOURCE_DIR='${TEMPIFY_PREBYTE_SOURCE_DIR}' is invalid. "
            "Expected Prebyte source root containing CMakeLists.txt."
        )
    endif()
    if(NOT EXISTS "${TEMPIFY_PREBYTE_ROOT}/src/main/include/PrebyteEngine.h")
        message(FATAL_ERROR
            "TEMPIFY_PREBYTE_SOURCE_DIR='${TEMPIFY_PREBYTE_SOURCE_DIR}' is invalid. "
            "Expected Prebyte source root containing src/main/include/PrebyteEngine.h."
        )
    endif()

    message(STATUS "Using local Prebyte source: ${TEMPIFY_PREBYTE_ROOT}")
    add_subdirectory("${TEMPIFY_PREBYTE_ROOT}" "${TEMPIFY_PREBYTE_BINARY_DIR}" EXCLUDE_FROM_ALL)
else()
    find_package(Git QUIET)
    if(NOT GIT_FOUND)
        message(FATAL_ERROR
            "Fetching Prebyte from https://github.com/Coditary/Prebyte.git at tag '${TEMPIFY_PREBYTE_VERSION}' requires git. "
            "Install git or configure with -DTEMPIFY_PREBYTE_SOURCE_DIR=/abs/path/to/Prebyte."
        )
    endif()

    FetchContent_Declare(
        prebyte
        GIT_REPOSITORY https://github.com/Coditary/Prebyte.git
        GIT_TAG ${TEMPIFY_PREBYTE_VERSION}
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
        EXCLUDE_FROM_ALL
    )
    message(STATUS "Fetching Prebyte ${TEMPIFY_PREBYTE_VERSION} from https://github.com/Coditary/Prebyte.git")
    FetchContent_MakeAvailable(prebyte)
endif()

file(GLOB_RECURSE TEMPIFY_SOURCES CONFIGURE_DEPENDS src/main/cpp/*.cpp)
list(FILTER TEMPIFY_SOURCES EXCLUDE REGEX ".*/main\\.cpp$")

add_library(tempify_core STATIC ${TEMPIFY_SOURCES})
target_include_directories(tempify_core PUBLIC src/main/include)
target_link_libraries(tempify_core PUBLIC prebyte_core CLI11::CLI11)

add_executable(tempify src/main/cpp/main.cpp)
target_link_libraries(tempify PRIVATE tempify_core)

if(TEMPIFY_ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        target_compile_options(tempify_core PUBLIC --coverage -O0 -g)
        target_link_options(tempify_core PUBLIC --coverage)
        target_compile_options(tempify PRIVATE --coverage -O0 -g)
        target_link_options(tempify PRIVATE --coverage)
    else()
        message(FATAL_ERROR "TEMPIFY_ENABLE_COVERAGE requires GCC-compatible coverage flags")
    endif()
endif()

install(TARGETS tempify RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES README.md LICENSE DESTINATION ${CMAKE_INSTALL_DATADIR}/tempify)

if(TEMPIFY_MSAN_LIBCXX_PREFIX AND EXISTS "${TEMPIFY_MSAN_LIBCXX_PREFIX}/lib/libc++.so")
    function(tempify_apply_msan_libcxx target)
        if(NOT TARGET ${target})
            return()
        endif()
        target_link_directories(${target} PRIVATE "${TEMPIFY_MSAN_LIBCXX_PREFIX}/lib")
        target_link_options(${target} PRIVATE
            "LINKER:-rpath,${TEMPIFY_MSAN_LIBCXX_PREFIX}/lib"
            -lc++
            -lc++abi
        )
    endfunction()

    tempify_apply_msan_libcxx(tempify_core)
    tempify_apply_msan_libcxx(tempify)
endif()

if(BUILD_TESTING)
    set(TEMPIFY_TEST_GLOBS
        tests/tempify/*.cpp
        tests/correctness/e2e/*.cpp
        tests/security/e2e/*.cpp
        tests/portability/cli/*.cpp
        tests/concurrency/e2e/*.cpp
    )
    set(TEMPIFY_TEST_SOURCES)
    foreach(TEMPIFY_TEST_GLOB IN LISTS TEMPIFY_TEST_GLOBS)
        file(GLOB TEMPIFY_TEST_GLOB_SOURCES CONFIGURE_DEPENDS ${TEMPIFY_TEST_GLOB})
        list(APPEND TEMPIFY_TEST_SOURCES ${TEMPIFY_TEST_GLOB_SOURCES})
    endforeach()
    set(TEMPIFY_TEST_ENVIRONMENT
        "TEMPIFY_TEST_BINARY=$<TARGET_FILE:tempify>;XDG_DATA_HOME=${CMAKE_SOURCE_DIR}/tests/test_workspace/.xdg-data;XDG_CONFIG_HOME=${CMAKE_SOURCE_DIR}/tests/test_workspace/.xdg-config")

    add_executable(tempify_tests
        tests/support/TestHarness.cpp
        tests/support/TestMain.cpp
        tests/support/CliProcess.cpp
        ${TEMPIFY_TEST_SOURCES}
    )
    target_include_directories(tempify_tests PRIVATE tests/support src/main/include)
    target_compile_definitions(tempify_tests PRIVATE
        TEMPIFY_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
        TEMPIFY_CLI_BINARY="$<TARGET_FILE:tempify>"
    )
    target_link_libraries(tempify_tests PRIVATE tempify_core)
    add_dependencies(tempify_tests tempify)

    if(TEMPIFY_MSAN_LIBCXX_PREFIX AND EXISTS "${TEMPIFY_MSAN_LIBCXX_PREFIX}/lib/libc++.so")
        tempify_apply_msan_libcxx(tempify_tests)
    endif()

    if(TEMPIFY_ENABLE_COVERAGE)
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
            target_compile_options(tempify_tests PRIVATE --coverage -O0 -g)
            target_link_options(tempify_tests PRIVATE --coverage)
        endif()
    endif()

    if(TEMPIFY_TEST_SOURCES)
        set(TEMPIFY_DISCOVERED_TEST_NAMES)
        foreach(TEMPIFY_TEST_SOURCE IN LISTS TEMPIFY_TEST_SOURCES)
            file(STRINGS ${TEMPIFY_TEST_SOURCE} TEMPIFY_TEST_DECLARATIONS REGEX "^[ \t]*TEST_CASE\\([A-Za-z_][A-Za-z0-9_]*\\)")

            foreach(TEMPIFY_TEST_DECLARATION IN LISTS TEMPIFY_TEST_DECLARATIONS)
                string(REGEX MATCH "TEST_CASE\\(([A-Za-z_][A-Za-z0-9_]*)\\)" TEMPIFY_TEST_MATCH "${TEMPIFY_TEST_DECLARATION}")
                set(TEMPIFY_TEST_NAME ${CMAKE_MATCH_1})

                if(TEMPIFY_TEST_NAME STREQUAL "")
                    continue()
                endif()

                if(TEMPIFY_TEST_NAME IN_LIST TEMPIFY_DISCOVERED_TEST_NAMES)
                    continue()
                endif()

                list(APPEND TEMPIFY_DISCOVERED_TEST_NAMES ${TEMPIFY_TEST_NAME})
                add_test(
                    NAME ${TEMPIFY_TEST_NAME}
                    COMMAND $<TARGET_FILE:tempify_tests> --run-test ${TEMPIFY_TEST_NAME}
                )
                set_tests_properties(${TEMPIFY_TEST_NAME} PROPERTIES
                    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                    ENVIRONMENT "${TEMPIFY_TEST_ENVIRONMENT}"
                )
            endforeach()
        endforeach()
    endif()

    add_test(NAME tempify_help COMMAND tempify --help)
    set_tests_properties(tempify_help PROPERTIES
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        ENVIRONMENT "${TEMPIFY_TEST_ENVIRONMENT}"
    )

    add_test(NAME tempify_list COMMAND tempify list)
    set_tests_properties(tempify_list PROPERTIES
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        ENVIRONMENT "${TEMPIFY_TEST_ENVIRONMENT}"
    )

    add_test(NAME tempify_prebyte_help COMMAND tempify -p -h)
    set_tests_properties(tempify_prebyte_help PROPERTIES
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        ENVIRONMENT "${TEMPIFY_TEST_ENVIRONMENT}"
    )
endif()

if(TEMPIFY_BUILD_FUZZERS)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang")
        message(FATAL_ERROR "TEMPIFY_BUILD_FUZZERS requires Clang with libFuzzer support")
    endif()

    if(NOT PREBYTE_ENABLE_SANITIZERS)
        message(FATAL_ERROR "TEMPIFY_BUILD_FUZZERS requires PREBYTE_ENABLE_SANITIZERS")
    endif()

    if(NOT TARGET tempify)
        message(FATAL_ERROR "TEMPIFY_BUILD_FUZZERS requires the tempify executable target")
    endif()

    set(TEMPIFY_FUZZ_TEST_ENVIRONMENT
        "TEMPIFY_TEST_BINARY=$<TARGET_FILE:tempify>;XDG_DATA_HOME=${CMAKE_SOURCE_DIR}/tests/test_workspace/.xdg-data;XDG_CONFIG_HOME=${CMAKE_SOURCE_DIR}/tests/test_workspace/.xdg-config")

    set(TEMPIFY_FUZZ_TARGETS
        fuzz_slugify
        fuzz_answer_json
        fuzz_cli_parser
    )

    set(TEMPIFY_FUZZ_SOURCES
        tests/fault_tolerance/fuzz/SlugFuzz.cpp
        tests/fault_tolerance/fuzz/AnswerJsonFuzz.cpp
        tests/fault_tolerance/fuzz/CliParserFuzz.cpp
    )

    list(LENGTH TEMPIFY_FUZZ_TARGETS TEMPIFY_FUZZ_TARGET_COUNT)
    math(EXPR TEMPIFY_FUZZ_LAST_INDEX "${TEMPIFY_FUZZ_TARGET_COUNT} - 1")

    if(DEFINED ENV{TEMPIFY_FUZZ_MAX_TOTAL_TIME})
        set(TEMPIFY_FUZZ_MAX_TOTAL_TIME "$ENV{TEMPIFY_FUZZ_MAX_TOTAL_TIME}")
    else()
        set(TEMPIFY_FUZZ_MAX_TOTAL_TIME 60)
    endif()

    foreach(TEMPIFY_FUZZ_INDEX RANGE ${TEMPIFY_FUZZ_LAST_INDEX})
        list(GET TEMPIFY_FUZZ_TARGETS ${TEMPIFY_FUZZ_INDEX} TEMPIFY_FUZZ_TARGET)
        list(GET TEMPIFY_FUZZ_SOURCES ${TEMPIFY_FUZZ_INDEX} TEMPIFY_FUZZ_SOURCE)

        add_executable(${TEMPIFY_FUZZ_TARGET} ${TEMPIFY_FUZZ_SOURCE})
        target_link_libraries(${TEMPIFY_FUZZ_TARGET} PRIVATE tempify_core)
        target_compile_options(${TEMPIFY_FUZZ_TARGET} PRIVATE -fsanitize=fuzzer)
        target_link_options(${TEMPIFY_FUZZ_TARGET} PRIVATE -fsanitize=fuzzer)

        set(TEMPIFY_FUZZ_CORPUS "${CMAKE_SOURCE_DIR}/tests/fault_tolerance/fuzz/corpus/${TEMPIFY_FUZZ_TARGET}")
        add_test(
            NAME ${TEMPIFY_FUZZ_TARGET}
            COMMAND ${TEMPIFY_FUZZ_TARGET}
                -max_total_time=${TEMPIFY_FUZZ_MAX_TOTAL_TIME}
                -close_fd_mask=3
                ${TEMPIFY_FUZZ_CORPUS}
        )
        set_tests_properties(${TEMPIFY_FUZZ_TARGET} PROPERTIES
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            ENVIRONMENT "${TEMPIFY_FUZZ_TEST_ENVIRONMENT}"
        )
    endforeach()
endif()
