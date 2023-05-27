# Copyright (C) 2023 xent
# Project is distributed under the terms of the GNU General Public License v3.0

if(NOT DEFINED BOARD)
    message(FATAL_ERROR "BOARD not defined")
endif()

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
list(APPEND SHARED_SOURCES "${PROJECT_BINARY_DIR}/version.c")

# Shared objects
file(GLOB_RECURSE SHARED_SOURCES "${BOARD}/shared/*.c")
list(APPEND SHARED_SOURCES "${PROJECT_BINARY_DIR}/version.c")

add_library(shared ${SHARED_SOURCES})
target_include_directories(shared PUBLIC "${BOARD}/shared")
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

    target_link_libraries(${EXECUTABLE_ARTIFACT} PRIVATE shared)
    target_link_libraries(${EXECUTABLE_ARTIFACT} PRIVATE -T"${PROJECT_BINARY_DIR}/application.ld" ${FLAGS_LINKER})

    if(USE_DBG)
        target_compile_definitions(${EXECUTABLE_ARTIFACT} PRIVATE -DENABLE_DBG)
    endif()
    if(USE_WDT)
        target_compile_definitions(${EXECUTABLE_ARTIFACT} PRIVATE -DENABLE_WDT)
    endif()

    add_custom_command(TARGET ${EXECUTABLE_ARTIFACT} POST_BUILD
            COMMAND "${CMAKE_OBJCOPY}" ${EXECUTABLE_ARTIFACT} ${FLAGS_OBJCOPY} -Oihex ${EXECUTABLE_NAME}.hex
    )
endforeach()
