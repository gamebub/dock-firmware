#![no_std]
#![no_main]

extern crate alloc;

mod api;
mod info;
mod logger;
mod sys;

use freertos_rust::{CurrentTask, Duration, FreeRtosAllocator, Task};

#[global_allocator]
static GLOBAL: FreeRtosAllocator = FreeRtosAllocator;

fn rust_printy(_task: Task) {
    log::info!("Rust task!");

    let mut i = 0;
    loop {
        CurrentTask::delay(Duration::ms(500));
        log::info!("hello from rust: {}", i);
        i += 1;
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn rust_init() {
    logger::init();

    log::info!("Game Bub Dock");
    log::info!(
        "Hardware Version: {:08X}",
        info::HardwareVersion::get().as_u32()
    );
    log::info!("Serial Number: {}", info::SerialNumber::get());

    Task::new().name("rust printy").start(rust_printy).unwrap();
}

#[cfg(not(test))]
#[panic_handler]
fn panic(_info: &core::panic::PanicInfo) -> ! {
    // TODO: print the message somehow
    unsafe {
        sys::panic("Panic!\0".as_ptr());
    }
}
