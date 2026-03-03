use freertos_rust::{Duration, Queue, Task};
use static_cell::StaticCell;

use crate::util::InitCell;

static QUEUE: InitCell<Queue<Message>> = InitCell::new();
static ENGINE: StaticCell<Engine> = StaticCell::new();

#[derive(Debug)]
pub enum Message {
    Ping,
}

/// Main state machine
struct Engine {}

impl Engine {
    fn new() -> Self {
        Engine {}
    }

    fn handle(&mut self, message: Message) {
        log::info!("{:?}", message);
    }
}

fn task(_task: Task) {
    let state = ENGINE.init(Engine::new());
    loop {
        let message = QUEUE.get().receive(Duration::infinite()).unwrap();
        state.handle(message);
    }
}

pub fn send(message: Message) {
    if let Err(_) = QUEUE.get().send(message, Duration::zero()) {
        log::warn!("Failed to post message");
    }
}

pub fn start_task() {
    QUEUE.set(Queue::new(32).unwrap());

    Task::new().name("engine").start(task).unwrap();
}
