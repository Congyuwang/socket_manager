use crate::c_api::on_conn::OnConnObj;
use crate::c_api::utils::{socket_addr, write_display_c_str};
use crate::SocketManager;
use libc::size_t;
use std::ffi::{c_char, c_int};
use std::ptr::null_mut;
use std::time::Duration;

/// Initialize a new `SocketManager` and return a pointer to it.
///
/// # Number of workers
/// If `n_threads` is 0, the number of workers will be set to the number of logical cores.
/// If `n_threads` is 1, uses single-threaded runtime.
/// `n_threads` is capped at 256.
///
/// # connection callback
/// `on_conn_self` is passed to the callback function `on_conn` as the first argument.
///
/// # Safety
/// The passed in callback pointers must live as long as the `SocketManager` does.
///
/// # Non-blocking
/// Must ensure that the callback functions of `callback_obj` are non-blocking.
///
/// # Errors
/// On Error, `err` will be set to a pointer to a C string allocated by `malloc`,
/// and the returned pointer will be null.
#[no_mangle]
pub unsafe extern "C" fn socket_manager_init(
    on_conn: OnConnObj,
    n_threads: size_t,
    err: *mut *mut c_char,
) -> *mut SocketManager {
    match SocketManager::init(on_conn, n_threads) {
        Ok(manager) => {
            *err = null_mut();
            Box::into_raw(Box::new(manager))
        }
        Err(e) => {
            write_display_c_str(e, err);
            null_mut()
        }
    }
}

/// Listen on the given address.
///
/// # ThreadSafety
/// Thread safe.
///
/// # Errors
/// Returns 1 on error, 0 on success.
/// On Error, `err` will be set to a pointer to a C string allocated by `malloc`.
#[no_mangle]
pub unsafe extern "C" fn socket_manager_listen_on_addr(
    manager: *mut SocketManager,
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
                write_display_c_str(e, err);
                1
            }
        },
        Err(e) => {
            write_display_c_str(e, err);
            1
        }
    }
}

/// Connect to the given address.
///
/// # Arguments
/// - `delay`: delay in milliseconds before connecting.
///
/// # Thread Safety
/// Thread safe.
///
/// # Errors
/// Returns 1 on error, 0 on success.
/// On Error, `err` will be set to a pointer to a C string allocated by `malloc`.
#[no_mangle]
pub unsafe extern "C" fn socket_manager_connect_to_addr(
    manager: *mut SocketManager,
    addr: *const c_char,
    delay: u64,
    err: *mut *mut c_char,
) -> c_int {
    let manager = &*manager;
    match socket_addr(addr) {
        Ok(addr) => match manager.connect_to_addr(addr, Duration::from_millis(delay)) {
            Ok(_) => {
                *err = null_mut();
                0
            }
            Err(e) => {
                write_display_c_str(e, err);
                1
            }
        },
        Err(e) => {
            write_display_c_str(e, err);
            1
        }
    }
}

/// Cancel listening on the given address.
///
/// # Thread Safety
/// Thread safe.
///
/// # Errors
/// Returns 1 on error, 0 on success.
/// On Error, `err` will be set to a pointer to a C string allocated by `malloc`.
#[no_mangle]
pub unsafe extern "C" fn socket_manager_cancel_listen_on_addr(
    manager: *mut SocketManager,
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
                write_display_c_str(e, err);
                1
            }
        },
        Err(e) => {
            write_display_c_str(e, err);
            1
        }
    }
}

/// Abort the `SocketManager`'s background runtime.
///
/// # Thread Safety
/// Thread safe.
///
/// # Arguments
/// - `wait`: if true, wait for the background runtime to finish.
///
/// # Errors
/// Returns 1 on error, 0 on success.
/// On Error, `err` will be set to a pointer to a C string allocated by `malloc`.
#[no_mangle]
pub unsafe extern "C" fn socket_manager_abort(
    manager: *mut SocketManager,
    wait: bool,
    err: *mut *mut c_char,
) -> c_int {
    let manager = &mut *manager;
    match manager.abort(wait) {
        Ok(()) => {
            *err = null_mut();
            0
        }
        Err(e) => {
            write_display_c_str(e, err);
            1
        }
    }
}

/// Join and wait on the `SocketManager`.
///
/// # Thread Safety
/// Thread safe. Calling a second time will return immediately.
///
/// This function will block until the `SocketManager`'s background runtime finishes,
/// (i.e., `abort` is called from another thread).
///
/// # Errors
/// Join returns error if the runtime panicked.
#[no_mangle]
pub unsafe extern "C" fn socket_manager_join(
    manager: *mut SocketManager,
    err: *mut *mut c_char,
) -> c_int {
    let manager = &mut *manager;
    match manager.join() {
        Ok(()) => {
            *err = null_mut();
            0
        }
        Err(e) => {
            write_display_c_str(e, err);
            1
        }
    }
}

/// Calling this function will abort all background runtime and join on them,
/// and free the `SocketManager`.
#[no_mangle]
pub unsafe extern "C" fn socket_manager_free(manager: *mut SocketManager) {
    drop(Box::from_raw(manager))
}
