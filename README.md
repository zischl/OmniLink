<h1 align="center">
  <img src="assets/AssetLogoNB.png" alt="OmniLink Logo" width="140" /><br>
  OmniLink
</h1>

<p align="center">
  <strong>Seamless multi-machine workspace unification over LAN.</strong><br>
  Built with modern C++20 for KVM-like <strong>Screen, Windows, Input, Clipboard,</strong> and <strong>Audio</strong> synchronization<br>
  with high performance in mind.
</p>

---

> [!NOTE]
> **Project Status**: I've been building this solo for over a year in whatever time I can carve out, so treat this as a working snapshot rather than a finished product.
>
> * **What works end-to-end (Windows):** Video capture/streaming, hardware encode/decode, input synthesis, WASAPI audio, and virtual clipboard/file streaming.
> * **In progress / Not yet wired:** Several UI controls in the dashboard are not yet connected to the backend, and WindowLink currently acts as window mirroring with remote control (true cross-machine window dragging is the final target).
> * **Linux Support:** Architecturally mapped out (Vulkan, PipeWire, epoll), but implementation is in progress.

<p align="center">
  <img width="1293" height="822" alt="OmniLink Dashboard" src="https://github.com/user-attachments/assets/9e863db8-d9fc-49d2-81b4-2319533a81ce" />
</p>

## Intro

OmniLink turns a handful of standalone computers on the same network into something that feels like one workspace. Share a screen, drag input, pipe audio, and paste files between them without it feeling like remote desktop software. 

I split my work across a PC and a laptop, and the laptop dual-boots into Arch sometimes. Mouse Without Borders got me most of the way for input, but the connection dropped more than I liked and audio was never part of the picture. Audio Relay covered sound, but it never felt seamless. And I didn’t want something like Parsec or Moonlight running in the background all day just to link two machines I sit in front of daily, and nothing out there gave that true OS-to-OS link feel, it was always one tool per problem, each with its own compromises. 

So I decided to build the all-in-one version, OmniLink is built from bare metal up in modern C++20 to combine all these capabilities into a single cohesive pipeline-leveraging hardware codecs, kernel-level asynchronous I/O, and low-level input hooks to achieve as latency low enough that it doesn’t feel like you’re using a second machine at all, partly because I wanted it to exist, partly because I wanted an excuse to get properly good at C++.

None of this was obvious going in. Every subsystem below started as a naive version first.

---

## Architecture

The core (discovery, pairing, feature state, packet handling) is entirely platform-agnostic. Everything that actually touches the OS (screen capture, audio APIs, clipboard, raw input) lives in System Link, a concrete struct with a separate definition per platform, selected at compile time. No shared base class, no virtual dispatch. Windows has all of it (DXGI/WGC, WASAPI, Win32 input/clipboard) implemented. Linux gets the same architecture, backed by Vulkan, PipeWire, and Linux equivalents for input/clipboard - most of that isn't written yet, but the split means filling it in is additive, not a rewrite.


```

src/
├── core/         # Platform-agnostic (BurstQ, ByteStream, InstanceRegistry, packets)
├── net/          # Networking logic + platform drivers
│   ├── platform/windows/   # IOCP UDP, sub-streams, OmniTCPStream
│   └── platform/linux/     # epoll UDP, sub-streams, OmniTCPStream
├── qrypt/        # Monocypher security engine
├── app/          # SystemLink.h platform boundary
│   ├── platform/windows/   # Win32, IOCP, WGC, WASAPI
│   └── platform/linux/     # POSIX, epoll, PipeWire, Vulkan
├── capture/      # WinCap.cpp  vs  PipeWireCap.cpp
├── codec/        # NVENC/NVDEC + platform interop
├── render/       # D3D11  vs  VkRenderer
├── input/        # IOLink.cpp per platform
├── window/       # WinForge  vs  LinuxForge
└── clipboard/    # COM virtualization  vs  data-control/XSelection

```

---

## Notes

This has been running for a bit over a year, 256 commits by the point I got around to writing this, which tells you roughly where documentation sat on the priority list. A good chunk of that commit history is just rewriting things once they became the bottleneck such as mutex-guarded queues became lock-free ones, `std::function` callbacks became raw function pointers, plain UDP became the IOCP net session and net sub-stream system described later on. Or maybe.. just maybe it's the fact that I tend to and like to micro optimize.

