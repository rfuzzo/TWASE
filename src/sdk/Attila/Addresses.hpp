#pragma once

namespace sdk::Attila::Addresses
{
	// hooks
	constexpr uint32_t LuaLog = 0x01A4AAD0;
	constexpr uint32_t RunStartupPath = 0x793B10;

	// patches
	constexpr uint32_t BitSetCrashAddr = 0x009150A7;


	// VFS
	constexpr uint32_t VFS_GetInstance = 0x1632750;
	constexpr uint32_t VFS_SearchFiles = 0x160F640;

	// Names
	constexpr uint32_t CName_ctor = 0xDAA40;

	// base
	constexpr uint32_t tw_free = 0xE4050;


	// LUA
	// Direct ScriptInterface* pointers
	constexpr uint32_t g_BattleScriptInterface  = 0x1E63058;
	constexpr uint32_t g_FrontendScriptInterface = 0x2365458;

	// Points to this+0x18, use getter convention like the game does
	constexpr uint32_t g_CampaignScriptInterface = 0x236547C;

	// Shared — holds last created LoggedScriptInterface
	constexpr uint32_t g_ConditionsEffects = 0x1CCE44C;

	// Linked list of all active lua states
	constexpr uint32_t g_RuntimeLuaListHead     = 0x01D3F978;
	constexpr uint32_t g_RuntimeLuaListSentinel = 0x01D3F97C;

	// Master lua state
	constexpr uint32_t g_RuntimeLuaState = 0x02375E1C;

	// lua_State* (__thiscall*)(void* thisPtr)
	constexpr uint32_t GetLuaState = 0x1600590;

} // namespace sdk::Attila::Addresses
