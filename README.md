# TWASE

A Total War: Attila Script extender similar to and heavily based on [RED4ext](https://github.com/wopss/RED4ext), [CET](https://github.com/maximegmd/CyberEngineTweaks) and [MWSE](https://github.com/MWSE/MWSE).

## Features

- Adds a debug console for logging
- Patches memory in the game to fix crashes and issues

### Patches

-[x] UnitSizePatch: Fixes crashes when too many units are spawned (e.g. with the mod Fireforeged Empire installed)

## Usage

- Extract `winmm.dll` to `<game directory>` (e.g. `SteamLibrary\steamapps\common\Total War Attila`)
- Extract `TWASE.dll` to `<game directory>/TWASE` (e.g. `SteamLibrary\steamapps\common\Total War Attila\TWASE`)

Logs are created in `<game directory>/TWASE/logs`.

## Configuration

You can configure certain things in `<game directory>/TWASE/config.ini`. Example:

```toml
version = 0

[logging]
level = "info"
flush_on = "info"
max_files = 5
max_file_size = 10

[dev]
console = true
wait_for_debugger = true
```

## Building

- Clone the repo to some folder
- Run `xmake`
- Run `xmake project -k vsxmake -m "debug,release"` to create a VS solution  

The xmake contains a post-build step which will copy the built DLL to the `TWASE` folder in your game directory. You can change this path in `xmake.lua`.
The game directory can also be specified via the `TWASE_GAMEROOT` environment variable.
