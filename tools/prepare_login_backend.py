"""Prepare the pinned greetd candidate for an ordinary-user offline build.

This reads archives as data. It neither executes Cargo/build scripts nor installs
PAM, services or accounts. All inputs are admitted before a fresh output exists;
failed/partial output is retained and must never be reused automatically.
"""
import argparse
import hashlib
import io
import json
from pathlib import Path, PurePosixPath
import re
import tarfile
import tomllib

ROOT = Path(__file__).resolve().parents[1]
MAX_ARCHIVE = 16 * 1024**2
MAX_MEMBER = 8 * 1024**2
MAX_TREE = 128 * 1024**2
MAX_FILES = 8192


def digest(body):
    return hashlib.sha256(body).hexdigest()


def input_bytes(path, entry):
    # A bounded snapshot is hashed and parsed directly, with no second path read.
    with Path(path).open('rb') as stream:
        body = stream.read(MAX_ARCHIVE + 1)
    if len(body) > MAX_ARCHIVE or len(body) != entry['size'] or digest(body) != entry['sha256']:
        raise ValueError('changed or oversized input: ' + str(path))
    return body


def unpack(body, prefix, license_alias=False):
    files, seen, total = {}, set(), 0
    alias = False
    with tarfile.open(fileobj=io.BytesIO(body), mode='r:gz') as archive:
        for item in archive:
            name = PurePosixPath(item.name)
            canonical = str(name)
            if (name.is_absolute() or '..' in name.parts or not name.parts or
                    name.parts[0] != prefix or
                    (item.name != canonical and not (item.isdir() and item.name == canonical + '/')) or
                    '\\' in item.name or ':' in item.name or canonical in seen):
                raise ValueError('noncanonical/duplicate archive member')
            seen.add(canonical)
            if len(seen) > MAX_FILES:
                raise ValueError('too many archive members')
            local = '/'.join(name.parts[1:])
            if item.isdir():
                continue
            if (license_alias and item.issym() and local == 'greetd_ipc/LICENSE'
                    and item.linkname == '../LICENSE'):
                alias = True
                continue
            if not item.isfile() or not local or item.size > MAX_MEMBER:
                raise ValueError('unsupported/oversized archive member')
            total += item.size
            if total > MAX_TREE:
                raise ValueError('oversized archive tree')
            payload = archive.extractfile(item).read(MAX_MEMBER + 1)
            if len(payload) != item.size:
                raise ValueError('truncated archive member')
            files[local] = payload
    if alias:
        # Preserve exact licence bytes without creating a filesystem alias.
        files['greetd_ipc/LICENSE'] = files['LICENSE']
    for name in files:
        if any(str(parent) in files for parent in PurePosixPath(name).parents):
            raise ValueError('file/directory collision')
    return files


def startup_notice(source):
    """Project the optional integration code into exactly the pinned source."""
    def replace(path, original, replacement):
        if source[path].count(original) != 1:
            raise ValueError('unexpected greetd integration site: ' + path)
        source[path] = source[path].replace(original, replacement)

    cargo = 'greetd/Cargo.toml'
    main = 'greetd/src/main.rs'
    config = 'greetd/src/config/mod.rs'
    replace(cargo, b'debug = []\n', b'debug = []\naetheri-startup = []\n')
    replace(main, b'mod config;\n', b'#[cfg(feature = "aetheri-startup")]\nmod aetheri_startup;\nmod config;\n')
    replace(config, b'env, fs::read_to_string,', b'env,')
    replace(config, b'use enquote::unquote;', b'#[cfg(not(feature = "aetheri-startup"))]\nuse std::fs::read_to_string;\n\nuse enquote::unquote;')
    replace(config, b'    let config_str = match matches.opt_str("config") {', b'''    #[cfg(feature = "aetheri-startup")]
    let config_str = {
        if args.len() != 3 || args[1] != "--config" {
            return Err(Error::ConfigError("expected exactly --config PATH".into()));
        }
        crate::aetheri_startup::read_config(&args[2])?
    };
    #[cfg(not(feature = "aetheri-startup"))]
    let config_str = match matches.opt_str("config") {''')
    replace(config, b'    Ok(Config { file, internal })\n}', b'''    #[cfg(feature = "aetheri-startup")]
    {
        if file.general.source_profile || file.initial_session.is_some() {
            return Err(Error::ConfigError("startup profile forbids profiles and automatic login".into()));
        }
        crate::aetheri_startup::publish(config_str.as_bytes())?;
    }
    Ok(Config { file, internal })
}''')
    path = 'greetd/src/aetheri_startup.rs'
    if path in source:
        raise ValueError('unexpected startup integration file')
    source[path] = (ROOT / 'patches/greetd/aetheri_startup.rs').read_bytes().replace(b'\r\n', b'\n')
    worker_lifetime(source)
    session_close(source)


