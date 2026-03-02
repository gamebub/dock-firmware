#![no_std]
#![no_main]

mod api;
mod logger;
mod sys;

extern crate alloc;

#[global_allocator]
static GLOBAL: FreeRtosAllocator = FreeRtosAllocator;

use freertos_rust::{CurrentTask, Duration, FreeRtosAllocator, Task};

#[unsafe(no_mangle)]
pub extern "C" fn rust_add(left: u32, right: u32) -> u32 {
    left + right
}

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
