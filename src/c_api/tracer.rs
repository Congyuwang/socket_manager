//! Define a layer to pass log message to foreign interface.
use super::utils::write_error_c_str;
use crate::init_logger;
use libc::size_t;
use std::{ffi::c_char, fmt, ptr::null_mut};
use tracing::{field::Field, Level};
use tracing_subscriber::{filter::LevelFilter, Layer};

const MESSAGE: &str = "message";
const EMPTY: &str = "";

/// Trace Level
#[repr(C)]
pub enum TraceLevel {
    /// The "trace" level.
    ///
    /// Designates very low priority, often extremely verbose, information.
    Trace = 0,
    /// The "debug" level.
    ///
    /// Designates lower priority information.
    Debug = 1,
    /// The "info" level.
    ///
    /// Designates useful information.
    Info = 2,
    /// The "warn" level.
    ///
    /// Designates hazardous situations.
    Warn = 3,
    /// The "error" level.
    ///
    /// Designates very serious errors.
    Error = 4,
    /// Turn off all levels.
    ///
    /// Disable log output.
    Off = 5,
}

/// Log Data
#[repr(C)]
pub struct LogData {
    pub level: TraceLevel,
    pub target: *const c_char,
    pub target_n: size_t,
    pub file: *const c_char,
    pub file_n: size_t,
    /// The `message` pointer is only valid for the duration of the callback.
    pub message: *const c_char,
    pub message_n: size_t,
}

/// Init logger.
///
/// # Arguments
/// - `tracer`: The tracer object.
/// - `tracer_max_level`: The max level of the tracer.
/// - `log_print_level`: The level of the log to print.
#[no_mangle]
pub unsafe extern "C" fn socket_manager_logger_init(
    tracer: unsafe extern "C" fn(LogData) -> (),
    tracer_max_level: TraceLevel,
    log_print_level: TraceLevel,
    err: *mut *mut c_char,
) {
    let foreign_logger = ForeignLogger(tracer).with_filter(tracer_max_level.into());
    match init_logger(log_print_level.into(), foreign_logger) {
        Ok(_) => *err = null_mut(),
        Err(e) => write_error_c_str(e, err),
    }
}

pub struct ForeignLogger(unsafe extern "C" fn(LogData) -> ());

impl<S> Layer<S> for ForeignLogger
where
    S: tracing::Subscriber,
{
    fn on_event(
        &self,
        event: &tracing::Event<'_>,
        _ctx: tracing_subscriber::layer::Context<'_, S>,
    ) {
        let mut get_msg = GetMsgVisitor(None);
        event.record(&mut get_msg);
        let file = if let (Some(f), Some(l)) = (event.metadata().file(), event.metadata().line()) {
            format!("{}:{}", f, l)
        } else {
            String::new()
        };
        let data = LogData {
            level: event.metadata().level().into(),
            target: event.metadata().target().as_ptr() as *const c_char,
            target_n: event.metadata().target().len(),
            file: file.as_ptr() as *const c_char,
            file_n: file.len(),
            message: get_msg
                .0
                .as_ref()
                .map(|s| s.as_ptr())
                .unwrap_or(EMPTY.as_ptr()) as *const c_char,
            message_n: get_msg.0.as_ref().map(|s| s.len()).unwrap_or(0),
        };
        unsafe { self.0(data) }
    }
}

// Helper methods and structs.

struct GetMsgVisitor(Option<String>);

impl tracing::field::Visit for GetMsgVisitor {
    fn record_debug(&mut self, field: &Field, value: &dyn fmt::Debug) {
        if field.name() == MESSAGE {
            self.0 = Some(format!("{:?}", value));
        }
    }
}

impl Into<LevelFilter> for TraceLevel {
    fn into(self) -> LevelFilter {
        match self {
            TraceLevel::Trace => LevelFilter::TRACE,
            TraceLevel::Debug => LevelFilter::DEBUG,
            TraceLevel::Info => LevelFilter::INFO,
            TraceLevel::Warn => LevelFilter::WARN,
            TraceLevel::Error => LevelFilter::ERROR,
            TraceLevel::Off => LevelFilter::OFF,
        }
    }
}

impl From<&Level> for TraceLevel {
    fn from(value: &Level) -> Self {
        match *value {
            Level::TRACE => TraceLevel::Trace,
            Level::DEBUG => TraceLevel::Debug,
            Level::INFO => TraceLevel::Info,
            Level::WARN => TraceLevel::Warn,
            Level::ERROR => TraceLevel::Error,
        }
    }
}
