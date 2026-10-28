# Configuration

TWASE reads `TWASE/config.ini` in the game folder at startup. It's a [TOML](https://toml.io) file, so text values need quotes and `true` / `false` are written without them. If the file doesn't exist, TWASE creates it with the defaults. Missing keys and values of the wrong type use their default. A syntax error in the file stops the game with an error message that names the file and the line.

Changes take effect at the next start. The exception is the *Tweaks* tab in the console, which changes its settings immediately and saves them to the file. Saving keeps your comments and layout.

## All settings

```toml
version = 0

[logging]
level = "info"
flush_on = "info"
max_files = 5
max_file_size = 10

[scripting]
enable_logging = true
auto_load_mods = true

[tweaks]
diplomacy_deal_score = true

[dev]
console = false
wait_for_debugger = false
trace_lua_loads = false
lua_dev_functions = false
```

`version` is the format of the file. Leave it at `0`.

### `[logging]`

The log files are in `TWASE/logs`, one per start (`twase-<date and time>.log`).

| Key | Default | Values | What it does |
|---|---|---|---|
| `level` | `"info"` | `"trace"`, `"debug"`, `"info"`, `"warning"` (or `"warn"`), `"error"` (or `"err"`), `"critical"`, `"off"` | The lowest level that's written. `"debug"` adds detail, e.g. the deal scores and region trading's evaluation; `"trace"` is very verbose. An unknown name keeps the default |
| `flush_on` | `"info"` | the same names except `"off"` | Messages at this level or above are written to the file immediately. With a lower level, the newest lines may be missing after a crash |
| `max_files` | `5` | 1 or more | How many old files a log keeps when it rotates |
| `max_file_size` | `10` | 1 or more, in MB | The size at which a log file rotates |

### `[scripting]`

| Key | Default | What it does |
|---|---|---|
| `enable_logging` | `true` | Copies the game's Lua log output (`out(...)` from the game's and mods' scripts) to the console and the log files |
| `auto_load_mods` | `true` | Loads every `scripting.lua` in `<campaign folder>/mods/<mod name>/` when a campaign starts, see [script mods](modding/script-mods.md) |

### `[tweaks]`

Also in the console's *Tweaks* tab.

| Key | Default | What it does |
|---|---|---|
| `diplomacy_deal_score` | `true` | Shows the AI's deal score in the diplomacy "Likelihood of success" tooltip. The AI accepts deals with a score of 0 or more |

### `[dev]`

For developing TWASE and mods. They're off by default.

| Key | Default | What it does |
|---|---|---|
| `console` | `false` | Windows: opens a separate console window that shows the log. Mac: also writes the log to standard output, which you see when the game was started from a terminal (`twase-launch.command`) |
| `wait_for_debugger` | `false` | TWASE waits at startup, before it does anything else, until a debugger is attached. The game hangs until then |
| `trace_lua_loads` | `false` | **Mac only.** Logs every Lua chunk the game loads. Needs `level = "trace"` |
| `lua_dev_functions` | `false` | Adds the `twase.dev_*` probes for reverse engineering to the Lua API, see [Lua API](modding/lua-api.md#development-probes) |

### `[plugins]`

TWASE reads `enabled` and `ignored` (a list) and writes them back, but nothing uses them yet. They're reserved for a plugin loader.
