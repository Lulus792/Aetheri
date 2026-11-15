// SPDX-License-Identifier: GPL-3.0-only
// Aetheri's bounded startup notice for the pinned greetd integration build.
use crate::error::Error;
use libc::{c_char, c_int, c_ulong, c_void};
use std::env;
use std::fmt::Write as _;
use std::fs::OpenOptions;
use std::io::Read as _;
use std::os::linux::net::SocketAddrExt;
use std::os::unix::ffi::OsStrExt;
use std::os::unix::fs::{MetadataExt, OpenOptionsExt};
use std::os::unix::net::{SocketAddr, UnixDatagram};
use std::path::Path;

const MAX_CONFIG: usize = 64 * 1024;

pub fn read_config(path: &str) -> Result<String, Error> {
    if !Path::new(path).is_absolute() {
        return Err(Error::ConfigError("expected an absolute configuration path".into()));
    }
    let file = OpenOptions::new()
        .read(true)
        .custom_flags(libc::O_NOFOLLOW | libc::O_CLOEXEC | libc::O_NONBLOCK)
        .open(path)?;
    let before = file.metadata()?;
    if !before.is_file()
        || before.uid() != 0
        || before.gid() != 0
        || !matches!(before.mode() & 0o7777, 0o444 | 0o644)
        || before.nlink() != 1
        || before.len() == 0
        || before.len() > MAX_CONFIG as u64
    {
        return Err(Error::ConfigError("configuration file metadata rejected".into()));
    }
    let mut text = String::new();
    (&file).take((MAX_CONFIG + 1) as u64).read_to_string(&mut text)?;
    let after = file.metadata()?;
    if text.len() as u64 != before.len()
        || before.len() != after.len()
        || (before.ctime(), before.ctime_nsec()) != (after.ctime(), after.ctime_nsec())
        || (before.mtime(), before.mtime_nsec()) != (after.mtime(), after.mtime_nsec())
        || (before.mode(), before.uid(), before.gid(), before.nlink())
            != (after.mode(), after.uid(), after.gid(), after.nlink())
        || text.as_bytes().contains(&0)
    {
        return Err(Error::ConfigError("configuration changed or contains NUL".into()));
    }
    Ok(text)
}

#[link(name = "crypto")]
unsafe extern "C" {
    fn OpenSSL_version_num() -> c_ulong;
    fn OSSL_LIB_CTX_new() -> *mut c_void;
    fn OSSL_LIB_CTX_free(context: *mut c_void);
    fn EVP_Q_digest(
        context: *mut c_void,
        algorithm: *const c_char,
        properties: *const c_char,
        data: *const c_void,
        length: usize,
        output: *mut u8,
        output_length: *mut usize,
    ) -> c_int;
}

fn digest(bytes: &[u8]) -> Result<[u8; 32], Error> {
    let mut output = [0; 64];
    let mut size = 0;
    // The slice remains live through the call. Output has EVP_MAX_MD_SIZE
    // bytes. The private context is owned here and released on every path.
    let success = unsafe {
        let version = OpenSSL_version_num();
        if !(0x30500090..0x30600000).contains(&version) {
            return Err(Error::ConfigError("unsupported startup SHA-256 library".into()));
        }
        let context = OSSL_LIB_CTX_new();
        if context.is_null() {
            return Err(Error::ConfigError("cannot create startup digest context".into()));
        }
        let result = EVP_Q_digest(
            context,
            b"SHA256\0".as_ptr().cast(),
            b"provider=default\0".as_ptr().cast(),
            bytes.as_ptr().cast(),
            bytes.len(),
            output.as_mut_ptr(),
            &mut size,
        );
        OSSL_LIB_CTX_free(context);
        result
    };
    if success != 1 || size != 32 {
        return Err(Error::ConfigError("cannot digest the parsed configuration".into()));
    }
    let mut sha256 = [0; 32];
    sha256.copy_from_slice(&output[..32]);
    Ok(sha256)
}

fn notice(invocation: &str, bytes: &[u8]) -> Result<String, Error> {
    if invocation.len() != 32
        || !invocation.bytes().all(|byte| byte.is_ascii_digit() || (b'a'..=b'f').contains(&byte))
        || invocation.bytes().all(|byte| byte == b'0')
    {
        return Err(Error::ConfigError("invalid startup invocation identity".into()));
    }
    let mut result = format!("STATUS=AETHERI_CONFIG_V1 {invocation} ");
    for byte in digest(bytes)? {
        write!(result, "{:02x}", byte).expect("writing to a string");
    }
    Ok(result)
}

pub fn publish(bytes: &[u8]) -> Result<(), Error> {
    let invocation = env::var("INVOCATION_ID")
        .map_err(|_| Error::ConfigError("missing startup invocation identity".into()))?;
    let addr = env::var_os("NOTIFY_SOCKET")
        .ok_or_else(|| Error::ConfigError("missing startup notify socket".into()))?;
    let addr = match addr.as_bytes() {
        [b'/', ..] => SocketAddr::from_pathname(Path::new(&addr))?,
        [b'@', name @ ..] if !name.is_empty() => SocketAddr::from_abstract_name(name)?,
        _ => return Err(Error::ConfigError("invalid startup notify socket".into())),
    };
    let message = notice(&invocation, bytes)?;
    let socket = UnixDatagram::unbound()?;
    socket.set_nonblocking(true)?;
    if socket.send_to_addr(message.as_bytes(), &addr)? != message.len() {
        return Err(Error::Io("incomplete startup notification".into()));
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::notice;

    #[test]
    fn canonical_invocation_and_digest() {
        let id = "0123456789abcdef0123456789abcdef";
        let actual = notice(id, b"abc").unwrap();
        assert_eq!(actual, format!("STATUS=AETHERI_CONFIG_V1 {id} ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
        assert!(notice(&id.to_uppercase(), b"abc").is_err());
        assert!(notice("00000000000000000000000000000000", b"abc").is_err());
        assert!(notice(&format!("{id}\nREADY=1"), b"abc").is_err());
    }
}
