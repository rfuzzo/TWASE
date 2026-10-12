# Script mods

A TWASE script mod is a `scripting.lua` that TWASE loads into a campaign's Lua environment, next to the campaign's own scripts. Unlike a mod that replaces `campaigns/<campaign>/scripting.lua`, any number of script mods can run together, and they keep working with mods that do replace it (e.g. Fireforged Empire).

## Where they go

```text
campaigns/<campaign>/mods/<mod name>/scripting.lua
```

`<campaign>` is the campaign's folder: `main_attila` (the grand campaign), `bel_attila` (The Last Roman), `cha_attila` (Age of Charlemagne), `pro_attila` (the prologue). A mod that should run in every campaign needs a `scripting.lua` in each of them; keep those files as one-line stubs that `require` the shared code (see below).

TWASE finds them in two places:

- **loose files** in the game's data folder: `data/campaigns/main_attila/mods/my_mod/scripting.lua` (on Mac `TotalWarAttilaData/data/...`)
- **inside a mod pack** that the game loads, at `campaigns/main_attila/mods/my_mod/scripting.lua`

Only direct sub folders of `mods/` count, and a `scripting.lua` directly in `mods/` is ignored.

> **Mac:** Feral's user folder (`…/VFS/User/AppData/Roaming/The Creative Assembly/Attila/maps/`) isn't searched, and the game empties it on every launch. Use the data folder or a pack.

## When they run

1. The game starts a campaign and runs `campaigns/<campaign>/scripting.lua` in the campaign's Lua environment (`EpisodicScriptingEnv`).
2. TWASE adds the global `twase` table ([Lua API](lua-api.md)).
3. TWASE loads the mods it found, **sorted by name**, each with `require('mods/<mod name>/scripting')`.

An error in one mod is logged (`[ModLoader] Error in <mod>: ...`) and the others still load. The log ends with `[ModLoader] Finished: N loaded, M failed`. The loader can be switched off with `auto_load_mods = false` in the `[scripting]` section of `TWASE/config.ini`.

Because the campaign's `scripting.lua` has already run, the campaign manager and the game's script libraries exist when your mod loads. Register your event listeners from there as usual.

## Structure

Keep the per-campaign `scripting.lua` files tiny and put the code in a folder of its own at the data root, which `require` also finds:

```text
campaigns/main_attila/mods/my_mod/scripting.lua     require("my_mod/init")
campaigns/bel_attila/mods/my_mod/scripting.lua      require("my_mod/init")
my_mod/init.lua                                     the mod
my_mod/util.lua                                     require("my_mod/util")
```

`require` caches modules, so shared modules are loaded once per campaign session.

## Requiring TWASE (or not)

If your mod should also work without TWASE, check for the table:

```lua
if twase then
    -- e.g. twase.version == "1.1.0"
end
```

A mod placed under `mods/` only runs with TWASE anyway: without it nothing loads that folder.

## Logging and the console

- `print(...)` from any game Lua goes to the TWASE console and, with `level = "debug"` (or lower) in the `[logging]` section of `config.ini`, to `TWASE/logs/` as `[Lua] ...` lines. A common prefix like `[my_mod]` makes your lines easy to find.
- The console (key below `Esc`) runs Lua in the selected context. Pick the campaign one with `.contexts` and `.switch <N>` (named `EpisodicScriptingEnv`), then call into your mod, e.g. `my_mod.report()`. Expose a global table for that.
- In the console the global `cm` is nil; use `get_cm()` for the campaign manager.

## Packs

For a release, build the mod's files into a pack with the same paths. Some content only loads from packs, e.g. UI layouts; loose files are best for scripts during development (edit, then reload the save).

When a loaded pack already has a `campaigns/<campaign>/mods/` folder, the game's file search only returns the pack's entries there. On Mac, TWASE also looks into the loose `mods/` folder on disk, so loose mods still load next to such a pack; on Windows a loose mod next to such a pack isn't found yet.
