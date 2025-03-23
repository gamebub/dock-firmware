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
set(sdkVersion 2.1.1)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.1.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================

set(SRC ${CMAKE_CURRENT_LIST_DIR}/src)
set(EXTERNAL_DIR ${CMAKE_CURRENT_LIST_DIR}/external)

set(PICO_BOARD pico_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(dock-firmware C CXX ASM)
set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 20)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

set(PICO_TINYUSB_PATH ${EXTERNAL_DIR}/tinyusb)
set(BTSTACK_ROOT ${PICO_SDK_PATH}/lib/btstack)
set(BLUEPAD32_ROOT ${EXTERNAL_DIR}/bluepad32)

# Add executable. Default name is the project name, version 0.1

add_executable(dock-firmware
    ${SRC}/main.cpp
    ${SRC}/bluepad32_platform.cpp
)

pico_set_program_name(dock-firmware "dock-firmware")
pico_set_program_version(dock-firmware "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(dock-firmware 1)
pico_enable_stdio_usb(dock-firmware 0)
target_compile_definitions(dock-firmware PRIVATE
  PICO_DEFAULT_UART=1
  PICO_DEFAULT_UART_TX_PIN=8
  PICO_DEFAULT_UART_RX_PIN=9

  PICO_USE_STACK_GUARDS=1
  PICO_STACK_SIZE=0x2000
  PICO_CORE1_STACK_SIZE=0
)

# Add the standard library to the build
target_link_libraries(dock-firmware
    pico_stdlib
)

# Add the standard include files to the build
target_include_directories(dock-firmware PRIVATE
    ${SRC}
)

# Needed for btstack_config.h / sdkconfig.h
# so that libbluepad32 can include them
include_directories(dock-firmware ${SRC})

# Add any user requested libraries
target_link_libraries(dock-firmware
    pico_multicore
    tinyusb_host
    pico_stdlib
    pico_cyw43_arch_none
    pico_btstack_classic
    pico_btstack_cyw43
    bluepad32
)

add_subdirectory(${BLUEPAD32_ROOT}/src/components/bluepad32 libbluepad32)

pico_add_extra_outputs(dock-firmware)

