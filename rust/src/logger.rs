use crate::sys;
use alloc::string::String;
use core::fmt::Write;
use freertos_rust::{Duration, FreeRtosUtils, Mutex};
use log::{Level, LevelFilter, Metadata, Record};
use static_cell::StaticCell;

struct StdioLogger {
    buffer: Mutex<String>,
}

impl log::Log for StdioLogger {
    fn enabled(&self, metadata: &Metadata) -> bool {
        metadata.level() <= Level::Info
    }

    fn log(&self, record: &Record) {
        if !self.enabled(record.metadata()) {
            return;
        }

        let time_ms = Duration::ticks(FreeRtosUtils::get_tick_count()).to_ms();

        let mut buffer = self.buffer.lock(Duration::infinite()).unwrap();
        buffer.clear();
        write!(
            buffer,
            "{} ({}) {}: {}\0",
            record.level(),
            time_ms,
            record.module_path().unwrap_or(""),
            record.args()
        )
        .unwrap();
        unsafe {
            sys::puts(buffer.as_ptr());
        }
    }

    fn flush(&self) {}
}

impl StdioLogger {
    pub fn new() -> Self {
        StdioLogger {
            buffer: Mutex::new(String::new()).unwrap(),
        }
    }
}

static LOGGER: StaticCell<StdioLogger> = StaticCell::new();

pub fn init() {
    let logger = LOGGER.init(StdioLogger::new());
    log::set_logger(logger).unwrap();
    log::set_max_level(LevelFilter::Info);
}
