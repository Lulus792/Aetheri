"""Run inside the disposable guest; emit facts and minimal carrier gates as JSON."""
import json
from pathlib import Path
import subprocess


def run(*arguments):
    result = subprocess.run(arguments, capture_output=True, text=True, timeout=15)
    return {"exit": result.returncode, "stdout": result.stdout.strip(), "stderr": result.stderr.strip()}


report = {
    "kernel": run("uname", "-r"),
    "firmware_uefi": Path("/sys/firmware/efi").is_dir(),
    "carrier_marker": Path("/etc/aetheri-p0-carrier").is_file(),
    "root_filesystem": run("findmnt", "-n", "-o", "FSTYPE", "/"),
    "selinux": run("getenforce"),
    "pid1": Path("/proc/1/comm").read_text().strip(),
    "system_state": run("systemctl", "is-system-running"),
    "cgroup_v2": Path("/sys/fs/cgroup/cgroup.controllers").is_file(),
    "cgroup_controllers": Path("/sys/fs/cgroup/cgroup.controllers").read_text().split(),
    "mount_namespace": run("sudo", "-n", "unshare", "--mount", "/usr/bin/true"),
    "dbus_credential_capabilities": run("busctl", "--system", "call", "org.freedesktop.DBus",
        "/org/freedesktop/DBus", "org.freedesktop.DBus", "GetConnectionCredentials", "s",
        "org.freedesktop.systemd1"),
    "packages": run("rpm", "-q", "--qf", "%{NAME} %{EPOCHNUM}:%{VERSION}-%{RELEASE}.%{ARCH} %{LICENSE}\n",
        "glibc", "systemd", "systemd-libs", "libselinux", "selinux-policy-targeted", "dbus-broker", "sqlite-libs"),
}
report["gates"] = {
    "uefi": report["firmware_uefi"],
    "disposable_carrier": report["carrier_marker"],
    "ext4": report["root_filesystem"]["stdout"] == "ext4",
    "selinux_enforcing": report["selinux"]["stdout"] == "Enforcing",
    "systemd": report["pid1"] == "systemd",
    "cgroup_v2": report["cgroup_v2"],
    "mount_namespace": report["mount_namespace"]["exit"] == 0,
    "bus_processfd": '"ProcessFD"' in report["dbus_credential_capabilities"]["stdout"],
    "bus_label": '"LinuxSecurityLabel"' in report["dbus_credential_capabilities"]["stdout"],
}
print(json.dumps(report, indent=2))
raise SystemExit(0 if all(report["gates"].values()) else 1)
