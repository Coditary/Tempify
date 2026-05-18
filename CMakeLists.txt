cmake_minimum_required(VERSION 3.31)

project(Tempify VERSION 0.1.0 LANGUAGES CXX)

include(CTest)
include(FetchContent)
include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(CLI11 CONFIG REQUIRED)

set(TEMPIFY_PREBYTE_VERSION "v1.0.0" CACHE STRING "Pinned Prebyte git tag")
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
target_link_libraries(tempify_core PUBLIC prebyte_core PRIVATE CLI11::CLI11)

add_executable(tempify src/main/cpp/main.cpp)
target_link_libraries(tempify PRIVATE tempify_core)

install(TARGETS tempify RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES README.md LICENSE DESTINATION ${CMAKE_INSTALL_DATADIR}/tempify)

if(BUILD_TESTING)
    file(GLOB TEMPIFY_TEST_SOURCES CONFIGURE_DEPENDS tests/tempify/*.cpp)
    set(TEMPIFY_TEST_ENVIRONMENT "TEMPIFY_TEST_BINARY=$<TARGET_FILE:tempify>")

    add_executable(tempify_tests
        tests/support/TestHarness.cpp
        tests/support/TestMain.cpp
        ${TEMPIFY_TEST_SOURCES}
    )
    target_include_directories(tempify_tests PRIVATE tests/support src/main/include)
    target_link_libraries(tempify_tests PRIVATE tempify_core)

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
