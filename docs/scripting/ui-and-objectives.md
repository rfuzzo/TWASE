# UI overrides, objectives and messages

How vanilla scripts control the campaign UI, show objectives and infotext, and pass messages between scripts. Library files: `lib_campaign_ui.lua`, `lib_objectives.lua`, `lib_script_messager.lua`. See [architecture](architecture.md) for the surrounding system.

## UI overrides and `ui_state`

A `ui_override` is a named switch for a piece of UI with a lock function (turn it **off**) and an unlock function (turn it **on**). `lib_campaign_ui.lua` creates one for each of the campaign's controls and keeps them all in the global table **`ui_state`**: `ui_state.end_turn`, `ui_state.diplomacy`, `ui_state.technology`, `ui_state.recruit_units`, `ui_state.saving`, `ui_state.disable_side_panels`, and so on (several dozen; the comment at the top of the table in the file lists them).

An override has two independent flags:

| | Meaning | Changed by |
|---|---|---|
| `is_allowed` | whether the feature is available **in the campaign's design** (e.g. "no diplomacy in the tutorial") | `set_allowed(bool)`; **saved with the game** |
| `current_state` | whether it is locked right now (temporarily) | `lock()`, `unlock()` |

`unlock()` only takes effect if the override is allowed; `lock()` always does. So a modal section can lock everything and unlock it afterwards without the script having to remember what was allowed.

Whole-UI helpers: `lock_campaign_ui()` and `unlock_campaign_ui()` lock/unlock every override, `set_all_campaign_ui()` puts each into its allowed/not allowed state (done once at `UICreated` in single player). `cm:lock_ui()` and `cm:unlock_ui()` add the selection whitelists and hiding the radar/info panel; the modal queue uses these.

```lua
ui_state.diplomacy:set_allowed(false);   -- diplomacy button off, remembered in saves
ui_state.end_turn:lock();                -- off for now
ui_state.end_turn:unlock();              -- back on, if allowed
```

The allowed flags are saved and loaded by the library with `save_named_value` under each override's name. `release_campaign_ui()` runs on `UIDestroyed` and allows everything again.

## Selection whitelists and highlights

When the UI is locked only whitelisted objects can be selected:

- `add_character_selection_whitelist(cqi)`, `add_settlement_selection_whitelist(settlement)`, `add_all_characters_for_faction_selection_whitelist(faction)`, the matching `remove_` and `clear_` functions, and `enable_`/`disable_character_selection_whitelist()`.
- `highlight_settlement`, `highlight_character`, `highlight_all_settlements_for_faction`, `highlight_character_for_selection(char, callback)` and their `unhighlight_` pairs mark things for the player; the `..._for_selection` ones also run a callback when the object is selected.
- `is_char_selected(char)`, `is_settlement_selected(s)` and `is_panel_open(panel)` read the selection listener's state (`start_campaign_selection_listener`, which tracks the selected character or settlement from the engine's selection events and `PanelOpenedCampaign`/`PanelClosedCampaign`).
- `set_end_of_turn_warning(warning, value)` and the suppression system hide the end-of-turn warning popups.

## Objectives

Objectives are shown in a panel of the UI: the script calls `InterfaceFunction("set_objective", key, ...)` on the objectives component, so the first argument is a **key** the UI resolves to text, not the text itself (which table it comes from has not been checked).

| Function | Does |
|---|---|
| `set_objective(key, a, b)` | show an objective; `a`/`b` are the optional numeric parameters of the text |
| `complete_objective(key)`, `fail_objective(key)`, `remove_objective(key)` | mark it done, failed, or remove it |
| `remove_all_objectives(except_key)` | clear the panel |
| `activate_objective_chain(name, key, a, b)` | start a chain: replaces the panel's content with `key` |
| `update_objective_chain(name, key, a, b)` | move to the next objective of the active chain |
| `end_objective_chain(name)` | clear and remember the chain as finished |
| `reset_objective_chain(name)` | forget that it was finished |
| `end_objective_chain_on_advice_dismissed(name)` | end it when the current advice is dismissed |

Details:

- The library keeps a list of the objectives it has set. `complete_objective` for an objective that has not been set yet is **remembered as completed**, and `set_objective` for such a key completes it at once (so a script can't miss a fast completion).
- A chain that has been ended is in `__previous_objective_chains`, and a later `activate_objective_chain` of the same name is ignored. This is state of the Lua session: it is **not saved**, so a faction script that restarts its chain after a load has to decide for itself (typically from a saved value) not to reactivate a finished one.
- An `update_objective_chain` for a chain that is not the active one is cached and used when the chain is activated.

Infotext (the text lines that explain a tutorial step) uses `add_infotext(delay, key, key, ...)`: each key is shown with the delay between lines (an optional first number), `clear_infotext()`, `remove_infotext(key)`, `clear_and_add_infotext(...)`. They are added with `add_callback`s, so they can be interrupted by `cancel_add_infotext()`.

Missions (the game's mission system) are separate: `cm:trigger_custom_mission(faction, mission_key, do_not_cancel)` cancels a running mission of that key (unless `do_not_cancel`) and triggers it through the `GAME` object; what the key refers to is data, not script.

## `script_messager`

A global, string keyed message bus for script files that should not know about each other (it is the same pattern as `cm:add_listener`, but with no engine events and no context).

```lua
local sm = get_messager();
sm:add_listener("seal_1_done", function() ... end, true);   -- true: keep listening
sm:trigger_message("seal_1_done");                           -- returns true if anyone heard it
```

- Listeners without the third argument are **removed after the first message**.
- The callbacks get no parameters; for data use a global or a saved value.
- `set_debug(true)` logs messages and matches; `dump()` lists the listeners.
- Only one messager exists (`get_messager()` creates it).

Prefer it over polling globals for cross-file signals; prefer `cm:add_listener` with a custom event (`cm:register_event` + `cm:trigger_event`) if you want a context object.
