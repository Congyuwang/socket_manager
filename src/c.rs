use crate::{CSocketManager, Msg};
use libc::size_t;
use std::sync::Arc;
use std::{
    ffi::{c_char, c_int, c_ulonglong, c_void, CStr, CString},
    fmt::Display,
    net::SocketAddr,
    ptr::null_mut,
};
use tokio::sync::mpsc::UnboundedSender;

/// The data pointer is only valid for the duration of the callback.
#[repr(C)]
pub struct ConnMsg {
    conn_id: c_ulonglong,
    bytes: *const c_char,
    len: size_t,
}

/// All data is only valid for the duration of the callback
/// except for the `sender` field of `OnConnect`.
#[repr(C)]
pub struct ConnStates {
    code: ConnStateCode,
    data: ConnStateData,
}

#[repr(C)]
pub enum ConnStateCode {
    Connect = 0,
    ConnectionClose = 1,
    ListenError = 2,
    ConnectError = 3,
}

#[repr(C)]
pub union ConnStateData {
    on_connect: OnConnect,
    on_connection_close: OnConnectionClose,
    on_listen_error: OnListenError,
    on_connect_error: OnConnectError,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct OnConnect {
    conn_id: c_ulonglong,
    local: *const c_char,
    peer: *const c_char,
    sender: *mut CMsgSender,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct OnConnectionClose {
    conn_id: c_ulonglong,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct OnListenError {
    addr: *const c_char,
    err: *const c_char,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct OnConnectError {
    addr: *const c_char,
    err: *const c_char,
}

pub struct CMsgSender {
    send: UnboundedSender<Vec<u8>>,
}

struct CallbackObj {
    inner: *mut c_void,
}

unsafe impl Send for CallbackObj {}
unsafe impl Sync for CallbackObj {}

/// Intialize a new `SocketManager` and return a pointer to it.
///
/// # Safety
/// The passed in function pointers must live as long as the `SocketManager` does.
///
/// # Thread Safety
/// Must ensure that the callback functions of `callback_obj` is thread safe! (i.e., synced).
///
/// # Errors
/// On Error, `err` will be set to a pointer to a C string allocated by `malloc`,
/// and the returned pointer will be null.
#[no_mangle]
pub unsafe extern "C" fn socket_manager_init(
    callback_obj: *mut c_void,
    on_conn: unsafe extern "C" fn(*mut c_void, ConnStates),
    on_msg: unsafe extern "C" fn(*mut c_void, ConnMsg),
    err: *mut *mut c_char,
) -> *mut CSocketManager {
    let callback_obj = Arc::new(CallbackObj {
        inner: callback_obj,
    });
    let callback_obj_clone = callback_obj.clone();
    let on_conn = move |conn_msg| match conn_msg {
        crate::ConnState::OnConnect {
            conn_id,
            local_addr,
            peer_addr,
            send,
        } => {
            let msg_sender = Box::into_raw(Box::new(CMsgSender { send }));
            let local = CString::new(local_addr.to_string()).unwrap();
            let peer = CString::new(peer_addr.to_string()).unwrap();
            let conn_msg = ConnStates {
                code: ConnStateCode::Connect,
                data: ConnStateData {
                    on_connect: OnConnect {
                        conn_id,
                        local: local.as_ptr(),
                        peer: peer.as_ptr(),
                        sender: msg_sender,
                    },
                },
            };
            on_conn(callback_obj.inner, conn_msg);
        }
        crate::ConnState::OnConnectionClose { conn_id } => {
            let conn_msg = ConnStates {
                code: ConnStateCode::ConnectionClose,
                data: ConnStateData {
                    on_connection_close: OnConnectionClose { conn_id },
                },
            };
            on_conn(callback_obj.inner, conn_msg);
        }
        crate::ConnState::OnListenError { addr, error } => {
            let addr = CString::new(addr.to_string()).unwrap();
            let error = CString::new(error.to_string()).unwrap();
            let conn_msg = ConnStates {
                code: ConnStateCode::ListenError,
                data: ConnStateData {
                    on_listen_error: OnListenError {
                        addr: addr.as_ptr(),
                        err: error.as_ptr(),
                    },
                },
            };
            on_conn(callback_obj.inner, conn_msg);
        }
        crate::ConnState::OnConnectError { addr, error } => {
            let addr = CString::new(addr.to_string()).unwrap();
            let error = CString::new(error.to_string()).unwrap();
            let conn_msg = ConnStates {
                code: ConnStateCode::ConnectError,
                data: ConnStateData {
                    on_connect_error: OnConnectError {
                        addr: addr.as_ptr(),
                        err: error.as_ptr(),
                    },
                },
            };
            on_conn(callback_obj.inner, conn_msg);
        }
    };
    let on_msg = move |msg: Msg| {
        on_msg(
            callback_obj_clone.inner,
            ConnMsg {
                conn_id: msg.conn_id,
                bytes: msg.bytes.as_ptr() as *const c_char,
                len: msg.bytes.len(),
            },
        );
    };
    match CSocketManager::init(on_conn, on_msg) {
        Ok(manager) => {
            *err = null_mut();
            Box::into_raw(Box::new(manager))
        }
        Err(e) => {
            write_error_c_str(e, err);
            null_mut()
        }
    }
}

/// Listen on the given address.
///
/// # Errors
/// Returns -1 on error, 0 on success.
/// On Error, `err` will be set to a pointer to a C string allocated by `malloc`.
#[no_mangle]
pub unsafe extern "C" fn socket_manager_listen_on_addr(
    manager: *mut CSocketManager,
    addr: *const c_char,
    err: *mut *mut c_char,
) -> c_int {
    let manager = &*manager;
    match socket_addr(addr) {
        Ok(addr) => match manager.listen_on_addr(addr) {
            Ok(_) => {
                *err = null_mut();
                0
            }
            Err(e) => {
                write_error_c_str(e, err);
                -1
            }
        },
        Err(e) => {
            write_error_c_str(e, err);
            -1
        }
    }
}

/// Connect to the given address.
///
/// # Errors
/// Returns -1 on error, 0 on success.
/// On Error, `err` will be set to a pointer to a C string allocated by `malloc`.
#[no_mangle]
pub unsafe extern "C" fn socket_manager_connect_to_addr(
    manager: *mut CSocketManager,
    addr: *const c_char,
    err: *mut *mut c_char,
) -> c_int {
    let manager = &*manager;
    match socket_addr(addr) {
        Ok(addr) => match manager.connect_to_addr(addr) {
            Ok(_) => {
                *err = null_mut();
                0
            }
            Err(e) => {
                write_error_c_str(e, err);
                -1
            }
        },
        Err(e) => {
            write_error_c_str(e, err);
            -1
        }
    }
}

/// Cancel listening on the given address.
///
/// # Errors
/// Returns -1 on error, 0 on success.
/// On Error, `err` will be set to a pointer to a C string allocated by `malloc`.
#[no_mangle]
pub unsafe extern "C" fn socket_manager_cancel_listen_on_addr(
    manager: *mut CSocketManager,
    addr: *const c_char,
    err: *mut *mut c_char,
) -> c_int {
    let manager = &*manager;
    match socket_addr(addr) {
        Ok(addr) => match manager.cancel_listen_on_addr(addr) {
            Ok(_) => {
                *err = null_mut();
                0
            }
            Err(e) => {
                write_error_c_str(e, err);
                -1
            }
        },
        Err(e) => {
            write_error_c_str(e, err);
            -1
        }
    }
}

/// Cancel (abort) a connection.
///
/// # Errors
/// Returns -1 on error, 0 on success.
/// On Error, `err` will be set to a pointer to a C string allocated by `malloc`.
#[no_mangle]
pub unsafe extern "C" fn socket_manager_cancel_connection(
    manager: *mut CSocketManager,
    id: c_ulonglong,
    err: *mut *mut c_char,
) -> c_int {
    let manager = &*manager;
    match manager.cancel_connection(id) {
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

/// Detach the `SocketManager`'s background runtime.
#[no_mangle]
pub unsafe extern "C" fn socket_manager_detach(
    manager: *mut CSocketManager,
    err: *mut *mut c_char,
) -> c_int {
    let manager = &mut *manager;
    manager.detach();
    *err = null_mut();
    0
}

/// Join and wait on the `SocketManager`.
#[no_mangle]
pub unsafe extern "C" fn socket_manager_join(
    manager: *mut CSocketManager,
    err: *mut *mut c_char,
) -> c_int {
    let manager = &mut *manager;
    match manager.join() {
        Ok(()) => {
            *err = null_mut();
            0
        }
        Err(e) => {
            write_error_c_str(e, err);
            -1
        }
    }
}

/// Destroy a `SocketManager` and free its memory.
#[no_mangle]
pub unsafe extern "C" fn socket_manager_free(manager: *mut CSocketManager) {
    drop(Box::from_raw(manager))
}

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

/// Parse a C string into a `SocketAddr`.
unsafe fn socket_addr(addr: *const c_char) -> std::io::Result<SocketAddr> {
    let addr = CStr::from_ptr(addr)
        .to_str()
        .map_err(|e| std::io::Error::new(std::io::ErrorKind::InvalidInput, format!("{e}")))?;
    addr.parse::<SocketAddr>()
        .map_err(|e| std::io::Error::new(std::io::ErrorKind::InvalidInput, format!("{e}")))
}

/// Turn an error with `Display` into a C string pointer using `malloc`.
unsafe fn write_error_c_str<E: Display>(e: E, error: *mut *mut c_char) {
    let error_str = CString::new(format!("{}", e)).unwrap();
    *error = libc::malloc(error_str.as_bytes().len() + 1) as *mut c_char;
    libc::strcpy(*error, error_str.as_ptr());
}
