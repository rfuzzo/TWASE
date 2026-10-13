# Diplomacy

How a negotiation, its actions and the AI's deal evaluation work, and the region trading Attila inherited from Rome II. Offsets and addresses per build: [Mac](../macos/addresses.md).

## Negotiations and actions

Opening diplomacy with a faction creates a **negotiation** between two factions (A and B). It holds one **diplomatic action** object per action type the campaign has, not indexed by type. Each action has:

- its side (offer or demand) and its type
- an **available** flag (shown in "Add Offer/Demand") and a **blocked** flag
- action specific data, e.g. the region lists of the region action

Action types (as in the button ids `diplomac_action_possible_<type>`):

| Type | Action | Type | Action |
|---|---|---|---|
| 0 | trade agreement | 14 | join war |
| 1 | cancel trade agreement | 15 | break trade agreement |
| 4 | military alliance | 16 | break military alliance |
| 5 | cancel military alliance | 19 | non-aggression pact |
| 6 | trade regions ("Gift Region") | 21 | military access |
| 9 | payments | 22 | cancel military access |
| 10 | create puppet state | 23 | defensive alliance |
| 12 | peace treaty | 25 | make tributary state |
| 13 | declare war | 28–31 | break / cancel pacts and puppet status |

At war, only peace is offered; once peace is in the deal, the others follow. "Make tributary state" is only offered to a faction that owns land.

Scripts can allow or forbid an action between two factions with `force_diplomacy` (2-bit permissions per relationship; the region action's permission is `"regions"`).

### The panel

- The "Add Offer/Demand" list is rebuilt from the available offer-side actions; each entry is a button for one action.
- A click on an entry goes to a dispatcher by action type:
  - most simple types (e.g. peace) add the action with empty terms
  - "make tributary state" sets the direction first. With empty terms the game would make *you* the tributary.
  - the region action opens the region panel
- The panel's "Likelihood of success" shows low / moderate / high (see below).

## Deal evaluation

The AI evaluates a proposed deal from its point of view:

1. One **deal component** is built per action in the deal (`CAI_DEAL_COMPONENT_*`). The evaluator switches over the action's **position in the negotiation's action array**, not its type: e.g. position 6 is Payments, type 9.
2. Each component is scored by the AI's personality and its relationship with the other side; the scores are summed.
3. The AI **accepts at a score of 0 or more**.

The likelihood shown in the UI buckets the same score with two tweakers, `CAI_DIPLOMACY_NEGOTIATION_DISPLAYED_LIKELIHOOD_THRESHOLD_LOW` / `_HIGH` (defaults -4 / 4): below low is "low", above high is "high", "moderate" in between. Forced outcomes (e.g. gifts) bypass the bucket. TWASE's deal score tweak writes the score into the tooltip.

### Gold (Payments)

The gift component only has a deal value when the AI pays; received gold only gives a goodwill event. Its score grows with the amount relative to the paying faction's GDP and to the AI's budget, weighted by its personality and capped.

The size class (small / medium / large) is `gift_base_value_*` (500 / 2,000 / 7,500) times the **economic power** of the gift's second faction:

```text
clamp(GDP / economic_power_gdp_factor + treasury / economic_power_treasury_factor, min, max)
```

The four values are 8,000, 10,000, 0.2 and 4, in that order. Small amounts relative to a rich AI's treasury score 0.

## Region trading

Attila keeps Rome II's region trading in full:

- the action type 6 (`DIPLOMATIC_ACTION_TRADE_REGIONS`)
- its permission (`"regions"`)
- the game command `CCQ_DIPLOMACY_PROPOSE_TRADE_REGIONS`
- the UI: the `trade_regions` panel with `list_offers` and `list_demands`, and the deal list row

Vanilla uses it only as **"Gift Region" to settle hordes**:

- **Available** only between a human non-horde and a horde that can own land and isn't at war with it.
- **Regions:** the giving side's regions except its capital and settlements with a garrisoned army or navy, and only those where an army of the receiver stands.
- **The panel** ticks one region at a time (one selection for both lists) and hides `list_demands`.
- **Executing** the deal transfers the regions of each list to its receiving faction, through the same settlement transfer as `transfer_region_to_faction` (with a different reason code). Both update the map at once.
- **A region scores +0.0.** The evaluation has no region component.
- **Transferring a province capital kills its governor.**

With `twase.enable_region_trading` ([Lua API](../modding/lua-api.md#region-trading)) TWASE widens this for a human and a settled AI faction:

- **Availability:** whenever the human has a region to give.
- **Regions:** every region of the giving side except the capital and garrisoned ones, for offers and demands.
- **Panel:** shows `list_demands` and keeps any number of ticked rows in both lists; OK puts them all into the deal, so one deal can swap regions.
- **Deal list row:** shows each column's own regions (the engine shows the demanded regions in both columns).
- **Evaluation:** adds the regions' value as if the AI paid or received that much gold: the negated score of a gift of the mod's value.
- **Hordes:** vanilla's horde case is also offered at war (the entries wait for peace in the deal, like every other term).

### Region for Troops

A second entry for the region action in "Add Offer/Demand" opens the same panel in troops mode:
- **Left:** your regions.
- **Right:** the warriors the AI offers for them, which come from the mod's `federates` handler.
- **Deal terms:** the deal also gets peace and, for a faction with land, "make tributary state".
- **Score:** the mod's `willingness` replaces the native scores of those two terms.

A people's own levy pool (what can be levied from them) is in the DB: `faction_to_mercenary_set_junctions` → `mercenary_pool_to_groups_junctions` → `mercenary_unit_groups` (`twase.levy_pool`). The recruitment panel offers "Recruit from Horde" when a source faction is a horde; "Levy from tributary" and "Levy from puppet state" are other sources of the same levy manager.

### Garrisons

In Lua, a settlement's garrison is a military force whose "general" is a `colonel` (or the province's governor, a `general`) and that costs no upkeep (`force:upkeep() == 0`); `has_garrison_residence()` is true for any army inside a settlement.
