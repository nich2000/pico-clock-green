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
set(PICO_SDK_PATH ${USERHOME}/.pico-sdk/sdk/2.2.0)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()

# ====================================================================================
set(PICO_BOARD pico_w CACHE STRING "Board type")

cmake_minimum_required(VERSION 3.16)

# include($ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)
include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)

# project(Pico-Clock-Green)
project(Pico-Clock-Green C CXX ASM)

pico_sdk_init()

add_executable(Pico-Clock-Green
        Pico-Clock-Green.c
        Ds3231.c Ds3231.h)

# Pull in our pico_stdlib which pulls in commonly used features
target_link_libraries(Pico-Clock-Green pico_stdlib hardware_i2c hardware_adc)

# create map/bin/hex file etc.
pico_add_extra_outputs(Pico-Clock-Green)

# add url via pico_set_program_url
#example_auto_set_url(Pico-Clock-Green)
