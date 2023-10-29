use std::ffi::{c_char, c_void, CStr, CString};
use std::fmt::Display;
use std::net::SocketAddr;

/// Parse a C string into a `SocketAddr`.
pub(crate) unsafe fn socket_addr(addr: *const c_char) -> std::io::Result<SocketAddr> {
    if addr.is_null() {
        Err(std::io::Error::new(
            std::io::ErrorKind::InvalidInput,
            "addr is null",
        ))
    } else {
        let addr = CStr::from_ptr(addr)
            .to_str()
            .map_err(|e| std::io::Error::new(std::io::ErrorKind::InvalidInput, format!("{e}")))?;
        addr.parse::<SocketAddr>()
            .map_err(|e| std::io::Error::new(std::io::ErrorKind::InvalidInput, format!("{e}")))
    }
}

/// Parse a malloced C string into a `String` and free the C string.
#[inline(always)]
pub(crate) unsafe fn parse_c_err_str(c_str: *mut c_char) -> Result<(), String> {
    if c_str.is_null() {
        Ok(())
    } else {
        let result = CStr::from_ptr(c_str).to_string_lossy().into_owned();
        libc::free(c_str as *mut c_void);
        Err(result)
    }
}

/// Turn an error with `Display` into a C string pointer using `malloc`.
pub(crate) unsafe fn write_display_c_str<D: Display>(d: D, error: *mut *mut c_char) {
    let error_str = CString::new(format!("{}", d)).unwrap();
    *error = libc::malloc(error_str.as_bytes().len() + 1) as *mut c_char;
    libc::strcpy(*error, error_str.as_ptr());
}
