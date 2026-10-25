# Technology options

25 October 2026. Original discussion retained with this current decision summary:
C17 primary, targeted Rust/small adapters, existing tools, glibc, the proposed
technical-library boundary and useful service backends are accepted. Original shell
and language start together. MIT is selected. Browser engine and exact dependency
versions remain open. No component has been installed or integrated.

Use [DECISIONS](../DECISIONS.md) for current choices; recommendations below explain
the alternatives considered and do not reopen settled questions.

## What control means

An understandable system needs explicit ownership, interfaces and resource limits.
Original code can provide control, but also creates the responsibility to maintain
formats, hardware behavior, security fixes and compatibility. Fewer files or
processes alone do not establish lower memory use, better speed or greater clarity.

Distinguish four kinds of external code:

1. Build/debug tools run when developing: compiler, linker, build tool, debugger.
2. Runtime libraries execute inside an application, normally without a separate daemon.
3. Services remain available to coordinate hardware or privileged operations.
4. Optional apps/features start only when used, possibly with isolated helper processes.

A library is reusable compiled functionality. An API is its programming interface.
An ABI defines binary calling/layout conventions. A standard such as Wayland is
not the same thing as reusing its reference implementation.

## Languages and compilers

| Option | Strengths for Aetheri | Costs and limits |
| --- | --- | --- |
| C17 | Matches NexaUI and Physim, direct native code, explicit data and ownership, small language | Bounds/lifetimes and many concurrency rules rely on implementation discipline; no inherent speed guarantee |
| C++ | Strong ecosystem and integration with browser/graphics components; native code | More language features and build complexity; needs a deliberate coding subset |
| Rust | Native code with compiler-checked ownership in safe code; no required garbage collector | Another language/toolchain, FFI boundaries and dependency discipline; unsafe code still needs review |
| New language/compiler | Full control over syntax and semantics | Parser/type system, code generation, optimizer, linker/debug integration, tooling, standard library, validation and ongoing maintenance |

Recommendation: C17 for original system/app code, existing Clang/LLVM toolchain,
small adapters where an external API requires another language. Decide explicitly
whether Rust is available for isolated memory-sensitive work. Dependencies may
already use C++, Rust or build-time Python even if our own code is C17.

A new compiler is a separate research project. A new language could target LLVM
or transpile to C, but that still requires a language definition and implementation.
It does not automatically improve optimized code. A language used for shell commands
or configuration can be much smaller and need not define the whole OS.

Clang, a linker, CMake/Ninja, Git, GDB/LLDB, Python helpers and QEMU can be development
tools without becoming resident runtime services. Some tools may also be packaged
for the user's development workflow. Do not require every tool at every build stage:
kernel and third-party packages retain their own actual build prerequisites.

