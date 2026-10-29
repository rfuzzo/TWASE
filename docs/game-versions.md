# Game versions

Status 2026-10-29. Total War: ATTILA is sold in several builds. TWASE's addresses, struct offsets and patches are made for one exact game binary each, so every build is a separate target, even with the same version number.

## The builds

| Build | Game code | Arch | Build ID | TWASE |
|---|---|---|---|---|
| Windows, Steam | `empire.retail.dll` (+ `Attila.exe`, `steam_api.dll`) | x86 (PE32) | `empire.retail.dll` PDB build ID `D49769ED8EE04D6F8D1ADE5F582D19BB1`, PE timestamp 2026-04-02 | supported |
| Windows, GOG | `empire.retail.dll` | x86 (PE32) | `empire.retail.dll` PDB build ID `38260D892D1D47DEBF34EA9A6420E4C71`, PE timestamp 2026-07-09 | supported |
| Windows, Epic | `empire.retail.dll` [INFERENCE] | x86 [INFERENCE] | unknown, no copy analysed | not supported, detected by the build gate [INFERENCE] |
| Mac, Steam | `Total War ATTILA.app/Contents/MacOS/Total War ATTILA` | arm64 | Feral 1.6.1 RC2, `CFBundleVersion 480285.103778`, `LC_UUID 392D6F66-9183-329E-8A98-8C90FC828326` | supported |
| Mac, App Store | `/Applications/Total War ATTILA.app/Contents/MacOS/Total War ATTILA` | arm64 | Feral 1.6.1 RC4, `CFBundleVersion 480969.104294`, `LC_UUID 196C74A0-71A2-3C28-80DF-BC3DB1C7FEB7` | development-copy support; original app is not injected |

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

