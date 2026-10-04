#include "stdafx.hpp"

#include "LuaApi.hpp"

#include "../Image.hpp"
#include "../Version.hpp"

#include "../../sdk/Attila/Addresses.hpp"
#include "../../sdk/Attila/Lua/LuaRuntime.hpp"
#include "../../sdk/Attila/VFSTypes.hpp"

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

// DATABASE

using DB_GetContainer_t = void* (*)(void* worldDatabaseField);
using DB_TableAccessor_t = void* (*)(void* container);
using StringHashMap_Find_t = int32_t* (*)(void* map, const sdk::Attila::TempString* key);
using String_ctor_t = sdk::Attila::TempString* (*)(sdk::Attila::TempString* self, const char* utf8);
using String_dtor_t = void (*)(sdk::Attila::TempString* self);

struct DBTableInfo
{
    const char* name;
    uint64_t accessor;
};
constexpr DBTableInfo kTables[] = {
    {"building_levels", Addr::DB_BuildingLevels},
    {"region_to_province_junctions", Addr::DB_RegionToProvinceJunctions},
    {"provinces", Addr::DB_Provinces},
    {"main_units", Addr::DB_MainUnits},
};

// The DB container through the campaign global, nullptr outside a campaign
void* Database()
{
    static const auto getContainer = Resolve<DB_GetContainer_t>(Addr::DB_GetContainer);

    const void* campaign = *Resolve<void* const*>(Addr::g_Campaign);
    const void* handle = campaign ? ReadPtr<const void*>(campaign, Addr::Campaign_Handle) : nullptr;
    uint8_t* world = handle ? ReadPtr<uint8_t*>(handle, Addr::ModelHandle_World) : nullptr;
    return world ? getContainer(world + Addr::World_Database) : nullptr;
}

void* Table(const char* aName)
{
    void* db = Database();
    if (!db)
    {
        return nullptr;
    }
    for (const auto& table : kTables)
    {
        if (std::strcmp(table.name, aName) == 0)
        {
            return Resolve<DB_TableAccessor_t>(table.accessor)(db);
        }
    }
    return nullptr;
}

// The record with the given key, nullptr if the table or the key don't exist
const void* Record(void* aTable, const char* aKey)
{
    static const auto find = Resolve<StringHashMap_Find_t>(Addr::StringHashMap_Find);
    static const auto stringCtor = Resolve<String_ctor_t>(Addr::String_ctor);
    static const auto stringDtor = Resolve<String_dtor_t>(Addr::String_dtor);

    sdk::Attila::TempString key;
    stringCtor(&key, aKey);
    const int32_t* index = find(static_cast<uint8_t*>(aTable) + Addr::DBTable_Index, &key);
    stringDtor(&key);

    const auto count = ReadPtr<uint32_t>(aTable, Addr::DBTable_Count);
    if (!index || static_cast<uint32_t>(*index) >= count)
    {
        return nullptr;
    }
    return ReadPtr<void* const*>(aTable, Addr::DBTable_Records)[*index];
}

uint32_t RecordCount(const void* aTable)
{
    return ReadPtr<uint32_t>(aTable, Addr::DBTable_Count);
}

const void* RecordAt(const void* aTable, uint32_t aIndex)
{
    return aIndex < RecordCount(aTable) ? ReadPtr<void* const*>(aTable, Addr::DBTable_Records)[aIndex] : nullptr;
}

// A TempString inside a record (keys are at +0), nullptr if empty
const char* StringAt(const void* aBase, uint64_t aOffset)
{
    const auto* string = reinterpret_cast<const sdk::Attila::TempString*>(static_cast<const uint8_t*>(aBase) + aOffset);
    return string->length > 0 && string->data ? string->data : nullptr;
}

void SetNumber(lua_State* L, const char* aField, const void* aRecord, uint64_t aOffset)
{
    LuaRuntime::pushnumber(L, static_cast<lua_Number>(ReadPtr<int32_t>(aRecord, aOffset)));
    LuaRuntime::setfield(L, -2, aField);
}

// The record for the string key argument 1 in the named table, raises a Lua error without a campaign
const void* KeyArgRecord(lua_State* L, const char* aTable, const char* aFunction)
{
    const char* key = LuaRuntime::type(L, 1) == LUA_TSTRING ? LuaRuntime::tolstring(L, 1, nullptr) : nullptr;
    if (!key)
    {
        LuaRuntime::error(L, "%s(key): expects a key string", aFunction);
        return nullptr;
    }
    void* table = Table(aTable);
    if (!table)
    {
        LuaRuntime::error(L, "%s: the DB isn't available outside a campaign", aFunction);
        return nullptr;
    }
    return Record(table, key);
}

