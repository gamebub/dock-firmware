use core::{
    cell::UnsafeCell,
    mem::MaybeUninit,
    sync::atomic::{AtomicBool, Ordering},
};

pub struct InitCell<T> {
    init_started: AtomicBool,
    init_finished: AtomicBool,
    data: UnsafeCell<MaybeUninit<T>>,
}

impl<T> InitCell<T> {
    pub const fn new() -> Self {
        InitCell {
            init_started: AtomicBool::new(false),
            init_finished: AtomicBool::new(false),
            data: UnsafeCell::new(MaybeUninit::uninit()),
        }
    }

    pub fn set(&self, value: T) {
        let started = self.init_started.swap(true, Ordering::SeqCst);
        assert!(!started);
        unsafe { (*self.data.get()).write(value) };
        self.init_finished.store(true, Ordering::SeqCst);
    }

    pub fn get(&self) -> &T {
        self.maybe_get().unwrap()
    }

    pub fn maybe_get(&self) -> Option<&T> {
        if self.init_finished.load(Ordering::SeqCst) {
            Some(unsafe { (*self.data.get()).assume_init_ref() })
        } else {
            None
        }
    }
}

unsafe impl<T: Sync> Sync for InitCell<T> {}
