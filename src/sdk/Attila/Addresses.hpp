#pragma once

#include <cstdint>

// Unslid addresses as shown in Binary Ninja (image base 0x100000000), resolve with Image::Resolve.
// See docs/addresses.md for how each one was found.
namespace sdk::Attila::Addresses
{
// Feral 1.6.1 RC2 (CFBundleVersion 480285.103778)
constexpr const char* SupportedUUID = "392D6F66-9183-329E-8A98-8C90FC828326";

// STARTUP
// int64_t (*)(void* instance, void* prevInstance, const char16_t* commandLine), CA's WinMain on the WinMain thread
constexpr uint64_t WinMain = 0x1032BC10C;

// GAME LOOP
// bool (*)(void* app, int, int, int, int), called once per frame by the run loop on the WinMain (game/Lua) thread
constexpr uint64_t GameTick = 0x1010ECD24;

// LUA LOG
// void (*)(const char*), called by print, out and the Lua error handler, falls back to fputs(stdout) if null
constexpr uint64_t g_LuaLogSink = 0x1056D6588;

// void (*)(void (*sink)(const char*)), the str instruction that stores the sink
constexpr uint64_t SetLuaLogSink_Store = 0x103F25DEC;

// SCRIPT RUNTIMES
// Linked list of all active lua states
constexpr uint64_t g_RuntimeLuaListHead = 0x10560B7C8;
constexpr uint64_t g_RuntimeLuaListSentinel = 0x10560B7D0;

// lua_State* (*)(void* scriptInterface)
constexpr uint64_t GetLuaState = 0x1038B377C;

// MOD LOADER
// void* (*)(ScriptingEnv* self), EPISODIC_SCRIPTING_ENV virtual slot 3, loads and runs campaigns/<name>/scripting.lua
constexpr uint64_t RunStartupPath = 0x102170104;

// VFS
// void* (*)(), returns the static VFS instance
constexpr uint64_t VFS_GetInstance = 0x10387C0A8;

// void (*)(void* vfs, CName* dir, CName* pattern, VFSSearchResults* out, int flags, int mode), a virtual of the VFS
constexpr uint64_t VFS_SearchFiles_VtableOffset = 0x58;

// CName* (*)(CName* self, const char* str)
constexpr uint64_t CName_ctor = 0x10109240C;

// void (*)(void* ptr), the game allocator's free
constexpr uint64_t tw_free = 0x1010ACB70;

// DIPLOMACY
// float (*)(void* deal, void* ctx, void* factionA, void* factionB)
// UI-only wrapper around the deal evaluator (0x101d7b25c), AI accepts deals with score >= 0
constexpr uint64_t Diplo_GetDisplayedDealScore = 0x101D7E890;

// int (*)(void* caiModule, void* deal)
// buckets the score: 2 low, 3 moderate, 4 high, thresholds are the tweakers
// CAI_DIPLOMACY_NEGOTIATION_DISPLAYED_LIKELIHOOD_THRESHOLD_LOW/HIGH (-4 / 4, value at +0x64 of 0x105600378 / 0x1056003e0)
constexpr uint64_t CAI_GetDealLikelihoodBucket = 0x102296A5C;

// void* (*)(DiplomacyDropdown* self, int likelihood (-1/0/1, -2 hidden), bool show)
constexpr uint64_t DiplomacyDropdown_SetLikelihood = 0x1035F4A1C;

// UI
// void (*)(UIComponent* self, const WString* text, bool allStates)
constexpr uint64_t UIComponent_SetTooltipText = 0x1032D2418;

// STRINGS
// TempString* (*)(TempString* self, const char* utf8)
constexpr uint64_t String_ctor = 0x10104CE1C;

// void (*)(TempString* self)
constexpr uint64_t String_dtor = 0x10104BA58;

// WStringResult (*)(const TempString* src), returns through x8
constexpr uint64_t String_ToUniString = 0x101049BB0;

// void (*)(WString* self)
constexpr uint64_t UniString_dtor = 0x101049D20;

// CAMPAIGN MODEL (for the twase.* Lua API)
// bool (*)(lua_State* L, int idx, FACTION_SCRIPT_INTERFACE** out), the game's own faction argument check:
// 0 if the value isn't a userdata, a Lua type error if it's another userdata type
constexpr uint64_t FactionScriptInterface_GetArg = 0x101D5C640;

// int (*)(CAI_INTERFACE* self, const FACTION& a, const FACTION& b), EMPIRECAMPAIGNAI::CAI_INTERFACE::get_faction_attitude:
// sums the attitude components, rounded. Found via the RTTI of its lambda (vtable 0x104c12268)
constexpr uint64_t CAI_GetFactionAttitude = 0x10229D814;

// Offsets
constexpr uint64_t FactionScriptInterface_Faction = 0x10;  // FACTION*
constexpr uint64_t Faction_ModelHandle = 0x18;             // -> +0x48 the campaign world
constexpr uint64_t ModelHandle_World = 0x48;
constexpr uint64_t World_CAIInterface = 0x2D60;            // CAI_INTERFACE*
constexpr uint64_t Faction_AliveMarker = 0x8F8;            // null once the faction is destroyed (FACTION::is_dead 0x101b65694)

// PATCHES
constexpr uint64_t BitSetCrashAddr = 0x101A51C58;

} // namespace sdk::Attila::Addresses
