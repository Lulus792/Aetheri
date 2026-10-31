"""P0 byte-contract reference; no filesystem, privilege or RPC implementation.

JSON is a development input notation only. Hashes cover the binary encoding in
spec/runtime.md, never a JSON serialization or a native in-memory structure.
"""
import hashlib
import json
import re
import struct


class InvalidContract(ValueError):
    pass


MAX_RECORD = 131072
MAX_INT = 2**63 - 1

def process_identity(value):
    fields(value,'boot pidfd_id')
    return b'AETHPID1'+rawhex(value['boot'],16)+struct.pack('>Q',integer(value['pidfd_id'],2**64-1,1))

def read_process_identity(data):
    require(type(data) is bytes and len(data)==32 and data[:8]==b'AETHPID1','process identity frame')
    value=dict(boot=data[8:24].hex(),pidfd_id=struct.unpack('>Q',data[24:])[0])
    require(process_identity(value)==data,'process identity canonical form')
    return value

def execution_process(value):
    fields(value,'role profile_digest boot pidfd_id invocation evidence_digest pid uid cgroup_id unit label cgroup')
    role=integer(value['role'],2,1)
    out=(b'AETHPRC1'+rawhex(value['profile_digest'],32)+struct.pack('>B3x',role)+
         process_identity(dict(boot=value['boot'],pidfd_id=value['pidfd_id']))+
         rawhex(value['invocation'],16)+rawhex(value['evidence_digest'],32)+
         struct.pack('>IIQ',integer(value['pid'],2**31-1,1),integer(value['uid'],2**32-1),integer(value['cgroup_id'],2**64-1,1)))
    for key,limit in (('unit',255),('label',511),('cgroup',1023)):
        text=value[key]; require(type(text) is str and 0<len(text)<=limit and all(0x21<=ord(c)<=0x7e for c in text),'process text')
        out+=struct.pack('>H',len(text))+text.encode('ascii')
    group=value['cgroup']
    require(group.startswith('/') and len(group)>1 and not group.endswith('/') and all(c not in ('','.','..') for c in group[1:].split('/')),'process cgroup')
    return out

def read_execution_process(data):
    require(type(data) is bytes and 149<=len(data)<=2048 and data[:8]==b'AETHPRC1' and data[41:44]==b'\0'*3,'process witness frame')
    identity=read_process_identity(data[44:76])
    value=dict(role=data[40],profile_digest=data[8:40].hex(),**identity,invocation=data[76:92].hex(),evidence_digest=data[92:124].hex())
    value['pid'],value['uid'],value['cgroup_id']=struct.unpack('>IIQ',data[124:140]); offset=140
    for key in ('unit','label','cgroup'):
        require(len(data)-offset>=2,'process text prefix'); size=struct.unpack('>H',data[offset:offset+2])[0]; offset+=2
        require(size<=len(data)-offset,'process text length')
        try: value[key]=data[offset:offset+size].decode('ascii')
        except UnicodeError as error: raise InvalidContract('process text encoding') from error
        offset+=size
    require(offset==len(data) and execution_process(value)==data,'process witness canonical form')
    return value

def _handoff_root(value):
    fields(value,'mount_id inode device_major device_minor uid gid mode filesystem_type filesystem_id birth_present birth_seconds birth_nanoseconds')
    mode=integer(value['mode'],65535); require(mode&0o170000==0o040000,'handoff directory')
    require(type(value['filesystem_type']) is int and value['filesystem_type']==0x63677270 and type(value['birth_present']) is bool,'handoff filesystem/birth')
    seconds=integer(value['birth_seconds'],MAX_INT,-2**63); nanos=integer(value['birth_nanoseconds'],999999999)
    require(value['birth_present'] or (seconds==0 and nanos==0),'absent handoff birth')
    return struct.pack('>QQIIIIIQ8sB3xqI4x',integer(value['mount_id'],2**64-1,1),integer(value['inode'],2**64-1,1),
        *(integer(value[k],2**32-1) for k in ('device_major','device_minor','uid','gid','mode')),
        value['filesystem_type'],rawhex(value['filesystem_id'],8),value['birth_present'],seconds,nanos)

