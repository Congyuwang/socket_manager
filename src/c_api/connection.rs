use crate::c_api::structs::{CConnection, CMsgSender, OnMsgCallback};
use crate::c_api::utils::write_error_c_str;
use std::ffi::c_char;
use std::ptr::null_mut;

/// Start a connection with the given `OnMsgCallback`, and return a pointer to a `CMsgSender`.
///
/// This function can only be called once per `CConnection`,
/// otherwise it returns error.
///
/// # Safety
/// The passed in callback must live as long as the connection is not closed !!
///
/// # Arguments
/// * `conn` - A pointer to a `CConnection`.
/// * `on_msg` - A callback function that will be called when a message is received.
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
    err: *mut *mut c_char,
) -> *mut CMsgSender {
    let conn = &mut (*conn).conn;
    match conn.start_connection(on_msg) {
        Ok(sender) => {
            *err = null_mut();
            Box::into_raw(Box::new(CMsgSender { send: sender }))
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
