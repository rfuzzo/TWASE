# Game versions

Status 2026-10-28. Total War: ATTILA is sold in several builds. TWASE's addresses, struct offsets and patches are made for one exact game binary each, so every build is a separate target, even with the same version number.

## The builds

| Build | Game code | Arch | Build ID | TWASE |
|---|---|---|---|---|
| Windows, Steam | `empire.retail.dll` (+ `Attila.exe`, `steam_api.dll`) | x86 (PE32) | file version only (see below) | supported |
| Windows, GOG | `empire.retail.dll` | x86 [INFERENCE] | unknown, no copy analysed | not supported, not detected |
| Windows, Epic | `empire.retail.dll` [INFERENCE] | x86 [INFERENCE] | unknown, no copy analysed | not supported, not detected |
| Mac, Steam | `Total War ATTILA.app/Contents/MacOS/Total War ATTILA` | arm64 | Feral 1.6.1 RC2, `CFBundleVersion 480285.103778`, `LC_UUID 392D6F66-9183-329E-8A98-8C90FC828326` | supported |
| Mac, App Store | `/Applications/Total War ATTILA.app/Contents/MacOS/Total War ATTILA` | arm64 | Feral 1.6.1 RC4, `CFBundleVersion 480969.104294`, `LC_UUID 196C74A0-71A2-3C28-80DF-BC3DB1C7FEB7` | development-copy support; original app is not injected |

The Epic store page mentions cross-play "only with GOG at the moment", so an Epic build exists alongside GOG ([store page](https://store.epicgames.com/p/total-war-attila-1b8242)).

## Windows

### Steam

The fixed RVA table in `src/platform/win32/sdk/Attila/Addresses.hpp` is made for the current Steam `empire.retail.dll` (tested in game on 2026-10-22). Its build ID isn't recorded in the repo yet: write its file version, PE timestamp and SHA-256 at the top of `Addresses.hpp`.

Previously analysed Windows binaries (file version 1.6.0.0, PE timestamp 2023-06-18) are older builds. The current table does not fit them: for example, `g_Campaign = 0x28D52B0` lies outside their `SizeOfImage 0x255F000`. They cannot validate the current Steam RVA table.

### No build check on Windows

The Windows DLL doesn't check which build it runs on:

- `Image::IsSupported()` compares the file version with a list that contains only the file version itself (`Image::GetSupportedVersions()`), and nothing calls it.
- The `App` constructor logs the file and product version; the minimum version check is commented out.
- The watcher thread in `App.cpp` applies the patches and attaches every hook at `empire.retail.dll` base + fixed RVA, whatever DLL is loaded.

On a build whose layout differs from the one the table was made for, TWASE can write patches and hooks to wrong addresses. There is no build gate to prevent that. GOG and Epic binaries have not been analysed, so their compatibility is unknown.

### GOG and Epic

No copy was analysed. What's likely [INFERENCE]: the same CA code with a different storefront layer (no `steam_api.dll`), so a different `empire.retail.dll` with shifted RVAs, while the struct layouts stay those of the x86 build. The `winmm.dll` proxy loader doesn't depend on Steam and should load. Each one needs its own address table, found again in IDA, as for the Mac port.

## Mac

### Steam (supported)

`Image::IsSupported()` in `src/platform/macos/dylib/Image.cpp` accepts only the two executable UUIDs above and the Attila bundle ID. `BuildAddress` stores independently verified Steam/App Store VAs; UUID selection happens before any patch, hook, or game API initializes. Object offsets remain separate integers. Unknown builds get no hooks. Earlier Mac analysis sections are Steam-specific unless headed otherwise.

Injection works because of the Steam build's entitlements (`codesign -dv --entitlements -`):

- `com.apple.security.app-sandbox` = false
- `com.apple.security.cs.allow-dyld-environment-variables` = true, so `DYLD_INSERT_LIBRARIES` from `twase-launch.command` reaches the game
- `com.apple.security.cs.disable-library-validation` = true, so the ad-hoc signed `libTWASE.dylib` may load
- `com.apple.security.cs.allow-unsigned-executable-memory` = true

### App Store

The App Store build has independently mapped addresses and requires a prepared development copy. The stock app
remains vendor-signed and sandboxed, without the DYLD/library-validation entitlements needed by TWASE. It is never
modified or used as the injected target.

`twase-appstore.command --prepare <new.app>` APFS-clones the installed app and receipt to a nonexistent destination,
then ad-hoc signs only the copy with hardened runtime and the three Steam-style injection entitlements. The
development copy is **unsandboxed and no longer vendor signed**. Receipt and licence validation are not patched or
bypassed. The launcher checks the exact App Store UUID, receipt presence, signature, and injection entitlements
before launch; it refuses the installed original, existing preparation destinations, and writable state inside either
app. Updates require a freshly prepared copy and an independently supported build.

There is no Steam dependency or Workshop lookup. The App Store launcher injects only TWASE and sets both `HOME`
and `CFFIXED_USER_HOME` to `<launcher directory>/TWASE/appstore-home`. Feral preferences, saves, VFS and local mods
are created there, separate from both the installed App Store container and Steam user data. TWASE config/logs are
in `<launcher directory>/TWASE/` (`TWASE_ROOT` can select another existing launcher root).

The app's read-only game data lives at `Contents/Resources/Data/data`; pack mods belong in the private Feral
`VFS/Local/local_mods/<mod>/` directory and are enabled through Feral's manager. Do not modify the signed bundle.
After an initial normal launch creates private preferences, `--skip-launcher` and `--load` use only that private
registry and restore their overrides after exit. Existing saves must be copied into the private save directory.
The Steam Workshop timing/order hook is not installed on the App Store build.

Static analysis matched all 111 native address declarations and four clean-text anchor locations independently.
Full routine comparison preserves object-field offsets, normalizing only PC-relative references and proven
ADRP-derived global addresses; data globals, vtables and the changed write-probe routine have separate evidence.
Build-specific Lua-sink, encyclopedia-load and integrity-call instructions are checked before patching.
See [App Store evidence](macos/addresses.md#app-store-arm64-rc4).

## Address-layer contract

Windows still uses its fixed RVA table. macOS now uses `BuildAddress{steamVA, appStoreVA}` for executable addresses,
selected once by exact UUID. Constant-initialized hooks retain references to these pairs rather than resolving
addresses during global initialization; the shared database accessor list likewise keeps the platform address type.
Object offsets and calling conventions are shared only where supported by the mapped routine evidence and runtime
smokes. No signature scanner runs in the game, and no constant-delta adjustment is used between builds.

The cheapest first step, independent of new builds: add the build check on Windows (record the ID the table was made for, compare it in `App.cpp` before patching), so TWASE stays inactive on GOG, Epic or a Steam update instead of patching wrong addresses.
