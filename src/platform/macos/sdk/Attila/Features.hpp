#pragma once

// Which twase.* functions this platform has the addresses for (src/core/Scripting/LuaApi.cpp)
#define TWASE_HAS_FACTION_ATTITUDE 1    // twase.faction_attitude
#define TWASE_HAS_REGION 1              // twase.region_wealth
#define TWASE_HAS_DATABASE 1            // the loaded DB tables, dev_db_record, dev_db_fields
#define TWASE_HAS_DB_RECORDS 1          // their record layouts: twase.building_level, twase.unit, twase.levy_pool, twase.region_province
#define TWASE_HAS_LOCALISATION 1        // twase.localise
#define TWASE_HAS_REGION_TRADING 1      // twase.enable_region_trading, dev_transfer_region: rules, evaluation, transfer
#define TWASE_HAS_REGION_TRADING_PANEL 1 // its panel (both lists, multi-select), deal list labels, Region for Troops
#define TWASE_HAS_GIFT_SCORE_PARTS 1    // the debug log's breakdown of the gift score (the gift component's vtable)
#define TWASE_HAS_DIPLOMACY_ACTIONS 1   // dev_diplomacy_actions
