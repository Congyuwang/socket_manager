use crate::c_api::structs::CMsgSender;
use crate::c_api::utils::write_error_c_str;
use libc::size_t;
use std::ffi::{c_char, c_int};
use std::ptr::null_mut;

/// Send a message via the given `CMsgSender`.
///
/// # Errors
/// Returns -1 on error, 0 on success.
/// On Error, `err` will be set to a pointer to a C string allocated by `malloc`.
#[no_mangle]
pub unsafe extern "C" fn msg_sender_send(
    sender: *const CMsgSender,
    msg: *const c_char,
    len: size_t,
    err: *mut *mut c_char,
) -> c_int {
    let sender = &(*sender).send;
    let msg = std::slice::from_raw_parts(msg as *const u8, len).to_vec();
    match sender.send(msg) {
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
#[no_mangle]
pub unsafe extern "C" fn msg_sender_free(sender: *mut CMsgSender) {
    drop(Box::from_raw(sender))
}