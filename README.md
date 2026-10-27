# TWASE

A script extender for **Total War: ATTILA** on Windows and Mac, similar to and heavily based on [RED4ext](https://github.com/wopss/RED4ext), [CET](https://github.com/maximegmd/CyberEngineTweaks) and [MWSE](https://github.com/MWSE/MWSE).

## Features

- In-game Lua console (toggle with the key below `Esc`, e.g. `~` on US or `^` on German keyboards)
- Captures the game's Lua log output and writes it to the console and to log files
- Auto-loads script mods from `<campaign folder>/mods/*/scripting.lua`
- A `twase` Lua table for script mods: faction attitudes, region wealth, DB records (building levels, units, levy pools, provinces), localised texts ([API](docs/modding/lua-api.md))
- Region trading for mods (`twase.enable_region_trading`): the engine's "Gift Region" between the player and any settled faction, giving and demanding several regions in one deal, valued by the mod, and "Region for Troops"
- Fixes the crash when too many units are spawned (e.g. with Fireforged Empire)
- Optional tweak: shows the AI's deal score in the diplomacy "Likelihood of success" tooltip. The AI accepts deals with a score of 0 or higher.
- Script-controlled encyclopedia URL and settlement-card building-slot display limits ([Lua API](docs/modding/lua-api.md#campaign-ui))
- Mac: start the game directly with a mod list or straight into a save

## Requirements

- **Windows:** Total War: ATTILA from Steam
- **Mac:** a Mac with Apple Silicon and Total War: ATTILA from Steam (Feral version 1.6.1). On other game versions TWASE stays inactive.

## Installation

Download the zip for your system from [Releases](https://github.com/rfuzzo/TWASE/releases): `TWASE-<version>.zip` for Windows, `TWASE-macOS-<version>.zip` for Mac. Open the game folder: in Steam, right-click *Total War: ATTILA* → *Manage* → *Browse local files*.

### Windows

Extract the zip into the game folder, so that `winmm.dll` is next to `Attila.exe` and `TWASE.dll` is in the `TWASE` folder. Then play as usual.

### Mac

1. Extract the zip into the game folder, so that `twase-launch.command` and the `TWASE` folder are next to `Total War ATTILA.app`.
2. macOS blocks downloaded files from unknown developers. Allow them once by running this in Terminal:

   ```bash
   cd "$HOME/Library/Application Support/Steam/steamapps/common/Total War Attila" && xattr -dr com.apple.quarantine TWASE twase-launch.command
   ```

3. To load TWASE when you press *Play* in Steam, set the game's launch options once: in Steam, right-click *Total War: ATTILA* → *Properties* → *General* → *Launch Options*, and enter (with your game folder):

   ```text
   "/Users/<you>/Library/Application Support/Steam/steamapps/common/Total War Attila/twase-launch.command" %command%
   ```

   Or start Steam and double-click `twase-launch.command` in the game folder.

The launcher can also start the game directly, without Feral's launcher window. Put the options in front of `%command%` in the launch options, or run it in Terminal from the game folder:

| Option | Description |
| --- | --- |
| `--skip-launcher` | Start the game directly |
| `--mods <file>` | Load the mods from a mod list (lines like `mod "my_mod.pack";`) instead of the ones enabled in Feral's mod manager |
| `--load "<save>"` | Load a campaign save, e.g. `--load "Saxons 395 AD Spring.save"` |

```bash
./twase-launch.command --mods mod_list.txt --load "Saxons 395 AD Spring.save"
```

## Console

Press the key below `Esc` to open or close the console. Type Lua code to run it in the selected Lua context (in a campaign, the campaign scripts' context `EpisodicScriptingEnv` is selected on its own, where `get_cm()` and `twase` are), or one of these commands:

| Command | Description |
| --- | --- |
| `.contexts` | List all active Lua contexts |
| `.active` | Show the selected Lua context |
| `.switch <N\|name>` | Switch context by index or name |
| `.globals` | List all global variables in the current context |
| `.list <table>` | List the fields of a table (e.g. `.list CampaignUI`) |
| `.help` | Show all commands |

The *Tweaks* tab turns the diplomacy deal score on or off.

## Script mods

TWASE loads every `scripting.lua` in `<campaign folder>/mods/<mod name>/` when a campaign starts, sorted by name, e.g. for the main campaign:

- loose files in the game's data folder: `data/campaigns/main_attila/mods/my_mod/scripting.lua` (on Mac `TotalWarAttilaData/data/...`)
- or inside a mod pack at `campaigns/main_attila/mods/my_mod/scripting.lua`

## Configuration and logs

Settings are in `TWASE/config.ini` in the game folder (created on the first start), logs are written to `TWASE/logs`.

```toml
[logging]
level = "info"          # trace, debug, info, warn, err, critical, off

[scripting]
enable_logging = true   # Forward game Lua log output to the console and log files
auto_load_mods = true   # Auto-load mods from <campaign_folder>/mods/*/scripting.lua

[tweaks]
diplomacy_deal_score = true
```

## Troubleshooting

- **Mac: "Apple could not verify … is free of malware"**: run the Terminal command from step 2 of the installation, then start again.
- **Mac: the game closes right away after updating TWASE** (crash report: "Code Signature Invalid"): macOS still remembers the old file. Delete the `TWASE` folder and extract the new one, instead of overwriting the files.
- **Mac: a modded save crashes while loading**: make sure the mods are enabled in Feral's launcher window (*Disable all mods* must be off), or use `--mods`.
- **Something else**: open an [issue](https://github.com/rfuzzo/TWASE/issues) and attach the newest file from `TWASE/logs`.

## Uninstalling

Delete the `TWASE` folder from the game folder, and `winmm.dll` (Windows) or `twase-launch.command` (Mac). On Mac, also clear the Steam launch options.

## Research credits

The encyclopedia URL override and settlement-card slot controls were inspired by
[bukowa/twdll](https://github.com/bukowa/twdll), particularly
[`campaign_ui.cpp` at commit `85c4db9`](https://github.com/bukowa/twdll/blob/85c4db9836e3150df8ec5b38315de9940f8d624c/src/attila/campaign_ui.cpp).
Thanks to bukowa and the twdll contributors for publishing that reverse-engineering research.
Upstream twdll is GPL-3.0; TWASE implements these controls independently for its own platform hooks and game layouts,
without vendoring twdll source. TWASE's license remains [MIT](LICENSE).

---

Modding docs and the `twase.*` Lua API: [docs](docs/README.md). Building and releasing: [docs/development.md](docs/development.md).
