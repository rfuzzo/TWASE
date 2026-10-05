#include "stdafx.hpp"

#include "LuaApi.hpp"

#include <mach/mach.h>
#include <mach/mach_vm.h>

#include "../Image.hpp"
#include "../Hooks/DiplomacyLikelihood.hpp"
#include "../Hooks/NativeRegionTrading.hpp"
#include "../Version.hpp"

#include "../../sdk/Attila/Addresses.hpp"
#include "../../sdk/Attila/Lua/LuaRuntime.hpp"
#include "../../sdk/Attila/UITypes.hpp"
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

using Localisation_Lookup_t = const sdk::Attila::WString* (*)(void* loc, const sdk::Attila::TempString* key, int flag);

// UTF-16 -> UTF-8 into aOut (NUL terminated, truncated to aSize)
void ToUtf8(const char16_t* aData, uint32_t aLength, char* aOut, size_t aSize)
{
    size_t n = 0;
    auto put = [&](char c) { if (n + 1 < aSize) aOut[n++] = c; };
    for (uint32_t i = 0; i < aLength; i++)
    {
        uint32_t cp = aData[i];
        if (cp >= 0xD800 && cp < 0xDC00 && i + 1 < aLength && aData[i + 1] >= 0xDC00 && aData[i + 1] < 0xE000)
        {
            cp = 0x10000 + ((cp - 0xD800) << 10) + (aData[++i] - 0xDC00);
        }
        if (cp < 0x80)
        {
            put(static_cast<char>(cp));
        }
        else if (cp < 0x800)
        {
            put(static_cast<char>(0xC0 | (cp >> 6)));
            put(static_cast<char>(0x80 | (cp & 0x3F)));
        }
        else if (cp < 0x10000)
        {
            put(static_cast<char>(0xE0 | (cp >> 12)));
            put(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            put(static_cast<char>(0x80 | (cp & 0x3F)));
        }
        else
        {
            put(static_cast<char>(0xF0 | (cp >> 18)));
            put(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            put(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            put(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }
    aOut[n] = '\0';
}

// twase.localise(key) -> string or nil
// The localised text of a loc key in the player's language, e.g. "regions_onscreen_att_reg_lusitania_olisipo".
int Localise(lua_State* L)
{
    static const auto stringCtor = Resolve<String_ctor_t>(Addr::String_ctor);
    static const auto stringDtor = Resolve<String_dtor_t>(Addr::String_dtor);
    static char utf8[0x2000];

    const char* key = LuaRuntime::type(L, 1) == LUA_TSTRING ? LuaRuntime::tolstring(L, 1, nullptr) : nullptr;
    if (!key)
    {
        return LuaRuntime::error(L, "twase.localise(key): expects a key string");
    }
    void* db = Database();
    void* loc = db ? ReadPtr<void*>(db, Addr::Database_Localisation) : nullptr;
    if (!loc)
    {
        return LuaRuntime::error(L, "twase.localise: the localisation isn't available outside a campaign");
    }
    const auto lookup = ReadPtr<Localisation_Lookup_t>(ReadPtr<const void*>(loc, 0), Addr::Localisation_LookupVtableOffset);

    sdk::Attila::TempString keyString;
    stringCtor(&keyString, key);
    const sdk::Attila::WString* text = lookup(loc, &keyString, 1);
    stringDtor(&keyString);

    if (!text || text->length == 0 || !text->data)
    {
        LuaRuntime::pushnil(L);
        return 1;
    }
    ToUtf8(text->data, text->length, utf8, sizeof(utf8));
    LuaRuntime::pushstring(L, utf8);
    return 1;
}

// Reads that fail cleanly on bad addresses, for the dev probes
template<typename T>
bool SafeRead(uintptr_t aAddress, T& aOut)
{
    mach_vm_size_t read = 0;
    return mach_vm_read_overwrite(mach_task_self(), aAddress, sizeof(T), reinterpret_cast<mach_vm_address_t>(&aOut), &read) == KERN_SUCCESS &&
           read == sizeof(T);
}

constexpr uint64_t kDiplomacyDropdownVtable = 0x104CC6A38; // UIDLL::DiplomacyDropdown (RTTI typeinfo 0x104cc6d38)

// twase.dev_diplomacy_actions() -> "type:side:available:blocked ..." or an error text
// Development helper: the negotiation's diplomatic actions of the open diplomacy screen (the DiplomacyDropdown
// the likelihood hook saw last, checked by its vtable).
// DiplomacyDropdown + 0x200 -> + 0xd0 = negotiation, + 0x700 = action list (count + 0xc, DIPLOMATIC_ACTION** + 0x10);
// action: + 0x10 side (1 offer), + 0x14 type, + 0x18 available, + 0x19 blocked.
int DevDiplomacyActions(lua_State* L)
{
    const auto dropdown = reinterpret_cast<uintptr_t>(Hooks::DiplomacyLikelihoodHook::LastDropdown());
    uintptr_t vt = 0;
    if (!dropdown || !SafeRead(dropdown, vt) || vt != kDiplomacyDropdownVtable + Image::Get()->GetSlide())
    {
        LuaRuntime::pushstring(L, "no open diplomacy screen");
        return 1;
    }
    uintptr_t context = 0, negotiation = 0, actions = 0;
    uint32_t count = 0;
    if (!dropdown)
    {
        LuaRuntime::pushstring(L, "no DiplomacyDropdown controller on this component");
        return 1;
    }
    if (!SafeRead(dropdown + 0x200, context) || !SafeRead(context + 0xd0, negotiation) || !negotiation ||
        !SafeRead(negotiation + 0x700 + 0xc, count) || !SafeRead(negotiation + 0x700 + 0x10, actions))
    {
        LuaRuntime::pushstring(L, "bad pointer chain from DiplomacyDropdown");
        return 1;
    }

    static char out[0x2000];
    size_t n = std::snprintf(out, sizeof(out), "%u actions: ", count);
    for (uint32_t i = 0; i < count && i < 256 && n + 32 < sizeof(out); i++)
    {
        uintptr_t a = 0;
        int32_t type = -1, side = -1;
        uint8_t flags[2] = {};
        if (SafeRead(actions + i * 8, a) && SafeRead(a + 0x14, type) && SafeRead(a + 0x10, side) && SafeRead(a + 0x18, flags))
        {
            n += std::snprintf(out + n, sizeof(out) - n, "%d:%d:%d:%d ", type, side, flags[0], flags[1]);
        }
    }
    LuaRuntime::pushstring(L, out);
    return 1;
}

// twase.dev_set_action_available(type, available) -> number of actions changed
// Development helper: force the "available" flag of the open negotiation's actions of a type (6 = trade regions).
int DevSetActionAvailable(lua_State* L)
{
    const int wanted = static_cast<int>(LuaRuntime::tonumber(L, 1));
    const uint8_t value = LuaRuntime::tonumber(L, 2) != 0 ? 1 : 0;
    const auto dropdown = reinterpret_cast<uintptr_t>(Hooks::DiplomacyLikelihoodHook::LastDropdown());
    uintptr_t vt = 0, context = 0, negotiation = 0, actions = 0;
    uint32_t count = 0, changed = 0;
    if (dropdown && SafeRead(dropdown, vt) && vt == kDiplomacyDropdownVtable + Image::Get()->GetSlide() &&
        SafeRead(dropdown + 0x200, context) && SafeRead(context + 0xd0, negotiation) && negotiation &&
        SafeRead(negotiation + 0x700 + 0xc, count) && SafeRead(negotiation + 0x700 + 0x10, actions))
    {
        for (uint32_t i = 0; i < count && i < 256; i++)
        {
            uintptr_t a = 0;
            int32_t type = -1;
            if (SafeRead(actions + i * 8, a) && a && SafeRead(a + 0x14, type) && type == wanted)
            {
                *reinterpret_cast<uint8_t*>(a + 0x18) = value;
                changed++;
            }
        }
    }
    LuaRuntime::pushnumber(L, static_cast<lua_Number>(changed));
    return 1;
}

// twase.dev_native_region_trading(enabled) -> enabled
// Development switch for the engine's own region trading in the diplomacy offers (Hooks/NativeRegionTrading.cpp).
int DevNativeRegionTrading(lua_State* L)
{
    if (LuaRuntime::type(L, 1) != LUA_TNONE && LuaRuntime::type(L, 1) != LUA_TNIL)
    {
        Hooks::NativeRegionTradingHook::SetEnabled(LuaRuntime::toboolean(L, 1) != 0);
    }
    LuaRuntime::pushboolean(L, Hooks::NativeRegionTradingHook::IsEnabled());
    return 1;
}

// twase.set_region_value_handler(function(region_key, faction_key) return gold end)
// The region trading mod's valuation: the gold value of a region for a faction (negative = a burden). Used when the AI
// evaluates a deal with regions (Hooks/NativeRegionTrading.cpp).
int SetRegionValueHandler(lua_State* L)
{
    if (LuaRuntime::type(L, 1) != LUA_TFUNCTION && LuaRuntime::type(L, 1) != LUA_TNIL)
    {
        return LuaRuntime::error(L, "twase.set_region_value_handler(fn): expects a function or nil");
    }
    LuaRuntime::pushvalue(L, 1);
    LuaRuntime::setfield(L, LUA_REGISTRYINDEX, "twase_region_value_handler");
    return 0;
}

// twase.set_region_transfer_handler(function(phase, region_key, to_faction_key) end)
// Called around a native region deal's transfer: "before" for each region, then "after" for each region. The region
// trading mod keeps the governor of a transferred province capital alive with it.
int SetRegionTransferHandler(lua_State* L)
{
    if (LuaRuntime::type(L, 1) != LUA_TFUNCTION && LuaRuntime::type(L, 1) != LUA_TNIL)
    {
        return LuaRuntime::error(L, "twase.set_region_transfer_handler(fn): expects a function or nil");
    }
    LuaRuntime::pushvalue(L, 1);
    LuaRuntime::setfield(L, LUA_REGISTRYINDEX, "twase_region_transfer_handler");
    return 0;
}

// twase.dev_region_value_sign(sign), twase.dev_last_deal_score() -> number
int DevRegionValueSign(lua_State* L)
{
    Hooks::NativeRegionTradingHook::SetValueSign(static_cast<int>(LuaRuntime::tonumber(L, 1)));
    return 0;
}

int DevLastDealScore(lua_State* L)
{
    LuaRuntime::pushnumber(L, Hooks::DiplomacyLikelihoodHook::LastScore());
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
    SetFunction(L, "localise", &Localise);
    SetFunction(L, "dev_db_record", &DevDbRecord);
    SetFunction(L, "dev_diplomacy_actions", &DevDiplomacyActions);
    SetFunction(L, "dev_set_action_available", &DevSetActionAvailable);
    SetFunction(L, "dev_native_region_trading", &DevNativeRegionTrading);
    SetFunction(L, "set_region_value_handler", &SetRegionValueHandler);
    SetFunction(L, "set_region_transfer_handler", &SetRegionTransferHandler);
    SetFunction(L, "dev_region_value_sign", &DevRegionValueSign);
    SetFunction(L, "dev_last_deal_score", &DevLastDealScore);

    LuaRuntime::setfield(L, LUA_GLOBALSINDEX, "twase");
    Hooks::NativeRegionTradingHook::SetLuaState(L);
    spdlog::info("[LuaApi] Registered the twase table");
}