def execution_handoff(value):
    fields(value,'process custodian custodian_invocation root group pid_namespace cgroup_namespace events_inode events_mount events_mode events_uid events_gid')
    process=execution_process(value['process']); require(value['process']['role']==2,'handoff app role')
    unit=value['custodian']; require(type(unit) is str and 8<len(unit)<=255 and unit.endswith('.service') and all(0x21<=ord(c)<=0x7e and c!='/' for c in unit),'handoff custodian')
    root,group=value['root'],value['group']; a,b=_handoff_root(root),_handoff_root(group)
    require(all(root[k]==group[k] for k in ('mount_id','device_major','device_minor','filesystem_id')) and root['inode']!=group['inode'] and group['inode']==value['process']['cgroup_id'],'handoff anchor/group')
    require(value['events_mount']==root['mount_id'] and type(value['events_mode']) is int and 0<=value['events_mode']<=65535 and value['events_mode']&0o170000==0o100000,'handoff events')
    return (b'AETHHND1'+rawhex(value['custodian_invocation'],16)+struct.pack('>HHQQ',len(unit),len(process),
            integer(value['pid_namespace'],2**64-1,1),integer(value['cgroup_namespace'],2**64-1,1))+a+b+
            struct.pack('>QQIII',integer(value['events_inode'],2**64-1,1),integer(value['events_mount'],2**64-1,1),
                integer(value['events_mode'],65535),integer(value['events_uid'],2**32-1),integer(value['events_gid'],2**32-1))+process+unit.encode('ascii'))

def read_execution_handoff(data):
    require(type(data) is bytes and 374<=len(data)<=2560 and data[:8]==b'AETHHND1','handoff frame')
    unit,length,pidns,cgroupns=struct.unpack('>HHQQ',data[24:44])
    require(8<unit<=255 and 149<=length<=2048 and 216+length+unit==len(data),'handoff lengths')
    roots=[]
    for offset in (44,116):
        names='mount_id inode device_major device_minor uid gid mode filesystem_type filesystem_id birth_present birth_seconds birth_nanoseconds'.split()
        v=dict(zip(names,struct.unpack('>QQIIIIIQ8sB3xqI4x',data[offset:offset+72])))
        require(v['birth_present'] in (0,1),'handoff birth flag'); v['birth_present']=bool(v['birth_present']); v['filesystem_id']=v['filesystem_id'].hex(); roots.append(v)
    inode,mount,mode,uid,gid=struct.unpack('>QQIII',data[188:216])
    try: custodian=data[216+length:].decode('ascii')
    except UnicodeError as error: raise InvalidContract('handoff custodian encoding') from error
    value=dict(process=read_execution_process(data[216:216+length]),custodian=custodian,custodian_invocation=data[8:24].hex(),root=roots[0],group=roots[1],
        pid_namespace=pidns,cgroup_namespace=cgroupns,events_inode=inode,events_mount=mount,events_mode=mode,events_uid=uid,events_gid=gid)
    require(execution_handoff(value)==data,'handoff canonical form'); return value

def run_receipt(value):
    fields(value,'kind profile_digest invocation previous_digest evidence_digest revision unix_ms')
    tags={'root_closing':b'AETHCLS1','user_quiet':b'AETHQSC1','root_released':b'AETHEND1'}
    require(type(value['kind']) is str and value['kind'] in tags,'run receipt kind')
    return (tags[value['kind']]+rawhex(value['profile_digest'],32)+rawhex(value['invocation'],16)+
            rawhex(value['previous_digest'],32)+rawhex(value['evidence_digest'],32)+
            struct.pack('>QQ',integer(value['revision'],MAX_INT,1),integer(value['unix_ms'])))

def run_end(value):
    fields(value,'profile_digest context_digest boot invocation cgroup_id pid group_removed unit_failed')
    require(type(value['group_removed']) is bool and type(value['unit_failed']) is bool,'end booleans')
    return (b'AETHREN1'+rawhex(value['profile_digest'],32)+rawhex(value['context_digest'],32)+
            rawhex(value['boot'],16)+rawhex(value['invocation'],16)+
            struct.pack('>QI??xx',integer(value['cgroup_id'],2**64-1,1),integer(value['pid'],2**31-1,1),value['group_removed'],value['unit_failed']))

