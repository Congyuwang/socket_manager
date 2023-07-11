use crate::c_api::callbacks::MsgSenderObj;
use crate::c_api::utils::write_error_c_str;
use crate::CMsgSender;
use libc::size_t;
use std::ffi::{c_char, c_int};
use std::ptr::null_mut;

/// Send a message via the given `CMsgSender`.
///
/// # Thread Safety
/// Thread safe.
///
/// This function should never be called within the context of the async callbacks
/// since it might block.
///
/// # Errors
/// Returns -1 on error, 0 on success.
/// On Error, `err` will be set to a pointer to a C string allocated by `malloc`.
#[no_mangle]
pub unsafe extern "C" fn msg_sender_send(
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
            -1
        }
    }
}

/// Try to send a message via the given `CMsgSender`.
///
/// # Thread Safety
/// Thread safe.
///
/// This function is non-blocking, pass the MsgSender class
/// to the waker_obj to receive notification to continue
/// sending the message.
///
/// # Errors
/// On Error, `err` will be set to a pointer to a C string allocated by `malloc`.
#[no_mangle]
pub unsafe extern "C" fn msg_sender_try_send(
    sender: *mut CMsgSender,
    msg: *const c_char,
    len: size_t,
    waker_obj: MsgSenderObj,
    err: *mut *mut c_char,
) -> size_t {
    let sender = &mut (*sender);
    let msg = std::slice::from_raw_parts(msg as *const u8, len);
    match sender.try_send(msg, waker_obj) {
        Ok(n) => {
            *err = null_mut();
            n
        }
        Err(e) => {
            write_error_c_str(e, err);
            0
        }
    }
}

/// Manually flush the message sender.
///
/// # Thread Safety
/// Thread safe.
///
/// # Errors
/// Returns -1 on error, 0 on success.
/// On Error, `err` will be set to a pointer to a C string allocated by `malloc`.
#[no_mangle]
pub unsafe extern "C" fn msg_sender_flush(sender: *mut CMsgSender, err: *mut *mut c_char) -> c_int {
    let sender = &mut (*sender);
    match sender.flush() {
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

/// Destructor of `MsgSender`.
/// Drop sender to actively close the connection.
#[no_mangle]
pub unsafe extern "C" fn msg_sender_free(sender: *mut CMsgSender) {
    drop(Box::from_raw(sender))
}
