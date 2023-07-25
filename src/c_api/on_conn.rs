use crate::c_api::conn_events::{
    ConnStateCode, ConnStateData, ConnStates, Connection, OnConnect, OnConnectError,
    OnConnectionClose, OnListenError,
};
use crate::c_api::on_msg::OnMsgObj;
use crate::c_api::utils::parse_c_err_str;
use std::ffi::{c_char, c_void, CString};
use std::ptr::null_mut;

/// Callback function for connection state changes.
///
/// `callback_self` is feed to the first argument of the callback.
///
/// # Error Handling
/// Returns null_ptr on success, otherwise returns a pointer to a malloced
/// C string containing the error message (the c string should be freed by the
/// caller).
///
/// # Safety
/// The callback pointer must be valid for the entire runtime lifetime!!
/// (i.e., before the runtime is aborted and joined).
///
/// # Thread Safety
/// Must be thread safe!
#[repr(C)]
#[derive(Copy, Clone)]
pub struct OnConnObj {
    this: *mut c_void,
}

#[link(name = "socket_manager")]
extern "C" {
    /// Rust calls this function to send `conn: ConnStates`
    /// to the `this: OnConnObj`. If the process has any error,
    /// pass error to `err` pointer.
    /// Set `err` to null_ptr if there is no error.
    pub(crate) fn socket_manager_extern_on_conn(
        this: OnConnObj,
        conn: ConnStates,
        err: *mut *mut c_char,
    );
}

impl OnConnObj {
    /// connection callback
    pub(crate) fn call_inner(&self, conn_states: crate::ConnState<OnMsgObj>) -> Result<(), String> {
        let on_conn = |conn| unsafe {
            let mut err: *mut c_char = null_mut();
            socket_manager_extern_on_conn(*self, conn, &mut err);
            parse_c_err_str(err)
        };
        match conn_states {
            crate::ConnState::OnConnect {
                local_addr,
                peer_addr,
                send,
                conn,
            } => {
                let local = CString::new(local_addr.to_string()).unwrap();
                let peer = CString::new(peer_addr.to_string()).unwrap();
                let send = Box::into_raw(Box::new(send));
                let conn = Box::into_raw(Box::new(Connection { conn }));
                let conn_msg = ConnStates {
                    code: ConnStateCode::Connect,
                    data: ConnStateData {
                        on_connect: OnConnect {
                            local: local.as_ptr(),
                            peer: peer.as_ptr(),
                            send,
                            conn,
                        },
                    },
                };
                if let Err(e) = on_conn(conn_msg) {
                    tracing::error!(
                        "Error thrown in OnConnect callback (local={local_addr}, peer={peer_addr}): {e}"
                    );
                    Err(e)
                } else {
                    Ok(())
                }
            }
            crate::ConnState::OnConnectionClose {
                local_addr,
                peer_addr,
            } => {
                let local = CString::new(local_addr.to_string()).unwrap();
                let peer = CString::new(peer_addr.to_string()).unwrap();
                let conn_msg = ConnStates {
                    code: ConnStateCode::ConnectionClose,
                    data: ConnStateData {
                        on_connection_close: OnConnectionClose {
                            local: local.as_ptr(),
                            peer: peer.as_ptr(),
                        },
                    },
                };
                if let Err(e) = on_conn(conn_msg) {
                    tracing::error!("Error thrown in OnConnectionClose callback (local={local_addr}, peer={peer_addr}): {e}");
                    Err(e)
                } else {
                    Ok(())
                }
            }
            crate::ConnState::OnListenError { addr, error } => {
                let c_addr = CString::new(addr.to_string()).unwrap();
                let error = CString::new(error.to_string()).unwrap();
                let conn_msg = ConnStates {
                    code: ConnStateCode::ListenError,
                    data: ConnStateData {
                        on_listen_error: OnListenError {
                            addr: c_addr.as_ptr(),
                            err: error.as_ptr(),
                        },
                    },
                };
                if let Err(e) = on_conn(conn_msg) {
                    tracing::error!("Error thrown in OnListenError callback addr={addr}: {e}");
                    Err(e)
                } else {
                    Ok(())
                }
            }
            crate::ConnState::OnConnectError { addr, error } => {
                let c_addr = CString::new(addr.to_string()).unwrap();
                let error = CString::new(error.to_string()).unwrap();
                let conn_msg = ConnStates {
                    code: ConnStateCode::ConnectError,
                    data: ConnStateData {
                        on_connect_error: OnConnectError {
                            addr: c_addr.as_ptr(),
                            err: error.as_ptr(),
                        },
                    },
                };
                if let Err(e) = on_conn(conn_msg) {
                    tracing::error!("Error thrown in OnConnectError callback addr={addr}: {e}");
                    Err(e)
                } else {
                    Ok(())
                }
            }
        }
    }
}

impl FnMut<(crate::ConnState<OnMsgObj>,)> for OnConnObj {
    extern "rust-call" fn call_mut(
        &mut self,
        conn_msg: (crate::ConnState<OnMsgObj>,),
    ) -> Self::Output {
        self.call_inner(conn_msg.0)
    }
}

impl FnOnce<(crate::ConnState<OnMsgObj>,)> for OnConnObj {
    type Output = Result<(), String>;

    extern "rust-call" fn call_once(self, conn_msg: (crate::ConnState<OnMsgObj>,)) -> Self::Output {
        self.call_inner(conn_msg.0)
    }
}

impl Fn<(crate::ConnState<OnMsgObj>,)> for OnConnObj {
    extern "rust-call" fn call(&self, conn_msg: (crate::ConnState<OnMsgObj>,)) -> Self::Output {
        self.call_inner(conn_msg.0)
    }
}

unsafe impl Send for OnConnObj {}

unsafe impl Sync for OnConnObj {}
