# Lua API (`twase.*`)

TWASE adds a global table `twase` to the campaign's Lua environment, before it loads the [script mods](script-mods.md). It gives scripts what the game's own campaign API doesn't expose: the AI's attitude, a region's wealth breakdown, the loaded database, the localisation, and the engine's region deals.

> **Platforms:** the `twase` table is in **TWASE 1.1.0** on Windows and Mac (only the probe `twase.dev_diplomacy_actions` is Mac only). Older versions don't have it: check with `if twase then ... end`.

Conventions:

- The game's Lua numbers are **32-bit floats** (`lua_Number` is `float` in Attila), so integers are exact only up to 16,777,216. Fine for gold, unit counts and keys, not for large ids or sums.
- "Faction" and "region" arguments are the game's script interfaces, e.g. `get_cm():model():world():faction_by_key("att_fact_western_roman_empire")` and `get_cm():model():world():region_manager():region_by_key("att_reg_lusitania_olisipo")`. Functions that take a key say so.
- Functions that read the database or the localisation only work inside a campaign; outside one they raise a Lua error.
- Wrong argument types raise a Lua error (catch it with `pcall` if you pass untrusted values).

## Version

### `twase.version`

The TWASE version string, e.g. `"1.1.0"`.

## Factions and regions

### `twase.faction_attitude(faction_a, faction_b) -> number | nil`

`faction_a`'s attitude towards `faction_b`: the number the diplomacy screen shows (the sum of the attitude modifiers, rounded). `nil` if either faction is destroyed or both are the same.

```lua
local world = get_cm():model():world();
local attitude = twase.faction_attitude(world:faction_by_key("att_fact_eastern_roman_empire"),
                                        world:faction_by_key("att_fact_western_roman_empire"));
```

### `twase.region_wealth(region) -> total, breakdown`

The region's wealth (its GDP before tax, the number in the province panel) and the breakdown the panel's wealth tooltip shows, as a table of the non-zero categories:

```lua
local total, breakdown = twase.region_wealth(region);
-- total = 373
-- breakdown = { farming = { base = 33, total = 33, bonus_percent = 0 },
--               fertility_agriculture = { base = 164, total = 164, bonus_percent = 0 },
--               sea_trade = { base = 176, total = 176, bonus_percent = 0 } }
```

Categories: `subsistence`, `animal_husbandry`, `farming`, `local_trade`, `land_trade`, `sea_trade`, `entertainment`, `learning`, `mining`, `manufacture`, `other`, `fertility`, `fertility_agriculture`, `fertility_livestock`, `poor_fertility_bonus`. The UI's names for them are the loc keys `campaign_localised_strings_string_gdp_<category>`.

### `twase.region_province(region) -> string | nil`

The key of the province the region belongs to (from `region_to_province_junctions`), e.g. `"att_prov_lusitania"`.

## Database

These read the **loaded** database tables: the game's data merged with every mod's, so they return what the campaign actually uses.

### `twase.building_level(key) -> table | nil`

A `building_levels` record: `{ level, create_time, create_cost, upkeep_cost, chain }` (`chain` is the building chain's key). `nil` for an unknown key.

### `twase.unit(key) -> table | nil`

A `main_units` record: `{ recruitment_cost, upkeep_cost, num_men, create_time }`. `nil` for an unknown key.

### `twase.levy_pool(faction_key) -> table | nil`

A faction's own levy pool: the units of that people others can levy from it (e.g. from a friendly horde), through `faction_to_mercenary_set_junctions` → `mercenary_pool_to_groups_junctions` → `mercenary_unit_groups`. A table keyed by unit group:

```lua
{ [group_key] = { unit = <main_units key>, group = group_key, max_count = n, initial_count = n,
                  max_replenish_per_turn = n, chance_to_replenish = n } }
```

`nil` if the faction has no pool. Takes a faction **key**, not a script interface.

## Localisation

### `twase.localise(key) -> string | nil`

The text of a loc key in the player's language, as UTF-8, e.g. `twase.localise("regions_onscreen_att_reg_lusitania_olisipo")`. `nil` for a missing key.

## Region trading

### `twase.enable_region_trading(handlers)`

Switches on the engine's own region deals between the player and a settled AI faction. Attila still has Rome II's region trading (the diplomacy action behind "Gift Region", which vanilla only offers for settling a horde); TWASE widens it (see [Diplomacy](../engine/diplomacy.md#region-trading)):

- the "Gift Region" option between a human and any settled AI faction, if the human has a region to give
- both sides can give any region except their capital and garrisoned settlements (offers and demands, also several in one deal)
- the AI scores the regions in the deal with the value your `value` handler returns, as if it paid or received that much gold
- "Region for Troops": a second region entry in "Add Offer/Demand" for giving regions in return for federate troops

`handlers` is a table of functions; only `value` is required. `twase.enable_region_trading(nil)` switches it off again.

| Handler | Called | Returns |
|---|---|---|
| `value(region_key, faction_key, other_key)` | when the AI evaluates a deal with regions, for each region | the region's gold value **for `faction_key`** in a deal with `other_key` (negative = a burden). Without a value the regions would be free. |
| `transfer(phase, region_key, to_faction_key)` | for an accepted deal: `"before"` for each region, then the engine transfers them, then `"after"` for each | nothing. E.g. move a governor out of a province capital, which would otherwise die with the transfer. |
| `federates(ai_key, player_key, region_key...)` | when the Region for Troops panel shows the AI's offer for the ticked regions | `count, cost, reason, names`: number of units, their recruitment cost, the reason when there are none (shown instead), their names one per line |
| `willingness(ai_key, player_key)` | when the AI evaluates a deal for troops | the AI's willingness to become your federates, in deal score points; it replaces the native scores of the peace / tributary terms in that deal |
| `foedus(phase, ai_key, player_key, tributary, region_key...)` | around the transfer of an accepted deal for troops, `"before"` and `"after"` | nothing. `tributary` is whether the deal has the native "make tributary" term (the engine doesn't offer it to a faction without land). Give the troops here. |

Errors in a handler are logged (`[RegionTrading] ... handler failed: ...`) and count as 0 / no offer.

```lua
twase.enable_region_trading({
    value = function(region_key, faction_key, other_key)
        return 1000; -- every region is worth 1,000 gold to everyone
    end,
});
```

A deal's score is the sum of its parts, and the AI accepts at 0 or more; the "Likelihood of success" tooltip shows the score when the deal score tweak is on.

## Development probes

With `lua_dev_functions = true` in the `[dev]` section of `config.ini`, the table also has probes for reverse engineering. They read raw memory and may change or go away in any version; don't use them in mods.

| Function | Returns |
|---|---|
| `twase.dev_db_record(table, key_or_index[, bytes])` | the first `bytes` (default 256) of a loaded DB record as hex, and the table's record count. Tables: the ones the API above reads |
| `twase.dev_db_fields(table, index[, bytes])` | each 8-byte slot of record `index` as a string (`off:'text'`), a reference to another record (`off:->key`) or two int32 / float values |
| `twase.dev_diplomacy_actions()` | the open diplomacy screen's actions as `type:side:available:blocked` |
| `twase.dev_transfer_region(region, faction, flag)` | transfers a region the way an accepted region deal does |
| `twase.dev_last_deal_score()` | the deal score the likelihood tooltip showed last |
