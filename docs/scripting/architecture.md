# Campaign scripting architecture

How the game's own campaign scripts (Attila 1.6, `main_attila`) are put together: which files load in which order, how an engine event ends up in a Lua callback, and how state survives a save. This is the vanilla system that [script mods](../modding/script-mods.md) plug into. For the engine side (Lua states, Lunar bindings) see [engine/lua](../engine/lua.md); for lists of functions and events see the [reference](../reference/README.md).

Source: the files below are paths inside the game's `data.pack`. Line numbers are not given because they differ between builds.

## The layers

```text
engine (C++)      raises events, calls into Lua with a context object
   │
lua_scripts/      events.lua            the event tables (one empty table per event)
                  episodicscripting.lua the engine-facing layer: GAME object, UI setup, AddEventCallBack
   │
script/_lib/      lib_*.lua             the script library: campaign_manager, event_handler, timers, UI, cutscenes ...
   │
campaigns/<c>/    scripting.lua         first file of a campaign; creates `cm`, requires the rest
                  att_*.lua, factions/  the campaign's content
```

Each layer only talks to the one below it. Campaign code calls `cm`; `cm` calls the `GAME` object and the event handler; the event handler attaches to the `events` tables that the engine fills.

## Startup sequence

1. **The engine starts the campaign's Lua environment** and runs `campaigns/<campaign>/scripting.lua` (`EpisodicScriptingEnv`, see [engine/lua](../engine/lua.md)).
2. `scripting.lua` does `require "lua_scripts.Campaign_Script_Header"`, forcing a reload of the library. The header calls `force_require("Lib_Header")`, which defines the `TYPE_*` constants and force-requires the libraries (below).
3. `scripting.lua` sets `campaign_name = get_folder_name_and_shortform()` (the name of the folder the calling file is in, e.g. `main_attila`) and adds `data/campaigns/<name>/?.lua` and `.../factions/?.lua` to `package.path`.
4. **`cm = campaign_manager:new(campaign_name)`**. This is the one place the manager is created (a second call returns the existing one). `new` loads `lua_scripts.EpisodicScripting`, creates the `event_handler` on top of its `AddEventCallBack`, starts the timer, selection, UI state, advice and pending battle listeners, and registers its own listeners for `NewSession`, `UICreated`, `UIDestroyed`, `FirstTickAfterWorldCreated`, `SavingGame` and `LoadingGame`.
5. `scripting.lua` registers the faction script loader (a UI created callback) and the first tick callback, then `require`s the campaign's files: `att_save_load`, `att_start`, `att_preservation`, `att_climate_change`, `att_traits`, `att_attila_story`, the three `restrict_*_building` files, `roman_empire_no_vassal`, and the `sands/` and `slavs/` DLC folders. These files run top level code only: they define functions, globals and **register callbacks and listeners**. Almost nothing happens yet.
6. **Later, driven by engine events**, roughly in this order (the engine decides; each is a `cm` callback list, see [callbacks](#lifecycle-callbacks)):

| Event | `cm` hook | What vanilla does there |
|---|---|---|
| `NewSession` | `register_session_created_callback` | `cm.game_interface` is linked to the `GAME` object; `cm:model()` is not safe yet |
| `LoadingGame` | `register_loading_game_callback` | the library restores its values, then `att_save_load` restores the campaign's globals |
| `UICreated` | `register_ui_created_callback` | local faction name is read; `scripting.lua` loads `factions/<local faction>.lua` and `<local faction>_intro.lua`; the intro movie is registered on a new game |
| `FirstTickAfterWorldCreated` | `register_first_tick_callback` | `start_game_for_faction()` (the faction's script, with its intro cutscene) and `start_game_all_factions()` in `att_start.lua`, which starts every system |
| turn events | `add_turn_start_callback` etc. | the game proper |
| `SavingGame` | `register_saving_game_callback` | values are written |
| `UIDestroyed` | `register_ui_destroyed_callback` | |

`Campaign_Script_Header` also handles the other game modes: `lib_type_battle` and `lib_type_frontend` load a different set of libraries (the battle and front end scripts are not covered here). Campaign mode loads `Lib_Misc_Campaign`, `Lib_Campaign_Cleanup`, `Lib_Campaign_Manager`, `Lib_Campaign_Cutscene`, `Lib_Campaign_Faction_Start`, `Lib_Objectives`, `Lib_Campaign_UI`, `Lib_Campaign_Defense`, after the common `Lib_Common`, `Lib_Event_Handler`, `Lib_Script_Messager`.

### Faction scripts

`factions/<faction key>.lua` holds the faction's start (`start_game_for_faction`, mission chains, intro cutscene) and `<faction key>_intro.lua` its intro. Only the **local** faction's files are loaded (`load_faction_script` wraps the `require` in `pcall` and logs a script error on failure). Because they load at `UICreated`, a faction file can rely on the model but its `cm:register_*` calls for the session and loading callbacks are too late; those belong in the main files.

## The campaign manager (`cm`)

`cm` is a table of its own state and methods whose metatable's `__index` **falls through to the `GAME` object**: `cm:model()`, `cm:faction_by_key(k)`, `cm:add_time_trigger(...)` and the rest of the [game interface](../reference/game-interface.md) work on `cm` without being defined there. The fall-through only works after `NewSession`; before that a lookup logs an error (this is why the library says `register_session_created_callback`, not top-level code, for anything that needs the game).

`get_cm()` returns it from anywhere. TWASE's console has no `cm` global (see [script mods](../modding/script-mods.md)), use `get_cm()`.

Things `cm` owns itself (the rest of `lib_campaign_manager.lua`):

- the lifecycle callback lists and `save_value` / `load_value`, `set_saved_value` / `get_saved_value` (below)
- the turn manager (`add_turn_start_callback`, `add_turn_end_callback`, `..._for_faction`)
- the advice helpers (`show_advice`, `progress_on_advice_dismissed`, ...) and the **modal queue** (`add_modal_section`, which serialises popups and UI locking)
- `create_force`, `attack`, `trigger_custom_mission`, `random_number`, turn countdown events, the pending battle cache (who attacked, who defended, for the post battle events)

Other library objects are created on demand: `campaign_cutscene`, `campaign_defense`, `faction_start`, the objectives (`lib_objectives`: objective chains, wrapper functions) and `script_messager`.

## Events

### From the engine to a callback

1. `lua_scripts/events.lua` defines one empty table per event (`TimeTrigger = {}`, `CharacterCreated = {}`, ...). The [events list](../reference/events.md) is its keys.
2. `AddEventCallBack(event, func)` in `episodicscripting.lua` appends `func` to `events[event]` (with a user defined list so `ClearEventCallbacks` can remove them).
3. When the game raises an event it calls every function in that table with a **context** object whose accessors depend on the event (`context:character()`, `context:faction()`, `context:region()`, `context:string`, `context.component`, ...).

### `event_handler` and `cm:add_listener`

You normally don't touch the tables; the `event_handler` (created by `cm`) is the layer on top:

```lua
cm:add_listener(
    "my_listener_name",       -- name, used by remove_listener (not unique)
    "CharacterCreated",       -- event
    true,                     -- condition: true or function(context) -> boolean
    function(context)         -- callback
        ...
    end,
    true                      -- persistent: false (default) = removed after the first match
);
```

How it works:

- The first listener for an event **attaches one dispatcher function** to that event through `AddEventCallBack`; later listeners for the same event only add an entry to the handler's list.
- On the event the dispatcher walks the listener list **in registration order**, evaluates each `condition(context)`, collects the callbacks of the ones that match, **removes the non persistent matches first**, and then calls the collected callbacks. So a one-shot listener can safely register itself again, and the order of callbacks is registration order.
- Listeners are plain Lua tables and are **not saved**. After a load the scripts' top level code registers them again (that is why listeners are registered from files that run on every start, and why state is rebuilt from saved values).
- `remove_listener(name)` removes **every** listener with that name.
- `cm:register_event`, `cm:trigger_event(name, ...)` let a script define and fire its own events (the turn countdown events use it). `trigger_event` builds a `custom_context` and calls the event's functions directly.

### Lifecycle callbacks

`cm:register_*_callback(fn)` (session created, UI created, UI destroyed, first tick, saving, loading) append to a list that the manager's own listener runs when its event fires. They are not listeners (no names, can't be removed). The first four get the event's context, the saving and loading ones the **save context** needed for `save_value`.

## State and save games

Lua state is not part of a save; only what scripts write into it is. There are two mechanisms:

1. **Named values with a save context**: inside `register_saving_game_callback(function(context) ... end)` call `cm:save_value(name, value, context)`, and in `register_loading_game_callback` read it with `cm:load_value(name, default, context)`. This calls the `GAME` object's `save_named_value` / `load_named_value`. `att_save_load.lua` is the vanilla example: the campaign's globals are listed twice, once to save and once to load, with defaults.
2. **Saved values**: `cm:set_saved_value(name, value)` and `get_saved_value(name)` keep booleans, numbers and strings in a table that the library itself writes (as one string `name:type:length:value;...`) and restores on load, so no callbacks are needed. Names can't contain `:`.

The library also saves its own values the same way: a save counter, the turn countdown events, the pending battle cache and the campaign defense state.

**New game or load**: `cm:is_new_game()` is `__save_counter == 1`. The counter is 0 before the start position is processed, 1 for a new game's first save, and goes up with every save, so it is only meaningful once the game has loaded (`LoadingGame` has run). Code that needs to set something up once (like the intro movie) tests it in `UICreated`/first tick callbacks.

## Timers

Two systems exist in the library; the campaign uses the first:

- **`add_callback(fn, seconds, name)`** and `add_repeat_callback`, `remove_callback(name)` in `lib_misc_campaign.lua`: a thin wrapper on the engine's `add_time_trigger(id, seconds)`. The wrapper keeps a list of `{id, callback, name}` and one listener on `TimeTrigger` that matches `context.string` to the id, removes the entry and runs the callback. Times are in **seconds**, not turns (the WH2 documentation says so for `add_time_trigger`; not checked for Attila). Vanilla uses it everywhere (`add_callback(` appears 105 times in the campaign files).
- **`timer_manager`** (`lib_timer_manager.lua`) is the battle and front end timer: callbacks in milliseconds on top of the battle's timers. Creating one in a campaign logs an error ("not supported").

Turn based timing is done with the turn manager (`add_turn_start_callback`) and turn countdown events (`add_turn_countdown_event(faction, turns, event, string)` fires a custom event after N turns of that faction and survives saves).

## Conventions in the vanilla code

- Argument checks use the `is_*` helpers of `lib_common` (`is_string`, `is_function`, `is_faction`, ...) and report with `script_error(msg)`, which prints a banner and a traceback to the log and returns; execution continues, so one bad call doesn't stop the script.
- Output: `output(str)` (indented with `inc_tab`/`dec_tab`) and `print`. `out.ting`, `out.tom` etc. are the engine's per-developer log channels.
- Globals carry type prefixes in `att_save_load.lua` (`BOOL_`, `INT_`).
- Each system exposes `start_...` and `Add_..._Listeners` functions that are called from `start_game_all_factions()` instead of registering at file load, so a system only listens when it was started (and multiplayer starts a smaller set).

## What a script mod gets from this

A TWASE script mod (see [script mods](../modding/script-mods.md)) is `require`d after the campaign's own `scripting.lua`, so `cm`, the library and the campaign's globals exist. Practically:

- Register your listeners and lifecycle callbacks from the mod's top level, exactly like a campaign file does. The events they react to have not happened yet (that is the point of the order above).
- For things that need the game (`cm:model()`, factions), use a UI created or first tick callback, or a listener; do not run them at load time.
- Save what you must keep with a saving/loading callback pair or `set_saved_value`; use your own name prefix, since the names share a namespace with the campaign's.
- `cm:add_listener`'s names are shared too; prefix them.

## Open questions

Things seen in the source that need confirming in the running game:

- `custom_context:faction()` returns `character_data` (the field the character accessor uses), and `trigger_event` starts reading its extra arguments at index 2 of `arg`. Both look like bugs, but no vanilla script depends on them (the only `trigger_event` user passes a string, which would then not arrive in `context.string`). Not tested.
- `on_turn_end` falls back to `self.default_turn_callback` (not `default_turn_end_callback`) when no end-of-turn callback matched and a default is set.
