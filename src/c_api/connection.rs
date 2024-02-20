use crate::c_api::conn_events::Connection;
use crate::c_api::on_msg::OnMsgObj;
use crate::c_api::utils::write_display_c_str;
use crate::conn::ConnConfig;
use libc::size_t;
use std::ffi::{c_char, c_int};
use std::os::raw::c_ulonglong;
use std::ptr::null_mut;
use std::time::Duration;

/// Start a connection with the given `OnMsgCallback`, and return a pointer to a `MsgSender`.
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
/// * `msg_buffer_size` - The size of the message buffer in bytes.
///    The minimum is 8KB, and the maximum is 8MB.
/// * `read_msg_flush_interval` - The interval in `milliseconds` of read message buffer
///    auto flushing. The value is ignored when `msg_buffer_size` is 0.
///    Set to 0 to disable auto flush (which is not recommended since there is no
///    manual flush, and small messages might get stuck in buffer).
/// * `write_flush_interval` - The interval in `milliseconds` of write buffer auto flushing.
///    Set to 0 to disable auto flush.
/// * `err` - A pointer to a pointer to a C string allocated by `malloc` on error.
///
/// # Errors
/// Returns 1 on error, 0 on success.
/// On Error, `err` will be set to a pointer to a C string allocated by `malloc`,
#[no_mangle]
pub unsafe extern "C" fn socket_manager_connection_start(
    conn: *mut Connection,
    on_msg: OnMsgObj,
    msg_buffer_size: size_t,
    read_msg_flush_interval: c_ulonglong,
    write_flush_interval: c_ulonglong,
    err: *mut *mut c_char,
) -> c_int {
    let conn = &mut (*conn).conn;
    let write_flush_interval = Duration::from_millis(write_flush_interval);
    let read_msg_flush_interval = Duration::from_millis(read_msg_flush_interval);
    match conn.start_connection(
        on_msg,
        ConnConfig {
            write_flush_interval,
            read_msg_flush_interval,
            msg_buffer_size,
        },
    ) {
        Ok(_) => {
            *err = null_mut();
            0
        }
        Err(e) => {
            write_display_c_str(e, err);
            1
        }
    }
}

/// Local address of the connection.
///
/// The returned string is malloced and should be freed by the caller.
#[no_mangle]
pub unsafe extern "C" fn socket_manager_connection_local_addr(
    conn: *mut Connection,
) -> *mut c_char {
    let conn = &mut (*conn).conn;
    let mut local: *mut c_char = null_mut();
    write_display_c_str(conn.local_addr, &mut local);
    local
}

/// Peer address of the connection.
///
/// The returned string is malloced and should be freed by the caller.
#[no_mangle]
pub unsafe extern "C" fn socket_manager_connection_peer_addr(conn: *mut Connection) -> *mut c_char {
    let conn = &mut (*conn).conn;
    let mut peer: *mut c_char = null_mut();
    write_display_c_str(conn.peer_addr, &mut peer);
    peer
}

/// The close API works either before `start_connection` is called
/// or after the `on_connect` callback.
/// It won't have effect between the two points.
///
/// # Thread Safety
/// Thread safe.
///
/// # Errors
/// Returns 1 on error, 0 on success.
/// On Error, `err` will be set to a pointer to a C string allocated by `malloc`.
#[no_mangle]
pub unsafe extern "C" fn socket_manager_connection_close(
    conn: *mut Connection,
    err: *mut *mut c_char,
) -> c_int {
    let conn = &mut (*conn).conn;
    match conn.close() {
        Ok(_) => {
            *err = null_mut();
            0
        }
        Err(e) => {
            write_display_c_str(e, err);
            1
        }
    }
}

/// Destructor of `Connection`.
#[no_mangle]
pub unsafe extern "C" fn socket_manager_connection_free(conn: *mut Connection) {
    drop(Box::from_raw(conn))
}
