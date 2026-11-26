# Integrating ScriptHookVI into VIctory

How to get VIctory to load ScriptHookVI (and with it, mods), written so you can
hand it to a Claude Code session that works **in the VIctory repository**.
Do the two steps as two separate pull requests.

Facts this guide relies on (checked against VIctory `main` of 2026-11-26):

* `apps/victory/src/run.rs` maps the title (`victory_loader::load`), binds
  imports, then runs it with `spawn_main`. `image.base()` is the mapped base,
  `image.info().size_of_image` its size, `hash` (BLAKE3 hex of the original
  executable) its build identity (`victory_game::identify`).
* `crates/victory-hle/src/modules/fidelityfx.rs` already loads a PC DLL
  (`LoadLibraryExW`, `GetProcAddress`) and intercepts the title's FidelityFX
  `ffxDispatch`, whose description holds colour, depth, motion vectors, jitter,
  motion-vector scale, sizes, reset flag and camera values.
* The title is offline by design, so ScriptHookVI's online guard is not needed
  (`SHVI_HOST_OFFLINE`).

ScriptHookVI's host API is `sdk/inc/host.h` in
<https://github.com/AcelezXYZ/ScripthookVI>. Plain C types.

---

## Step 0: build ScriptHookVI (on Windows)

```bash
git clone https://github.com/AcelezXYZ/ScripthookVI
cd ScripthookVI
cmake -S . -B build -A x64
cmake --build build --config Release
```

Create a folder outside the VIctory repo (asset policy), e.g.
`D:\victory-data\scripthook\`, and put there `ScriptHookVI.dll` and a copy of
`config/ScriptHookVI.ini`. Plugins go into `plugins\` below it (`*.asi`).

Note: without `ScriptTick` / `GetNativeHandler` / `GlobalBase` for the title
build, a plugin's `DllMain` still runs, but no script ticks and no native
works. Those need per-build offsets in `ScriptHookVI.ini` (`.Rva`), which only
you can find; they belong in your ini, not in a repository.

---

## Step 1: loader and attach (PR 1)

Paste this to Claude in the VIctory repo:

> Read `docs/asset-policy.md`, `crates/victory-hle/src/modules/fidelityfx.rs`
> and `apps/victory/src/run.rs`. Add optional ScriptHookVI support.
>
> 1. **Config** (`crates/victory-core/src/config.rs`): a new optional path
>    `scripthook_dir`, env `VICTORY_SCRIPTHOOK_DIR`, key `[paths].scripthook_dir`,
>    exactly like `fidelityfx_dir` (field, `FilePaths`, `load`, tests, the
>    `victory.example.toml` comment, and a line in `victory config` in
>    `apps/victory/src/main.rs`). It must pass the asset-policy "outside the
>    repo" check like the other paths.
> 2. **Module** `crates/victory-hle/src/scripthook.rs` (`#[cfg(windows)]`, listed
>    in `modules/mod.rs` or `lib.rs` like its neighbours; follow the style of
>    `fidelityfx.rs`: `LoadLibraryExW` with `LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR |
>    LOAD_LIBRARY_SEARCH_SYSTEM32`, a full path, `GetProcAddress`). Load
>    `ScriptHookVI.dll` from the configured folder and resolve `shviAttach` and
>    `shviDetach`. Define the C struct and assert its layout:
>
>    ```rust
>    #[repr(C)]
>    struct ShviAttachInfo {
>        struct_size: u32,          // size_of::<ShviAttachInfo>()
>        flags: u32,                // 1 = SHVI_HOST_OFFLINE
>        image_base: usize,
>        image_size: usize,
>        build_id: *const c_char,   // NUL-terminated BLAKE3 hex, may be null
>        config_dir: *const u16,    // NUL-terminated UTF-16, may be null
>        game_dir: *const u16,      // NUL-terminated UTF-16, may be null
>    }
>    const _: () = assert!(size_of::<ShviAttachInfo>() == 48);
>    ```
>
>    `pub fn attach(dir: &Path, base: usize, size: usize, build_id: &str)`:
>    load the DLL, call `shviAttach` with `flags = 1`, `config_dir = dir`,
>    `game_dir = dir` (plugins are then in `dir\plugins`), log the returned
>    value (1 = hooks installed, 0 = not, reason in `ScriptHookVI.log`). Any
>    failure is a `tracing::warn!` and VIctory carries on without it; never
>    fail the run because of ScriptHookVI. Keep the module handle alive for
>    the process lifetime (never unload).
> 3. **Call site** (`apps/victory/src/run.rs`): after the `if !execute {
>    return Ok(0) }` check and before `spawn_main`, only when
>    `cfg.scripthook_dir` is set: `scripthook::attach(dir, image.base(),
>    image.info().size_of_image as usize, &hash)`. It must run after the image
>    is mapped and **before the title's entry point runs**. Never from a
>    `DllMain`.
> 4. Do not add any game data, offsets or patterns to the repository. Run
>    `cargo fmt`, `cargo clippy --workspace`, `cargo test --workspace`,
>    `cargo xtask policy`, and `cargo check --target x86_64-pc-windows-msvc`.
>    Add unit tests for what runs without the DLL (config parsing, the struct
>    layout, the "no folder / no DLL means VIctory runs as before" path).
> 5. Update the README (Getting started: the new variable and what it does).