Partway through, I got annoyed enough with managing CMake presets that I wrote a Neovim plugin for it ([Build Sentry](https://github.com/zischl/build-sentry)). Soon after, I got tired of writing commits, so I built ([AI-Commits.nvim](https://github.com/zischl/AI-Commits.nvim)) an AI commit message generator plugin - which is probably why the messages get more consistent in the back half of the history.
Right now I'm mostly heads-down on finishing WindowLink's actual cross-machine drag behavior and wiring up the remaining dashboard controls, adding more encoders for wider support, and Linux is the next big push after that, not before. If you're poking around the repo and something looks unfinished or inconsistent, it probably is. This section is the accurate picture, the rest of the doc describes the engine as designed.
There are still uncommitted changes sitting in my working tree as I write this. Development’s ongoing, not wrapped up.

And somewhere in the code there's a `Liss`. What or who that is, I'm taking to the grave.

---

## Spatial Grid & Topology Subsystem

OmniLink arranges connected nodes across a physical 3×3 grid:

<p align="center">
  <img width="300" height="300" alt="Spatial Grid Topology" src="https://github.com/user-attachments/assets/8bb23c0d-9f5a-45d7-84c4-00215e91005b" />
</p>

* **Bitmask Slot Allocation O(1):** Topology positions are tracked using a 9-bit mask. Hardware bit-scan intrinsics allocate available slots and count active nodes in single-cycle operations, supporting live dynamic grid swapping without dropping connections.
* **Monotonic Handshake Lifecycle:** Connection transitions follow a strict monotonic 7-state machine, ensuring duplicate or out-of-order UDP datagrams cannot regress an active handshake.
* **Asynchronous Handshake Retries:** Background worker threads handle transient UDP packet loss via bounded retries, non-blocking condition variables, and a 30-second pairing timeout.
* **Subnet Discovery & Symmetrical Duel Resolution:** Broadcasts over UDP port `58426` auto-discover local nodes. Simultaneous mutual handshakes are deterministically resolved by IP arbitration, eliminating race conditions and deadlocks.
* **Group Presets:** Save and restore persistent multi-node workspace layouts (up to 8 clients + host) with full spatial mappings.

---

## Core Concurrency & Memory Architecture

OmniLink achieves predictable, sub-millisecond execution times by eliminating dynamic heap allocations and kernel lock contention in steady-state operations:

* **Cache-Isolated Lock-Free Queues (`BurstQ`):** Power-of-two ring buffers eliminate modulo division via bitwise masking. Head and tail indices are aligned to 64-byte boundaries (`alignas(64)`) to prevent cross-core false sharing and cache thrashing.
* **Multi-Tier Dispatch Pipeline:** Parameterless commands dispatch in $O(1)$ via static function pointer tables, while payload-bearing commands queue through type-safe variant queues without heap allocations.
* **Compile-Time Packet Deserialization:** Replaces polymorphic base classes and virtual table (`vptr`) indirection with `std::variant` and C++ template fold expressions, deserializing datagrams with zero dynamic allocation.
* **Zero-Allocation Binary Wire Format:** Branchless, endian-safe bitstream readers and writers operate directly over stack buffers and pre-reserved memory to prevent heap fragmentation.
* **Dedicated Worker Thread Models:** Decouples background ingestion via specialized cached (ring-buffered) and uncached (high-frequency spin-loop) worker threads.

---

## Hybrid Network Transport Architecture

OmniLink partitions network traffic across two transport paradigms: low-latency, real-time channels run over **Connected UDP via Windows IOCP**, while bulk and reliable transfers run over a **Cross-Platform TCP Engine**.

### 1. Control & Message Transport (`OmniNetSession`)
* **Connected UDP with Kernel Binding:** Sockets are pre-associated with peer addresses via `connect()`, enabling low-overhead asynchronous overlapped I/O instead of per-packet route lookups.
* **Shared IOCP Multiplexing:** All active sessions multiplex across I/O Completion Port worker pools for non-blocking event completion.
* **Zero-Copy Scatter-Gather Framing:** Frame payloads and protocol headers are dispatched as contiguous scatter-gather buffers without heap reallocation.
* **Atomic In-Flight Backpressure:** Transmit queues use atomic ring buffers with `_mm_pause()` backpressure, dropping non-critical frames under backpressure without blocking capture pipelines.

### 2. High-Throughput Real-Time Media (`OmniNetSubStream`)
* **Dedicated Channels for Heavy Media:** Isolates heavy real-time pipelines (ScreenLink, WindowLink, AudioLink) into independent ephemeral UDP sub-streams to prevent HoL-blocking on the control plane.
* **$O(1)$ Event Routing:** Registers direct instance pointers as IOCP completion keys, bypassing runtime lookup tables upon packet dequeue.
* **Zero-Copy Kernel-to-Renderer Pipeline:** Datagram chunks exceeding MTU are deposited directly into consumer memory pools (such as sample rings or frame queues) by the kernel, completing with atomic token tracking.

### 3. Reliable Bulk Streaming (`OmniTCPStream`)
* **Guaranteed Delivery:** Cross-platform TCP stream engine for lossless payloads, including clipboard manifests, directory structures, and virtual file transfers.
* **High-Throughput IO:** Configured with 64 KB streaming chunks and 1 MB kernel socket buffers to eliminate delayed ACKs.
* **Direct Disk Streaming:** Supports direct disk-to-network streaming with non-blocking timeouts, progress callbacks, and thread cancellation.

---

## Feature Implementations & Performance Architecture

OmniLink is built from the hardware level up for sub-millisecond local-area peer-to-peer interaction. The pipeline eliminates intermediate staging buffers, bypasses OS compositor overhead, leverages lock-free memory rings aligned to CPU cache lines, and offloads heavy compute directly to dedicated GPU silicon.

### ScreenLink (Desktop Video Pipeline)
ScreenLink captures, encodes, transmits, decodes, and presents the full desktop display with sub-frame processing overhead.

* **Zero-Copy Capture Engines:**
  * **Windows Graphics Capture (WGC):** Free-threaded frame pool in `B8G8R8A8` normalized format. Driven entirely by `FrameArrived` callbacks consuming 0% CPU when the desktop is static. Direct surface queries extract `ID3D11Texture2D` handles without host staging copies.
  * **DXGI Desktop Duplication Fallback:** Polling engine using non-blocking duplication calls driven by a dedicated high-frequency spin-worker.
* **Cached NVENC Texture Pools:**
  * Replaces standard per-frame resource registration (`~10 µs` driver cost) with a pre-registered 3-slot resource pool (`PoolCache`). Resource resolution completes in **~2 ns** via pointer identity checks.
* **Triple-Buffered Bitstream Ring Pool:**
  * Decouples the hardware encoder from UDP transmission via a 3-slot asynchronous bitstream pool.
  * **Deferred Bitstream Unlocking:** Windows IOCP completion callbacks release buffer reservations atomically; hardware unlocking (`nvEncUnlockBitstream`) is deferred until the encoder loops back, eliminating thread stalls.
  * **Frame-Drop Safeguards & Instant IDR:** Network congestion triggers clean frame drops without stalling capture, while packet loss triggers atomic IDR keyframe injection.
* **Compute Shader Color Space Conversion:**
  * High-throughput 16×16 compute shader executing studio-swing BT.709 color matrix conversion with unrolled 2×2 box filtering directly to planar NV12 textures.
* **Zero-Copy CUDA Direct Surface Writes:**
  * Maps an intermediate DirectX 11 shared texture into CUDA via `cuGraphicsD3D11RegisterResource`.
  * Custom 2D CUDA kernel (`CastNV12toBGRA`) executes across 16×16 warp thread blocks, decoding NV12 and writing BGRA pixels directly to the DirectX 11 surface via `surf2Dwrite`, completely bypassing host RAM.
* **Decoupled Presentation Engine:**
  * Dedicated message and render loop consuming an independent frame ring buffer.
  * Presents fullscreen quads using `DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL` with `DXGI_PRESENT_ALLOW_TEARING` and double buffering to bypass DWM compositor latency.

### WindowLink (Isolated Application Streaming)
Streams isolated Win32 application windows rather than the entire desktop display:

* **Dynamic HWND Surface Binding:** Captures target window handles using `IGraphicsCaptureItemInterop`, streaming only client area pixels regardless of occluding windows.
* **Live Viewport Resizing & Cache Purging:** Detects dynamic window geometry changes on the fly, recreating capture frame pools and purging stale Shader Resource Views (SRVs) without texture distortion or pipeline resets.
* **Minimization & Lifecycle Guards:** Subscribes to capture closure tokens and intercepts zero-dimension minimization states, gracefully pausing frame flow to preserve DirectX device state.
* **High-Fidelity Input Forwarding:**
  * Viewport cursor inputs normalize to 16-bit virtual space (`[0..65535]`) and synthesize directly into the host OS.
  * Enforces `SetCapture` / `ReleaseCapture` states across drag operations, with full pass-through for horizontal/vertical wheel deltas, extended buttons (`XBUTTON1`/`XBUTTON2`), and hardware scancodes.

### InputLink (Sub-Millisecond KVM Over IP)
Shares keyboard and mouse control across physical machines over local network sub-streams with zero hardware switches.

* **Dual-Path Capture Architecture:**
  * Combines global low-level hooks (`WH_KEYBOARD_LL`, `WH_MOUSE_LL`) with dynamically registered raw input sinks (`RAWINPUTDEVICE` with `RIDEV_INPUTSINK`).
  * Unfiltered mouse movement deltas harvest directly from the USB HID stack, bypassing OS cursor clamping and acceleration curves.
* **16-Byte SIMD-Aligned Input Packets:**
  * Input datagrams are explicitly packed and aligned to 16-byte boundaries (`alignas(16)`), allowing socket buffers to be ingested via direct pointer casts with zero deserialization overhead.
* **Proportional Edge Continuity & 8-Zone Topology:**
  * Normalizes screen exit coordinates into 16-bit fixed-point ratios, mapping cursor arrivals proportionally across cardinal boundaries and corner zones (`LU1`, `RU1`, `LD1`, `RD1`).
* **Hysteresis-Protected Return Boundary:**
  * Accumulates local raw deltas with a 150-pixel return threshold and a 300 ms debounce window to eliminate edge boundary flutter.
* **Dual-Mode Hybrid Synthesis:**
  * **Desktop Navigation Mode:** Uses `SetCursorPos` for sub-pixel desktop responsiveness alongside `SendInput` for button and wheel events.
  * **3D FPS Gaming Mode:** Tracks foreground window cursor clipping via `GetCursorInfo`. When a game locks or hides the hardware cursor, input switches to uncoalesced relative events (`MOUSEEVENTF_MOVE_NOCOALESCE`), eliminating camera jitter and viewport hitching.
* **Anti-Loopback Tagging & Breakout:**
  * Injected inputs carry signature cookies in `dwExtraInfo` to bypass local hook recursion.
  * Includes a hardcoded emergency breakout hotkey (`Ctrl + Alt + 1`) to reclaim local control instantly.

### AudioLink (WASAPI Loopback & Multi-Clock Mixing)
Captures system loopback and microphone inputs, mixes them across independent clock domains, and transmits audio over dedicated UDP sub-streams:

* **Low-Latency WASAPI Engine:**
  * Uses `IAudioClient3` shared-mode engine periodicity for minimal hardware buffer latency, with fallback to standard `IAudioClient` (20 ms) on legacy audio drivers.
  * Supports runtime toggling across desktop capture, microphone capture, and dual capture loops.
* **Hardware Clock Drift Mitigation:**
  * Decouples DAC (desktop output) and ADC (microphone input) clock domains using a 65,536-sample lock-free SPSC ring buffer with branchless index wrapping and 64-byte cache-line isolation.
  * Sample deficits during ring drains are padded with clean floating-point silence to eliminate underrun pops and clicks.
* **Real-Time Normalizing Mixer:**
  * Mixes and clamps desktop and microphone channels in 32-bit floating-point space before normalizing to signed 16-bit PCM or raw floats.
* **Lock-Free Playback & Direct IOCP Ingestion:**
  * A 512 KB lock-free circular playback buffer wires directly into `OmniNetSubStream` IOCP completion callbacks via pre-allocated receive pools.
  * Automated 4-way sample conversion handles bit-depth adaptations and channel remapping transparently.

### ClipboardLink & Virtual File Streaming
Provides peer-to-peer clipboard synchronization across machines, handling everything from plain text snippets to multi-gigabyte file transfers:

* **Three-Tier Transport Architecture:**
  * **Tier 1 (LightGram):** Payloads $\le$ 1400 bytes (text snippets, short URLs) synchronize immediately over UDP with zero connection overhead.
  * **Tier 2 (ClipboardManifest):** Complex payloads (images, rich text, multi-file lists) serialize a lightweight binary manifest containing item descriptors, MIME types, and stream ports without pre-fetching data.
  * **Tier 3 (On-Demand TCP Streaming):** Bulk payloads stream over dedicated TCP sockets (`OmniTCPStream`) only when the remote user triggers a paste action.
* **Win32 Delayed Rendering & Shell Virtualization:**
  * Registers empty promise handles via `SetClipboardData` for native clipboard formats (`CF_HDROP`, `CFSTR_FILEDESCRIPTORW`, `CFSTR_FILECONTENTS`, `CF_DIB`, `CF_UNICODETEXT`).
  * On paste requests (`WM_RENDERFORMAT`), constructs `FILEGROUPDESCRIPTORW` with `FD_PROGRESSUI`, triggering the native Windows Explorer copy progress animation with accurate file sizes.
* **Streaming COM Virtualization:**
  * Implements `IDataObject` and `IStream` (`OmniVirtualIStream`), piping chunks directly from the TCP receive socket into consumer file handles in 64 KB streaming blocks.
* **Progress Tracking & Cancellation:**
  * Streams exceeding 1 MB register non-blocking transfer progress monitors with real-time UI callbacks and cooperative cancellation flags.

---

## Zero-Trust Cryptographic Subsystem

OmniLink leverages [Monocypher 4.0.2](https://monocypher.org/) for authenticated, end-to-end encrypted peer communications across untrusted networks:

### 1. Dual-OS CSPRNG & Key Generation
* **System Entropy Gathering:** Direct platform entropy sources (`BCryptGenRandom` on Windows, `/dev/urandom` on POSIX).
* **Deterministic Expansion:** Seed material is expanded via the ChaCha20 stream cipher to generate high-entropy keying material.

### 2. Ephemeral Key Exchange (X25519)
* Fresh 32-byte ephemeral X25519 key pairs are generated per session.
* Shared secrets are derived via Diffie-Hellman over Curve25519, guaranteeing forward secrecy.

### 3. MitM-Proof Mutual Passkey Verification
To defend against active Man-in-the-Middle (MitM) attacks without a public PKI:
* Public keys are sorted lexicographically by byte value to establish deterministic input ordering.
* A canonical 96-byte transcript buffer (both public keys + derived shared secret) is hashed using **BLAKE2b**.
* The first 4 bytes of the digest yield a big-endian 6-digit mutual passkey presented simultaneously on both screens for out-of-band visual confirmation.

### 4. Cryptographic Action Tokens & Replay Defense
* High-privilege remote execution commands (input injection, stream control, clipboard sync) require a validated HMAC action token.
* Tokens are bound to session nonces and monotonic sequence counters to eliminate command injection and replay attacks.

### 5. Persistent Pairing & Trust Registry
* **Long-Term Pairing:** Verified peers derive a 32-byte pairing token from the shared secret via BLAKE2b with domain-separated context keys.
* **Fast Re-Authentication:** Paired devices exchange and verify authenticated tokens upon reconnect, bypassing manual passkey prompts.

### 6. Cryptographic Hygiene & Memory Sanitization
* All ephemeral private keys, session nonces, and shared secrets are explicitly securely wiped from memory upon session termination or object teardown.

---

## GUI

Built with Dear ImGui on a Direct3D 11 backend and soon vulkan for linux, featuring a custom borderless window with proper hit-testing for dragging/resizing, DWM drop shadows, per-monitor DPI awareness, and system tray minimization.

* **Nexus:** Feature toggles, live radial device ring showing connected peers and link states, and a real-time metrics bar (latency and bandwidth).
* **Instances:** Active node cards, saved workspace groups for one-click multi-device connection, and a trusted node registry backed with per-device token revocation.
* **Keybinds:** Global shortcut reference and custom bindings.
* **Settings:** UDP ports, discovery options, framerate caps, and notification toggles.

Pairing prompts display as centered modals with a 30-second countdown and Accept/Reject actions. Link state changes and connection events appear as non-blocking toasts in the bottom-right corner, falling back to native OS notifications when the application window is hidden.

---

## Linux Parity Architecture & Cross-Platform Roadmap

OmniLink is architected with a strict separation between core engine logic and operating system APIs, enabling clean cross-platform implementation:

| Subsystem | Windows | Linux | Status |
|---|---|---|---|
| Video capture | WGC / DXGI | PipeWire + DMA-BUF | Scaffolded |
| Codec | NVENC/NVDEC D3D11 (Intel, AMD planned) | NVENC/NVDEC CUDA + `VK_KHR_video_queue` | Concept verified |
| Presentation | D3D11 swapchain | Vulkan swapchain | Scaffolded |
| Input capture | Windows Hooks + Raw Input | `evdev` / `libinput` | Mapped |
| Input synthesis | `SendInput` | `uinput` | Mapped |
| Audio | WASAPI | PipeWire / PulseAudio | Mapped |
| Clipboard | COM `IDataObject` | `wl-data-control` / `XSelection` | Scaffolded |
| TCP | Winsock | POSIX | **Done** |
| UDP | IOCP | epoll | Scaffolded |
| Windowing | Win32 | SDL3 / Wayland | Scaffolded |

---

## Building

Windows only for now. CMake-based, MSVC. Requires cuda for the Nvidia Encoder/Decoder edition (and maybe more soon after Intel and Amd versions are added) the capture pipeline is built primarily around NVENC/NVDEC for now.
Monocypher and a couple of other dependencies come in via `FetchContent`. Check `CMakeLists.txt` / `CMakePresets.json` for the full dependency list.

**Prerequisites:** Windows 10/11 64-bit, VS 2019/2022 (MSVC, C++20), CUDA 11.0+ (Turing or newer, cc 7.5+), CMake 3.18+, AVX2 CPU.

```powershell
# Clone the repository
git clone https://github.com/zischl/OmniLink.git
cd OmniLink

# Generate build configuration
cmake --preset Default -DCMAKE_EXPORT_COMPILE_COMMANDS=1

# Build targets
cmake --build --preset Default --target main --config Release

```

The output executable (`main.exe`) and required external runtime libraries are automatically placed in the build output directory via post-build commands. (Although tbh LZ4 is no longer used anymore).

---

## Running Unit Tests

OmniLink includes a test suite covering core subsystems:

```powershell
# Build test suite
cmake --build --preset Default --target tests --config Release

```

The test runner covers (although some are temporarily disabled while testing newer features):

* `PlatformIntrinsicsTest`: Bit-scan forward and popcount intrinsics.
* `OmniUUIDTest`: 12-byte random node ID generation and collision resistance.
* `BurstQTest`: SPSC circular queue push/pop, acquire-release orderings, and boundary wrapping.
* `ByteStreamTest`: Big-endian serialization, buffer bounds checking, and string safety.
* `InstanceRegistryTest`: 3x3 slot bitmask allocation, dynamic swapping, and group persistence.
* `WinForgeTest`: Frame queue packing, decode buffer advancing, and decoder concept swapping.
* `D3D11RendererTest`: Device creation, deferred contexts, and swapchain initialization.
* `WinCapTest`: WGC event-driven frame pool acquisition and DXGI desktop duplication.
* `VarianceTest`: Template fold-expression variant deserializer and compile-time index resolution.
* `NetSessionTest`: Connected UDP sockets, scatter-gather framing, and chunked frame tokens.
* `NVEncodeDecodeTest`: Cached NVENC texture pools, triple-buffered bitstream output, and CUDA NVDEC decoding.
* `AudioCapTest`: WASAPI loopback capture, microphone acquisition, and dual-clock mixing.
* `AudioRenderTest`: SPSC sample playback ring buffer and 4-way bit-perfect sample conversion.
* `QryptTest`: Ephemeral X25519 ECDH key generation, BLAKE2b passkeys, and HMAC action tokens.

---

## License

Distributed under the [MIT License](https://www.google.com/search?q=MIT+LICENSE).

```

```
