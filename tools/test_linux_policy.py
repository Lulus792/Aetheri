"""Explicit root-only SELinux fixture test inside the registered disposable carrier.

Not part of ordinary contract tests. Failure preserves its owned fixture/module
for inspection; it never changes enforcing mode or guesses that a timed-out app
has exited. This is a test controller, not an Aetheri installer or authority.
"""
import bz2
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import uuid

ROOT = Path("/var/lib/aetheri-p0-policy-test")
MODULE = "aetheri_p0_fixture"


def run(*arguments, **options):
    return subprocess.run(arguments, check=True, capture_output=True, text=True,
                          timeout=120, **options).stdout.strip()


def label(path, value):
    run("chcon", value, str(path))


def file(path, data, context, uid=0, mode=0o600):
    with path.open("xb") as output:
        output.write(data)
    os.chown(path, uid, uid)
    path.chmod(mode)
    label(path, context)


def cleanup_owned(record):
    if record.get("dispatch_in_progress"):
        raise RuntimeError("unacknowledged dispatch needs supervisor reconciliation; no cleanup")
    if ROOT.is_symlink() or ROOT.resolve() != ROOT or ROOT.stat().st_uid != 0:
        raise RuntimeError("fixture root identity changed")
    token = record["token"]
    expected_units = [f"aetheri-p0-policy-{token}-{name}.service" for name in ("a", "b")]
    if re.fullmatch("[0-9a-f]{32}", token) is None or record["units"] != expected_units:
        raise RuntimeError("invalid owned unit identities")
    pending = run("systemctl", "list-jobs", "--no-legend", "--no-pager")
    for unit in expected_units:
        properties = dict(line.split("=", 1) for line in run("systemctl", "show", unit,
            "--property=ActiveState,MainPID,ControlGroup").splitlines())
        if (properties.get("ActiveState") not in ("inactive", "failed")
                or properties.get("MainPID") != "0" or properties.get("ControlGroup")
                or unit in pending):
            raise RuntimeError("unit dispatch/process state is unresolved; preserve fixture")
    module_file = ROOT / (MODULE + ".cil")
    cil = module_file.read_bytes()
    if hashlib.sha256(cil).hexdigest() != record["policy_sha256"]:
        raise RuntimeError("owned module input changed")
    stored = Path("/var/lib/selinux/targeted/active/modules/400") / MODULE / "cil"
    installed = stored.exists()
    if installed:
        actual = stored.read_bytes()
        if actual.startswith(b"BZh"):
            actual = bz2.decompress(actual)
        if actual != cil:
            raise RuntimeError("loaded module differs from owned input; preserve it")
    for leaf in ROOT.iterdir():
        if leaf.name not in {"a", "b", "probe", "broker.db", "unselected-document", "controller.json", MODULE + ".cil"}:
            raise RuntimeError("unexplained fixture entry; preserve it")
    probe = ROOT / "probe"
    if probe.exists() and hashlib.sha256(probe.read_bytes()).hexdigest() != record["probe_sha256"]:
        raise RuntimeError("owned probe changed")
    # Preserve the attempt before removing only its enumerated files/directories.
    Path(f"/home/aetheri/build/policy-attempt-{token}.json").write_text(
        json.dumps(record, indent=2), encoding="utf-8")
    for name in ("a", "b"):
        directory = ROOT / name
        if directory.exists():
            if directory.is_symlink() or any(p.name not in {"own", "result", "created"} for p in directory.iterdir()):
                raise RuntimeError("unexplained private fixture entries")
            for leaf in ("own", "result", "created"):
                path = directory / leaf
                if path.exists():
                    path.unlink()
            directory.rmdir()
    for leaf in ("probe", "broker.db", "unselected-document"):
        path = ROOT / leaf
        if path.exists():
            path.unlink()
    if installed:
        run("semodule", "-X", "400", "-r", MODULE)
    module_file.unlink()
    (ROOT / "controller.json").unlink()
    ROOT.rmdir()


