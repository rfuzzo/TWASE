# Development

## Layout

```text
xmake.lua                    shared settings, includes the platform of the build
src/core/                    shared by both platforms: Config, Paths.hpp, Version.hpp, the console (UI/), Platform.hpp
src/platform/win32/          Windows: TWASE.dll (dll/), the winmm.dll proxy loader (loader/), sdk/ (32-bit Attila.exe)
src/platform/macos/          Mac: libTWASE.dylib (dylib/), twase-launch.command (launcher/), sdk/ (arm64 Feral port)
docs/macos/                  history of the macOS port and its addresses
patches/                     binary patches
```

The core is compiled into each platform's library. It includes the platform's headers by name (`App.hpp`, `Hooks/...`, `Attila/...` from the platform's `sdk/`), so both platforms provide them with the same interface, and it calls the platform only through `Platform.hpp` (message boxes, exit), which each platform implements in its `Platform.cpp`. Everything bound to the game binary (addresses, struct layouts, hooks) and to the OS (injection, renderer and input hooks) stays in the platform folders.

## Building

### Windows

- Clone the repo to some folder
- Run `xmake`
- Run `xmake project -k vsxmake -m "debug,release"` to create a VS solution

The build copies `TWASE.dll` (and its PDB) to `<game directory>/TWASE` and `winmm.dll` to the game directory. The game directory is set in `src/platform/win32/xmake.lua` and can be overridden with the `TWASE_GAMEROOT` environment variable. `xmake run` starts `Attila.exe`.

### Mac

- Install the Xcode command line tools and xmake (`brew install xmake`)
- Run `xmake`

The build copies `libTWASE.dylib` to `<game folder>/TWASE` and `twase-launch.command` to the game folder (default `~/Library/Application Support/Steam/steamapps/common/Total War Attila`, or `TWASE_GAMEROOT`). `xmake run` starts the game through the launcher; Steam must be running.

Use `xmake f --disable_copy=y` on either platform to build without copying into the game.

## Releasing

Every push to `main` builds both platforms and updates the zips on the [nightly release](https://github.com/rfuzzo/TWASE/releases/tag/nightly).

To publish a versioned release:

1. Bump the version in `src/platform/win32/dll/Version.hpp` and `src/platform/macos/dylib/Version.hpp` and merge it to `main`
2. Tag the commit and push the tag, e.g. `git tag v1.1.0 && git push origin v1.1.0`

The CI builds the tag and creates the release `TWASE v1.1.0` with `TWASE-v1.1.0.zip` (Windows) and `TWASE-macOS-v1.1.0.zip`, and notes generated from the merged PRs. The build fails if the tag doesn't match `TWASE_VERSION_STR` of both platforms.
