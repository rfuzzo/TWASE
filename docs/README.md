# TWASE docs

Documentation for modders and for reverse engineering Total War: ATTILA. For installing and using TWASE, see the [README](../README.md).

## Modding

- [Script mods](modding/script-mods.md): where TWASE looks for `scripting.lua` mods, when they run, how to structure, log and debug them
- [Lua API](modding/lua-api.md): the `twase.*` functions (factions, regions, the loaded database, localisation, region trading)

## Engine

What we found out about the game, independent of the platform. Exact addresses and offsets are per game build, see below.

- [Lua](engine/lua.md): the interpreter, Lua environments, threads, the log sink, script interfaces (Lunar)
- [Campaign model](engine/campaign-model.md): factions, regions, the loaded database, localisation, strings
- [Diplomacy](engine/diplomacy.md): negotiations and actions, the AI's deal evaluation, region trading, Region for Troops

## Addresses and builds

- Mac (Feral 1.6.1): [addresses](macos/addresses.md), and the [history of the macOS port](macos/PLAN.md)
- Windows (`Attila.exe`): `src/platform/win32/sdk/Attila/Addresses.hpp`, [patches](../patches/patches.md)

## Development

- [Building and releasing](development.md), the repo layout

## Still to do

- The vanilla campaign API: the classes and methods of the game's script interfaces. 36 method tables with 805 methods are found in the Mac build, but only a few classes are named yet.
