# ScriptHookVI

A ScriptHookV-style script host for GTA VI as run by
[VIctory](https://github.com/AcelezXYZ/VIctory) (the Xbox build running natively
on Windows): a plugin loader, a cooperative script runtime and a native-call
interface behind a small C API, plus a render-hook scaffold.

**Status.** The mechanics are tested end to end against a stand-in title
under Wine (see [Tests](#tests)). **It does not attach to the real title yet**:
that needs per-build targets (`ScriptTick`, `GetNativeHandler`, ...) for the
title image, which are not known, and VIctory itself does not render the world
stably yet. Nothing here was run against the real game.

## How it fits together

```
VIctory ── maps the title image ──> LoadLibrary("ScriptHookVI.dll")
        └─ shviAttach(imageBase, imageSize, buildId, flags)    sdk/inc/host.h
              core/attach.cpp   log, ini, targets, hook, plugins
              game/             target (RVA or pattern) -> address, ScriptTick detour
              runtime/          fibers (scripts), native invoker, keyboard, *.asi loader
              render/           stage callbacks + frame history (no backend yet)
              exports.cpp       plugin C API (sdk/inc/main.h, render.h)
plugin.asi ── scriptRegister(module, main) ─> run as fiber on the title's script thread
```

* **Host interface** ([sdk/inc/host.h](sdk/inc/host.h)): the host loads
  `ScriptHookVI.dll` and calls `shviAttach` after the title image is mapped and
  **before its entry point runs**, so the hook is in place before any title
  thread executes the hooked code. Do not call it from `DllMain`.
  `SHVI_HOST_OFFLINE` tells ScriptHookVI the title cannot go online (VIctory is
  offline by design), so no `OnlineFlag` is needed.
* **Targets**: addresses are relative to the **title image** the host passed,
  not to the host executable. Per target either `<Name>.Rva` (a per-build
  offset, preferred) or `<Name>.Pattern` in `ScriptHookVI.ini`.
* **Script runtime**: every script is a Windows fiber. `scriptWait(ms)` switches
  back to the host; the hook resumes it when the time is up. Scripts run on one
  thread only (the first thread seen in the hooked function).
* **Natives**: `nativeInit` / `nativePush64` / `nativeCall`, typed through
  `invoke<R>(hash, args...)` in `sdk/inc/nativeCaller.h`.
* **Plugins**: any `*.asi` in the game folder (host-provided `gameDir`) or `plugins/`.
* **Standalone mode** (ordinary Windows game): the `dinput8.dll` proxy loads the
  DLL and calls `shviAttachMainModule`. Not used with VIctory.

## Using it from VIctory (sketch, not compiled)

```rust
#[repr(C)]
struct ShviAttachInfo {
    struct_size: u32,
    flags: u32,            // 1 = SHVI_HOST_OFFLINE
    image_base: usize,
    image_size: usize,
    build_id: *const c_char,   // e.g. BLAKE3 hex of the executable, or null
    config_dir: *const u16,    // folder for ScriptHookVI.ini / .log, or null
    game_dir: *const u16,      // folder searched for *.asi, or null
}
// with libloading: let lib = Library::new("ScriptHookVI.dll")?;
// let attach: Symbol<unsafe extern "C" fn(*const ShviAttachInfo) -> i32> = lib.get(b"shviAttach")?;
// Call it after mapping the title image, before running its entry point.
```

## Build

Windows (MSVC or MinGW-w64), 64-bit:

```bash
cmake -S . -B build -A x64
cmake --build build --config Release
```

Cross-compile from Linux (checked with MinGW-w64 13):

```bash
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake
cmake --build build
```

Output in `build/out/`: `ScriptHookVI.dll`, `dinput8.dll` (standalone mode only),
`hello.asi` (sample). Put `ScriptHookVI.dll` where the host can load it and
`config/ScriptHookVI.ini` into the host's `configDir`; `ScriptHookVI.log` is
written there.

## Writing a plugin

Include `sdk/inc/main.h`, link `ScriptHookVI`, build a DLL, rename it `.asi`.
See [samples/hello/hello.cpp](samples/hello/hello.cpp). API:
`scriptRegister/-Unregister*`, `scriptWait`, `keyboardHandlerRegister`,
`nativeInit/Push64/Call`, `getGlobalPtr`, `getGameExeTimestamp`,
`shviIsAttached`.

## What is still needed to make it work

Each target is one entry in `[Patterns]` (`.Rva` or `.Pattern`) or a verification:

| Item | Needed for | Assumption in the code |
|------|-----------|------------------------|
| `ScriptTick` | a per-frame function on the title's script thread | takes ≤ 4 integer args; first ≥ 14 bytes (`.Stolen`) are position independent |
| `GetNativeHandler` | native calls | `fn(hash) -> void(*)(NativeContext*)` |
| `NativeContext` layout, vector write-back | native calls | same as earlier RAGE titles; unverified (`runtime/native_invoker.h`) |
| Native hashes / names | writing real scripts | none known |
| `GlobalBase` | `getGlobalPtr` | 64 pages × 0x40000 slots |
| `OnlineFlag` | safety guard | not needed under VIctory (host offline) |
| Window/keyboard | keyboard handlers | normal top-level window of the process |

Not included yet: graphics backends (see below), a native name/hash database,
hot-reload of plugins, a Lua/.NET host. Plugins are native DLLs and cannot be
sandboxed; a file API for a scripting layer would need path confinement.

## Render hooks (scaffold)

Goal: let render extensions (FSR 4 / DLSS-style upscalers and frame generation)
get the current and previous frame's colour and motion vectors **before the UI
pass**. What exists:

* `sdk/inc/render.h` / `render_types.h`: stage callbacks (`PRE_UI`, `POST_UI`,
  `PRESENT`), `renderGetResource(slot, history)` for colour, motion vectors,
  depth and final colour, `renderGetFrameInfo` (render size, jitter, motion
  vector scale, view-projection of this and the previous frame, reset flag).
* `src/render/`: frame/history bookkeeping, tested natively
  (`tests/render_manager_test.cpp`): previous-frame data is a backend-owned
  copy valid for one more frame; `resetHistory` is set on the first frame,
  resolution change, frame-index gap or when the backend asks (camera cut).
* Raw API handles (`ID3D12Resource*` / `VkImage`) are hidden unless
  `[Render] AllowNativeHandles=1`.

What does **not** exist, and is the hard part:

* A D3D12 backend (`IRenderBackend`). Under VIctory the graphics API is the
  host's D3D12 and VIctory owns queue, command lists and present
  (`victory-gpu`), so the backend belongs on that side. Still open: finding
  the pre-UI boundary in the title's command stream, locating the motion
  vector and depth targets, and copying them for history.
* Whether the game writes usable motion vectors at all.
* The exact input requirements of the vendor SDKs (e.g. DLSS 5), which I have
  not seen. Do not treat the current struct layout as final.

Run the logic tests natively:

```bash
g++ -std=c++20 -Isdk/inc -Isrc/render tests/render_manager_test.cpp src/render/render_manager.cpp -o rmt && ./rmt
```

## Tests

* `tests/render_manager_test.cpp`: frame/history logic, runs natively.
* End to end, under Wine: `tests/run-e2e.sh` cross-compiles a stand-in title
  (`testtitle.dll`, our own code with a hookable tick, a native handler and a
  globals table), a plugin (`selftest.asi`) and a host (`testhost.exe`) that
  plays VIctory's role. Four scenarios: `pass` (hook attaches; the script
  ticks and is throttled by `scriptWait`; a native call and `getGlobalPtr`
  work; the hooked function still returns its original result), `nopatterns`,
  `guard` (online guard blocks) and `badimage` (no crash). Wine 9 aborts when
  started from some environments, hence the clean `env -i` in the script.
* Not covered: the dinput8 standalone path, keyboard input (no window under
  test), the real title.

## Safety

Single-player modding tool. Scripts are paused while the game reports an
online session. Unless the host asserts `SHVI_HOST_OFFLINE`, the hook refuses
to attach when it cannot detect that (`RequireOnlineGuard=1`). There is no anti-cheat bypass here and
none will be accepted. See [Legal](#legal).

## Licence

GPL-3.0 (see [LICENSE](LICENSE)). Original code; it contains no game code,
assets or keys.

## Legal

Interoperability and modding tool for copies of the game you own. This
repository contains no game files, extracted assets, keys, proprietary SDK
material or game patterns, and contributions that bypass copy protection or
anti-cheat will not be accepted. Not legal advice: laws on reverse
engineering and modding differ by country. Independent project, not
affiliated with, endorsed by or sponsored by Rockstar Games, Take-Two
Interactive or Microsoft. Asset-format research for the game lives in
[GTA6-Asset-Info](https://github.com/AcelezXYZ/GTA6-Asset-Info).