def worker_lifetime(source):
    """Keep configuring-worker cancellation and the sole reaper serialized."""
    def replace(path, original, replacement):
        if source[path].count(original) != 1:
            raise ValueError('unexpected greetd worker integration site: ' + path)
        source[path] = source[path].replace(original, replacement)

    main = 'greetd/src/main.rs'
    interface = 'greetd/src/session/interface.rs'
    context = 'greetd/src/context.rs'
    replace(main, b'mod context;\n', b'#[cfg(feature = "aetheri-startup")]\nmod aetheri_worker;\nmod context;\n')
    replace(interface, b'        let raw_child = childfd.as_raw_fd();', b'''        // Only the parent endpoint enters Tokio; the worker keeps blocking I/O.
        // Set this before fork so a failed setup has created no child.
        #[cfg(feature = "aetheri-startup")]
        parentfd.set_nonblocking(true)?;
        let raw_child = childfd.as_raw_fd();''')
    replace(interface, b'pub struct Session {\n    task: Pid,', b'''pub struct Session {
    #[cfg(feature = "aetheri-startup")]
    worker: crate::aetheri_worker::Worker,
    task: Pid,''')
    replace(interface, b'        Ok(Session {\n            task: child,', b'''        Ok(Session {
            #[cfg(feature = "aetheri-startup")]
            worker: crate::aetheri_worker::Worker::new(child),
            task: child,''')
    replace(interface, b'''        self.last_msg = None;
        ParentToSessionChild::Cancel.send(&mut self.sock).await?;
        Ok(())''', b'''        #[cfg(feature = "aetheri-startup")]
        {
            let sock = &mut self.sock;
            self.worker.cancel(|| ParentToSessionChild::Cancel.send(sock)).await?;
            self.last_msg = None;
            Ok(())
        }
        #[cfg(not(feature = "aetheri-startup"))]
        {
            self.last_msg = None;
            ParentToSessionChild::Cancel.send(&mut self.sock).await?;
            Ok(())
        }''')
    replace(interface, b'    /// Send a response to an authentication question,', b'''    #[cfg(feature = "aetheri-startup")]
    pub(crate) fn record_worker_exit(&mut self, status: nix::sys::wait::WaitStatus) {
        self.worker.reaped(status);
    }

    /// Send a response to an authentication question,''')

    # The same lock must cover creation/initial admission as well as every wait.
    # Otherwise a reaper can consume a child status before its slot is recorded.
    start = b'    pub async fn create_session(&self, username: String) -> Result<(), Error> {\n'
    end = b'    /// Cancel the session being configured.\n'
    if source[context].count(start) != 1 or source[context].count(end) != 1:
        raise ValueError('unexpected greetd create integration site')
    begin = source[context].index(start)
    finish = source[context].index(end, begin)
    original = source[context][begin:finish]
    body = original[len(start):]
    if not body.endswith(b'    }\n\n'):
        raise ValueError('unexpected greetd create function ending')
    replacement = start + b'''        #[cfg(feature = "aetheri-startup")]
        {
            let mut inner = self.inner.write().await;
            if inner.current.is_none() {
                return Err("session not active".into());
            }
            if inner.configuring.is_some() || inner.scheduled.is_some() {
                return Err("a session is already being configured or scheduled".into());
            }
            let session_set = SessionSet {
                session: Session::new_external()?,
                time: Instant::now(),
            };
            // Retain the original slot before the first fallible async send.
            inner.configuring = Some(session_set);
            let session_set = inner.configuring.as_mut().unwrap();
            session_set.session.initiate(
                &self.pam_service, SessionClass::User, &username, true,
                &self.term_mode, self.source_profile, &self.listener_path,
            ).await?;
            Ok(())
        }
        #[cfg(not(feature = "aetheri-startup"))]
        {
''' + body[:-len(b'    }\n\n')] + b'        }\n    }\n\n'
    replace(context, original, replacement)
    replace(context, b'''        if let Some(mut s) = inner.configuring.take() {
            s.session.cancel().await?;
        }
        Ok(())
    }

    /// Retrieve a question''', b'''        #[cfg(feature = "aetheri-startup")]
        {
            if let Some(s) = inner.configuring.as_mut() {
                s.session.cancel().await?;
            }
            inner.configuring.take();
        }
        #[cfg(not(feature = "aetheri-startup"))]
        if let Some(mut s) = inner.configuring.take() {
            s.session.cancel().await?;
        }
        Ok(())
    }

    /// Retrieve a question''')
    replace(context, b'''        loop {
            match waitpid(None, Some(WaitPidFlag::WNOHANG)) {''', b'''        loop {
            // Exclusive with configuring-worker observation and cancellation.
            #[cfg(feature = "aetheri-startup")]
            let mut inner = self.inner.write().await;
            match waitpid(None, Some(WaitPidFlag::WNOHANG)) {''')
    replace(context, b'''                Ok(WaitStatus::Exited(pid, ..)) | Ok(WaitStatus::Signaled(pid, ..)) => {
                    let mut inner = self.inner.write().await;''', b'''                Ok(_status @ WaitStatus::Exited(pid, ..)) | Ok(_status @ WaitStatus::Signaled(pid, ..)) => {
                    #[cfg(not(feature = "aetheri-startup"))]
                    let mut inner = self.inner.write().await;
                    #[cfg(feature = "aetheri-startup")]
                    {
                        if let Some(s) = inner.configuring.as_mut() {
                            s.session.record_worker_exit(_status);
                        }
                        if let Some(s) = inner.scheduled.as_mut() {
                            s.session.record_worker_exit(_status);
                        }
                    }''')
    path = 'greetd/src/aetheri_worker.rs'
    if path in source:
        raise ValueError('unexpected worker integration file')
    source[path] = (ROOT / 'patches/greetd/aetheri_worker.rs').read_bytes().replace(b'\r\n', b'\n')


