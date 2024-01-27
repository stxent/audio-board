# Copyright (C) 2023 xent
# Project is distributed under the terms of the GNU General Public License v3.0

cmake_minimum_required(VERSION 3.21)
project(AudioBoard C)

option(USE_DBG "Enable debug messages." OFF)
option(USE_LTO "Enable Link Time Optimization." OFF)
option(USE_WDT "Enable watchdog timer." OFF)

# Default compiler flags
set(FLAGS_PROJECT "-fdata-sections -ffunction-sections -pedantic -Wall -Wextra -Wshadow")
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_EXTENSIONS OFF)

if(USE_LTO)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
    if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
        set(CMAKE_C_COMPILE_OPTIONS_IPO -flto=auto -ffat-lto-objects)
    endif()
endif()

# Configure XCORE library
add_subdirectory(libs/xcore xcore)

if(USE_DBG)
    set(CONFIG_CORE_CORTEX_MEMORY_DEBUG ON)
    set(CONFIG_CORE_CORTEX_MEMORY_PATTERN "0xDEADBEEF")
    set(CONFIG_GENERIC_WQ_LOAD ON)
    set(CONFIG_GENERIC_WQ_PM OFF)
endif()

# Configure HALM library, HALM_CONFIG_FILE should be defined
set(HALM_CONFIG_FILE "${PROJECT_SOURCE_DIR}/board/${BOARD}/halm.config" CACHE INTERNAL "Path to the library configuration file" FORCE)
add_subdirectory(libs/halm halm)

# Configure DPM library
add_subdirectory(libs/dpm dpm)

# Add project directories
add_subdirectory(core)
add_subdirectory(board)
