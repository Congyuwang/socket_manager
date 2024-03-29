use crate::c_api::async_ffi::notifier::Notifier;
use crate::c_api::utils::write_display_c_str;
use crate::msg_sender::MsgSender;
use std::ffi::{c_char, c_int, c_long};
use std::ptr::null_mut;
use std::task::Poll;

pub const PENDING: c_long = -1;

/// Send a message via the given `MsgSender` synchronously.
/// This is a blocking API.
///
/// # Thread Safety
/// Thread safe.
///
/// This function should never be called within the context of the async callbacks
/// since it might block.
///
/// # Errors
/// If the connection is closed, the function will return 1 and set `err` to a pointer
/// with WriteZero error.
///
/// Returns 1 on error, 0 on success.
/// On Error, `err` will be set to a pointer to a C string allocated by `malloc`.
#[no_mangle]
pub unsafe extern "C" fn socket_manager_msg_sender_send_block(
    sender: *mut MsgSender,
    msg: *const c_char,
    len: usize,
    err: *mut *mut c_char,
) -> c_int {
    let sender = &mut (*sender);
    let msg = std::slice::from_raw_parts(msg as *const u8, len);
    match sender.send_block(msg) {
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

/// Send a message via the given `MsgSender` .
/// This is a non-blocking API.
/// All sent data is buffered in a chain of ring buffer.
/// This method does not implement back pressure since it
/// caches all received data.
/// Use `send_async` or `send_block` for back pressure.
///
/// # Thread Safety
/// Thread safe.
///
/// This function can be called within the context of the async callbacks.
///
/// # Errors
/// If the connection is closed, the function will return 1 and set `err` to a pointer
/// with WriteZero error.
///
/// Returns 1 on error, 0 on success.
/// On Error, `err` will be set to a pointer to a C string allocated by `malloc`.
#[no_mangle]
pub unsafe extern "C" fn socket_manager_msg_sender_send_nonblock(
    sender: *mut MsgSender,
    msg: *const c_char,
    len: usize,
    err: *mut *mut c_char,
) -> c_int {
    let sender = &mut (*sender);
    let msg = std::slice::from_raw_parts(msg as *const u8, len);
    match sender.send_nonblock(msg) {
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

/// Try to send a message via the given `MsgSender` asynchronously.
///
/// # Thread Safety
/// Thread safe.
///
/// # Async control flow (IMPORTANT)
///
/// This function is non-blocking, it returns `PENDING = -1`
/// if the send buffer is full. So the caller should wait
/// by passing a `Notifier` which will be called when the
/// buffer is ready.
///
/// When the buffer is ready, the function returns number of bytes sent.
///
/// # Errors
/// Use `err` pointer to check for error.
/// On Error, `err` will be set to a pointer to a C string allocated by `malloc`.
#[no_mangle]
pub unsafe extern "C" fn socket_manager_msg_sender_send_async(
    sender: *mut MsgSender,
    msg: *const c_char,
    len: usize,
    notifier: Notifier,
    err: *mut *mut c_char,
) -> c_long {
    let sender = &mut (*sender);
    let msg = std::slice::from_raw_parts(msg as *const u8, len);
    // notifier should not be null
    assert!(!notifier.this.is_null());
    let waker = notifier.to_waker();
    match sender.send_async(msg, waker) {
        Poll::Ready(Ok(n)) => {
            *err = null_mut();
            n as c_long
        }
        Poll::Ready(Err(e)) => {
            write_display_c_str(e, err);
            0 as c_long
        }
        Poll::Pending => PENDING,
    }
}

/// Manually flush the message sender.
///
/// # Thread Safety
/// Thread safe.
///
/// # Errors
/// Returns 1 on error, 0 on success.
/// On Error, `err` will be set to a pointer to a C string allocated by `malloc`.
#[no_mangle]
pub unsafe extern "C" fn socket_manager_msg_sender_flush(
    sender: *mut MsgSender,
    err: *mut *mut c_char,
) -> c_int {
    let sender = &mut (*sender);
    match sender.flush() {
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

/// Destructor of `MsgSender`.
/// Drop sender to actively close the `Write` side of the connection.
#[no_mangle]
pub unsafe extern "C" fn socket_manager_msg_sender_free(sender: *mut MsgSender) {
    drop(Box::from_raw(sender))
}
