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
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0)
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
set(PICO_CYW43_SUPPORTED 1)
#set(PICO_BOARD pico2_w)
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
    ${SRC}/hwinfo/hwinfo.cpp
    ${SRC}/usb_device/usb_device.cpp
    ${SRC}/usb_device/descriptors.c
    ${SRC}/usb_device/reset_interface.c
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
  DOCK_FW_VERSION_MAJOR=0
  DOCK_FW_VERSION_MINOR=1
  DOCK_FW_VERSION_PATCH=0

  PICO_RP2350A=1
  PICO_RP2350_A2_SUPPORTED=1

  PICO_DEFAULT_UART=0
  PICO_DEFAULT_UART_TX_PIN=16
  PICO_DEFAULT_UART_RX_PIN=17
  # TODO flash section
  CYW43_WL_GPIO_COUNT=3
  CYW43_PIN_WL_DYNAMIC=0
  CYW43_DEFAULT_PIN_WL_REG_ON=21
  CYW43_DEFAULT_PIN_WL_DATA_OUT=23
  CYW43_DEFAULT_PIN_WL_DATA_IN=23
  CYW43_DEFAULT_PIN_WL_HOST_WAKE=23
  CYW43_DEFAULT_PIN_WL_CLOCK=24
  CYW43_DEFAULT_PIN_WL_CS=22

#   PICO_FLASH_ASSUME_CORE0_SAFE=1
  PICO_CYW43_ARCH_FREERTOS=1
  CYW43_LWIP=0
  CYW43_TASK_STACK_SIZE=2048  # Larger stack, because this calls into btstack/bluepad

  # Set system clock to 120 MHz
  SYS_CLK_HZ=120000000
  PLL_SYS_VCO_FREQ_HZ=1440000000
  PLL_SYS_POSTDIV1=6
  PLL_SYS_POSTDIV2=2

  PIN_BUTTON=0
  PIN_HDMI_5V_EN=1
  PIN_HDMI_CEC=2
  PIN_HDMI_VBIAS=3
  PIN_HDMI_SDA=4
  PIN_HDMI_SCL=5
  PIN_HDMI_HPD=6
  PIN_HDMI_OE_N=7
  PIN_SBU1=8
  PIN_SBU2=9
  PIN_USB_SBU_BIAS=10
  PIN_USB_HANDHELD_CC=11
  PIN_USB_RESET_N=12
  PIN_USB_CLK_OUT=13
  PIN_USB_DP=14
  PIN_USB_DM=15
  PIN_UART_TX=16
  PIN_UART_RX=17
  PIN_USB_VBUS_FAULT_N=19
  PIN_USB_VBUS_EN=20
  PIN_LED=25
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
    tinyusb_device
    tinyusb_host
    pico_stdlib
    pico_cyw43_arch_sys_freertos
    pico_btstack_classic
    pico_btstack_cyw43
    pico_usb_reset_interface_headers
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

target_compile_definitions(bluepad32 PUBLIC
  CYW43_WL_GPIO_COUNT=3
  CYW43_PIN_WL_DYNAMIC=0
  CYW43_DEFAULT_PIN_WL_REG_ON=21
  CYW43_DEFAULT_PIN_WL_DATA_OUT=23
  CYW43_DEFAULT_PIN_WL_DATA_IN=23
  CYW43_DEFAULT_PIN_WL_HOST_WAKE=23
  CYW43_DEFAULT_PIN_WL_CLOCK=24
  CYW43_DEFAULT_PIN_WL_CS=22
)

pico_add_extra_outputs(dock-firmware)

pico_generate_pio_header(dock-firmware ${CMAKE_CURRENT_LIST_DIR}/src/led/ws2812.pio OUTPUT_DIR ${CMAKE_BINARY_DIR}/generated)
