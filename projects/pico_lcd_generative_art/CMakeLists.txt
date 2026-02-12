# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico2_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(GenerativeArt C CXX ASM)

# Enable optimizations
add_compile_options(-O3)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

if(NOT DEFINED PICO_PROJECTS_ROOT)
    get_filename_component(PICO_PROJECTS_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
endif()

add_subdirectory("${PICO_PROJECTS_ROOT}/shared/pico_display" "${CMAKE_BINARY_DIR}/shared_pico_display")

# Add executable. Default name is the project name, version 0.1
add_executable(GenerativeArt
        GenerativeArt.c
)

pico_set_program_name(GenerativeArt "GenerativeArt")
pico_set_program_version(GenerativeArt "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(GenerativeArt 0)
pico_enable_stdio_usb(GenerativeArt 1)

# Add the standard include files to the build
target_include_directories(GenerativeArt PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

# Add any user requested libraries
target_link_libraries(GenerativeArt
        pico_stdlib
        shared_lcd_driver
        hardware_dma
        hardware_timer
        pico_rand
        pico_multicore
)

pico_add_extra_outputs(GenerativeArt)
