#pragma once

// Which twase.* functions this platform has the addresses for (src/core/Scripting/LuaApi.cpp)
#define TWASE_HAS_FACTION_ATTITUDE 1    // twase.faction_attitude
#define TWASE_HAS_REGION 1              // twase.region_wealth
#define TWASE_HAS_DATABASE 1            // twase.building_level, twase.unit, twase.levy_pool, twase.region_province, dev_db_*
#define TWASE_HAS_LOCALISATION 1        // twase.localise
#define TWASE_HAS_REGION_TRADING 1      // twase.enable_region_trading, dev_transfer_region
#define TWASE_HAS_DIPLOMACY_ACTIONS 1   // dev_diplomacy_actions
