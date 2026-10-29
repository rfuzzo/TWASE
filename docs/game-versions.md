# Game versions

Status 2026-10-28. Total War: ATTILA is sold in several builds. TWASE's addresses, struct offsets and patches are made for one exact game binary each, so every build is a separate target, even with the same version number.

## The builds

| Build | Game code | Arch | Build ID | TWASE |
|---|---|---|---|---|
| Windows, Steam | `empire.retail.dll` (+ `Attila.exe`, `steam_api.dll`) | x86 (PE32) | `empire.retail.dll` PDB build ID `D49769ED8EE04D6F8D1ADE5F582D19BB1`, PE timestamp 2026-04-02 | supported |
| Windows, GOG | `empire.retail.dll` | x86 (PE32) | `empire.retail.dll` PDB build ID `38260D892D1D47DEBF34EA9A6420E4C71`, PE timestamp 2026-07-09 | not supported, detected (TWASE stays inactive) |
| Windows, Epic | `empire.retail.dll` [INFERENCE] | x86 [INFERENCE] | unknown, no copy analysed | not supported, detected by the build gate [INFERENCE] |
| Mac, Steam | `Total War ATTILA.app/Contents/MacOS/Total War ATTILA` | arm64 | Feral 1.6.1 RC2, `CFBundleVersion 480285.103778`, `LC_UUID 392D6F66-9183-329E-8A98-8C90FC828326` | supported |
| Mac, App Store | `/Applications/Total War ATTILA.app/Contents/MacOS/Total War ATTILA` | arm64 | Feral 1.6.1 RC4, `CFBundleVersion 480969.104294`, `LC_UUID 196C74A0-71A2-3C28-80DF-BC3DB1C7FEB7` | not supported, TWASE can't be injected |

