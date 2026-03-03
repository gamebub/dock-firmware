#![no_std]
#![no_main]

extern crate alloc;

mod api;
mod engine;
mod info;
mod logger;
mod sys;
mod util;

use freertos_rust::{CurrentTask, Duration, FreeRtosAllocator, Task};

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

    engine::start_task();

    Task::new()
        .start(|_| {
            loop {
                engine::send(engine::Message::Ping);
                CurrentTask::delay(Duration::ms(1000));
            }
        })
        .unwrap();
}

#[cfg(not(test))]
#[panic_handler]
fn panic(_info: &core::panic::PanicInfo) -> ! {
    // TODO: print the message somehow
    unsafe {
        sys::panic("Panic!\0".as_ptr());
    }
}
