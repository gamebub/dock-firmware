cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 20)
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
set(INCLUDE ${CMAKE_CURRENT_LIST_DIR}/include)
set(EXTERNAL_DIR ${CMAKE_CURRENT_LIST_DIR}/external)

set(PICO_TINYUSB_PATH ${EXTERNAL_DIR}/tinyusb)
set(BTSTACK_ROOT ${PICO_SDK_PATH}/lib/btstack)
set(BLUEPAD32_ROOT ${EXTERNAL_DIR}/bluepad32)
set(PICO_PIO_USB_PATH ${EXTERNAL_DIR}/Pico-PIO-USB)
set(FREERTOS_KERNEL_PATH ${EXTERNAL_DIR}/FreeRTOS-Kernel)
set(TUSB_XINPUT_PATH ${EXTERNAL_DIR}/tusb_xinput)

set(PICO_PLATFORM rp2350)
set(TINYUSB_OPT_OS OPT_OS_FREERTOS)
set(TINYUSB_DEBUG_LEVEL 1)

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(dock-firmware C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

include(FreeRTOS_Kernel_import.cmake)

# Add executable. Default name is the project name, version 0.1

add_executable(dock-firmware
    ${SRC}/main.cpp
    ${SRC}/bluetooth/bluetooth.cpp
    ${SRC}/core/core.cpp
    ${SRC}/handheld/handheld.cpp
    ${SRC}/gpio/gpio.cpp
    ${SRC}/led/led.cpp
    ${SRC}/usb_host/hid.cpp
    ${SRC}/usb_host/usb_host.cpp
    ${SRC}/usb_host/xinput.cpp
    ${SRC}/usb_host/hid_drivers/generic.cpp
    ${PICO_TINYUSB_PATH}/src/portable/raspberrypi/pio_usb/dcd_pio_usb.c
    ${PICO_TINYUSB_PATH}/src/portable/raspberrypi/pio_usb/hcd_pio_usb.c
)

pico_set_program_name(dock-firmware "dock-firmware")
pico_set_program_version(dock-firmware "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(dock-firmware 1)
pico_enable_stdio_usb(dock-firmware 0)
target_compile_definitions(dock-firmware PRIVATE
  DOCK_SW_VERSION="0.1"
  DOCK_HW_VERSION="1"
  DOCK_SERIAL_NUM="0" # TODO

  PICO_DEFAULT_UART=1
  PICO_DEFAULT_UART_TX_PIN=8
  PICO_DEFAULT_UART_RX_PIN=9

#   PICO_FLASH_ASSUME_CORE0_SAFE=1
  PICO_CYW43_ARCH_FREERTOS=1
  CYW43_LWIP=0
  CYW43_TASK_STACK_SIZE=2048  # Larger stack, because this calls into btstack/bluepad

  # Set system clock to 192 MHz
  SYS_CLK_HZ=192000000
  PLL_SYS_VCO_FREQ_HZ=1536000000
  PLL_SYS_POSTDIV1=4
  PLL_SYS_POSTDIV2=2

  PIN_USB_DP=6
  PIN_LED=13
  PIN_BUTTON=3
  PIN_HDMI_DDC_EN=20
  PIN_HDMI_OE_N=21
  PIN_HDMI_HPD=22
)

# Add the standard library to the build
target_link_libraries(dock-firmware
    pico_stdlib
)

# Add the standard include files to the build
target_include_directories(dock-firmware PRIVATE
    ${INCLUDE}
)

# Needed for btstack_config.h / sdkconfig.h
# so that libbluepad32 can include them
include_directories(dock-firmware ${INCLUDE})

# Add any user requested libraries
target_link_libraries(dock-firmware
    pico_multicore
    tinyusb_host
    pico_stdlib
    pico_cyw43_arch_sys_freertos
    pico_btstack_classic
    pico_btstack_cyw43
    bluepad32
    pico_pio_usb
    FreeRTOS-Kernel
    hardware_pwm
    xinput_host
)

add_subdirectory(${BLUEPAD32_ROOT}/src/components/bluepad32 libbluepad32 SYSTEM)
add_subdirectory(${PICO_PIO_USB_PATH} ${CMAKE_BINARY_DIR}/Pico-PIO-USB SYSTEM)
add_subdirectory(${TUSB_XINPUT_PATH} xinput_host SYSTEM)

# Needed for xinput_host
add_library(tinyusb ALIAS tinyusb_host)

pico_add_extra_outputs(dock-firmware)