- `src/platform/win32/sdk/Attila/Build.hpp` has the known builds (`Build { Unknown, Steam, GOG }`), their build IDs (`SteamBuildId`, `GogBuildId`, each with file version, PE timestamp, `SizeOfImage` and SHA-256) and `BuildAddress { steam, gog }`, the same shape as the Mac `Build.hpp` (see [Address layer](#address-layer)).
- `Image::SelectBuild()` in `src/platform/win32/dll/Image.cpp` reads the build ID of the loaded `empire.retail.dll` and selects the build; `Image::IsSupported()` is true only for a known build.
- The watcher thread in `App.cpp` waits for `empire.retail.dll`, selects the build and logs its build ID. On an unknown build it logs `This game build is not supported (expected empire.retail.dll build ID … (Steam) or … (GOG))` and applies no patch and no hook, D3D11 and DirectInput hooks included.
- The `App` constructor still logs the `Attila.exe` file and product version, for information only.

When the Steam or GOG build changes, the gate turns TWASE off on it until the tables are checked against the new DLL and its build ID and addresses are updated.

Previously analysed Windows binaries (file version 1.6.0.0, PE timestamp 2023-06-18) are older builds. The current table does not fit them: for example, `g_Campaign = 0x28D52B0` lies outside their `SizeOfImage 0x255F000`.

### GOG

GOG release 1.6.7 (GOG build ID `59847060534356892`). The install has `steam_api.dll` and `GalaxySteamWrapper.dll` next to `Galaxy.dll`, `EOSSDK-Win32-Shipping.dll` and `clockwork_gog.Release.x86.dll`, so the storefront layer is swapped around a Steam-style API rather than removed. There is no Steam Workshop, so subscribed Workshop mods don't appear; pack mods started through a mod manager (Runcher, `mod_list.txt`) load. The `winmm.dll` proxy loader loads TWASE.

Tested in game on 2026-10-29 with the TDD and FE mods: TWASE selects the GOG build and initializes; the Lua console, diplomacy (likelihood, region trading) and the tweaks work.

`empire.retail.dll` has the same section layout and `SizeOfImage` as Steam's, but it is a different link. The GOG column of the address tables was mapped on 2026-10-29 from the Steam table:

- **Functions (93 entries):** each Steam function body (up to its first `ret`) was searched in the GOG `.text` with call/jump targets and relocated addresses masked, the rest compared byte for byte, so field offsets used inside the bodies are identical. 78 match exactly once. The other 15 are short or duplicated bodies (`String_dtor`/`WString_dtor`, one-call thunks such as `DiplomaticAction_FactionA`); each was picked as the candidate at the shift of its neighbouring entries, and the picks agree with what the matched functions call. All 243 call targets and globals referenced from the matched bodies map consistently between the builds. The GOG IDA database has a function start at every mapped address.
- **Patch site:** `BitSetCrashAddr` is unchanged and holds the bytes `Patches.cpp` expects.
- **Data (`g_RuntimeLuaListHead`, `g_RuntimeLuaListSentinel`, `g_Campaign`):** unchanged; every uniquely matched referencing instruction (2, 6 and 1,761) points at the same RVA.
- **Run-time targets:** the `CampaignUI` search, run offline on the GOG file, resolves the same encyclopedia pointer (`0x1D33D00`), settlement `Initialize` (`0x1407530`) and UI helpers as on Steam.

Only 8 entries differ; the code is shifted in blocks, not by one offset:

| Entry | Steam RVA | GOG RVA |
|---|---|---|
| `DiplomacyListTradeRegionsButton_Setup` | `0x15E2340` | `0x15E2380` |
| `TradeRegionsPanel_OnClick` | `0x15E6590` | `0x15E65D0` |
| `TradeRegionsPanel_Populate` | `0x15FDB80` | `0x15FDBD0` |
| `TradeRegionsPanel_SelectRow` | `0x1606DC0` | `0x1606E10` |
| `GetLuaState` | `0x1626AA0` | `0x1626B00` |
| `VFS_SearchFiles` | `0x1635B50` | `0x1635BB0` |
| `VFS_GetInstance` | `0x1658C80` | `0x1658CD0` |
| `LuaLog` | `0x1A76CF0` | `0x1A76D20` |

Object offsets, vtable slots and sizes are shared with Steam.

### Epic

No copy was analysed. A separately linked `empire.retail.dll` has its own PDB GUID, so the gate keeps TWASE inactive on it [INFERENCE]. It needs its own table too.

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

TWASE Launcher (`src/platform/macos/app/`) drives the same script with `TWASE_ROOT=~/Library/Application Support/TWASE`,
so the private home is `~/Library/Application Support/TWASE/TWASE/appstore-home`. Its Mods tab symlinks selected packs
from Feral's normal `local_mods` (or user-added folders) into the private `local_mods`; Feral's Mod Manager then lists
them. Double-clicking a prepared copy runs it unsandboxed without TWASE, against the normal shared Feral folder.

Static analysis matched all 111 native address declarations and four clean-text anchor locations independently.
Full routine comparison preserves object-field offsets, normalizing only PC-relative references and proven
ADRP-derived global addresses; data globals, vtables and the changed write-probe routine have separate evidence.
Build-specific Lua-sink, encyclopedia-load and integrity-call instructions are checked before patching.
See [App Store evidence](macos/addresses.md#app-store-arm64-rc4).

## Address layer

Both platforms use the same shape, one `sdk/Attila/Build.hpp` per OS:

- `enum class Build` lists the supported builds of that OS plus `Unknown`; each build has an exact build ID (Mac `LC_UUID`, Windows `empire.retail.dll` PDB GUID + age).
- `Image` selects the build by exact ID before any patch, hook or game API initializes. Unknown builds get no patches and no hooks.
- Every executable address in `Addresses.hpp` and `Lua/Addresses.hpp` is a `BuildAddress` with one field per build (Mac `{steam, appStore}` VAs, Windows `{steam, gog}` RVAs). It converts to the selected build's address, `0` while the build is `Unknown`. Object offsets, vtable slots and sizes stay plain integers.
- Global hooks are initialized before the build is selected, so `Hook` keeps a reference to its `BuildAddress` and converts it on `Attach`. Windows `Hook` also takes a plain RVA for targets found at run time (`CampaignUI`).
- Shared core code keeps the platform's address type (`decltype(Addr::DB_BuildingLevels)` in `LuaApi.cpp`), so it converts at use, not at static initialization.

A new build adds a field to `BuildAddress` and a value to every address. A build is only supported with a complete, verified table: there are no runtime feature flags for builds that lack some addresses (`Features.hpp` stays compile-time per OS). No signature scanner runs in the game, and no constant offset is applied between builds. Object offsets and calling conventions are shared between builds of an OS only where the mapped routines and runtime smokes support it; they would be split per build where a layout actually differs. Addresses docs are keyed by build ID.
