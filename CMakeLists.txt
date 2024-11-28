cmake_minimum_required(VERSION 3.13.1)

set(BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/.)

if(NOT DEFINED BOARD)
    set(BOARD "nrf52840dk/nrf52840")
endif()

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(zephyr-linux-ble-copro)

set(app_sources
    src/main.c
    src/ble_observer.c
)

target_sources_ifdef(CONFIG_COPRO_USB_NETWORK app PRIVATE src/usb_net.c)
target_sources_ifdef(CONFIG_COPRO_STREAM_CLIENT app PRIVATE src/stream_client.c)
target_sources_ifdef(CONFIG_COPRO_XIAOMI_LYWSD03MMC app PRIVATE src/xiaomi.c)

target_include_directories(app PRIVATE include)
target_sources(app PRIVATE ${app_sources})