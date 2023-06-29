use crate::c_api::structs::{
    CConnection, ConnMsg, ConnStateCode, ConnStateData, ConnStates, OnConnect, OnConnectError,
    OnConnectionClose, OnListenError,
};
use crate::c_api::utils::parse_c_err_str;
use std::ffi::{c_char, c_void, CString};

/// Callback function for receiving messages.
///
/// `callback_self` is feed to the first argument of the callback.
///
/// # Error Handling
/// Returns null_ptr on success, otherwise returns a pointer to a malloced
/// C string containing the error message (the c string should be freed by the
/// caller).
///
/// # Safety
/// The callback pointer must be valid before connection is closed!!
///
/// # Thread Safety
/// Must be thread safe!
#[repr(C)]
#[derive(Clone)]
pub struct OnMsgCallback {
    callback_self: *mut c_void,
    callback: unsafe extern "C" fn(*mut c_void, ConnMsg) -> *mut c_char,
}

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
#[derive(Clone)]
pub struct OnConnCallback {
    callback_self: *mut c_void,
    callback: unsafe extern "C" fn(*mut c_void, ConnStates) -> *mut c_char,
}

impl OnMsgCallback {
    pub fn call_inner(&self, conn_msg: crate::Msg<'_>) -> Result<(), String> {
        let conn_msg = ConnMsg {
            bytes: conn_msg.bytes.as_ptr() as *const c_char,
            len: conn_msg.bytes.len(),
        };
        unsafe {
            let cb_result = (self.callback)(self.callback_self, conn_msg);
            if let Err(e) = parse_c_err_str(cb_result) {
                tracing::error!("Error thrown in OnMsg callback: {e}");
                Err(e)
            } else {
                Ok(())
            }
        }
    }
}

impl FnMut<(crate::Msg<'_>,)> for OnMsgCallback {
    extern "rust-call" fn call_mut(&mut self, conn_msg: (crate::Msg<'_>,)) -> Self::Output {
        self.call_inner(conn_msg.0)
    }
}

impl FnOnce<(crate::Msg<'_>,)> for OnMsgCallback {
    type Output = Result<(), String>;

    extern "rust-call" fn call_once(self, conn_msg: (crate::Msg<'_>,)) -> Self::Output {
        self.call_inner(conn_msg.0)
    }
}

impl Fn<(crate::Msg<'_>,)> for OnMsgCallback {
    extern "rust-call" fn call(&self, conn_msg: (crate::Msg<'_>,)) -> Self::Output {
        self.call_inner(conn_msg.0)
    }
}

impl OnConnCallback {
    /// connection callback
    pub(crate) fn call_inner(
        &self,
        conn_states: crate::ConnState<OnMsgCallback>,
    ) -> Result<(), String> {
        let on_conn = |msg| unsafe {
            let cb_result = (self.callback)(self.callback_self, msg);
            parse_c_err_str(cb_result)
        };
        match conn_states {
            crate::ConnState::OnConnect {
                local_addr,
                peer_addr,
                conn,
            } => {
                let local = CString::new(local_addr.to_string()).unwrap();
                let peer = CString::new(peer_addr.to_string()).unwrap();
                let conn = Box::into_raw(Box::new(CConnection { conn }));
                let conn_msg = ConnStates {
                    code: ConnStateCode::Connect,
                    data: ConnStateData {
                        on_connect: OnConnect {
                            local: local.as_ptr(),
                            peer: peer.as_ptr(),
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

impl FnMut<(crate::ConnState<OnMsgCallback>,)> for OnConnCallback {
    extern "rust-call" fn call_mut(
        &mut self,
        conn_msg: (crate::ConnState<OnMsgCallback>,),
    ) -> Self::Output {
        self.call_inner(conn_msg.0)
    }
}

impl FnOnce<(crate::ConnState<OnMsgCallback>,)> for OnConnCallback {
    type Output = Result<(), String>;

    extern "rust-call" fn call_once(
        self,
        conn_msg: (crate::ConnState<OnMsgCallback>,),
    ) -> Self::Output {
        self.call_inner(conn_msg.0)
    }
}

impl Fn<(crate::ConnState<OnMsgCallback>,)> for OnConnCallback {
    extern "rust-call" fn call(
        &self,
        conn_msg: (crate::ConnState<OnMsgCallback>,),
    ) -> Self::Output {
        self.call_inner(conn_msg.0)
    }
}

unsafe impl Send for OnMsgCallback {}
unsafe impl Send for OnConnCallback {}
