# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

# list(APPEND CONF_FILE
#   ${CMAKE_CURRENT_SOURCE_DIR}/nrf52840dongle_nrf52840.conf)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(slimevr-central)

target_sources(app PRIVATE
  src/main.c
  src/connectionManager.c
  src/echo_server.c
  src/udp.c
  src/usb.c
)

zephyr_library_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
