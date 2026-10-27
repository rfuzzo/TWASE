# Game versions

Status 2026-10-28. Total War: ATTILA is sold in several builds. TWASE's addresses, struct offsets and patches are made for one exact game binary each, so every build is a separate target, even with the same version number.

## The builds

| Build | Game code | Arch | Build ID | TWASE |
|---|---|---|---|---|
| Windows, Steam | `empire.retail.dll` (+ `Attila.exe`, `steam_api.dll`) | x86 (PE32) | file version only (see below) | supported |
| Windows, GOG | `empire.retail.dll` | x86 [INFERENCE] | unknown, no copy analysed | not supported, not detected |
| Windows, Epic | `empire.retail.dll` [INFERENCE] | x86 [INFERENCE] | unknown, no copy analysed | not supported, not detected |
| Mac, Steam | `Total War ATTILA.app/Contents/MacOS/Total War ATTILA` | arm64 | Feral 1.6.1 RC2, `CFBundleVersion 480285.103778`, `LC_UUID 392D6F66-9183-329E-8A98-8C90FC828326` | supported |
| Mac, App Store | `/Applications/Total War ATTILA.app/Contents/MacOS/Total War ATTILA` | arm64 | Feral 1.6.1 RC4, `CFBundleVersion 480969.104294`, `LC_UUID 196C74A0-71A2-3C28-80DF-BC3DB1C7FEB7` | not supported, TWASE can't be injected |

