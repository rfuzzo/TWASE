# TWASE

A Total War: Attila Script extender similar to and heavily based on [RED4ext](https://github.com/wopss/RED4ext), [CET](https://github.com/maximegmd/CyberEngineTweaks) and [MWSE](https://github.com/MWSE/MWSE).

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
