# TWASE Launcher (macOS)

A small SwiftUI front end for the existing shell backends in `../launcher/`, so Mac players do not need Terminal.
It selects the installation, prepares the private App Store copy, plays, links selected mod packs into the private
copy, copies saves on request, and opens the data folders. It is not a mod manager: no Workshop, pack editing,
dependency resolution or profiles.

## Build

```sh
src/platform/macos/app/build-app.sh --dylib build/macosx/arm64/release/libTWASE.dylib --output <dir> [--version x.y.z]
```

Produces `<dir>/TWASE Launcher.app` (arm64, macOS 14+, ad-hoc signed). Needs Xcode or the Command Line Tools (Swift
6.0 or newer); there is no Xcode project. The sources are compiled with `-swift-version 5` so that older Swift 6
compilers on CI accept them, but they also type-check in Swift 6 mode. `--version` defaults to `TWASE_VERSION_STR`
from `src/core/Version.hpp` and is stored as `TWASEVersion` (and the bundle versions) in `Info.plist`.

The bundle's Resources contain `twase-appstore.command`, `twase-launch.command` and `libTWASE.dylib` (signed
separately, because `codesign --deep` does not sign files in Resources).

## Behaviour

- **Installations**: Steam (`~/Library/Application Support/Steam/steamapps/common/Total War Attila/`), the App Store
  original (`/Applications/Total War ATTILA.app`), prepared copies (`~/Applications/Total War ATTILA*.app`), and any
  `.app` chosen by the user. They are classified by the LC_UUID of the arm64 slice of the game executable (parsed in
  Swift) and, for App Store builds, by `codesign -dv` reporting `Signature=adhoc` (prepared copy). Choices are kept
  in the app's user defaults (`io.github.rfuzzo.twase.launcher`).
- **Backend**: at every start, the bundled scripts and dylib are installed into the TWASE root
  `~/Library/Application Support/TWASE/TWASE/` when they differ (copy to a temporary file, then `rename`, never
  overwritten in place). The installed version is recorded in `TWASE/launcher-backend-version`. Where the launcher
  itself runs from (including App Translocation) does not matter. For Steam, `twase-launch.command` and
  `TWASE/libTWASE.dylib` are installed into the game folder only when the user presses *Install/Update TWASE*.
- **App Store**: *Prepare private copy* runs `twase-appstore.command --prepare` (default destination
  `~/Applications/Total War ATTILA (TWASE).app`). *Play* runs `twase-appstore.command [--skip-launcher] -- <copy>`
  with `TWASE_ROOT` set to the TWASE root.
- **Steam**: *Play* runs `<game folder>/twase-launch.command [--skip-launcher]` without Steam variables, so the
  backend starts the game directly. A warning is shown if `steam_osx` is not running.
- **Mods (App Store only)**: lists `*.pack` directly in, or one folder below, each source (Feral's normal
  `VFS/Local/local_mods` plus user-added folders, e.g. the Steam game's `TotalWarAttilaData/data`). *Link selected*
  symlinks the resolved pack to `<private home>/…/VFS/Local/local_mods/<folder the pack is in>/<pack>`; existing
  different entries are never replaced, and *Unlink* only removes symbolic links. Packs are enabled and ordered in
  Feral's Mod Manager.
- **Saves (App Store only)**: copies selected saves from Feral's normal save folder into the private one, never
  overwriting.
- **Quarantine:** the backend files the launcher installs (its own sealed, signed resources, which the user already
  approved by opening the launcher) have the download quarantine flag cleared on the installed copies. Any other
  quarantined file, e.g. a manually replaced dylib or a prepared copy, is refused before launch with the exact
  `xattr` command to run if the user trusts it.

Folders are only created below the TWASE root, except `~/Applications` (created when preparing to the default
destination) and the Steam game's `TWASE` folder (explicit install).

## Development

`TWASE_LAUNCHER_ROOT=<dir>` replaces the TWASE root, so a test run does not touch the real one:

```sh
open -n --env TWASE_LAUNCHER_ROOT=/tmp/twase-root "<dir>/TWASE Launcher.app"
```

Sources are in `Sources/`: `AppModel.swift` (main-actor state and actions), the views, and plain helpers for
paths (`Paths.swift`), Mach-O parsing, installation inspection, backend installation, process streaming, mod
linking and save copying.
