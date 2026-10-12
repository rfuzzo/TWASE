# TWASE for macOS

A script extender for **Total War: ATTILA** on Mac, the macOS version of [TWASE](https://github.com/rfuzzo/TWASE).

## Features

- In-game Lua console (toggle with the key below `Esc`, e.g. `^` on German or `` ` `` on US keyboards)
- Captures the game's Lua log output and writes it to the console and to log files
- Auto-loads script mods from `<campaign folder>/mods/*/scripting.lua`
- Fixes the crash when too many units are spawned (e.g. with Fireforged Empire)
- Optional tweak: shows the AI's deal score in the diplomacy "Likelihood of success" tooltip
- Start the game directly with a mod list or straight into a save

## Requirements

- A Mac with Apple Silicon
- Total War: ATTILA from Steam (Feral version 1.6.1). On other game versions TWASE stays inactive.

## Installation

1. Download `TWASE-macOS-<version>.zip` from [Releases](https://github.com/rfuzzo/twase_macos/releases).
2. Open the game folder: in Steam, right-click *Total War: ATTILA* → *Manage* → *Browse local files*.
3. Extract the zip there, so that `twase-launch.command` and the `TWASE` folder are next to `Total War ATTILA.app`.
4. macOS blocks downloaded files from unknown developers. Allow them once by running this in Terminal:

   ```bash
   cd "$HOME/Library/Application Support/Steam/steamapps/common/Total War Attila" && xattr -dr com.apple.quarantine TWASE twase-launch.command
   ```

## Playing

To load TWASE when you press *Play* in Steam, set the game's launch options once: in Steam, right-click *Total War: ATTILA* → *Properties* → *General* → *Launch Options*, and enter (with your game folder):

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

Press the key below `Esc` to open or close the console. Type Lua code to run it in the selected Lua context, or one of these commands:

| Command | Description |
| --- | --- |
| `.contexts` | List all active Lua contexts |
| `.switch <N\|name>` | Switch context by index or name |
| `.globals` | List all global variables in the current context |
| `.list <table>` | List the fields of a table (e.g. `.list CampaignUI`) |
| `.help` | Show all commands |

The *Tweaks* tab turns the diplomacy deal score on or off.

## Script mods

TWASE loads every `scripting.lua` in `<campaign folder>/mods/<mod name>/` when a campaign starts, e.g. for the main campaign:

- loose files in the game folder: `TotalWarAttilaData/data/campaigns/main_attila/mods/my_mod/scripting.lua`
- or inside a mod pack at `campaigns/main_attila/mods/my_mod/scripting.lua` (enable the pack in Feral's mod manager or with `--mods`)

## Configuration and logs

Settings are in `TWASE/config.ini` (created on the first start), logs are written to `TWASE/logs`.

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

- **"Apple could not verify … is free of malware"**: run the Terminal command from step 4 of the installation, then start again.
- **The game closes right away after updating TWASE** (crash report: "Code Signature Invalid"): macOS still remembers the old file. Delete the `TWASE` folder and extract the new one, instead of overwriting the files.
- **A modded save crashes while loading**: make sure the mods are enabled in Feral's launcher window (*Disable all mods* must be off), or use `--mods`.
- **Something else**: open an [issue](https://github.com/rfuzzo/twase_macos/issues) and attach the newest file from `TWASE/logs`.

## Uninstalling

Delete `twase-launch.command` and the `TWASE` folder from the game folder.

---

Developer notes are in [PLAN.md](PLAN.md) and [docs](docs/).
