# ScriptHookVI

Base framework for a ScriptHookV-style script host for GTA VI: a plugin
loader, a cooperative script runtime and a native-call interface behind a
small C API.

**Status: skeleton.** It builds, but it does **not attach to the game yet**.
Everything that depends on the game's internals is read from byte patterns
in `ScriptHookVI.ini`, and **no patterns are shipped** because none have been
verified for GTA VI (the format notes in
[GTA6-Asset-Info](https://github.com/AcelezXYZ/GTA6-Asset-Info) cover assets,
not the executable). Until `ScriptTick` is configured the log says
`hook NOT attached` and no script runs. Nothing here was run against the game.

## How it fits together

```
game.exe ── loads ──> dinput8.dll (proxy)          src/proxy/
                        └─ LoadLibrary ──> ScriptHookVI.dll
                              dllmain.cpp   bootstrap thread: ini, hook, input, plugins
                              game/         pattern -> addresses, ScriptTick detour
                              runtime/      fibers (scripts), native invoker, keyboard, *.asi loader
                              exports.cpp   the public C API
plugin.asi ── scriptRegister(module, main) ─> run as fiber on the game's script thread
```

* **Loader**: `dinput8.dll` forwards `DirectInput8Create` and loads
  `ScriptHookVI.dll`. Assumption: the game imports `dinput8.dll`. If not, use
  another proxy DLL or injector; the core does not care how it is loaded.
* **Script runtime**: every script is a Windows fiber. `scriptWait(ms)`
  switches back to the host; the hook resumes it when the time is up. Scripts
  run on one thread only (the first thread seen in the hooked function).
* **Natives**: `nativeInit` / `nativePush64` / `nativeCall`, typed through
  `invoke<R>(hash, args...)` in `sdk/inc/nativeCaller.h`. Handlers come from
  the `GetNativeHandler` pattern.
* **Plugins**: any `*.asi` in the game folder or `plugins/`.

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

Output in `build/out/`: `ScriptHookVI.dll`, `dinput8.dll`, `hello.asi`
(sample). Copy them and `config/ScriptHookVI.ini` next to the game
executable. `ScriptHookVI.log` is written beside the DLL.

## Writing a plugin

Include `sdk/inc/main.h`, link `ScriptHookVI`, build a DLL, rename it `.asi`.
See [samples/hello/hello.cpp](samples/hello/hello.cpp). API:
`scriptRegister/-Unregister*`, `scriptWait`, `keyboardHandlerRegister`,
`nativeInit/Push64/Call`, `getGlobalPtr`, `getGameExeTimestamp`,
`shviIsAttached`.

## What is still needed to make it work

Each item is one entry in `[Patterns]` (see the ini) or a verification:

| Item | Needed for | Assumption in the code |
|------|-----------|------------------------|
| `ScriptTick` | a per-frame function on the script thread | takes ≤ 4 integer args; first ≥ 14 bytes are position independent |
| `GetNativeHandler` | native calls | `fn(hash) -> void(*)(NativeContext*)` |
| `NativeContext` layout, vector write-back | native calls | same as earlier RAGE titles; unverified (`runtime/native_invoker.h`) |
| Native hashes / names | writing real scripts | none known |
| `GlobalBase` | `getGlobalPtr` | 64 pages × 0x40000 slots |
| `OnlineFlag` | safety guard | one byte, non-zero online |
| Window/keyboard | keyboard handlers | normal top-level window of the process |

Not included yet: graphics backends (see below), a native name/hash database,
hot-reload of plugins, a .NET/Lua host.

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

* A D3D12 and/or Vulkan backend (`IRenderBackend`): hooking the API, finding
  the pre-UI boundary in the game's renderer, locating the motion vector and
  depth targets, and copying them for history. This needs reverse engineering
  of the game and knowing which graphics API the PC build uses.
* Whether the game writes usable motion vectors at all.
* The exact input requirements of the vendor SDKs (e.g. DLSS 5), which I have
  not seen. Do not treat the current struct layout as final.

Run the logic tests natively:

```bash
g++ -std=c++20 -Isdk/inc -Isrc/render tests/render_manager_test.cpp src/render/render_manager.cpp -o rmt && ./rmt
```

## Safety

Single-player modding tool. Scripts are paused while the game reports an
online session, and by default (`RequireOnlineGuard=1`) the hook refuses to
attach when it cannot detect that. There is no anti-cheat bypass here and
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
