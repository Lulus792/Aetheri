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