Evidence: [Clang](https://clang.llvm.org/),
[Rust ownership](https://doc.rust-lang.org/book/ch04-01-what-is-ownership.html);
local NexaUI and Physim source inventories.

## libc and system foundations

A C runtime/libc provides such things as allocation, file APIs, strings, threads,
math and access to kernel facilities. Writing programs around direct system calls
does not eliminate the need to implement these behaviors somewhere.

glibc is the mainstream GNU C runtime; musl is an alternative designed around a
small, understandable implementation. A smaller individual library can still
produce a larger total integration effort when browser binaries or other libraries
need adaptations. There is no universal faster-libc rule.

Recommendation: allow a maintained libc permanently and compare the complete
chosen browser/app stack before selecting it. Provisionally favor glibc for
integration, compare musl if the whole stack works within the budget. An existing
musl-based build host does not force the target to use musl; a cross-toolchain must
deliberately select the target runtime.

The boot path, PID 1, supervision, device permissions, authentication and session
management remain separate architecture choices. A small original supervisor is a
possible Aetheri component; adopting existing service backends does not require a
full existing desktop environment. No init system has been chosen.

## Proposed dependency boundary

| Area | Candidate direction | Why reuse or write it |
| --- | --- | --- |
| Kernel | Linux with selected drivers | Accepted basis; configure for the actual platform |
| Device firmware | Required vendor firmware only | Accepted by user; ship only needed, redistributable files |
| Graphics | Mesa, libdrm, selected EGL/OpenGL/Vulkan path | Physim needs OpenGL; NexaUI has Vulkan; kernel driver alone is insufficient |
| Input/display protocol | Wayland, libinput, xkbcommon candidates | Device quirks, layouts and interoperability; own window policy/compositor still needs a separate design |
| App toolkit | NexaUI first candidate | Original project code already exists; adapt/measure before considering replacement |
| Crypto/TLS | Maintained library such as OpenSSL | Do not make novel cryptography a prerequisite for daily-use HTTPS or update signatures |
| Disk encryption | Standard kernel/userspace encryption stack, e.g. LUKS tooling | Original Settings/installer UI can control an established format and mechanism |
| Browser engine | Existing maintained engine, Chromium/CEF or a WebKit embedding candidate | Original browser chrome with modern web implementation; compare integration and footprint |
| Documents/images | Mature PDF renderer, e.g. PDFium, and selected image decoders | Original viewer UI without immediately implementing every file format |
| Audio/Bluetooth/WLAN | PipeWire, BlueZ and a WLAN backend are candidates | Original settings UI can use maintained protocol/hardware services |
| Target apps | Existing Physim and selected NexaCloud Desktop | Preserve their actual dependencies initially; do not rewrite unrelated apps by default |

This table is a candidate boundary, not an approved list or an install script.
libinput and PipeWire purpose pages were inspected; the full versions, licenses,
transitive graph and build/runtime footprint of each candidate remain to be audited.
A compositor helper library versus more original server code is a separate choice.
Do not quietly add a complete desktop or a web-based UI runtime.

Before adoption, record purpose, alternatives, source/version/hash, license,
transitive dependencies, build-only versus shipped role, installed locations,
runtime process behavior, expected footprint and replacement/review conditions.
A browser bundle can contain many libraries; counting it as one package does not
make the underlying code disappear. No lockfile with invented selections is created.

## Performance and NexaUI

An OS-specific toolkit is not inherently faster. Performance depends on algorithms,
memory traffic, redraw area, text caching, GPU transfers, synchronization and input
handling. Measure NexaUI first; a thin Aetheri adapter/theme may be sufficient.

The laptop has two physical CPU cores and integrated graphics. Blur and transparency
need bounded affected areas, caching, low idle work and an opaque/reduced-effects
fallback. These are design hypotheses requiring target measurements, not reasons
to promise a particular frame rate.

Benchmark useful outcomes: input-to-frame latency, frame-time distribution,
idle wakeups/CPU, PSS/private memory, startup, disk footprint and battery behavior.
Processes sharing library pages should not have their memory counted twice.

## Files, directories and background work

The product goal is that a user can identify what a component does and why it is
installed. A modern browser, driver set and fonts may still contain thousands of
legitimate files. Combining everything into one huge binary is not automatically
cleaner or more efficient.

Proposed source organization, to create only as needed:
system services, apps, shared libraries, platform integration, tools, tests,
documentation and the existing project memory. Keep downloaded sources, caches,
build outputs and VM images out of the source tree and out of Git.

No target filesystem layout is decided. A conventional /usr, /etc, /var and /home
layout is a candidate because it separates installed software, configuration,
mutable service data and user data. A custom visible layout has integration costs
and must be compared before selection.

Every installed file should map to a package/component, version, role and removal
rules. Every long-lived service needs an owner, reason for activation, privileges,
resource measurements and stop behavior. Bound logs and caches. Never delete user
data or needed recovery/evidence merely to meet a file-count target.

Confirmed product requirement: a component view showing installation size,
dependency reasons, owned files/state and associated services, plus understandable
process purposes and complete-removal controls. See [application lifecycle](app-lifecycle.md).
This is not yet implemented.

Examples of legitimate processes: PID 1/supervisor; compositor; WLAN service while
networking is enabled; Bluetooth service while in use; audio graph/policy as needed;
browser renderer/network/GPU helpers while browsing; explicit compiler jobs while
building. Some kernel threads are also legitimate and not removable app clutter.
Exact topology and activation guarantees require implementation and testing.
Background global search should have an explicit index scope and bounded work.
Updates should not require constant polling or a login account.

## Terminal, editor, shell and viewers

A full-screen terminal program uses the terminal's viewport, cursor addressing and
input, e.g. a modal editor. It does not mean an exclusive full-screen OS window.
A multiplexer manages multiple terminal panes/sessions; some retain sessions while
the terminal window is disconnected. App-local split panes do not necessarily need
a permanent multiplexer service.

An original editor can start with modes, movement, undo/redo, search, files, splits,
syntax display and compiler diagnostics. Exact Neovim plugin/Lua compatibility is
a much larger requirement. Reuse shared text storage/rendering where appropriate,
but do not assume a toolkit's edit field is already a complete editor.

A shell parses commands and controls jobs. Familiar commands, pipes, redirects and
background jobs are a useful first scope. It can have original implementation
without requiring a new general-purpose language/compiler. Script compatibility
needs explicit tests and is not implied by similar syntax.

Images and PDF pages can be displayed in a graphical terminal as pixels. A terminal
graphics protocol or an internal document pane are alternatives. Kitty's graphics
protocol is one existing standard to evaluate; supporting it does not require using
Kitty's implementation. PDF rendering and image decoders can be shared with a
standalone viewer. Plan zoom, search, selection, paging, memory limits and keyboard
navigation before choosing the integration.

## Browser and resource budget

A retained engine permits original tabs, navigation, bookmarks, history, downloads,
permissions, settings and local data policy. It does not make the engine, its
updates or helper processes our code. CEF is an embedding framework, not an entire
ready-made Aetheri browser.

Multiple processes help isolate hostile content and failures. Removing the sandbox
or merging everything into one process to reduce a process count is not an
acceptable performance comparison for an everyday browser.

Compare Chromium/CEF and a suitable WebKit embedding on website behavior,
sandbox, GPU/display integration, resource sharing/copies, memory and update delivery.
Do not assume an offscreen pixel-copy path is efficient or that browser integration
works on an arbitrary custom compositor/libc.

Clarify normal sites, video formats, extension support, WebRTC, password storage,
developer tools and DRM before choosing. A retained engine does not automatically
supply Chrome extension parity, proprietary codecs or DRM-provider permission.

The original 32 GB discussion would not fit the documented full Chromium source
build, which calls for at least 100 GB free. F: has since been allocated and declared
ready; its existing Physim/NexaCloud migrations must be left untouched. Use only
F:/Aetheri and estimate total source/build/cache/VM needs before a large download. A verified prebuilt package can
be an initial option if approved; its ABI, dependencies, origin and security-update
path must be reviewed. No engine download/build is authorized by this proposal.

Sources: [CEF sandbox](https://github.com/chromiumembedded/cef/blob/master/docs/sandbox_setup.md),
[Chromium build requirements](https://chromium.googlesource.com/chromium/src/+/main/docs/linux/build_instructions.md),
[terminal graphics](https://sw.kovidgoyal.net/kitty/graphics-protocol/).

## Terms the user asked about

External monitors are additional physical displays connected directly or through
a dock/adapter. Count, ports, resolution, refresh, hotplug and per-display scaling
are separate acceptance cases.

A Start menu is an app launcher/menu such as Windows offers. Global search may
cover that need, so a separate menu need not be added. A global menu bar places
the active app's menus in a shared screen-edge bar, as on macOS, rather than in
each window. Both remain product choices.

Secure Boot makes UEFI verify boot software against trusted signatures/hashes.
It can protect the boot chain from unauthorized replacements; it does not encrypt
the drive. It can use user-managed trust rather than inherently requiring an online
account. The user now allows Secure Boot if useful, superseding the earlier exclusion.
Without trusted boot, encryption alone does not
establish the integrity of unencrypted early boot components. Keep encryption,
signed updates and trusted boot as distinct mechanisms.
[Microsoft's description](https://learn.microsoft.com/en-us/windows-hardware/design/device-experiences/oem-secure-boot).

## License choices

| Policy | Meaning for original distributed code |
| --- | --- |
| MIT | Short permissive license; reuse and closed derivatives allowed, notices retained |
| Apache-2.0 | Permissive, explicit patent grant and additional notice/change requirements |
| GPL-3.0 | Strong copyleft for covered conveyed works; corresponding-source obligations; not an automatic license for every separate OS component |
| MPL/LGPL families | Narrower copyleft boundaries for files/libraries; exact linking/distribution conditions need review |
| AGPL family | Adds source-offer requirements for certain modified network-accessible programs |

All these families can permit commercial use; open source does not mean
non-commercial. A public repository without an explicit license does not grant
general reuse permission merely because its code is visible.

Recommendation for this user's simplicity/interop goal: MIT for original code,
unless requiring redistributed modifications to stay open is a stronger goal.
Apache-2.0 is an alternative for explicit patent terms. The user chooses.
Kernel and external components keep their own licenses, notices and source
distribution obligations. Firmware and media/DRM terms need component-specific review.
MIT has now been selected for original Aetheri code/documentation. External
components retain their own licenses; this does not relicense an assembled OS.

Primary texts: [MIT](https://opensource.org/license/mit),
[Apache-2.0](https://www.apache.org/licenses/LICENSE-2.0),
[GPL-3.0](https://www.gnu.org/licenses/gpl-3.0.html),
[kernel license boundary](https://docs.kernel.org/process/license-rules.html).
