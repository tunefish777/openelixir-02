# Copyright (C) 2026 Erica Synths
# SPDX-License-Identifier: GPL-3.0-only

cmake_minimum_required(VERSION 3.21)

# OpenElixir-02: custom firmware for the Erica Synths LXR-02 desktop drum
# machine (STM32F765VG, Cortex-M7), based on the open-source LXR V2 firmware.
#
#   cmake --preset release
#   cmake --build --preset release
#
# For blank devices it can also produce a full-flash image (bootloader +
# firmware) and a "flash_full" target, see LXR_BOOTLOADER_HEX / LXR_BOOTLOADER_DIR.
#
# See README.md for details.

# CMake modules and tools (toolchain file, MCU settings, output generation).
set(LXR_CMAKE_DIR "${CMAKE_CURRENT_LIST_DIR}/cmake")
if(NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${LXR_CMAKE_DIR}/arm-none-eabi.cmake")
endif()

# The project name is the firmware's name: it names the build outputs and is
# shown on the start-up screen and as the USB-MIDI device name (FIRMWARE_NAME).
project(OpenElixir-02
    VERSION 0.1
    DESCRIPTION "OpenElixir-02, custom firmware for the Erica Synths LXR-02 drum machine"
    LANGUAGES C ASM
)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type (Debug or Release)" FORCE)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Same optimisation as the original STM32CubeIDE configurations: the audio
# engine has to run at -O3 to keep up in real time, even in Debug builds.
set(CMAKE_C_FLAGS_DEBUG     "-O3 -g3")
set(CMAKE_C_FLAGS_RELEASE   "-O3 -g0")
set(CMAKE_ASM_FLAGS_DEBUG   "-g3")
set(CMAKE_ASM_FLAGS_RELEASE "")

# The 16 KB ITCM is tight. Measured .itcm_text (without linker stubs, ~0.5 KB)
# with Arm GNU 15.2.rel1: -O3 14564 B, -O2 10420 B (with -fno-thread-jumps on
# ResonantFilter.c, see Core/CMakeLists.txt; without it -O3 18724 B, -O2 16152 B);
# with Arm GNU 11.3.rel1: -O3 14552 B. Set e.g. -DLXR_ITCM_OPT=-O2 if a future
# compiler overflows again. See LXR_ITCM_SOURCES in Core/CMakeLists.txt.
set(LXR_ITCM_OPT "" CACHE STRING
    "Optimisation flag for translation units with IN_ITCM_RAM code (empty = same as the rest)")

# Bootloader for the full-flash image and "flash_full" (blank devices only).
# Normal firmware updates don't need it, so by default neither is set and the
# build produces just the firmware. Set at most one of them:
#   A: a prebuilt bootloader .hex, e.g. from a bootloader release
#   B: the bootloader source tree, which is then built together with the firmware
set(LXR_BOOTLOADER_HEX "" CACHE FILEPATH "Prebuilt LXR bootloader .hex for the full-flash image (optional)")
set(LXR_BOOTLOADER_DIR "" CACHE PATH "LXR bootloader source tree to build for the full-flash image (optional)")

# Memory usage report printed after every build is compared against this file
# (if it exists). "cmake --build --preset release --target memory_baseline" saves
# the current build as the new baseline.
set(LXR_MEMORY_BASELINE "${CMAKE_CURRENT_SOURCE_DIR}/memory_baseline.json" CACHE FILEPATH
    "Saved memory usage report to compare each build against")

include(${LXR_CMAKE_DIR}/stm32f765.cmake)
include(${LXR_CMAKE_DIR}/firmware_outputs.cmake)

# Board configuration headers (main.h, stm32f7xx_hal_conf.h, ...) and the
# code-generation flags of the original build. Needed by the HAL and middleware
# as well as by the application.
add_library(lxr_board_config INTERFACE)
target_include_directories(lxr_board_config INTERFACE Core/Inc)
target_compile_definitions(lxr_board_config INTERFACE "FIRMWARE_NAME=\"${PROJECT_NAME}\"")
target_compile_options(lxr_board_config INTERFACE
    $<$<COMPILE_LANGUAGE:C>:-ffast-math -freciprocal-math -mfp16-format=ieee -fomit-frame-pointer>
)

add_subdirectory(Drivers)
add_subdirectory(Middlewares)
add_subdirectory(Core)

add_executable(${PROJECT_NAME})
target_link_libraries(${PROJECT_NAME} PRIVATE
    # Object libraries only contribute their objects to direct dependents,
    # so every one of them is listed here.
    stm32f7_hal
    fatfs
    usb_midi
    ${LXR_APP_MODULES}
)

# Linked at 0x08008000, behind the 32 KB bootloader.
lxr_firmware_executable(${PROJECT_NAME} LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/STM32F765VGTX_FLASH.ld)
# The update file name has to start with "LXR" and the image needs the LXRV2IMG
# magic: that is what the bootloader on the device looks for.
set(update_image_name LXR_${PROJECT_NAME}_V${PROJECT_VERSION})
lxr_add_firmware_outputs(${PROJECT_NAME} IMAGE_MAGIC LXRV2IMG IMAGE_NAME ${update_image_name})

# "flash": application only, the bootloader on the device is left alone.
lxr_add_openocd_flash_target(flash
    FILE $<TARGET_FILE:${PROJECT_NAME}>
    DEPENDS ${PROJECT_NAME}
    COMMENT "Flashing the application with OpenOCD"
)

# --- Bootloader + combined image for blank devices ----------------------------
set(bootloader "")
if(LXR_BOOTLOADER_HEX AND LXR_BOOTLOADER_DIR)
    message(FATAL_ERROR "Set only one of LXR_BOOTLOADER_HEX and LXR_BOOTLOADER_DIR.")
elseif(LXR_BOOTLOADER_HEX)
    get_filename_component(bootloader "${LXR_BOOTLOADER_HEX}" ABSOLUTE BASE_DIR "${CMAKE_BINARY_DIR}")
    if(NOT EXISTS "${bootloader}")
        message(FATAL_ERROR "LXR_BOOTLOADER_HEX: file not found: ${bootloader}")
    endif()
    if(Python3_EXECUTABLE)  # report a wrong file now rather than at build time
        execute_process(
            COMMAND ${Python3_EXECUTABLE} ${LXR_CMAKE_TOOLS_DIR}/check_bootloader_hex.py --magic LXRV2IMG ${bootloader}
            RESULT_VARIABLE check_result
            OUTPUT_QUIET
            ERROR_VARIABLE check_error
            ERROR_STRIP_TRAILING_WHITESPACE
        )
        if(check_result)
            message(FATAL_ERROR "LXR_BOOTLOADER_HEX: ${check_error}")
        endif()
    endif()
    message(STATUS "Bootloader for the full-flash image: ${bootloader}")
elseif(LXR_BOOTLOADER_DIR)
    get_filename_component(bootloader_dir "${LXR_BOOTLOADER_DIR}" ABSOLUTE BASE_DIR "${CMAKE_BINARY_DIR}")
    set(bootloader_project "")
    if(EXISTS "${bootloader_dir}/CMakeLists.txt")
        file(STRINGS "${bootloader_dir}/CMakeLists.txt" bootloader_project REGEX "^project\\(LXR_V2_STM32F765_Bootloader")
    endif()
    if(NOT bootloader_project)
        message(FATAL_ERROR "LXR_BOOTLOADER_DIR: ${bootloader_dir} is not the LXR bootloader project "
                            "(no CMakeLists.txt with project(LXR_V2_STM32F765_Bootloader ...)).")
    endif()
    set(LXR_BOOTLOADER_EURORACK OFF)  # the desktop firmware needs the desktop bootloader
    add_subdirectory(${bootloader_dir} bootloader)
    set(bootloader lxr_bootloader)
    message(STATUS "Bootloader for the full-flash image: built from ${bootloader_dir}")
else()
    message(STATUS "No bootloader configured: building the firmware only (no full-flash image, no flash_full). "
                   "Set LXR_BOOTLOADER_HEX or LXR_BOOTLOADER_DIR for blank devices.")
endif()

string(TOLOWER "${CMAKE_BUILD_TYPE}" preset)
if(NOT bootloader)
    # Explain how to get flash_full instead of Ninja's "unknown target" error.
    lxr_add_unavailable_target(flash_full
        "'flash_full' is not available: no bootloader is configured for this build."
        "It is only needed for blank devices. To update a working LXR, use the SD card"
        "update file (.img), or the 'flash' target which writes only the firmware."
        "For a blank device, set up the build with the bootloader, then run flash_full again:"
        "  cmake --preset ${preset} -DLXR_BOOTLOADER_DIR=path/to/LXR_V2_STM32F765_Bootloader"
        "or, with a prebuilt bootloader:"
        "  cmake --preset ${preset} -DLXR_BOOTLOADER_HEX=path/to/LXR_V2_STM32F765_Bootloader.hex"
        "(Give the path with -DLXR_BOOTLOADER_DIR= or -DLXR_BOOTLOADER_HEX=, not on its own.)"
    )
endif()

if(bootloader)
    # <name>_V<version>_full.hex/.bin: bootloader at 0x08000000 + firmware at 0x08008000.
    lxr_add_combined_image(full_image
        OUTPUT_NAME ${PROJECT_NAME}_V${PROJECT_VERSION}_full
        BOOTLOADER ${bootloader}
        APPLICATION ${PROJECT_NAME}
        BOOTLOADER_MAGIC LXRV2IMG
    )

    # "flash_full": erases and programs bootloader and firmware. Asks for
    # confirmation and lifts the bootloader's sector-0 write protection first
    # (the bootloader re-protects itself on boot).
    if(TARGET full_image)
        get_target_property(full_hex full_image LXR_IMAGE_HEX)
        lxr_add_openocd_flash_target(flash_full
            FILE ${full_hex}
            DEPENDS full_image
            OVERWRITES_BOOTLOADER
            COMMENT "Flashing bootloader + firmware with OpenOCD"
        )
    else()  # no Python 3: lxr_add_combined_image() already warned at configure time
        lxr_add_unavailable_target(flash_full
            "'flash_full' needs Python 3 to create the full-flash image, but Python 3 was not found."
            "Install Python 3, then set up the build again with:  cmake --preset ${preset}"
        )
    endif()
endif()

# --- Memory usage (printed after every build) -----------------------------------
# "memory_report" additionally lists the largest symbols per region.
lxr_add_memory_report(memory_usage
    FIRMWARE ${PROJECT_NAME}
    BASELINE ${LXR_MEMORY_BASELINE}
    DETAIL_TARGET memory_report
    BASELINE_TARGET memory_baseline
)

# --- How to install the update (printed at the end of every build) -------------
set(info_depends ${PROJECT_NAME})
if(TARGET memory_usage)
    list(APPEND info_depends memory_usage)  # keep the instructions last
endif()
if(TARGET full_image)
    list(APPEND info_depends full_image)  # print after everything else is done
endif()
lxr_add_update_info(update_info
    IMAGE ${CMAKE_CURRENT_BINARY_DIR}/${update_image_name}.img
    DEPENDS ${info_depends}
)
