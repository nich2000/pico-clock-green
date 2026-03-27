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

include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(Pico-Clock-Green C CXX ASM)

pico_sdk_init()

add_executable(Pico-Clock-Green
        Pico-Clock-Green.c Pico-Clock-Green.h
        Ds3231.c Ds3231.h
        clock_tcp.c clock_tcp.h)

target_link_libraries(Pico-Clock-Green pico_stdlib hardware_i2c hardware_adc pico_cyw43_arch_lwip_poll)

pico_add_extra_outputs(Pico-Clock-Green)

target_include_directories(Pico-Clock-Green PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)

file(READ "${CMAKE_CURRENT_LIST_DIR}/.env" ENV_FILE)

string(REGEX MATCH "WIFI_SSID=([^\n]+)" _ ${ENV_FILE})
set(WIFI_SSID ${CMAKE_MATCH_1})

string(REGEX MATCH "WIFI_PASSWORD=([^\n]+)" _ ${ENV_FILE})
set(WIFI_PASSWORD ${CMAKE_MATCH_1})

target_compile_definitions(Pico-Clock-Green PRIVATE
    WIFI_SSID=\"${WIFI_SSID}\"
    WIFI_PASSWORD=\"${WIFI_PASSWORD}\"
)

pico_enable_stdio_usb(Pico-Clock-Green 1)
pico_enable_stdio_uart(Pico-Clock-Green 0)
