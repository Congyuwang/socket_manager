use std::ffi::{c_char, CStr, CString};
use std::fmt::Display;
use std::net::SocketAddr;

/// Parse a C string into a `SocketAddr`.
pub(crate) unsafe fn socket_addr(addr: *const c_char) -> std::io::Result<SocketAddr> {
    let addr = CStr::from_ptr(addr)
        .to_str()
        .map_err(|e| std::io::Error::new(std::io::ErrorKind::InvalidInput, format!("{e}")))?;
    addr.parse::<SocketAddr>()
        .map_err(|e| std::io::Error::new(std::io::ErrorKind::InvalidInput, format!("{e}")))
}

/// Turn an error with `Display` into a C string pointer using `malloc`.
pub(crate) unsafe fn write_error_c_str<E: Display>(e: E, error: *mut *mut c_char) {
    let error_str = CString::new(format!("{}", e)).unwrap();
    *error = libc::malloc(error_str.as_bytes().len() + 1) as *mut c_char;
    libc::strcpy(*error, error_str.as_ptr());
}