The Epic store page mentions cross-play "only with GOG at the moment", so an Epic build exists alongside GOG ([store page](https://store.epicgames.com/p/total-war-attila-1b8242)).

## Windows

### Steam

The fixed RVA table in `src/platform/win32/sdk/Attila/Addresses.hpp` is made for the current Steam `empire.retail.dll` (tested in game on 2026-10-22). Its build ID isn't recorded in the repo yet: write its file version, PE timestamp and SHA-256 at the top of `Addresses.hpp`.

The copies on the Mac (the CrossOver bottle and `~/at/Total War Attila`, file version 1.6.0.0, PE timestamp 2023-06-18) are older builds. That is why the table doesn't fit them (e.g. `g_Campaign = 0x28D52B0` lies outside their `SizeOfImage 0x255F000`), and why the RVAs in [windows-port.md](windows-port.md#binary-evidence-and-resolution) are of an older image.

### No build check on Windows

The Windows DLL doesn't check which build it runs on:

- `Image::IsSupported()` compares the file version with a list that contains only the file version itself (`Image::GetSupportedVersions()`), and nothing calls it.
- The `App` constructor logs the file and product version; the minimum version check is commented out.
- The watcher thread in `App.cpp` applies the patches and attaches every hook at `empire.retail.dll` base + fixed RVA, whatever DLL is loaded.

So on any build other than the one the table was made for (GOG, Epic, another Steam update), TWASE writes patches and hooks to wrong addresses. Only the campaign UI controls (`Hooks/CampaignUI.cpp`) resolve their targets by searching the image and refuse an unknown layout.

### GOG and Epic

No copy was analysed. What's likely [INFERENCE]: the same CA code with a different storefront layer (no `steam_api.dll`), so a different `empire.retail.dll` with shifted RVAs, while the struct layouts stay those of the x86 build. The `winmm.dll` proxy loader doesn't depend on Steam and should load. Each one needs its own address table, found again in IDA, as for the Mac port.

## Mac

### Steam (supported)

`Image::IsSupported()` in `src/platform/macos/dylib/Image.cpp` compares the executable's `LC_UUID` with `sdk::Attila::Addresses::SupportedUUID`; on any other build TWASE logs an error and hooks nothing. All Mac docs ([addresses](macos/addresses.md), [anti-tamper](macos/anti-tamper-overview.md)) are for this build.

Injection works because of the Steam build's entitlements (`codesign -dv --entitlements -`):

- `com.apple.security.app-sandbox` = false
- `com.apple.security.cs.allow-dyld-environment-variables` = true, so `DYLD_INSERT_LIBRARIES` from `twase-launch.command` reaches the game
- `com.apple.security.cs.disable-library-validation` = true, so the ad-hoc signed `libTWASE.dylib` may load
- `com.apple.security.cs.allow-unsigned-executable-memory` = true

### App Store

Same version number, but a later release candidate (RC4) and another UUID; the bytes at the same addresses differ ([integrity-and-quirks.md](macos/integrity-and-quirks.md)). The Binary Ninja database in `~/GitHub/open-twa/bn/` is this build ([re-tooling.md](macos/re-tooling.md)).

Differences that matter for TWASE:

- **Injection is blocked.** Both builds have the hardened runtime (`flags=0x10000(runtime)`), but the App Store build has the sandbox on and neither `allow-dyld-environment-variables` nor `disable-library-validation`. dyld drops `DYLD_INSERT_LIBRARIES`, and a dylib not signed by Feral's team (`XH73Y9BGP8`) isn't allowed to load. Getting in would mean re-signing the app with changed entitlements; whether the game still starts then (App Store receipt in `Contents/_MASReceipt`, Feral's code signature checks, `CodeSign_CheckSelf_*` in the Steam build) is untested.
- **No Steam.** No `libsteam_api.dylib` in `Contents/Frameworks`, so no Workshop folder and no `%command%` launch options; `twase-launch.command` assumes Steam (`SteamAppId`, `STEAM_DYLD_INSERT_LIBRARIES`, the Workshop pack lookup).
- **Sandboxed paths.** Feral's preferences and VFS are in `~/Library/Containers/com.feralinteractive.attilatw/Data/Library/Application Support/Feral Interactive/Total War ATTILA/` instead of `~/Library/Application Support/Feral Interactive/Total War ATTILA/`. The game is in `/Applications`, so there's no Steam game folder for `TWASE/` (config, logs) next to the `.app`, and the sandbox limits where the game may write.
- **Addresses.** Every address in `src/platform/macos/sdk/Attila/Addresses.hpp` would have to be found again. Struct layouts are probably the same as the Steam build (same version, same arch) [INFERENCE].

## What this means for the repo

Today a platform and a build are the same thing: `src/platform/<os>/sdk/Attila/Addresses.hpp` is one `constexpr` table per OS, `Features.hpp` sets compile-time `TWASE_HAS_*` flags per OS, and `src/core` uses both by name. That holds as long as each OS has exactly one supported build. It doesn't change the core/platform split ([development.md](development.md)): the shared logic, the console and the `twase.*` API stay as they are.

Supporting a second build on the same OS changes the address layer:

| Concern | Now | With several builds per OS |
|---|---|---|
| Build detection | Mac: `LC_UUID` gate. Windows: none | Both: an exact build ID (Mac `LC_UUID`; Windows PE timestamp + `SizeOfImage`, or a hash of `empire.retail.dll`), no hooks on an unknown build |
| Addresses | one `constexpr` namespace per OS | one table per build, picked at startup; `constexpr` names become fields of a table the platform hands to the core (or a namespace per build behind one accessor) |
| Struct offsets | per OS (pointer size) | still per OS and arch; only split per build where a layout actually differs |
| Feature flags | `#define TWASE_HAS_*` per OS | per build at run time, since a new build may lack some addresses; the `twase.*` functions then report unavailable, like the campaign UI setters do |
| Resolution | fixed addresses; `CampaignUI.cpp` searches the image | fixed tables are cheaper to make per build; searching the image (as `CampaignUI.cpp` does) covers several builds at once, at the cost of a checked signature per target |
| Docs | `docs/macos/addresses.md` is per build | addresses docs keyed by build ID, as they are already headed |

The Mac App Store build adds more than a table: an injection method that works under the sandbox and without the DYLD entitlements, and paths outside a Steam game folder. That is a launcher/loader problem in `src/platform/macos/`, not in the core.

The cheapest first step, independent of new builds: add the build check on Windows (record the ID the table was made for, compare it in `App.cpp` before patching), so TWASE stays inactive on GOG, Epic or a Steam update instead of patching wrong addresses.