def read_run_receipt(data):
    require(type(data) is bytes and len(data)==136,'run receipt length')
    kinds={b'AETHCLS1':'root_closing',b'AETHQSC1':'user_quiet',b'AETHEND1':'root_released'}
    require(data[:8] in kinds,'run receipt tag')
    value=dict(kind=kinds[data[:8]],profile_digest=data[8:40].hex(),invocation=data[40:56].hex(),
               previous_digest=data[56:88].hex(),evidence_digest=data[88:120].hex())
    value['revision'],value['unix_ms']=struct.unpack('>QQ',data[120:])
    require(run_receipt(value)==data,'run receipt canonical form')
    return value

def execution_completion(value):
    fields(value,'system_epoch system_runtime boot operation launch lease invocation profile_digest observation_digest revision completed_unix_ms')
    result=b'AETHLCR1'
    for key in ('system_epoch','system_runtime','boot','operation','launch','lease'):
        data=rawhex(value[key],16); require(data[6]>>4==4 and data[8]&0xc0==0x80,'completion UUID'); result+=data
    invocation=rawhex(value['invocation'],16); require(any(invocation),'completion invocation')
    return result+invocation+rawhex(value['profile_digest'],32)+rawhex(value['observation_digest'],32)+struct.pack('>QQ',integer(value['revision'],MAX_INT,1),integer(value['completed_unix_ms']))

def image_record(value):
    fields(value,"inode size device_major device_minor uid gid mode birth_seconds birth_ns mtime_seconds mtime_ns ctime_seconds ctime_ns digest")
    require(value['uid']==0 and value['gid']==0 and value['mode']==0o100555,"image ownership/mode")
    b=b'AETHIMG1'+struct.pack('>QQIIIII',integer(value['inode'],2**64-1,1),integer(value['size'],16*1024*1024,64),
        *(integer(value[k],2**32-1) for k in ('device_major','device_minor','uid','gid','mode')))
    b+=struct.pack('>qIqIqI',integer(value['birth_seconds']),integer(value['birth_ns'],999999999),
        integer(value['mtime_seconds'],MAX_INT,-2**63),integer(value['mtime_ns'],999999999),
        integer(value['ctime_seconds'],MAX_INT,-2**63),integer(value['ctime_ns'],999999999))
    return b+rawhex(value['digest'],32)


def require(condition, message):
    if not condition:
        raise InvalidContract(message)


def fields(value, names):
    require(type(value) is dict and set(value) == set(names.split()), "record fields")


def integer(value, maximum=MAX_INT, minimum=0):
    require(type(value) is int and minimum <= value <= maximum, "integer range")
    return value


def rawhex(value, size):
    require(type(value) is str and re.fullmatch(r"[0-9a-f]+", value) is not None
            and len(value) == 2 * size, "lowercase fixed hex")
    result = bytes.fromhex(value)
    require(size != 16 or any(result), "nil identifier")
    return result


def ident(value):
    return rawhex(value, 16)


def blob(value, minimum, maximum):
    require(type(value) is str and len(value) % 2 == 0
            and re.fullmatch(r"[0-9a-f]*", value) is not None, "hex blob")
    result = bytes.fromhex(value)
    require(minimum <= len(result) <= maximum, "blob length")
    return struct.pack(">I", len(result)) + result


def text(value, maximum, *, pattern=None, empty=False):
    require(type(value) is str, "text type")
    try:
        data = value.encode("utf-8", errors="strict")
    except UnicodeError as error:
        raise InvalidContract("UTF-8") from error
    require((empty or data) and len(data) <= maximum
            and all(ord(c) >= 32 and not 127 <= ord(c) <= 159 for c in value), "text bounds")
    require(pattern is None or re.fullmatch(pattern, value) is not None, "text alphabet")
    return struct.pack(">I", len(data)) + data


def token(value, maximum=64):
    return text(value, maximum, pattern=r"[a-z0-9._-]+")


def member(value, choices):
    require(type(value) is str and value in choices, "enum value")
    return value


def relative(value):
    encoded = text(value, 256, pattern=r"[a-zA-Z0-9._/-]+")
    require(all(part not in ("", ".", "..") and len(part) <= 64
                for part in value.split("/")), "relative path")
    return encoded


