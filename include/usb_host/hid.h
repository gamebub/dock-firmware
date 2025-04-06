#pragma once

#include <cstdint>
#include <memory>
#include <span>

#include "core/core.h"

class UsbHidDriver;

class UsbHidGamepad {
   public:
    /// Slot ID
    uint32_t gamepad_id;

    /// USB device address
    uint8_t dev_addr;

    /// USB interface instance
    uint8_t instance;

    /// Driver
    std::unique_ptr<UsbHidDriver> driver = nullptr;
};

class UsbHidDriver {
   public:
    UsbHidDriver(UsbHidGamepad& gamepad) : gamepad_(gamepad) {}

    virtual ~UsbHidDriver() = default;

    virtual void Initialize(std::span<const uint8_t> descriptor) = 0;
    virtual void OnReportReceived(std::span<const uint8_t> report) = 0;

    virtual uni_controller_type_t GetGamepadType() = 0;

   protected:
    void RequestReport();
    void ReportData(GamepadData data);

    UsbHidGamepad& gamepad_;
};