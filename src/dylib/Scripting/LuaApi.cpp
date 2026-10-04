#include "stdafx.hpp"

#include "LuaApi.hpp"

#include "../Image.hpp"
#include "../Version.hpp"

#include "../../sdk/Attila/Addresses.hpp"
#include "../../sdk/Attila/Lua/LuaRuntime.hpp"

// The C functions below can raise Lua errors, which longjmp out of them: keep them free of objects with destructors.

namespace
{
namespace Addr = sdk::Attila::Addresses;

using FactionScriptInterface_GetArg_t = bool (*)(lua_State* L, int idx, void** out);
using CAI_GetFactionAttitude_t = int (*)(void* cai, const void* factionA, const void* factionB);

template<typename T>
T Resolve(uint64_t aAddress)
{
    return reinterpret_cast<T>(aAddress + Image::Get()->GetSlide());
}

template<typename T>
T ReadPtr(const void* aBase, uint64_t aOffset)
{
    return *reinterpret_cast<T const*>(static_cast<const uint8_t*>(aBase) + aOffset);
}

// FACTION* of a faction script interface argument, nullptr if it isn't one or the faction is destroyed.
// Raises a Lua type error for a userdata of another type, like the game's own methods.
const void* FactionArg(lua_State* L, int aIdx)
{
    static const auto getArg = Resolve<FactionScriptInterface_GetArg_t>(Addr::FactionScriptInterface_GetArg);

    void* scriptInterface = nullptr;
    if (!getArg(L, aIdx, &scriptInterface) || !scriptInterface)
    {
        return nullptr;
    }

    const void* faction = ReadPtr<const void*>(scriptInterface, Addr::FactionScriptInterface_Faction);
    if (!faction || !ReadPtr<const void*>(faction, Addr::Faction_AliveMarker))
    {
        return nullptr;
    }
    return faction;
}

// twase.faction_attitude(faction_a, faction_b) -> number or nil
// a's attitude towards b (the value the diplomacy screen shows), nil for destroyed factions or a == b.
// The arguments are faction script interfaces, e.g. get_cm():model():world():faction_by_key(key).
int FactionAttitude(lua_State* L)
{
    static const auto getAttitude = Resolve<CAI_GetFactionAttitude_t>(Addr::CAI_GetFactionAttitude);

    const void* a = FactionArg(L, 1);
    const void* b = FactionArg(L, 2);
    if (!a || !b || a == b)
    {
        LuaRuntime::pushnil(L);
        return 1;
    }

    const void* handle = ReadPtr<const void*>(a, Addr::Faction_ModelHandle);
    const void* world = handle ? ReadPtr<const void*>(handle, Addr::ModelHandle_World) : nullptr;
    void* cai = world ? ReadPtr<void*>(world, Addr::World_CAIInterface) : nullptr;
    if (!cai)
    {
        return LuaRuntime::error(L, "twase.faction_attitude: campaign AI not available");
    }

    LuaRuntime::pushnumber(L, static_cast<lua_Number>(getAttitude(cai, a, b)));
    return 1;
}

// The script interface pointer of a Lunar userdata argument of the given class, nullptr otherwise.
void* ScriptInterfaceArg(lua_State* L, int aIdx, const char* aClassName)
{
    if (LuaRuntime::type(L, aIdx) != LUA_TUSERDATA || !LuaRuntime::getmetatable(L, aIdx))
    {
        return nullptr;
    }
    LuaRuntime::getfield(L, LUA_REGISTRYINDEX, aClassName);
    const bool isClass = LuaRuntime::rawequal(L, -1, -2) != 0;
    LuaRuntime::settop(L, -3);

    void** userdata = isClass ? static_cast<void**>(LuaRuntime::touserdata(L, aIdx)) : nullptr;
    return userdata ? *userdata : nullptr;
}

// REGION + 0x7a0, as read by the wealth breakdown tooltip (0x1034d2444)
struct RegionWealthCategory
{
    int32_t base;
    int32_t total;
    float bonusPercent;
};
static_assert(sizeof(RegionWealthCategory) == 12);

// The order of the game's category tables (loc keys campaign_localised_strings_string_gdp_* at 0x104bc7590)
constexpr const char* kWealthCategories[Addr::RegionWealthCategoryCount] = {
    "subsistence", "animal_husbandry", "farming",  "local_trade", "land_trade",
    "sea_trade",   "entertainment",    "learning", "mining",      "manufacture",
    "other",       "fertility",        "fertility_agriculture", "fertility_livestock", "poor_fertility_bonus"};

// twase.region_wealth(region) -> total, breakdown
// The region's wealth (its GDP before tax, the number the province panel shows) and a table
// { <category> = { base = n, total = n, bonus_percent = n } } with the non-zero categories.
// The argument is a region script interface, e.g. get_cm():model():world():region_manager():region_by_key(key).
int RegionWealth(lua_State* L)
{
    const void* scriptInterface = ScriptInterfaceArg(L, 1, "REGION_SCRIPT_INTERFACE");
    const void* region = scriptInterface ? ReadPtr<const void*>(scriptInterface, Addr::RegionScriptInterface_Region) : nullptr;
    if (!region)
    {
        return LuaRuntime::error(L, "twase.region_wealth(region): expects a region script interface");
    }

    LuaRuntime::pushnumber(L, static_cast<lua_Number>(ReadPtr<int32_t>(region, Addr::Region_Wealth)));

    const auto* categories = reinterpret_cast<const RegionWealthCategory*>(static_cast<const uint8_t*>(region) + Addr::Region_WealthBreakdown);
    LuaRuntime::createtable(L, 0, Addr::RegionWealthCategoryCount);
    for (int i = 0; i < Addr::RegionWealthCategoryCount; i++)
    {
        const auto& category = categories[i];
        if (category.base == 0 && category.total == 0)
        {
            continue;
        }
        LuaRuntime::createtable(L, 0, 3);
        LuaRuntime::pushnumber(L, static_cast<lua_Number>(category.base));
        LuaRuntime::setfield(L, -2, "base");
        LuaRuntime::pushnumber(L, static_cast<lua_Number>(category.total));
        LuaRuntime::setfield(L, -2, "total");
        LuaRuntime::pushnumber(L, category.bonusPercent);
        LuaRuntime::setfield(L, -2, "bonus_percent");
        LuaRuntime::setfield(L, -2, kWealthCategories[i]);
    }
    return 2;
}

void SetFunction(lua_State* L, const char* aName, lua_CFunction aFunction)
{
    LuaRuntime::pushcclosure(L, aFunction, 0);
    LuaRuntime::setfield(L, -2, aName);
}
} // namespace

void Scripting::LuaApi::Register(lua_State* L)
{
    LuaRuntime::createtable(L, 0, 4);

    LuaRuntime::pushstring(L, TWASE_VERSION_STR);
    LuaRuntime::setfield(L, -2, "version");

    SetFunction(L, "faction_attitude", &FactionAttitude);
    SetFunction(L, "region_wealth", &RegionWealth);

    LuaRuntime::setfield(L, LUA_GLOBALSINDEX, "twase");
    spdlog::info("[LuaApi] Registered the twase table");
}