def session_close(source):
    """Keep the original PAM worker alive through supervised session shutdown."""
    def replace(path, original, replacement):
        if source[path].count(original) != 1:
            raise ValueError('unexpected greetd session-close integration site: ' + path)
        source[path] = source[path].replace(original, replacement)
    main = 'greetd/src/main.rs'
    worker = 'greetd/src/session/worker.rs'
    interface = 'greetd/src/session/interface.rs'
    context = 'greetd/src/context.rs'
    replace(worker, b'    sys::wait::waitpid,\n', b'')
    replace(worker, b'use nix::{\n', b'#[cfg(not(feature = "aetheri-startup"))]\nuse nix::sys::wait::waitpid;\nuse nix::{\n')
    replace(worker, b'pub fn main(sock: &UnixDatagram) -> Result<(), Error> {', b'''pub fn main(sock: &UnixDatagram) -> Result<(), Error> {
    #[cfg(feature = "aetheri-startup")]
    {
        use std::os::fd::AsRawFd;
        use nix::fcntl::{fcntl, FcntlArg, FdFlag};
        // The first external worker exec required this FD. Later ordinary
        // shell/greeter/session execs must never inherit its privileged channel.
        let fd = sock.as_raw_fd();
        let mut flags = FdFlag::from_bits_retain(fcntl(fd, FcntlArg::F_GETFD)?);
        flags.insert(FdFlag::FD_CLOEXEC);
        fcntl(fd, FcntlArg::F_SETFD(flags))?;
    }''')
    replace(main, b'mod aetheri_worker;\n', b'mod aetheri_worker;\n#[cfg(feature = "aetheri-startup")]\nmod aetheri_close;\n')
    replace(main, b'''    if let Err(e) = res {
        eprintln!("error: {}", e);
    }''', b'''    if let Err(e) = res {
        eprintln!("error: {}", e);
        // A worker error must never look like successful PAM teardown to the
        // original parent. Upstream otherwise returns zero after logging it.
        #[cfg(feature = "aetheri-startup")]
        std::process::exit(1);
    }''')
    replace(interface, b'pub struct SessionChild {\n    pub task: Pid,', b'''pub struct SessionChild {
    #[cfg(feature = "aetheri-startup")]
    pub(crate) shutdown_sent: bool,
    pub task: Pid,''')
    replace(interface, b'        Ok(SessionChild {\n            task: self.task,', b'''        Ok(SessionChild {
            #[cfg(feature = "aetheri-startup")]
            shutdown_sent: false,
            task: self.task,''')
    replace(interface, b'    pub sub_task: Pid,', b'    #[cfg(not(feature = "aetheri-startup"))]\n    pub sub_task: Pid,')
    replace(interface, b'        let sub_task = loop {', b'        let _sub_task = loop {')
    replace(interface, b'            sub_task,\n', b'            #[cfg(not(feature = "aetheri-startup"))]\n            sub_task: _sub_task,\n')
    replace(interface, b'''        let _ = nix::sys::signal::kill(self.sub_task, Signal::SIGTERM);''', b'''        #[cfg(feature = "aetheri-startup")]
        let _ = nix::sys::signal::kill(self.task, Signal::SIGTERM);
        #[cfg(not(feature = "aetheri-startup"))]
        let _ = nix::sys::signal::kill(self.sub_task, Signal::SIGTERM);''')
    replace(interface, b'''        let _ = nix::sys::signal::kill(self.sub_task, Signal::SIGKILL);
        let _ = nix::sys::signal::kill(self.task, Signal::SIGKILL);''', b'''        #[cfg(feature = "aetheri-startup")]
        let _ = nix::sys::signal::kill(self.task, Signal::SIGUSR1);
        #[cfg(not(feature = "aetheri-startup"))]
        {
            let _ = nix::sys::signal::kill(self.sub_task, Signal::SIGKILL);
            let _ = nix::sys::signal::kill(self.task, Signal::SIGKILL);
        }''')
    replace(worker, b'    let pam_username = pam.get_user()?;', b'''    #[cfg(feature = "aetheri-startup")]
    let mut close_signals = crate::aetheri_close::Signals::prepare()?;
    let pam_username = pam.get_user()?;''')
    replace(worker, b'''            prctl(PrctlOption::SET_PDEATHSIG(libc::SIGTERM)).expect("unable to set death signal");''', b'''            prctl(PrctlOption::SET_PDEATHSIG(libc::SIGTERM)).expect("unable to set death signal");
            #[cfg(feature = "aetheri-startup")]
            close_signals.child().expect("original PAM parent/signal setup lost");''')
    replace(worker, b'''    // Wait for process to terminate, handling EINTR as necessary.
    loop {''', b'''    #[cfg(feature = "aetheri-startup")]
    {
        close_signals.activate();
        close_signals.wait(child);
    }
    // The ordinary upstream profile keeps its original wait implementation.
    #[cfg(not(feature = "aetheri-startup"))]
    loop {''')
    # Keep scheduled start and current-slot enrollment under the same reaper lock.
    replace(context, b'''                            drop(inner);
                            let s = match scheduled.session.start().await {''', b'''                            #[cfg(not(feature = "aetheri-startup"))]
                            drop(inner);
                            let s = match scheduled.session.start().await {''')
    replace(context, b'''                            let mut inner = self.inner.write().await;
                            inner.current = Some(SessionChildSet {''', b'''                            #[cfg(not(feature = "aetheri-startup"))]
                            let mut inner = self.inner.write().await;
                            inner.current = Some(SessionChildSet {''')
    replace(context, b'''                    let (was_greeter, sesion_length) = match &inner.current {''', b'''                    #[cfg(feature = "aetheri-startup")]
                    if let Some(current) = inner.current.as_ref() {
                        if current.child.owns_pid(pid) && !crate::aetheri_close::completed(current.child.task, _status) {
                            // Already reaped, so never signal that numeric PID.
                            // Preserve the original slot and reaper lock; a
                            // replacement greeter/session would hide lost close.
                            eprintln!("AETHERI_PAM_SHUTDOWN_PENDING abnormal current worker end");
                            std::future::pending::<()>().await;
                        }
                    }
                    let (was_greeter, sesion_length) = match &inner.current {''')
    replace(context, b'''                Err(nix::errno::Errno::ECHILD) => break Ok(()),''', b'''                Err(nix::errno::Errno::ECHILD) => {
                    #[cfg(feature = "aetheri-startup")]
                    if inner.current.is_some() {
                        eprintln!("AETHERI_PAM_SHUTDOWN_PENDING current worker observation lost");
                        std::future::pending::<()>().await;
                    }
                    break Ok(())
                },''')
    original = b'''        let mut inner = self.inner.write().await;
        if let Some(mut sess) = inner.configuring.take() {
            let _ = sess.session.cancel().await;
        }
        if let Some(mut sess) = inner.scheduled.take() {
            let _ = sess.session.cancel().await;
        }
        if let Some(sess) = inner.current.take() {
            sess.child.term();
        }
        Ok(())'''
    replacement = b'''        let mut inner = self.inner.write().await;
        #[cfg(feature = "aetheri-startup")]
        {
            // Keep every original slot and the exclusive reaper lock until its
            // own evidence completes. No ACK/exit/timeout substitutes for close.
            let result = async {
                if let Some(sess) = inner.configuring.as_mut() { sess.session.cancel().await?; }
                inner.configuring.take();
                if let Some(sess) = inner.scheduled.as_mut() { sess.session.cancel().await?; }
                inner.scheduled.take();
                if let Some(sess) = inner.current.as_mut() {
                    crate::aetheri_close::finish(sess.child.task, &mut sess.child.shutdown_sent).await?;
                }
                inner.current.take();
                Ok::<(), Error>(())
            }.await;
            if let Err(error) = result {
                eprintln!("AETHERI_PAM_SHUTDOWN_PENDING {}", error);
                // Keep the daemon, original slots and lock. No service-manager
                // forced-kill deadline is permitted for this deployment.
                std::future::pending::<()>().await;
            }
            Ok(())
        }
        #[cfg(not(feature = "aetheri-startup"))]
        {
''' + original[len(b'        let mut inner = self.inner.write().await;\n'):] + b'\n        }'
    replace(context, original, replacement)
    path = 'greetd/src/aetheri_close.rs'
    if path in source:
        raise ValueError('unexpected session-close integration file')
    source[path] = (ROOT / 'patches/greetd/aetheri_close.rs').read_bytes().replace(b'\r\n', b'\n')