The Epic store page mentions cross-play "only with GOG at the moment", so an Epic build exists alongside GOG ([store page](https://store.epicgames.com/p/total-war-attila-1b8242)).

## Windows

Since 2026-10-28 the Windows DLL gates on the build like the Mac one: TWASE patches and hooks only the exact `empire.retail.dll` its address tables are made for, and stays inactive on any other.

### Build ID

The Windows build ID is the CodeView record in the PE debug directory of `empire.retail.dll`: the PDB GUID and age, written as the symbol server key (GUID as 32 hex digits, then the age in hex). The linker writes a new GUID on every link, so like the Mac `LC_UUID` it names one exact binary. The record is in `.rdata`, so it is read from the loaded module; no file access is needed.

File version and `SizeOfImage` don't tell the builds apart:

| Value | Steam | GOG |
|---|---|---|
| `empire.retail.dll` file version | 1.6.0.0 | 1.6.0.0 |
| `empire.retail.dll` `SizeOfImage` | `0x2B9C000` | `0x2B9C000` |
| `empire.retail.dll` PE timestamp | `0x69CE1786` (2026-04-02) | `0x6A4F4723` (2026-07-09) |
| `empire.retail.dll` SHA-256 | `8A40CBCF…E2A353D` | `0C0BE767…E0E0DF70` |
| `Attila.exe` file version | 1.6.0.0 | 1.6.7.0 |
| PDB GUID + age | `D49769ED8EE04D6F8D1ADE5F582D19BB1` | `38260D892D1D47DEBF34EA9A6420E4C71` |

### The gate

- `sdk::Attila::Addresses::SupportedBuildId` in `src/platform/win32/sdk/Attila/Addresses.hpp` records the build ID (with file version, PE timestamp, `SizeOfImage` and SHA-256) of the Steam `empire.retail.dll` that table and `Lua/Addresses.hpp` are made for (tested in game on 2026-10-22).
- `Image::ReadBuildId()` in `src/platform/win32/dll/Image.cpp` reads the build ID from a loaded module.
- The watcher thread in `App.cpp` waits for `empire.retail.dll`, logs its build ID and compares it with `SupportedBuildId`. On a mismatch it logs `This game build is not supported (expected empire.retail.dll build ID …)` and applies no patch and no hook, D3D11 and DirectInput hooks included.
- The `App` constructor still logs the `Attila.exe` file and product version, for information only.

When the Steam build changes, the gate turns TWASE off until the tables are checked against the new DLL and `SupportedBuildId` is updated.

Previously analysed Windows binaries (file version 1.6.0.0, PE timestamp 2023-06-18) are older builds. The current table does not fit them: for example, `g_Campaign = 0x28D52B0` lies outside their `SizeOfImage 0x255F000`.

### GOG

GOG release 1.6.7 (GOG build ID `59847060534356892`), analysed 2026-10-28. The install has `steam_api.dll` and `GalaxySteamWrapper.dll` next to `Galaxy.dll`, `EOSSDK-Win32-Shipping.dll` and `clockwork_gog.Release.x86.dll`, so the storefront layer is swapped around a Steam-style API rather than removed. The `winmm.dll` proxy loader doesn't depend on Steam and should load [INFERENCE, not run].

`empire.retail.dll` has the same section layout and `SizeOfImage` as Steam's, but it is a different link and the code is shifted by varying amounts. Bytes at Steam table RVAs, compared with the GOG file:

| Steam address | Steam RVA | Same bytes in GOG at |
|---|---|---|
| `RunStartupPath` | `0x79B980` | `0x79B980` (+0), but the calls inside it point elsewhere |
| `TradeRegionsPanel_Populate` | `0x15FDB80` | `0x15FDBD0` (+0x50) |
| `GetLuaState` | `0x1626AA0` | `0x1626B00` (+0x60) |
| `VFS_SearchFiles` | `0x1635B50` | `0x1635BB0` (+0x60) |
| `LuaLog` | `0x1A76CF0` | `0x1A76D20` (+0x30) |

No single offset maps the Steam table onto GOG; with the Steam table, TWASE would hook `LuaLog` and others in the middle of other code. GOG needs its own table (struct layouts are probably the same, same compiler and arch [INFERENCE]).

### Epic

No copy was analysed. A separately linked `empire.retail.dll` has its own PDB GUID, so the gate keeps TWASE inactive on it [INFERENCE]. It needs its own table too.

## Mac

### Steam (supported)

`Image::IsSupported()` in `src/platform/macos/dylib/Image.cpp` compares the executable's `LC_UUID` with `sdk::Attila::Addresses::SupportedUUID`; on any other build TWASE logs an error and hooks nothing. All Mac docs ([addresses](macos/addresses.md), [anti-tamper](macos/anti-tamper-overview.md)) are for this build.

Injection works because of the Steam build's entitlements (`codesign -dv --entitlements -`):

- `com.apple.security.app-sandbox` = false
- `com.apple.security.cs.allow-dyld-environment-variables` = true, so `DYLD_INSERT_LIBRARIES` from `twase-launch.command` reaches the game
- `com.apple.security.cs.disable-library-validation` = true, so the ad-hoc signed `libTWASE.dylib` may load
- `com.apple.security.cs.allow-unsigned-executable-memory` = true

### App Store

Same version number, but a later release candidate (RC4) and another UUID; the bytes at the same addresses differ ([integrity-and-quirks.md](macos/integrity-and-quirks.md)). Keep App Store and Steam analysis databases separate and identify them by the executable UUID ([re-tooling.md](macos/re-tooling.md)).

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
| Build detection | one supported build ID per OS: Mac `LC_UUID`, Windows `empire.retail.dll` PDB GUID + age; no patches or hooks on any other build | a list of build IDs, each picking its table |
| Addresses | one `constexpr` namespace per OS | one table per build, picked at startup; `constexpr` names become fields of a table the platform hands to the core (or a namespace per build behind one accessor) |
| Struct offsets | per OS (pointer size) | still per OS and arch; only split per build where a layout actually differs |
| Feature flags | `#define TWASE_HAS_*` per OS | per build at run time, since a new build may lack some addresses; unavailable `twase.*` functions must be omitted or report unavailability |
| Resolution | fixed addresses | one verified table per build, or checked signatures that resolve targets at startup; an unknown or ambiguous layout must not be hooked |
| Docs | `docs/macos/addresses.md` is per build | addresses docs keyed by build ID, as they are already headed |

The Mac App Store build adds more than a table: an injection method that works under the sandbox and without the DYLD entitlements, and paths outside a Steam game folder. That is a launcher/loader problem in `src/platform/macos/`, not in the core.
