# Events and contexts

What the vanilla campaign scripts listen to and what the context object of each event gives them. The mechanism (how an event reaches a listener) is in the [architecture](architecture.md#events); the full list of names is in the [events reference](../reference/events.md).

## What is an event

260 names defined in `lua_scripts/events.lua`, raised by the game with a **context**: an object with accessors for what happened. Only some of the events are relevant to the campaign; the list also holds battle, front end and UI events and some left over from other Total War games (`FactionCapturesKyoto`, `FactionBecomesShogun`, `TestEvent`, `DummyEvent`). Listening to an event that is never raised is harmless.

| Group | Examples |
|---|---|
| Session and loading (library) | `NewSession`, `LoadingGame`, `SavingGame`, `UICreated`, `UIDestroyed`, `FirstTickAfterWorldCreated`, `LoadingScreenDismissed`, `WorldCreated`, `ModelCreated`, `NewCampaignStarted`, `FirstTickAfterNewCampaignStarted` |
| Turns | `FactionTurnStart`, `FactionTurnEnd`, `FactionBeginTurnPhaseNormal`, `FactionAboutToEndTurn`, `FactionRoundStart`, `CharacterTurnStart`, `RegionTurnStart`, `RegionTurnEnd`, `UnitTurnEnd`, `SlotTurnStart` |
| Characters (54) | `CharacterCreated`, `CharacterComesOfAge`, `CharacterSelected`, `CharacterCompletedBattle`, `CharacterEntersGarrison`, `CharacterBesiegesSettlement`, `CharacterRankUp`, ... |
| Battles | `PendingBattle`, `PreBattle`, `BattleCompleted`, `CharacterCompletedBattle`, `HistoricBattleEvent`; the `Battle*` ones (22) are for the battle's own scripts |
| Factions and diplomacy | `FactionFameLevelUp`, `FactionEncountersOtherFaction`, `FactionLeaderDeclaresWar`, `DiplomacyNegotiationStarted`, `DiplomaticOfferRejected`, `PositiveDiplomaticEvent`, `ClanBecomesVassal` |
| Regions and settlements | `RegionRebels`, `RegionRiots`, `RegionSelected`, `SettlementSelected`, `SettlementOccupied`, `GarrisonOccupiedEvent`, `GovernorAssignedCharacterEvent`, `GovernorshipTaxRateChanged` |
| Missions, dilemmas | `MissionIssued`, `MissionSucceeded`, `MissionFailed`, `MissionCancelled`, `Mission*` checks (31), `DilemmaIssuedEvent`, `DilemmaChoiceMadeEvent`, `DillemaOrIncidentStarted`, `IncidentOccuredEvent` |
| Economy and buildings | `BuildingCompleted`, `TradeRouteEstablished`, `ResearchCompleted`, `UnitTrained`, `RecruitmentItemIssuedByPlayer`, `BuildingConstructionIssuedByPlayer` |
| World | `ClimatePhaseChange`, `VictoryConditionMet`, `AreaEntered`, `AreaExited`, `LocationEntered` |
| UI | `PanelOpenedCampaign`, `PanelClosedCampaign`, `ComponentLClickUp`, `ComponentMouseOn`, `ComponentLinkClicked`, `ShortcutTriggered`, `AdviceIssued`, `AdviceDismissed`, `TimeTrigger` |

Spellings are the game's (`DillemaOrIncidentStarted`, `IncidentOccuredEvent`, `GarrisonResidenceCaptured`...). Use the reference list to copy a name exactly: a wrong name is not an error, the listener is just never called.

## Contexts

A context is an object the engine creates per event. Vanilla scripts only use methods that return a game object or a string:

| Accessor | Returns | Events seen in vanilla |
|---|---|---|
| `context:character()` | character | `CharacterCreated`, `CharacterComesOfAge`, `CharacterSelected`, `CharacterTurnStart`, `CharacterCompletedBattle`, `CharacterParticipatedAsSecondaryGeneralInBattle`, `CharacterEntersAttritionalArea`, `CharacterPerformsOccupationDecisionRaze`, `MovementPointsExhausted`, `GarrisonOccupiedEvent` (the character that occupied) |
| `context:faction()` | faction | `FactionTurnStart`, `FactionTurnEnd`, `FactionAboutToEndTurn`, `FactionFameLevelUp`, `FactionEncountersOtherFaction`, `FactionBecomesLiberationVassal`, `TradeRouteEstablished`, `GovernorshipTaxRateChanged`, `DilemmaChoiceMadeEvent`, `MissionIssued` |
| `context:other_faction()` | faction | `FactionEncountersOtherFaction` |
| `context:liberating_character()` | character | `FactionBecomesLiberationVassal` |
| `context:region()` | region | `RegionRebels`, `RegionTurnStart` |
| `context:military_force()` | military force | `CharacterMilitaryForceTraditionPointAvailable` |
| `context:garrison_residence()` | garrison residence (`:region()`, `:faction()`) | `GarrisonOccupiedEvent`, `SettlementSelected`, `BuildingCompleted` (the building's owner) |
| `context:building()` | building (`:chain()`, `:name()`) | `BuildingCompleted` |
| `context:pending_battle()` | pending battle (`has_attacker`, `attacker`, `has_defender`, `defender`) | `PendingBattle` |
| `context:mission()` | mission (`:mission_record_key()`) | `MissionIssued`, `MissionSucceeded`, `MissionFailed` |
| `context:world()` | world (`:climate_phase_index()`) | `ClimatePhaseChange` |
| `context:province()` | the province key (a string) | `GovernorAssignedCharacterEvent` |
| `context:dilemma()` | the dilemma key (string) | `DilemmaChoiceMadeEvent` |
| `context:choice()` | number, the 0 based index of the choice | `DilemmaChoiceMadeEvent` |
| `context.string` | string: the dilemma or incident key, the panel name (`"esc_menu_campaign"`, `"settlement_panel"`), the component id, the timer id | `DillemaOrIncidentStarted` (the key), `PanelOpenedCampaign`, `PanelClosedCampaign`, `ComponentLClickUp`, `ComponentMouseOn`, `ComponentLinkClicked`, `TimeTrigger`, `AreaExited`, `FactionRoundStart` |
| `context.component` | UI component | `UICreated`, `ComponentLClickUp`, `PanelOpenedCampaign` |

Notes:

- The list is **what vanilla uses**, not what the engine offers: the contexts are engine objects and their accessors are not in the [API dump](../reference/README.md) (it lists the script classes in the registry; contexts are not among them). What an accessor does on an event it doesn't belong to has not been tested. To find out for an event, call it under `pcall` from a listener and log the result.
- `context.string` and `context.component` are **fields**, the others are methods.
- The names are the same across events (`faction()`, `character()`), so `FactionTurnStart` and `BuildingCompleted` can use one helper.
- Contexts are valid **during the callback**. Don't keep one for later; read what you need (`cqi`, name) into your own variables.

### The `faction()` of a custom context

The scripted events (below) use `custom_context` from `lib_event_handler.lua`. Its `faction()` accessor returns the **character** data, and `add_data` skips the first extra argument of `trigger_event` (see the [open questions](architecture.md#open-questions)). With the vanilla library, avoid passing data through `trigger_event` and keep the information in your own table or saved value.

## Listening patterns

Taken from the vanilla files.

**Filter in the condition, not in the callback.** The condition runs for every event and decides; the callback does the work. Non persistent listeners are removed only when the **condition matched**:

```lua
cm:add_listener(
    "Hun_Preservation_Turn_Listener",
    "FactionTurnStart",
    function(context) return context:faction():name() == faction_name end,
    function(context) ... end,
    true
);
```

`FactionTurnStart` and `FactionTurnEnd` are raised for **every faction**, AI factions included (the vanilla listeners all test `context:faction():name()` or `is_human()`). Always test the faction.

**One shot listeners.** Pass `false` (or leave the argument out) for "wait for the first time this happens":

```lua
cm:add_listener("wait_for_x", "PanelOpenedCampaign",
    function(context) return context.string == "diplomacy_dropdown" end,
    function(context) ... end,
    false);
```

**A listener per start function.** The vanilla files wrap them in `Add_X_Listeners()` functions that `start_game_all_factions()` calls, so they can be limited (for example not in multiplayer, see [architecture](architecture.md#conventions-in-the-vanilla-code)).

**Pending battle and results.** `PendingBattle` has the attacker and defender (`has_attacker()` first). The vanilla `BattleCompleted` listeners don't read the context at all; they use state set at `PendingBattle`, and the library caches the pending battle in `cm` for this (`pending_battle_cache_faction_was_attacker(name)`, `..._was_defender`, `..._was_involved`, `..._char_was_*`, `..._mf_was_*`). Use them in a `BattleCompleted` listener.

**Missions.** Compare `context:mission():mission_record_key()` in the condition (the pattern of the prologue's scripts). `MissionIssued` also has `context:faction()`; the vanilla `MissionSucceeded` and `MissionFailed` listeners read only the mission.

**Dilemmas.** `DillemaOrIncidentStarted` gives the key in `context.string`; `DilemmaChoiceMadeEvent` gives `context:dilemma()` (the key) and `context:choice()`, the zero based index (vanilla compares with `0` for the first choice).

## Custom events

The library lets scripts define their own events on the same machinery:

```lua
cm:register_event("my_event");                 -- optional: creates the event's table
cm:add_listener("l", "my_event", true, function(context) ... end, true);
cm:trigger_event("my_event", "a string");      -- calls the event's functions with a custom_context
```

- A listener on a name that isn't an engine event attaches to `events["my_event"]`, creating the table (`event_handler:register_event`); engine events are the ones the game calls itself.
- `trigger_event` calls the functions in the event's table **directly and synchronously**, with a `custom_context` (`context.string`, `context:character()`, ...). It's an ordinary function call chain, so a listener that errors stops the rest of the event's functions.
- The only vanilla use is `cm:add_turn_countdown_event(faction, turns, event_name, context_str)`: after `turns` turns of `faction` (counted at its `FactionTurnStart`) the event is triggered with the string. The countdowns are saved with the game; the listeners are not, register them on every start.
- The same goes for `script_messager` ([UI and objectives](ui-and-objectives.md#script_messager)): use it when there is no data.

## Not covered yet

- The accessors each event's context really has. This needs a dump from the game: a listener on every event name that probes the context with `pcall(context.<accessor>, context)` for a list of candidates and writes the results to a file (the same approach as `tools/apigen/dump.lua`).
- Whether `FactionTurnStart` and `FactionBeginTurnPhaseNormal` (the turn manager uses the latter) fire in a fixed order relative to each other and to `CharacterTurnStart`.
- Battle and front end events.