// twase.building_level(key) -> { level, create_time, create_cost, upkeep_cost, chain } or nil
// From the loaded building_levels table (game and mods merged).
int BuildingLevel(lua_State* L)
{
    const void* record = KeyArgRecord(L, "building_levels", "twase.building_level");
    if (!record)
    {
        LuaRuntime::pushnil(L);
        return 1;
    }
    LuaRuntime::createtable(L, 0, 5);
    SetNumber(L, "level", record, Addr::BuildingLevel_Level);
    SetNumber(L, "create_time", record, Addr::BuildingLevel_CreateTime);
    SetNumber(L, "create_cost", record, Addr::BuildingLevel_CreateCost);
    SetNumber(L, "upkeep_cost", record, Addr::BuildingLevel_UpkeepCost);
    const void* chain = ReadPtr<const void*>(record, Addr::BuildingLevel_Chain);
    if (const char* chainKey = chain ? StringAt(chain, 0) : nullptr)
    {
        LuaRuntime::pushstring(L, chainKey);
        LuaRuntime::setfield(L, -2, "chain");
    }
    return 1;
}

// twase.unit(key) -> { recruitment_cost, upkeep_cost, num_men, create_time } or nil
// From the loaded main_units table (game and mods merged).
int Unit(lua_State* L)
{
    const void* record = KeyArgRecord(L, "main_units", "twase.unit");
    if (!record)
    {
        LuaRuntime::pushnil(L);
        return 1;
    }
    LuaRuntime::createtable(L, 0, 4);
    SetNumber(L, "recruitment_cost", record, Addr::MainUnit_RecruitmentCost);
    SetNumber(L, "upkeep_cost", record, Addr::MainUnit_UpkeepCost);
    SetNumber(L, "num_men", record, Addr::MainUnit_NumMen);
    SetNumber(L, "create_time", record, Addr::MainUnit_CreateTime);
    return 1;
}

// twase.region_province(region) -> province key or nil
// The province a region belongs to, from the loaded region_to_province_junctions table.
int RegionProvince(lua_State* L)
{
    const void* scriptInterface = ScriptInterfaceArg(L, 1, "REGION_SCRIPT_INTERFACE");
    const void* region = scriptInterface ? ReadPtr<const void*>(scriptInterface, Addr::RegionScriptInterface_Region) : nullptr;
    const void* regionRecord = region ? ReadPtr<const void*>(region, Addr::Region_Record) : nullptr;
    const char* regionKey = regionRecord ? StringAt(regionRecord, 0) : nullptr;
    if (!regionKey)
    {
        return LuaRuntime::error(L, "twase.region_province(region): expects a region script interface");
    }

    const void* table = Table("region_to_province_junctions");
    const uint32_t count = table ? RecordCount(table) : 0;
    for (uint32_t i = 0; i < count; i++)
    {
        const void* record = RecordAt(table, i);
        const char* key = record ? StringAt(record, Addr::RegionToProvince_Region) : nullptr;
        if (key && std::strcmp(key, regionKey) == 0)
        {
            LuaRuntime::pushstring(L, StringAt(record, Addr::RegionToProvince_Province));
            return 1;
        }
    }
    LuaRuntime::pushnil(L);
    return 1;
}

// twase.dev_db_record(table, key_or_index, bytes) -> hex string, record count
// Development helper for finding record layouts: the first `bytes` bytes of the record as hex.
int DevDbRecord(lua_State* L)
{
    const char* tableName = LuaRuntime::tolstring(L, 1, nullptr);
    const bool byIndex = LuaRuntime::type(L, 2) == LUA_TNUMBER;
    const char* key = byIndex ? "" : LuaRuntime::tolstring(L, 2, nullptr);
    const int bytes = LuaRuntime::type(L, 3) == LUA_TNUMBER ? static_cast<int>(LuaRuntime::tonumber(L, 3)) : 0x100;
    if (!tableName || !key || bytes <= 0 || bytes > 0x1000)
    {
        return LuaRuntime::error(L, "twase.dev_db_record(table, key[, bytes]): bad arguments");
    }

    void* table = Table(tableName);
    if (!table)
    {
        return LuaRuntime::error(L, "twase.dev_db_record: unknown table '%s' or no campaign", tableName);
    }
    const void* record = byIndex ? RecordAt(table, static_cast<uint32_t>(LuaRuntime::tonumber(L, 2))) : Record(table, key);
    if (!record)
    {
        LuaRuntime::pushnil(L);
        LuaRuntime::pushnumber(L, static_cast<lua_Number>(ReadPtr<uint32_t>(table, Addr::DBTable_Count)));
        return 2;
    }

    static char hex[0x2000 + 1];
    const auto* data = static_cast<const uint8_t*>(record);
    for (int i = 0; i < bytes; i++)
    {
        std::snprintf(hex + i * 2, 3, "%02x", data[i]);
    }
    LuaRuntime::pushstring(L, hex);
    LuaRuntime::pushnumber(L, static_cast<lua_Number>(ReadPtr<uint32_t>(table, Addr::DBTable_Count)));
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
    LuaRuntime::createtable(L, 0, 8);

    LuaRuntime::pushstring(L, TWASE_VERSION_STR);
    LuaRuntime::setfield(L, -2, "version");

    SetFunction(L, "faction_attitude", &FactionAttitude);
    SetFunction(L, "region_wealth", &RegionWealth);
    SetFunction(L, "building_level", &BuildingLevel);
    SetFunction(L, "unit", &Unit);
    SetFunction(L, "region_province", &RegionProvince);
    SetFunction(L, "dev_db_record", &DevDbRecord);

    LuaRuntime::setfield(L, LUA_GLOBALSINDEX, "twase");
    spdlog::info("[LuaApi] Registered the twase table");
}
