#pragma once

namespace sdk::Attila::Addresses
{
	// hooks
	constexpr uint32_t LuaLog = 0x01A76CF0;
	constexpr uint32_t RunStartupPath = 0x79B980;

	// patches
	constexpr uint32_t BitSetCrashAddr = 0x0091CB57;


	// VFS
	constexpr uint32_t VFS_GetInstance = 0x1658C80;
	constexpr uint32_t VFS_SearchFiles = 0x1635B50;

	// Names
	constexpr uint32_t CName_ctor = 0xDF290;

	// base
	constexpr uint32_t tw_free = 0xE98C0;


	// LUA
	// Direct ScriptInterface* pointers
	constexpr uint32_t g_BattleScriptInterface  = 0x1F49E0C;
	constexpr uint32_t g_FrontendScriptInterface = 0x287CFB8;

	// Points to this+0x18, use getter convention like the game does
	constexpr uint32_t g_CampaignScriptInterface = 0x287CFD4;

	// Shared — holds last created LoggedScriptInterface
	constexpr uint32_t g_ConditionsEffects = 0x1CCE44C; // TODO: stale, not yet re-located for the current game version

	// Linked list of all active lua states
	constexpr uint32_t g_RuntimeLuaListHead     = 0x01DA4230;
	constexpr uint32_t g_RuntimeLuaListSentinel = 0x01DA4234;

	// Master lua state
	constexpr uint32_t g_RuntimeLuaState = 0x0291DE00;

	// lua_State* (__thiscall*)(void* thisPtr)
	constexpr uint32_t GetLuaState = 0x1626AA0;

} // namespace sdk::Attila::Addresses
