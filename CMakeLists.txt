cmake_minimum_required(VERSION 3.0)

project(appimaged)

# include own FindXXXX scripts
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# determine Git commit ID
execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    OUTPUT_VARIABLE APPIMAGED_GIT_COMMIT
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
mark_as_advanced(APPIMAGED_GIT_COMMIT)

# set version and build number
set(APPIMAGED_VERSION 1-alpha)
mark_as_advanced(APPIMAGED_VERSION)

if("$ENV{TRAVIS_BUILD_NUMBER}" STREQUAL "")
    set(APPIMAGED_BUILD_NUMBER "<local dev build>")
else()
    set(APPIMAGED_BUILD_NUMBER "$ENV{TRAVIS_BUILD_NUMBER}")
endif()
mark_as_advanced(APPIMAGED_BUILD_NUMBER)

# get current date
execute_process(
    COMMAND env LC_ALL=C date -u "+%Y-%m-%d %H:%M:%S %Z"
    OUTPUT_VARIABLE APPIMAGED_BUILD_DATE
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
mark_as_advanced(APPIMAGED_BUILD_DATE)

add_definitions(
    -DAPPIMAGED_GIT_COMMIT="${APPIMAGED_GIT_COMMIT}"
    -DAPPIMAGED_BUILD_NUMBER="${APPIMAGED_BUILD_NUMBER}"
    -DAPPIMAGED_BUILD_DATE="${APPIMAGED_BUILD_DATE}"
)

# C and C++ versions
set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 98)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# include external dependencies
add_subdirectory(lib)

# include resources
add_subdirectory(resources)

# include source dir
add_subdirectory(src)

# include packaging configuration
include(cmake/cpack_debs.cmake)
include(cmake/cpack_rpms.cmake)
# CPack must be included once only, after setting all the variables
include(CPack)
