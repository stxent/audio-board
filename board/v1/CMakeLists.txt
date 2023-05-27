# Copyright (C) 2022 xent
# Project is distributed under the terms of the GNU General Public License v3.0

# Set version variables
set(VERSION_HW_MAJOR 1 PARENT_SCOPE)
set(VERSION_HW_MINOR 0 PARENT_SCOPE)

# Linker script for an application
configure_file("memory.ld" "${PROJECT_BINARY_DIR}/application.ld")

set(FLAGS_LINKER "--specs=nosys.specs --specs=nano.specs -Wl,--gc-sections" PARENT_SCOPE)