def plan(source_archive, crate_cache, lockfile, pins, *, notice=False):
    lock_body = input_bytes(lockfile, pins['lock'])
    lock = tomllib.loads(lock_body.decode('utf-8'))
    locked = {(e['name'], e['version']): e['checksum'] for e in lock['package'] if 'source' in e}
    if any(e.get('source', 'registry+https://github.com/rust-lang/crates.io-index') !=
           'registry+https://github.com/rust-lang/crates.io-index' for e in lock['package']):
        raise ValueError('unexpected registry or git dependency')
    selected = {(e['name'], e['version']): e['sha256'] for e in pins['crates']}
    if locked != selected or len(selected) != len(pins['crates']):
        raise ValueError('lock and pinned graph differ')
    source = unpack(input_bytes(source_archive, pins['source']), 'greetd-0.10.3', True)
    original = b'members = ["inish", "greetd_ipc", "greetd", "agreety", "fakegreet"]'
    if source['Cargo.toml'].count(original) != 1:
        raise ValueError('unexpected workspace')
    source['Cargo.toml'] = source['Cargo.toml'].replace(original, b'members = ["inish", "greetd_ipc", "greetd"]')
    original = b'tokio = { version = "1",'
    if source['greetd/Cargo.toml'].count(original) != 1:
        raise ValueError('unexpected Tokio dependency')
    source['greetd/Cargo.toml'] = source['greetd/Cargo.toml'].replace(original, b'tokio = { version = "=1.53.3",')
    source['Cargo.lock'] = lock_body
    if notice:
        startup_notice(source)
    if any(name.startswith('.cargo/') for name in source):
        raise ValueError('unexpected source Cargo configuration')
    source['.cargo/config.toml'] = (
        '[source.crates-io]\nreplace-with = "aetheri-vendor"\n'
        '[source.aetheri-vendor]\ndirectory = "../vendor"\n'
        '[net]\noffline = true\n').encode()
    output = {'source/' + name: body for name, body in source.items()}
    for entry in pins['crates']:
        name, version = entry['name'], entry['version']
        if not re.fullmatch(r'[a-z0-9_-]+', name) or not re.fullmatch(r'[0-9][a-zA-Z0-9.+-]*', version):
            raise ValueError('invalid crate identity')
        label = name + '-' + version
        files = unpack(input_bytes(Path(crate_cache) / (label + '.crate'), entry), label)
        manifest = tomllib.loads(files['Cargo.toml'].decode('utf-8'))['package']
        if manifest['name'] != name or manifest['version'] != version or '.cargo-checksum.json' in files:
            raise ValueError('unexpected crate manifest/checksum')
        checksums = {key: digest(body) for key, body in files.items()}
        files['.cargo-checksum.json'] = json.dumps({'package': entry['sha256'], 'files': checksums}, sort_keys=True).encode()
        if sum(map(len, output.values())) + sum(map(len, files.values())) > MAX_TREE:
            raise ValueError('oversized complete input')
        output.update({'vendor/' + label + '/' + key: body for key, body in files.items()})
    if len(output) > MAX_FILES or sum(map(len, output.values())) > MAX_TREE:
        raise ValueError('oversized complete input')
    return output


