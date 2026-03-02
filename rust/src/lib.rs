#![no_std]
#![no_main]

use freertos_rust::{CurrentTask, Duration, FreeRtosAllocator, Task};

#[global_allocator]
static GLOBAL: FreeRtosAllocator = FreeRtosAllocator;

mod api;

#[unsafe(no_mangle)]
pub extern "C" fn rust_add(left: u32, right: u32) -> u32 {
    left + right
}

unsafe extern "C" {
    unsafe fn puts(str: *const u8);
}

fn rust_printy(_task: Task) {
    unsafe {
        puts("rust task!\0".as_ptr());
    }

    loop {
        CurrentTask::delay(Duration::ms(500));
        unsafe { puts("rust!\0".as_ptr()) };
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn rust_init() {
    Task::new().name("rust printy").start(rust_printy).unwrap();
}

#[cfg(not(test))]
#[panic_handler]
fn panic(_info: &core::panic::PanicInfo) -> ! {
    loop {}
}
