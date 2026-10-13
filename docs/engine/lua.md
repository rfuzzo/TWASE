# Lua in Attila

What we know about how Attila runs Lua, from reverse engineering the Windows game and the Mac (Feral) port. Addresses and offsets per build: [Mac](../macos/addresses.md); the Windows ones are in `src/platform/win32/sdk/Attila/Addresses.hpp`.

## The interpreter

- **Lua 5.1**, linked into the game. Scripts get the standard libraries plus the game's bindings.
- **`lua_Number` is `float`**, not `double`: `lua_pushnumber` stores a single precision value. Integers are exact up to 2^24 only, which matters for large sums and ids.
- The Mac binary contains **two** Lua 5.1 copies: the game's (used by every script) and one of Feral's own. Code that calls into Lua must use the game's copy; values from one copy can't be passed to the other.

## Lua environments

Every Lua state belongs to an environment object. All of them derive from `UTILITYDLL::LUA::LuaEnv`; its virtual function in slot 3 loads and runs the environment's startup script. The class names come from the RTTI:

| Class | Lua state name | Runs |
|---|---|---|
| `EMPIRECAMPAIGN::EPISODIC_SCRIPTING_ENV` | `EpisodicScriptingEnv` | the campaign's `campaigns/<campaign>/scripting.lua`; also derives from `EMPIREUTILITY::KEY_CONTROLLED` |
| `EMPIREUTILITY::EMPIRE_LUA_ENV` | `EmpireLuaEnv` | its own startup script (not examined yet); its constructors install the log sink |
| `FRONTEND::AUTORUN_SCRIPTING_ENV` | | the front end's autorun scripts |
| `EMPIREBATTLE::BATTLE_EDITOR_SCRIPT_INTERFACE` | | battle scripts |
| `EMPIRECAMPAIGN::CAMPAIGN_UI_SCRIPT_INTERFACE`, `UIDLL::FRONTEND_UI_SCRIPT_INTERFACE` | | bindings only, no startup script |

`EPISODIC_SCRIPTING_ENV` keeps the campaign folder (`campaigns/main_attila`) and the script path (`campaigns/main_attila/scripting.lua`) as strings in the object. Its startup function reads the script through the game's file system, sets `package.path`, loads the chunk with `luaL_loadbuffer` and runs it. TWASE's mod loader hooks exactly this function: after the original has run, it registers `twase` and `require`s the mods (see [script mods](../modding/script-mods.md)).

The campaign script runs in an **environment of its own**, not `_G`, and the game's `require` loads files into the caller's environment: the script library and the script mods all share that environment. `_G` only holds Lua's standard library and a few engine functions (`CliExecute`, `RequireRegister`, `system`, `vfs`).

The game keeps a linked list of its script runtimes, each with its Lua state; the console's `.contexts` lists them, and the active one is where typed Lua runs.

## Threads

The game runs its Lua on the game thread (the thread of the game's `WinMain`). Touching a Lua state from another thread corrupts it sooner or later. On Mac, rendering (Metal present) and input run on other threads, so the console queues typed commands and runs them from a hook on the game loop's tick. On Windows they run from the D3D11 Present hook, where they have always run.

## Output

`print`, the game's `out` functions and the Lua error handler all write through one global log sink function pointer. The retail game installs an empty sink, and every scripting environment's constructor sets it again; TWASE patches the setter so its own sink stays and forwards the lines to the console and the log. The sink gets fragments (each `print` argument, the tabs and the newline separately), so they have to be joined into lines.

## Script interfaces (Lunar)

The campaign model is bound with the **Lunar** C++ template. A script object (e.g. what `faction_by_key` returns) is a userdata holding a pointer to a `<CLASS>_SCRIPT_INTERFACE` object, which points to the game object (e.g. `FACTION`). The class's metatable is in the Lua registry under the class name (`"FACTION_SCRIPT_INTERFACE"`), so a userdata's class can be checked by comparing its metatable with the registry entry.

Each class has a static method table of 24-byte entries `{ const char* name, member function, this adjustment }`, ending at a null name. The methods are called as `(T* this, lua_State* L)`. On Mac there are 36 such tables with 805 methods (battle and campaign classes); the campaign ones are matched to their classes in the [campaign API reference](../reference/README.md).

Campaign methods read their arguments and push their results through a few shared helpers that take a context object (the Lua state plus a "still valid" flag that a type mismatch clears, after logging "Trying to retrieve type %s, found type %s"). Battle methods use a different, variadic convention.
