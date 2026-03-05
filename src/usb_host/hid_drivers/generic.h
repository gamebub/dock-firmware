#pragma once

#include "controller/uni_controller_type.h"
#include "controller/uni_gamepad.h"
#include "usb_host/hid.h"

class GenericHidDriver : public UsbHidDriver {
   public:
    GenericHidDriver(UsbHidGamepad& gamepad) : UsbHidDriver(gamepad) {}

    ~GenericHidDriver() override {};

    void Initialize(std::span<const uint8_t> descriptor) override;
    void OnReportReceived(std::span<const uint8_t> report) override;

    uni_controller_type_t GetGamepadType() override;
};