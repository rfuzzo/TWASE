#pragma once

// Which twase.* functions this platform has the addresses for (src/core/Scripting/LuaApi.cpp).
// See docs/windows-port.md for what each one still needs from empire.retail.dll.
#define TWASE_HAS_FACTION_ATTITUDE 1    // twase.faction_attitude
#define TWASE_HAS_REGION 0              // twase.region_wealth
#define TWASE_HAS_DATABASE 0            // twase.building_level, twase.unit, twase.levy_pool, twase.region_province, dev_db_*
#define TWASE_HAS_LOCALISATION 0        // twase.localise
#define TWASE_HAS_REGION_TRADING 0      // twase.enable_region_trading, dev_transfer_region
#define TWASE_HAS_DIPLOMACY_ACTIONS 0   // dev_diplomacy_actions
