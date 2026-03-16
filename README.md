# TWASE

A Total War: Attila Script extender similar to and heavily based on [RED4ext](https://github.com/wopss/RED4ext), [CET](https://github.com/maximegmd/CyberEngineTweaks) and [MWSE](https://github.com/MWSE/MWSE).

## Features

- In-game debug console for logging and executing Lua code (toggle with `~`)
- Captures the game's Lua log output and forwards it to the console and log files
- Auto-loads mods from `<campaign_folder>/mods/*/scripting.lua`
- Patches memory in the game to fix crashes and issues

### Console Commands

| Command | Description |
| --- | --- |
| `.contexts` | List all active Lua contexts |
| `.active` | Show the currently active Lua context |
| `.switch <N\|name>` | Switch context by index or partial name match |
| `.globals` | List all global variables in the current context |
| `.list <table>` | List fields of a table (e.g. `.list CampaignUI`) |
| `.help` | Show all available commands |

Any other input is executed directly as Lua code in the active context.

### Patches

- [x] UnitSizePatch: Fixes crashes when too many units are spawned (e.g. with the mod Fireforeged Empire installed)

## Usage

- Extract `winmm.dll` to `<game directory>` (e.g. `SteamLibrary\steamapps\common\Total War Attila`)
- Extract `TWASE.dll` to `<game directory>/TWASE` (e.g. `SteamLibrary\steamapps\common\Total War Attila\TWASE`)

Logs are created in `<game directory>/TWASE/logs`.

### Mod Auto-Loading

Place your mods under `<campaign_folder>/mods/`. Each mod must have a `scripting.lua` entry point:

```text
mods/
  mymod/
    scripting.lua
  anothermod/
    scripting.lua
```

TWASE will execute `require('mods/mymod/scripting')` for each discovered mod, sorted alphabetically. Enable this feature via `scripting.auto_load_mods = true` in the config.

## Configuration

You can configure certain things in `<game directory>/TWASE/config.ini`. Example:

```toml
version = 0

[logging]
level = "info"        # trace, debug, info, warn, err, critical, off
flush_on = "info"
max_files = 5
max_file_size = 10    # MB

[scripting]
enable_logging = true   # Forward game Lua log output to the console and log files
auto_load_mods = true   # Auto-load mods from <campaign_folder>/mods/*/scripting.lua

[plugins]
enabled = true
ignored = []

[dev]
console = false
wait_for_debugger = false
```

## Building

- Clone the repo to some folder
- Run `xmake`
- Run `xmake project -k vsxmake -m "debug,release"` to create a VS solution

The xmake contains a post-build step which will copy the built DLL to the `TWASE` folder in your game directory. You can change this path in `xmake.lua`.
The game directory can also be specified via the `TWASE_GAMEROOT` environment variable.