def main():
    if sys.platform != "linux" or os.geteuid() != 0:
        raise RuntimeError("requires root in the disposable Linux carrier")
    import pwd
    if (Path("/sys/class/dmi/id/product_name").read_text().strip() != "Aetheri P0 disposable VM"
            or not Path("/etc/aetheri-p0-carrier").is_file()
            or not Path("/sys/firmware/efi").is_dir()
            or run("getenforce") != "Enforcing"):
        raise RuntimeError("carrier/enforcing preflight failed")
    account = pwd.getpwnam("aetheri")
    if account.pw_uid != 1000 or account.pw_gid != 1000:
        raise RuntimeError("unexpected fixture account")
    if sys.argv[1:] == ["--recover-owned-fixture"]:
        record_path = ROOT / "controller.json"
        if record_path.is_symlink() or record_path.stat().st_uid != 0:
            raise RuntimeError("missing trusted fixture record")
        cleanup_owned(json.loads(record_path.read_text()))
        print("Reconciled and removed only the verified owned fixture/module.")
        return
    if sys.argv[1:]:
        raise RuntimeError("unsupported arguments")
    if ROOT.exists() or ROOT.is_symlink() or MODULE in run("semodule", "-l").split():
        raise RuntimeError("existing fixture/module needs explicit reconciliation")
    source = Path(__file__).resolve().parents[1]
    probe = Path("/home/aetheri/build/release/probe_policy")
    if probe.is_symlink() or not probe.is_file():
        raise RuntimeError("build the release policy probe first")
    cil = (source / "policy/p0-fixture.cil").read_bytes()
    binary = probe.read_bytes()
    token = uuid.uuid4().hex
    units = [f"aetheri-p0-policy-{token}-{name}.service" for name in ("a", "b")]
    ROOT.mkdir(mode=0o755)
    record = {"token": token, "units": units, "state": "preparing", "dispatch_in_progress": None,
              "policy_sha256": hashlib.sha256(cil).hexdigest(),
              "probe_sha256": hashlib.sha256(binary).hexdigest()}
    record_path = ROOT / "controller.json"

    def save():
        record_path.write_text(json.dumps(record, indent=2), encoding="utf-8")
        record_path.chmod(0o600)

    save()
    module_file = ROOT / (MODULE + ".cil")
    module_file.write_bytes(cil)
    module_file.chmod(0o600)
    try:
        run("semodule", "-X", "400", "-i", str(module_file))
        record["state"] = "policy-loaded"
        save()
        label(ROOT, "system_u:object_r:aetheri_p0_fixture_t:s0")
        file(ROOT / "probe", binary, "system_u:object_r:aetheri_p0_exec_t:s0", mode=0o555)
        file(ROOT / "broker.db", b"synthetic broker state", "system_u:object_r:aetheri_p0_private_t:s0", uid=1000)
        file(ROOT / "unselected-document", b"unselected fixture", "system_u:object_r:aetheri_p0_document_t:s0", uid=1000)
        for name, cats in (("a", "c21,c22"), ("b", "c23,c24")):
            directory = ROOT / name
            directory.mkdir(mode=0o700)
            os.chown(directory, 1000, 1000)
            context = f"system_u:object_r:aetheri_p0_data_t:s0:{cats}"
            label(directory, context)
            file(directory / "own", f"own{name}".encode(), context, uid=1000)
            file(directory / "result", b"", context, uid=1000)
        # All four negative targets are readable to this same UID outside the
        # app domain: DAC or an absent file cannot explain the probe's denials.
        for target in ("a/own", "b/own", "broker.db", "unselected-document"):
            run("sudo", "-n", "-u", "aetheri", "--", "cat", str(ROOT / target))
        if run("sudo", "-n", "-u", "aetheri", "--", "sudo", "-n", "id", "-u") != "0":
            raise RuntimeError("outside-app sudo control did not reach the expected fixture root")
        record["state"] = "dispatching-fixtures"
        save()
        reports = {}
        for unit, name, cats in zip(units, ("a", "b"), ("c21,c22", "c23,c24")):
            properties = {
                "User": "aetheri", "Group": "aetheri", "NoNewPrivileges": "yes",
                "CapabilityBoundingSet": "", "UMask": "0077", "PrivateMounts": "yes",
                "ProtectSystem": "strict", "ReadWritePaths": str(ROOT / name),
                "ProtectHome": "yes", "ProtectControlGroups": "yes",
                "RestrictNamespaces": "yes", "RestrictSUIDSGID": "yes",
                "SELinuxContext": f"system_u:system_r:aetheri_p0_app_t:s0:{cats}",
                "StandardOutput": "null", "StandardError": "null",
                "KillMode": "control-group", "SendSIGKILL": "no", "TimeoutStopSec": "5s",
            }
            command = ["systemd-run", "--unit=" + unit, "--wait", "--service-type=exec"]
            command.extend("--property=" + key + "=" + value for key, value in properties.items())
            command.extend([str(ROOT / "probe"), name])
            record["dispatch_in_progress"] = unit
            save()
            try:
                run(*command)
            except subprocess.CalledProcessError as error:
                observed = dict(line.split("=", 1) for line in run("systemctl", "show", unit,
                    "--property=ActiveState,ExecMainStatus").splitlines())
                if observed == {"ActiveState": "failed", "ExecMainStatus": str(error.returncode)}:
                    record["dispatch_in_progress"] = None
                raise
            record["dispatch_in_progress"] = None
            save()
            result = dict(line.split("=", 1) for line in (ROOT / name / "result").read_text().splitlines())
            expected = {"uid": "1000", "label": f"system_u:system_r:aetheri_p0_app_t:s0:{cats}",
                        "own": "read-write-create", "sibling": "denied", "broker": "denied",
                        "document": "denied", "sudo": "denied", "nnp": "1", "capabilities": "empty"}
            if result != expected or (ROOT / name / "own").read_bytes() != f"own{name}+".encode():
                raise RuntimeError("probe evidence disagrees with expected fixture")
            if (ROOT / name / "created").read_bytes() != b"created":
                raise RuntimeError("private-file creation did not persist")
            actual_label = run("ls", "-Zd", str(ROOT / name / "created")).split()[0]
            if actual_label != f"system_u:object_r:aetheri_p0_data_t:s0:{cats}":
                raise RuntimeError("new file did not inherit the exact data label")
            reports[name] = result
        # A successful manager wait is required; unknown/timed-out dispatches
        # leave everything for reconciliation instead of triggering cleanup.
        pending = run("systemctl", "list-jobs", "--no-legend", "--no-pager")
        for unit in units:
            state = run("systemctl", "show", unit, "--property=ActiveState", "--value")
            if state != "inactive" or unit in pending:
                raise RuntimeError("fixture unit has not settled")
        if run("getenforce") != "Enforcing":
            raise RuntimeError("enforcement changed during test")
        record.update(state="passed", reports=reports)
        save()
        report = Path("/home/aetheri/build/policy-report.json")
        report.write_text(json.dumps(record, indent=2), encoding="utf-8")
        cleanup_owned(record)
        print(json.dumps({"fixtures": 2, "same_uid": 1000, "enforcing": True,
                          "own_read_write_create": True, "negative_targets_denied": True,
                          "owned_fixture_and_module_removed": True}))
    except BaseException as error:
        record.update(state="needs-inspection", error=type(error).__name__ + ": " + str(error))
        save()
        if isinstance(error, subprocess.CalledProcessError):
            print(error.stderr, file=sys.stderr)
        raise


if __name__ == "__main__":
    main()
