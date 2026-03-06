#![no_std]
#![no_main]

extern crate alloc;

mod bluetooth;
mod c_api;
mod engine;
mod gamepad;
mod handheld;
mod info;
mod led;
mod logger;
mod sys;
mod usb_host;
mod util;

use freertos_rust::FreeRtosAllocator;

#[global_allocator]
static GLOBAL: FreeRtosAllocator = FreeRtosAllocator;

#[unsafe(no_mangle)]
pub extern "C" fn rust_init() {
    logger::init();

    log::info!("Game Bub Dock");
    log::info!(
        "Hardware Version: {:08X}",
        info::HardwareVersion::get().as_u32()
    );
    log::info!("Serial Number: {}", info::SerialNumber::get());
    log::info!("Firmware Version: {}", info::FirmwareVersion::get());

    led::start_task();
    engine::start_task();

    led::set(led::LedState::Standby);
}

#[cfg(not(test))]
#[panic_handler]
fn panic(info: &core::panic::PanicInfo) -> ! {
    log::error!("PANIC: {}", info.message());
    if let Some(location) = info.location() {
        log::error!(" at {}", location);
    }

    unsafe {
        sys::panic("Panic!\0".as_ptr());
    }
}