def prepare(source_archive, crate_cache, lockfile, output, pins, *, notice=False):
    output = Path(output)
    files = plan(source_archive, crate_cache, lockfile, pins, notice=notice)
    output.mkdir(mode=0o700)  # Existing output, including a failed attempt, refuses.
    for name, body in files.items():
        path = output / name
        path.parent.mkdir(parents=True, exist_ok=True)
        with path.open('xb') as stream:
            stream.write(body)
    receipt = {'format': 1, 'runtime_admitted': False, 'package_code_executed': False,
               'files': {name: {'size': len(body), 'sha256': digest(body)} for name, body in files.items()}}
    with (output / 'prepared.json').open('x', encoding='utf-8') as stream:
        json.dump(receipt, stream, indent=2)
    return receipt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-archive', required=True, type=Path)
    parser.add_argument('--crate-cache', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--startup-notice', action='store_true')
    args = parser.parse_args()
    pins = json.loads((ROOT / 'spec/login-backend-inputs.json').read_bytes())
    receipt = prepare(args.source_archive, args.crate_cache, ROOT / 'spec/login-backend.Cargo.lock', args.output, pins,
                      notice=args.startup_notice)
    print(json.dumps({'files': len(receipt['files']), 'output': str(args.output), 'runtime_admitted': False}))


if __name__ == '__main__':
    main()