def sequence(value, maximum, minimum=0):
    require(type(value) is list and minimum <= len(value) <= maximum, "list bounds")
    return struct.pack(">I", len(value))


def optional(value, size):
    return b"\0" if value is None else b"\1" + rawhex(value, size)


def finish(value):
    require(len(value) <= MAX_RECORD, "record limit")
    return value


def execution_views(value):
    """Protected preallocated scaffold evidence; no allocation/admission effect."""
    fields(value,"profile_sha256 objects")
    output=b"AETHVWS1"+rawhex(value["profile_sha256"],32)
    sequence(value["objects"],4,4)
    for obj in value["objects"]:
        fields(obj,"filesystem_type filesystem_id inode birth_seconds birth_nanoseconds")
        output+=struct.pack(">Q",integer(obj["filesystem_type"]))+rawhex(obj["filesystem_id"],8)
        output+=struct.pack(">QQI",integer(obj["inode"],minimum=1),integer(obj["birth_seconds"]),
                            integer(obj["birth_nanoseconds"],maximum=999999999))
    return finish(output)


def fixture(value):
    fields(value, "component publisher version target files actions dependencies")
    require(value["target"] == "x86_64-linux-gnu", "P0 target")
    out = (b"AETHFIX1" + token(value["component"], 128) + ident(value["publisher"])
           + text(value["version"], 128) + text(value["target"], 128))
    files = value["files"]
    out += sequence(files, 32, 1)
    by_path = {}
    total = 0
    for item in files:
        fields(item, "path mode size sha256")
        relative(item["path"])
        require(item["path"] not in by_path, "duplicate file")
        require(item["mode"] in (0o444, 0o555) and type(item["mode"]) is int, "file mode")
        total += integer(item["size"], 16 * 1024 * 1024)
        by_path[item["path"]] = item
    require(total <= 32 * 1024 * 1024, "payload limit")
    paths = sorted(by_path)
    require(not any("/".join(path.split("/")[:n]) in by_path
                    for path in paths for n in range(1, len(path.split("/")))), "file ancestor")
    for path in paths:
        item = by_path[path]
        out += (relative(path) + struct.pack(">IQ", item["mode"], item["size"])
                + rawhex(item["sha256"], 32))
    actions = value["actions"]
    out += sequence(actions, 8)
    seen = set()
    encoded_actions = []
    for action in actions:
        fields(action, "id executable arguments")
        key = action["id"]
        token(key)
        require(key not in seen, "duplicate action")
        seen.add(key)
        executable = action["executable"]
        require(type(executable) is str and executable in by_path
                and by_path[executable]["mode"] == 0o555, "declared executable")
        data = token(key) + relative(executable) + sequence(action["arguments"], 8)
        for argument in action["arguments"]:
            data += text(argument, 256, empty=True)
        encoded_actions.append((key, data))
    out += b"".join(data for _, data in sorted(encoded_actions))
    dependencies = value["dependencies"]
    out += sequence(dependencies, 8)
    depbytes = [rawhex(dep, 32) for dep in dependencies]
    require(len(set(depbytes)) == len(depbytes), "duplicate dependency")
    return finish(out + b"".join(sorted(depbytes)))


OWNER_FIELDS = "machine volume account storage data allocation_operation"


def owner(value):
    fields(value, OWNER_FIELDS)
    return b"AETHOWN1" + b"".join(ident(value[key]) for key in OWNER_FIELDS.split())


def check_owner(data, expected):
    require(type(data) is bytes and data == owner(expected), "ownership mismatch")
    return True


READ_KINDS = {"scope", "component", "install", "generation", "data", "policy",
              "resource", "grant", "gate"}
EFFECT_KINDS = {"install", "generation", "data", "grant", "integration", "storage"}
DISPOSITIONS = {"create", "activate", "stop", "remove", "retain", "revoke"}


