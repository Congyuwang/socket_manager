use crate::c_api::callbacks::OnMsgCallback;
use crate::c_api::structs::CConnection;
use crate::c_api::utils::write_error_c_str;
use crate::{CMsgSender, ConnConfig};
use std::ffi::c_char;
use std::os::raw::c_ulonglong;
use std::ptr::null_mut;
use std::time::Duration;

/// Start a connection with the given `OnMsgCallback`, and return a pointer to a `CMsgSender`.
///
/// The `start` function must be called exactly once.
/// Calling it twice will result in runtime error.
/// Not calling it will result in resource leak
/// (i.e. the connection will likely hangs).
///
/// # Safety
/// The passed in callback must live as long as the connection is not closed !!
///
/// # ThreadSafety
/// Thread safe, but should be called exactly once.
///
/// # Arguments
/// * `conn` - A pointer to a `CConnection`.
/// * `on_msg` - A callback function that will be called when a message is received.
/// * `write_flush_interval` - The interval in `milliseconds` of write buffer auto flushing.
///    Set to 0 to disable auto flush.
/// * `err` - A pointer to a pointer to a C string allocated by `malloc` on error.
///
/// # Returns
/// A pointer to a `CMsgSender` on success, null on error.
///
/// # Errors
/// On Error, `err` will be set to a pointer to a C string allocated by `malloc`,
/// and the returned pointer will be null.
#[no_mangle]
pub unsafe extern "C" fn connection_start(
    conn: *mut CConnection,
    on_msg: OnMsgCallback,
    write_flush_interval: c_ulonglong,
    err: *mut *mut c_char,
) -> *mut CMsgSender {
    let conn = &mut (*conn).conn;
    let write_flush_interval = if write_flush_interval == 0 {
        None
    } else {
        Some(Duration::from_millis(write_flush_interval))
    };
    match conn.start_connection(
        on_msg,
        ConnConfig {
            write_flush_interval,
        },
    ) {
        Ok(sender) => {
            *err = null_mut();
            Box::into_raw(Box::new(sender))
        }
        Err(e) => {
            write_error_c_str(e, err);
            null_mut()
        }
    }
}

/// Destructor of `Connection`.
#[no_mangle]
pub unsafe extern "C" fn connection_free(conn: *mut CConnection) {
    drop(Box::from_raw(conn))
}
