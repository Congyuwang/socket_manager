use crate::c_api::callbacks::OnMsgObj;
use crate::c_api::structs::CConnection;
use crate::c_api::utils::write_error_c_str;
use crate::{CMsgSender, ConnConfig};
use std::ffi::{c_char, c_int};
use std::os::raw::c_ulonglong;
use std::ptr::null_mut;
use std::time::Duration;

/// Start a connection with the given `OnMsgCallback`, and return a pointer to a `CMsgSender`.
///
/// Only one of `connection_start` or `connection_close` should be called,
/// or it will result in runtime error.
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
    on_msg: OnMsgObj,
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

/// Close the connection without using it.
///
/// Only one of `connection_start` or `connection_close` should be called,
/// or it will result in runtime error.
///
/// # Thread Safety
/// Thread safe.
///
/// # Errors
/// Returns -1 on error, 0 on success.
/// On Error, `err` will be set to a pointer to a C string allocated by `malloc`.
#[no_mangle]
pub unsafe extern "C" fn connection_close(conn: *mut CConnection, err: *mut *mut c_char) -> c_int {
    let conn = &mut (*conn).conn;
    match conn.close() {
        Ok(_) => {
            *err = null_mut();
            0
        }
        Err(e) => {
            write_error_c_str(e, err);
            -1
        }
    }
}

/// Destructor of `Connection`.
#[no_mangle]
pub unsafe extern "C" fn connection_free(conn: *mut CConnection) {
    drop(Box::from_raw(conn))
}