def intent(value):
    fields(value, "plan authority actor scope kind component install generation data artifact grant "
           "retention action boot expires reads effects")
    kind = value["kind"]
    member(kind, ("install", "activate", "remove", "purge", "launch", "revoke"))
    member(value["retention"], ("none", "keep_data", "purge_data"))
    require((kind == "launch") == (value["action"] is not None), "action shape")
    require((kind == "revoke") == (value["grant"] is not None), "grant shape")
    i, g, d, a = (value[key] is not None for key in ("install", "generation", "data", "artifact"))
    require({"install": not i and a, "activate": i and a,
             "remove": i and value["retention"] != "none",
             "purge": d and value["retention"] == "purge_data",
             "launch": i and g and d and value["retention"] == "none",
             "revoke": i and d and not g and not a and value["retention"] == "none"}[kind],
            "intent target shape")
    out = b"AETHINT1" + b"".join(ident(value[key]) for key in ("plan", "authority", "actor", "scope"))
    out += token(kind) + token(value["component"], 128)
    out += b"".join(optional(value[key], size) for key, size in
                    (("install", 16), ("generation", 16), ("data", 16), ("artifact", 32), ("grant", 16)))
    out += token(value["retention"])
    out += b"\0" if value["action"] is None else b"\1" + token(value["action"])
    out += ident(value["boot"]) + struct.pack(">Q", integer(value["expires"]))
    reads = value["reads"]
    out += sequence(reads, 128)
    encoded_reads = []
    seen = set()
    for row in reads:
        fields(row, "epoch kind ref revision")
        member(row["kind"], READ_KINDS)
        epoch = ident(row["epoch"])
        reference = blob(row["ref"], 1, 128)
        key = (epoch, row["kind"], bytes.fromhex(row["ref"]))
        require(key not in seen, "duplicate read")
        seen.add(key)
        encoded_reads.append((key, epoch + token(row["kind"]) + reference
                              + struct.pack(">Q", integer(row["revision"], minimum=1))))
    out += b"".join(data for _, data in sorted(encoded_reads))
    effects = value["effects"]
    out += sequence(effects, 128)
    seen = set()
    encoded_effects = []
    for row in effects:
        fields(row, "number kind component ref disposition reason")
        number = integer(row["number"], 127)
        require(number not in seen, "duplicate effect number")
        seen.add(number)
        member(row["kind"], EFFECT_KINDS)
        member(row["disposition"], DISPOSITIONS)
        require(type(row["ref"]) is str and len(row["ref"]) in (32, 64), "effect reference")
        data = (struct.pack(">I", number) + token(row["kind"]) + token(row["component"], 128)
                + blob(row["ref"], 16, 32) + token(row["disposition"]) + token(row["reason"]))
        encoded_effects.append((number, data))
    require(seen == set(range(len(effects))), "effect numbering")
    return finish(out + b"".join(data for _, data in sorted(encoded_effects)))


ENCODERS = {"fixture": fixture, "owner": owner, "intent": intent}


def digest(data):
    return hashlib.sha256(data).hexdigest()


def verify_payload(manifest, expected_artifact, payload):
    """Check supplied immutable bytes; caller still owes trusted provenance/IO.

    No pathname opening or installation. A valid hash is not publisher trust.
    """
    encoded = fixture(manifest)
    require(digest(encoded) == rawhex(expected_artifact, 32).hex(), "artifact mismatch")
    require(type(payload) is dict and set(payload) == {f["path"] for f in manifest["files"]},
            "payload inventory")
    for row in manifest["files"]:
        data = payload[row["path"]]
        require(type(data) is bytes and len(data) == row["size"]
                and digest(data) == row["sha256"], "payload mismatch")
    return True


def load_json(data):
    require(type(data) is bytes and len(data) <= MAX_RECORD, "JSON input bounds")

    def pairs(items):
        result = {}
        for key, value in items:
            require(key not in result, "duplicate JSON key")
            result[key] = value
        return result

    try:
        result = json.loads(data.decode("utf-8"), object_pairs_hook=pairs,
                            parse_constant=lambda _: require(False, "JSON constant"))
        pending = [(result, 0)]
        while pending:
            value, depth = pending.pop()
            require(depth <= 16, "JSON nesting")
            if type(value) is dict:
                pending.extend((child, depth + 1) for child in value.values())
            elif type(value) is list:
                pending.extend((child, depth + 1) for child in value)
        return result
    except (ValueError, RecursionError) as error:
        raise InvalidContract("JSON syntax") from error
