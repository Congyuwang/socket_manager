use crate::c_api::async_ffi::notifier::Notifier;
use crate::c_api::utils::write_error_c_str;
use crate::msg_sender::CMsgSender;
use libc::size_t;
use std::ffi::{c_char, c_int, c_long};
use std::ptr::null_mut;
use std::task::Poll;

pub const PENDING: c_long = -1;

/// Send a message via the given `CMsgSender` synchronously.
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
    sender: *mut CMsgSender,
    msg: *const c_char,
    len: size_t,
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
            write_error_c_str(e, err);
            1
        }
    }
}

/// Try to send a message via the given `CMsgSender` asynchronously.
///
/// # Thread Safety
/// Thread safe.
///
/// This function is non-blocking, it returns `PENDING = -1`
/// if the send buffer is full.
///
/// Pass the `Notifier` to the `socket_manager_msg_sender_send_async` function
/// to receive the notification when the buffer is ready for writing.
///
/// When write is ready, the `socket_manager_msg_sender_send_async` function
/// returns the number of bytes written.
///
/// # Errors
/// Use `err` pointer to check for error.
/// On Error, `err` will be set to a pointer to a C string allocated by `malloc`.
#[no_mangle]
pub unsafe extern "C" fn socket_manager_msg_sender_send_async(
    sender: *mut CMsgSender,
    msg: *const c_char,
    len: size_t,
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
            write_error_c_str(e, err);
            0 as c_long
        }
        Poll::Pending => {
            PENDING
        }
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
    sender: *mut CMsgSender,
    err: *mut *mut c_char,
) -> c_int {
    let sender = &mut (*sender);
    match sender.flush() {
        Ok(_) => {
            *err = null_mut();
            0
        }
        Err(e) => {
            write_error_c_str(e, err);
            1
        }
    }
}

/// Destructor of `MsgSender`.
/// Drop sender to actively close the connection.
#[no_mangle]
pub unsafe extern "C" fn socket_manager_msg_sender_free(sender: *mut CMsgSender) {
    drop(Box::from_raw(sender))
}