Accept when: with `VICTORY_SCRIPTHOOK_DIR` unset nothing changes; with it set
and the DLL present, `ScriptHookVI.log` in that folder shows `target image ...`,
the build id, and the plugins it loaded.

---

## Step 2: render feed for upscalers / frame generation (PR 2)

Needs ScriptHookVI built from branch `claude/render-host-feed` or later
(exports `shviRender*`). Paste this to Claude:

> Read `crates/victory-hle/src/modules/fidelityfx.rs`,
> `crates/victory-gpu/src/external.rs` and ScriptHookVI's `sdk/inc/host.h` and
> `sdk/inc/render_types.h`. Feed the title's FidelityFX upscale dispatches to
> ScriptHookVI so plugins can read the current and previous frame's colour,
> motion vectors and depth before the UI pass.
>
> * Resolve the `shviRender*` exports in `scripthook.rs` (they are optional:
>   an older DLL without them just disables the feed). Register a backend with
>   `shviRenderSetBackend` (api = 1 for D3D12, a `copyForHistory` callback). It
>   returns 0 unless the user's `ScriptHookVI.ini` has `[Render] Enabled=1`.
> * In `ffxDispatch` (only on the path where the host's DLL serves, after the
>   resources were converted to host textures): build `ShviFrameInfo` from
>   `DispatchUpscale::rest` (indices: 0-1 jitter f32, 2-3 motion-vector scale
>   f32, 4-5 render size, 6-7 upscale size, 8 sharpen flag, 9 sharpness, 10
>   frame time, 11 pre-exposure, 12 reset, 13 near, 14 far, 15 vertical fov,
>   16 view-space-to-metres, 17 flags). `frameIndex` is a counter you keep,
>   `resetHistory` is `rest[12] != 0`, `api = 1`, matrices all zero. Then:
>   `shviRenderBeginFrame`, `shviRenderPublish` for colour (slot 0), motion
>   vectors (slot 1), depth (slot 2) with `nativeHandle` = the host
>   `ID3D12Resource*`, `shviRenderStage(0)` (PRE_UI), the existing host
>   dispatch, `shviRenderEndFrame()`.
> * `copyForHistory`: allocate a default-heap texture with the source's
>   description (cache one per slot, recreate when the description changes),
>   initial state `COPY_DEST`; in the title's host list (use
>   `victory_gpu::external::record` with the list of the current dispatch, kept
>   in a thread-local while the dispatch runs): barrier from the texture's
>   current state to `COPY_SOURCE`, `CopyResource`, barrier back to the state it
>   had, so what the list records next finds it where it expects. The copy
>   stays in `COPY_DEST` between frames; document that a plugin must
>   transition it itself.
> * This code cannot be run here and touches the GPU. Gate it behind the
>   ScriptHookVI ini switch, log once on first use (formats, sizes), never
>   panic or fail the dispatch because of the feed (on any error: skip the
>   feed for that frame), and say in the PR description that the D3D12 part
>   is unverified on real hardware. Add unit tests for the `rest` decoding and
>   struct layouts.
> * Not covered yet, say so in the README: POST_UI and PRESENT stages, view
>   projection matrices, Series X mode if it does not use FidelityFX, and frame
>   generation (VIctory refuses non-upscale FidelityFX dispatches).

Accept when: with a test plugin that registers a `PRE_UI` callback
(`renderCallbackRegister`), the callback fires once per upscale dispatch,
`renderGetResource(SHVI_RES_COLOR, SHVI_HISTORY_PREVIOUS, ...)` succeeds from
the second frame on, and `resetHistory` is 1 on the first frame.

---

## Checking it without the game

* ScriptHookVI: `tests/run-e2e.sh` (needs MinGW-w64 and Wine) runs a stand-in
  title, a plugin and a host that plays VIctory's role. It proves the host
  interface works; it cannot prove the real title.
* VIctory: `cargo test --workspace` needs no game data.
* With the game: start `victory run <exe> --execute` with
  `VICTORY_SCRIPTHOOK_DIR` set and read `ScriptHookVI.log`. Expect
  `ScriptTick: not configured` until you have found its offset. Do not enter
  the world on a real GPU without knowing the display-driver-reset warning in
  VIctory's README.

## Safety

A `*.asi` is arbitrary native code with the rights of the VIctory process.
There is no sandbox; load only mods you trust. ScriptHookVI exposes no file API
today. If a scripting layer is added later, confine its file access to the mod
folder (canonical paths, no `..`, no symlink escapes, an extension allowlist).
