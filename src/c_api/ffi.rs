use crate::c_api::structs::{ConnMsg, ConnStates};
use std::ffi::{c_char, c_void};

#[link(name = "socket_manager")]
extern "C" {
    /// Callback function for receiving messages.
    pub(crate) fn socket_manager_extern_on_msg(this: *mut c_void, msg: ConnMsg) -> *mut c_char;

    /// Callback function for connection state changes.
    pub(crate) fn socket_manager_extern_on_conn(this: *mut c_void, conn: ConnStates)
        -> *mut c_char;
}
