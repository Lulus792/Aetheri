# Development desktop protocols

`aetheri-development-desktop-v1.xml` is original Aetheri code under MIT. It is
private development window management, available only to one independently
inherited connection. It is not the app SDK, an account/run identity contract,
an application-launch API or a trusted lock/permission protocol.

`wlr-layer-shell-unstable-v1.xml` is copied byte-for-byte from the already reviewed
wlroots 0.20.2 archive, member `wlroots-0.20.2/protocol/wlr-layer-shell-unstable-v1.xml`.
Its upstream copyright and permissive license remain inside the XML. The exact
source archive and copied-byte hashes are in [graphics inputs](../spec/graphics-inputs.json).
CMake verifies the copied hash before generating the server/client bindings.

See [desktop connection and acceptance](../spec/desktop.md) for roles, ownership,
failure behavior, native test evidence and the remaining product UI work. These
new Wayland contracts do not change any frozen lifecycle SQL or D-Bus XML.
