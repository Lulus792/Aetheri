"""Generate native method tables from accepted XML; no handwritten signature copy."""
import json
from pathlib import Path
import re
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]


def main():
    lines = ["/* Generated from the accepted XML contracts. Do not edit. */"]
    descriptors = []
    for file in ("management-v1.xml", "documents-v1.xml", "runtime-v1.xml"):
        node = ET.parse(ROOT / "spec/api" / file).getroot()
        for interface in node.findall("interface"):
            name = interface.attrib["name"]
            if name == "org.aetheri.Subscription1":
                continue  # No subscription objects exist until that capability is implemented.
            identifier = "wire_" + name.replace(".", "_")
            roles = {"org.aetheri.Management1": 3, "org.aetheri.Runtime1": 1,
                     "org.aetheri.User1": 2, "org.aetheri.Documents1": 2}
            if name not in roles:
                raise ValueError("interface requires an explicit authority role")
            role = roles[name]
            path = node.get("name", "/org/aetheri/Management1")
            lines.append(f"static const sd_bus_vtable {identifier}[] = {{ SD_BUS_VTABLE_START(0),")
            for method in interface.findall("method"):
                args = method.findall("arg")
                inputs = [arg for arg in args if arg.get("direction", "in") == "in"]
                outputs = [arg for arg in args if arg.get("direction", "in") == "out"]
                def names(group):
                    for arg in group:
                        if re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", arg.attrib["name"]) is None:
                            raise ValueError("unsupported argument identifier")
                    return " ".join("SD_BUS_PARAM(" + arg.attrib["name"] + ")" for arg in group) or '""'
                callback = "method_info" if name == "org.aetheri.Management1" and method.attrib["name"] == "GetProtocolInfo" else "method_unavailable"
                fields = [json.dumps(method.attrib["name"]), json.dumps("".join(a.attrib["type"] for a in inputs)),
                          names(inputs), json.dumps("".join(a.attrib["type"] for a in outputs)), names(outputs),
                          callback, "SD_BUS_VTABLE_UNPRIVILEGED"]
                lines.append("    SD_BUS_METHOD_WITH_NAMES(" + ", ".join(fields) + "),")
            lines.append("    SD_BUS_VTABLE_END }; ")
            descriptors.append("    {" + f"{role}, {json.dumps(path)}, {json.dumps(name)}, {identifier}" + "},")
    lines.extend(["static const struct wire_interface wire_interfaces[] = {", *descriptors,
                  "    {0, NULL, NULL, NULL}", "};"])
    destination = Path(sys.argv[1])
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")


if __name__ == "__main__":
    main()
