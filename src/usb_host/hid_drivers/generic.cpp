#include "generic.h"

#include "log/log.h"

void GenericHidDriver::Initialize(std::span<const uint8_t> descriptor) {
    log_info("Initialized GenericHidDriver, descriptor len=%u", descriptor.size());
    RequestReport();
}

void GenericHidDriver::OnReportReceived(std::span<const uint8_t> report) {
    log_info("HID report: len=%zu", report.size());
    // TODO parse and call ReportData(...)
    RequestReport();
}

uni_controller_type_t GenericHidDriver::GetGamepadType() {
    return k_eControllerType_GenericController;
}
