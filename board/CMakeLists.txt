# Copyright (C) 2023 xent
# Project is distributed under the terms of the GNU General Public License v3.0

if(NOT DEFINED BOARD)
    message(FATAL_ERROR "BOARD not defined")
endif()
if(NOT DEFINED PLATFORM)
    message(FATAL_ERROR "PLATFORM not defined")
endif()

# State of the board configuration switches
set(OVERRIDE_SW "" CACHE STRING "Override state of the board switches, use empty value to disable.")

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/libs/xcore/cmake")
include(GetGitInfo)
include(ListDirectories)

add_subdirectory("${BOARD}")

# Generate version file
cmake_host_system_information(RESULT BUILD_HOSTNAME QUERY HOSTNAME)
string(TIMESTAMP BUILD_TIMESTAMP "%s")
git_commit_count(VERSION_SW_REVISION)
git_hash(VERSION_SW_HASH)
git_version(VERSION_SW_MAJOR VERSION_SW_MINOR)
configure_file("version_template.c" "${PROJECT_BINARY_DIR}/version.c")

# Find source files
file(GLOB_RECURSE SHARED_SOURCES "${BOARD}/shared/*.c")
list(APPEND SHARED_SOURCES "${PROJECT_BINARY_DIR}/version.c")

# Shared package
add_library(shared ${SHARED_SOURCES})
target_include_directories(shared PUBLIC "${BOARD}/shared")
target_link_options(shared PUBLIC SHELL:${FLAGS_CPU} SHELL:${FLAGS_LINKER})
target_link_libraries(shared PUBLIC core dpm)

set(EXECUTABLES "")

# Application packages
list_directories(APPS_LIST "${CMAKE_CURRENT_SOURCE_DIR}/${BOARD}/applications")

foreach(APP_NAME ${APPS_LIST})
    if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug" AND "${APP_NAME}" STREQUAL "active")
        # Debug build of the active application is too big and should be skipped
        message(STATUS "Active application skipped")
    else()
        file(GLOB_RECURSE APP_SOURCES "${BOARD}/applications/${APP_NAME}/*.c")
        list(APPEND EXECUTABLES "${APP_NAME}")
        set(${APP_NAME}_DIR "${BOARD}/applications/${APP_NAME}")
        set(${APP_NAME}_SOURCES ${APP_SOURCES})
    endif()
endforeach()

# Test packages
list_directories(TESTS_LIST "${CMAKE_CURRENT_SOURCE_DIR}/${BOARD}/tests")

foreach(TEST_NAME ${TESTS_LIST})
    file(GLOB_RECURSE TEST_SOURCES "${BOARD}/tests/${TEST_NAME}/*.c")
    list(APPEND EXECUTABLES "test_${TEST_NAME}")
    set(test_${TEST_NAME}_DIR "${BOARD}/tests/${TEST_NAME}")
    set(test_${TEST_NAME}_SOURCES ${TEST_SOURCES})
endforeach()

# Make application and test executables
foreach(EXECUTABLE_NAME ${EXECUTABLES})
    set(EXECUTABLE_ARTIFACT "${EXECUTABLE_NAME}.elf")

    add_executable(${EXECUTABLE_ARTIFACT} ${${EXECUTABLE_NAME}_SOURCES})
    target_include_directories(${EXECUTABLE_ARTIFACT} PRIVATE ${EXECUTABLE_NAME}_DIR)
    target_link_options(${EXECUTABLE_ARTIFACT} PRIVATE SHELL:-T"${PROJECT_BINARY_DIR}/memory.ld")
    target_link_libraries(${EXECUTABLE_ARTIFACT} PRIVATE shared)

    if(NOT OVERRIDE_SW STREQUAL "")
        target_compile_definitions(${EXECUTABLE_ARTIFACT} PRIVATE -DCONFIG_OVERRIDE_SW=${OVERRIDE_SW})
    endif()
    if(USE_DBG)
        target_compile_definitions(${EXECUTABLE_ARTIFACT} PRIVATE -DENABLE_DBG)
        target_link_options(${EXECUTABLE_ARTIFACT} PRIVATE SHELL:"-Wl,--print-memory-usage")
    endif()
    if(USE_WDT)
        target_compile_definitions(${EXECUTABLE_ARTIFACT} PRIVATE -DENABLE_WDT)
    endif()

    add_custom_command(TARGET ${EXECUTABLE_ARTIFACT} POST_BUILD
            COMMAND "${CMAKE_OBJCOPY}" ${EXECUTABLE_ARTIFACT} ${FLAGS_OBJCOPY} -Oihex ${EXECUTABLE_NAME}.hex
    )
endforeach()
