# Faction starts, cutscenes and the modal queue

The parts of the vanilla campaign scripts that run a campaign's opening and anything that takes the player's control away. Read the [architecture](architecture.md) first; it explains `cm`, callbacks and timers that are used here.

Library files: `lib_campaign_faction_start.lua`, `lib_campaign_cutscene.lua`, and the modal queue in `lib_campaign_manager.lua`.

## `faction_start`

One `faction_start` per playable faction decides what happens when the campaign starts: run set-up code, play an intro cutscene or just put the camera in place. The faction's script file (`campaigns/<c>/factions/<faction key>.lua`, see [architecture](architecture.md#faction-scripts)) builds it:

```lua
fs_player = faction_start:new(local_faction, cam_x, cam_y, zoom, heading);

fs_player:register_intro_cutscene_callback(function() cutscene_intro_start() end);
fs_player:register_new_sp_game_callback(function() ... end);    -- new single player game only
fs_player:register_each_sp_game_callback(function() ... end);   -- every start/load, single player
fs_player:register_new_mp_game_callback(function() ... end);
fs_player:register_each_mp_game_callback(function() ... end);
```

and the file's `start_game_for_faction(should_show_cutscene)` calls `fs_player:start(should_show_cutscene, wait_for_loading_screen)`. That function is called from the campaign's first tick callback in `scripting.lua`.

`process_start` then does, in this order:

1. If it's a **new game**: the new SP or MP callback (one of them).
2. The **each game** callback for SP or MP, new or not.
3. If `set_force_intro_cutscene_playback(true)` was set and a cutscene exists: play it and stop (used for testing).
4. New game, single player, `should_show_cutscene` true and a cutscene callback registered: run the cutscene callback and stop. Otherwise the camera is set to the default position (`CampaignUI.SetCameraZoom`, `SetCameraHeading`, `SetCameraTargetInstant`).
5. An existing game: nothing is replayed.
6. Event panel auto open is switched back on and the zoom limit is restored.

Notes:

- Only one `faction_start` may play at a time (the global `__faction_start_playing` guards it); a second logs a script error.
- Registering the cutscene callback on a new game turns off the event panel auto open until the start has finished, so mission and event popups don't appear in the intro.
- Faction files run with `setfenv(1, _G.script_env)`. The campaign script runs in its own environment (see [engine/lua](../engine/lua.md)), and `scripting.lua` stores it in `_G.script_env` before it `require`s the faction files so that they share it. A script of yours that is `require`d the normal way already is in that environment.

## `campaign_cutscene`

A timed list of actions on top of `add_callback`:

```lua
local cutscene = campaign_cutscene:new(
    "intro",            -- name (the cutscene is called "Campaign_Cutscene_intro")
    60,                 -- length in seconds
    function() ... end  -- end callback (optional)
);

cutscene:action(function() cm:scroll_camera(...) end, 0);   -- at 0 s
cutscene:action(function() cm:show_advice("...") end, 2);   -- at 2 s
cutscene:set_skippable(true, function() ... end);           -- callback if skipped
cutscene:start();
```

Behaviour:

- `action(callback, delay)` keeps the list sorted by delay; `start()` runs the first action and each following action is scheduled with `add_callback` for the difference of the delays (equal delays run immediately, in order). A final action calls `finish()` at the cutscene's length unless `set_do_not_end(true)`.
- `start()` refuses to run while another cutscene runs (`is_any_cutscene_running`). It steals the player's input (`cm:steal_user_input(true)`), switches to the cinematic zoom limit, turns off the events panel, snapshots the shroud and reveals neighbouring regions, shows cinematic borders (`CampaignUI.ToggleCinematicBorders`) and overrides the UI (`disable_advice_changes`, `disable_settlement_labels`).
- A skippable cutscene (the default) listens for `AdviceDismissed` and for the `esc_menu_campaign` panel opening, and then `skip()`s: the actions are not run, the end state (camera via `set_skip_camera(x, y, h, r)`) is applied, and `finish()` restores everything: input, UI, shroud, borders, event panel, advice.
- `wait_for_advisor()` makes the next action wait (polling every 0.5 s) while advice audio plays.
- `reveal_region_by_proximity` reveals a region when the camera gets within a distance of a position.
- `set_*` options: `dismiss_advice_on_end`, `use_cinematic_borders`, `restore_ui`, `disable_settlement_labels`, `neighbouring_regions_visible`, `disable_shroud`, `do_not_end`, `set_debug`.
- `skip_all_campaign_cutscenes()` and a debug key handler (`OnKeyPressed`) exist for development.

All timing is in seconds and runs on `TimeTrigger`s. The cutscene's action list is Lua state and the library does not save it, so what a save made during a cutscene does after loading is not covered by the script; `faction_start` only plays the intro on a new game.

## The modal queue

Popups, advice sequences and mission introductions must not overlap. `cm:add_modal_section(name, callback, params, lock_ui, is_filler)` queues a **section**: a function that takes control and calls `cm:next_modal_section()` when it is done.

- `add_modal_section` appends, `insert_modal_section` puts it at the front.
- When no section is active the next one starts at once: the UI is **locked** (`cm:lock_ui()`, see [UI and objectives](ui-and-objectives.md)) unless `lock_ui == false`, the local faction's movement is disabled, and the callback is run through `progress_on_loading_screen_dismissed`, so it never starts under a loading screen.
- When the queue is empty `modal_sections_end()` unlocks the UI, the end turn button and the faction's movement.
- **Filler** sections only run if nothing else is queued, and wait a second to see whether something arrives behind them.
- `lock_modal_queue(true)` makes the queue refuse additions; `clear_modal_queue()` empties it.
- `suppress_events_popup_on_player_turn_start(ui_override, value)` keeps the events popup from appearing at the player's turn start while a section runs.

There is no automatic hand-over: a section that never calls `next_modal_section()` keeps the UI locked and blocks the queue.
