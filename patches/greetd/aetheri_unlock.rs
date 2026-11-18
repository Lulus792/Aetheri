// SPDX-License-Identifier: GPL-3.0-only
// Original PAM owner bridge for the isolated C password verifier.
use std::{
    convert::TryFrom,
    ffi::CStr,
    os::fd::{AsRawFd, FromRawFd, IntoRawFd, OwnedFd},
    ptr::{self, NonNull},
};

use nix::{
    errno::Errno,
    sys::{signal::Signal, signalfd::SignalFd},
    unistd::Pid,
};

use crate::error::Error;

const DESKTOP: &[u8] = b"/usr/libexec/aetheri/aetheri-session";
const HANDOFF: libc::c_int = 3;

#[repr(C)]
struct NativeAuthority {
    _private: [u8; 0],
}

#[link(name = "aetheri_unlock_pam", kind = "static")]
extern "C" {
    fn ae_unlock_authority_pair(pair: *mut libc::c_int) -> libc::c_int;
    fn ae_unlock_authority_open(
        out: *mut *mut NativeAuthority,
        fd: libc::c_int,
        child: libc::c_int,
        uid: libc::uid_t,
        gid: libc::gid_t,
        account: *const libc::c_char,
    ) -> libc::c_int;
    fn ae_unlock_authority_poll(
        authority: *const NativeAuthority,
        fds: *mut libc::pollfd,
        timeout: *mut libc::c_int,
    );
    fn ae_unlock_authority_step(authority: *mut NativeAuthority) -> libc::c_int;
    fn ae_unlock_authority_close(authority: *mut *mut NativeAuthority);
}

pub(crate) fn selected(command: &[std::ffi::CString], authenticated_user: bool) -> bool {
    authenticated_user && matches!(command, [program] if program.as_bytes() == DESKTOP)
}

pub(crate) struct Pair {
    owner: OwnedFd,
    desktop: OwnedFd,
}

impl Pair {
    pub(crate) fn prepare() -> Result<Self, Error> {
        let mut fds = [-1; 2];
        // The native call initializes exactly two fresh, distinct, owned FDs.
        if unsafe { ae_unlock_authority_pair(fds.as_mut_ptr()) } != 0 {
            return Err("unable to create the private unlock pair".into());
        }
        Ok(unsafe { Self { owner: OwnedFd::from_raw_fd(fds[0]), desktop: OwnedFd::from_raw_fd(fds[1]) } })
    }

    pub(crate) fn inherit(self) -> bool {
        let Self { owner, desktop } = self;
        drop(owner);
        let original = desktop.as_raw_fd();
        // This runs in the original final fork, after setuid, just before exec.
        // The desktop entry discards the other inherited descriptors.
        if original == HANDOFF {
            if unsafe { libc::fcntl(original, libc::F_SETFD, 0) } != 0 {
                return false;
            }
            let _ = desktop.into_raw_fd();
            true
        } else {
            unsafe { libc::dup3(original, HANDOFF, 0) == HANDOFF }
        }
    }

    pub(crate) fn bind(self, child: Pid, uid: libc::uid_t, gid: libc::gid_t,
                       account: &CStr) -> Option<Authority> {
        let Self { owner, desktop } = self;
        drop(desktop);
        // child is the original unreaped result of fork, owned by this worker.
        let fd = unsafe { libc::syscall(libc::SYS_pidfd_open, child.as_raw(), 0u32) };
        if fd < 0 {
            return None;
        }
        let pin = unsafe { OwnedFd::from_raw_fd(fd as libc::c_int) };
        let mut raw = ptr::null_mut();
        // Native open copies the account and retains its own pidfd.
        let status = unsafe {
            ae_unlock_authority_open(&mut raw, owner.as_raw_fd(), pin.as_raw_fd(),
                uid, gid, account.as_ptr())
        };
        if status != 0 {
            return None;
        }
        let handle = NonNull::new(raw)?;
        let _ = owner.into_raw_fd();
        Some(Authority(handle))
    }
}

pub(crate) struct Authority(NonNull<NativeAuthority>);

impl Drop for Authority {
    fn drop(&mut self) {
        let mut raw = self.0.as_ptr();
        // Only this guard owns the native authority and its private channel.
        unsafe { ae_unlock_authority_close(&mut raw) };
    }
}

pub(crate) fn next_signal(authority: &mut Option<Authority>, events: &mut SignalFd) -> nix::Result<Signal> {
    loop {
        let mut fds = [libc::pollfd { fd: -1, events: libc::POLLIN, revents: 0 }; 4];
        fds[0].fd = events.as_raw_fd();
        let mut timeout = -1;
        if let Some(root) = authority.as_ref() {
            // The native contract fills exactly three pollfd entries.
            unsafe { ae_unlock_authority_poll(root.0.as_ptr(), fds[1..].as_mut_ptr(), &mut timeout) };
        }
        if unsafe { libc::poll(fds.as_mut_ptr(), fds.len() as libc::nfds_t, timeout) } < 0 {
            return Err(Errno::last());
        }
        if fds[0].revents != 0 {
            let event = events.read_signal()?.ok_or(Errno::EIO)?;
            let signal = Signal::try_from(event.ssi_signo as libc::c_int)?;
            if matches!(signal, Signal::SIGTERM | Signal::SIGHUP | Signal::SIGUSR1) {
                // Invalidate the verifier before the normal child retirement.
                drop(authority.take());
            }
            return Ok(signal);
        }
        if let Some(root) = authority.as_ref() {
            if unsafe { ae_unlock_authority_step(root.0.as_ptr()) } != 0 {
                eprintln!("AETHERI_UNLOCK_AUTHORITY_ENDED");
                drop(authority.take());
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::ffi::CString;

    #[test]
    fn only_the_authenticated_literal_desktop_receives_a_channel() {
        let desktop = CString::new(DESKTOP).unwrap();
        assert!(selected(&[desktop.clone()], true));
        assert!(!selected(&[desktop.clone()], false));
        assert!(!selected(&[desktop, CString::new("--extra").unwrap()], true));
        assert!(!selected(&[CString::new("/usr/libexec/aetheri/aetheri-greeter").unwrap()], true));
        assert!(!selected(&[], true));
    }
}
