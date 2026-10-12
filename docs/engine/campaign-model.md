# Campaign model

The game objects behind the campaign's script interfaces, and how to reach the loaded database and the localisation. Offsets per build: [Mac](../macos/addresses.md).

## Reaching the world

- A **faction** points to a model handle, which points to the campaign **world**.
- The world holds the campaign AI interface (`CAI_INTERFACE`), the database container and most global state.
- Without a faction at hand, the campaign global that the UI uses leads to the same world.

## Factions (`FACTION`)

Known fields:

- the faction's `factions` DB record (its key is the record's first field)
- an id, used as the key of the relationship map: per other faction a relationship object (alliances, war and peace, the diplomatic permissions)
- alive marker: a destroyed faction has it at 0 (`is_dead`)
- is human, is a horde, can own land (false e.g. for the Huns as a horde; checked by `transfer_region_to_faction` too)
- capital, the list of its regions (the Lua `region_list`), treasury

`CAI_INTERFACE::get_faction_attitude(a, b)` sums the attitude modifiers of `a` towards `b` and rounds them: the number the diplomacy screen shows (`twase.faction_attitude`).

## Regions (`REGION`)

Known fields:

- the `regions` DB record (the Lua `name()` returns its key)
- the owning faction (`owning_faction`), the settlement, the campaign province object
- **wealth**: an int, the region's GDP before tax as the province panel shows it
- **wealth breakdown**: 15 categories of `{ int base, int total, float bonus_percent }`, in the order of the loc keys `campaign_localised_strings_string_gdp_*` (subsistence, animal_husbandry, farming, local_trade, land_trade, sea_trade, entertainment, learning, mining, manufacture, other, fertility, fertility_agriculture, fertility_livestock, poor_fertility_bonus). This is the data of the panel's wealth tooltip.
- town wealth growth, the wealth change percentage, squalor and sanitation (which read the same sub-object)

## Database

The game loads the DB tables (game data merged with every mod's) into a container reached from the world. Each table has its own accessor function that loads the table on first use (logging `Loading database: <table>`) and caches it in the container.

A loaded table has:

- a hash index from key to record index (looked up with a `CA::String` key)
- an array of record pointers and the record count

Records start with their key and then follow the schema's column order. References to other tables are stored as **pointers to the referenced record**, not as keys; the referenced key is that record's first field. Junction tables have composite keys and aren't indexed usefully, so they are scanned.

Record layouts can be found by dumping records in the game (`twase.dev_db_record`, `twase.dev_db_fields`) and matching them against the tables exported from the packs.

## Localisation

The localisation manager hangs off the DB container. Its lookup is a virtual function: `(loc, key, 1)` returns the UTF-16 text (`CA::UniString`), or an empty or null string for a missing key (`twase.localise`).

## Strings

- `CA::String`: `{ u32 length, u32 capacity, char* data }`, UTF-8.
- `CA::UniString`: the same with `char16_t* data`; the data has an allocation header in front.
- A few game functions return a string by value. On Mac (arm64) that goes through the indirect result register `x8`; declaring the C++ return type with a non-trivial destructor makes clang use the same convention.
