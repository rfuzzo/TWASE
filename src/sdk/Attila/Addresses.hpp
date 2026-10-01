#pragma once

#include "Signature.hpp"

// RVAs in empire.retail.dll (Total War: Attila 1.6.0.0). Each address has a *_Sig signature that finds it by pattern;
// the signatures are not used yet, the RVAs are.
namespace sdk::Attila::Addresses
{
	// hooks
	constexpr uint32_t LuaLog = 0x01A76CF0;
	constexpr Signature LuaLog_Sig{ "A1 ?? ?? ?? ?? 85 C0 74 02 FF E0 6A 01" };

	constexpr uint32_t RunStartupPath = 0x79B980;
	constexpr Signature RunStartupPath_Sig{ "83 EC 6C 53 55 56 8B D9 57" };

	// patches
	constexpr uint32_t BitSetCrashAddr = 0x0091CB57;
	constexpr Signature BitSetCrashAddr_Sig{ "83 FF 40 0F 83 ?? ?? ?? ?? 33 D2 33 C9" };


	// VFS
	constexpr uint32_t VFS_GetInstance = 0x1658C80;
	constexpr Signature VFS_GetInstance_Sig{ "E8 ?? ?? ?? ?? 80 3D ?? ?? ?? ?? 00 75 06 B8 ?? ?? ?? ?? C3" };

	constexpr uint32_t VFS_SearchFiles = 0x1635B50;
	constexpr Signature VFS_SearchFiles_Sig{ "83 EC 08 80 3D ?? ?? ?? ?? 00 56 57" };

	// Names
	constexpr uint32_t CName_ctor = 0xDF290;
	constexpr Signature CName_ctor_Sig{ "83 EC 0C 56 FF 74 24 14 8B F1 8D 4C 24 08 E8 ?? ?? ?? ?? 8D 44 24 04 50 E8" };

	// base
	constexpr uint32_t tw_free = 0xE98C0;
	constexpr Signature tw_free_Sig{ "83 EC 08 56 8B 74 24 10 85 F6 74 2C" };


	// LUA
	// Direct ScriptInterface* pointers
	constexpr uint32_t g_BattleScriptInterface  = 0x1F49E0C;
	constexpr Signature g_BattleScriptInterface_Sig{ "8B 0D ?? ?? ?? ?? 84 C0 0F 95 44 24 08 E8", SignatureType::Abs32, 2 };

	constexpr uint32_t g_FrontendScriptInterface = 0x287CFB8;
	constexpr Signature g_FrontendScriptInterface_Sig{ "A1 ?? ?? ?? ?? 80 B8 D0 01 00 00 00 0F 85", SignatureType::Abs32, 1 };

	// Points to this+0x18, use getter convention like the game does
	constexpr uint32_t g_CampaignScriptInterface = 0x287CFD4;
	constexpr Signature g_CampaignScriptInterface_Sig{ "C3 C7 05 ?? ?? ?? ?? 00 00 00 00 33 C0 C3", SignatureType::Abs32, 3 };

	// Shared — holds last created LoggedScriptInterface
	constexpr uint32_t g_ConditionsEffects = 0x1CCE44C; // TODO: stale, not yet re-located for the current game version

	// Linked list of all active lua states
	constexpr uint32_t g_RuntimeLuaListHead     = 0x01DA4230;
	constexpr Signature g_RuntimeLuaListHead_Sig{ "8B 15 ?? ?? ?? ?? 56 57 8B F2 33 FF 81 FE", SignatureType::Abs32, 2 };

	constexpr uint32_t g_RuntimeLuaListSentinel = 0x01DA4234;
	constexpr Signature g_RuntimeLuaListSentinel_Sig{ "81 FA ?? ?? ?? ?? 74 10 8B 52 04 40 81 FA", SignatureType::Abs32, 2 };

	// Master lua state
	constexpr uint32_t g_RuntimeLuaState = 0x0291DE00;
	constexpr Signature g_RuntimeLuaState_Sig{ "8B 35 ?? ?? ?? ?? 83 C4 48 8B 44 24 1C 85 C0", SignatureType::Abs32, 2 };

	// lua_State* (__thiscall*)(void* thisPtr)
	// too small to be unique, resolved from a call site
	constexpr uint32_t GetLuaState = 0x1626AA0;
	constexpr Signature GetLuaState_Sig{ "83 C4 04 8B CE E8 ?? ?? ?? ?? 8B C6 5E C2 04 00", SignatureType::Rel32, 6 };


	// DIPLOMACY
	// float (__cdecl*)(void* deal, int ctx, void* factionA, void* factionB)
	// UI-only wrapper around the deal evaluator, AI accepts deals with score >= 0
	constexpr uint32_t Diplo_GetDisplayedDealScore = 0xA6E980;
	constexpr Signature Diplo_GetDisplayedDealScore_Sig{ "83 EC 08 8D 44 24 03 50 8D 44 24 08 50 FF 74 24 20" };

	// int (__thiscall*)(void* caiModule, void* deal, int)
	// buckets the score: <= LOW tweaker (-4) low, >= HIGH tweaker (4) high, else moderate
	constexpr uint32_t CAI_GetDealLikelihoodBucket = 0xD2A7A0;
	constexpr Signature CAI_GetDealLikelihoodBucket_Sig{ "51 53 8B 5C 24 0C 55 8B E9 8B CB 56 E8 ?? ?? ?? ?? 80 B8 4C 08 00 00 00" };

	// int (__thiscall*)(DiplomacyDropdown* this, int likelihood (-1/0/1, -2 hidden), bool show)
	constexpr uint32_t DiplomacyDropdown_SetLikelihood = 0x14F95A0;
	constexpr Signature DiplomacyDropdown_SetLikelihood_Sig{ "83 EC 0C 53 56 8B F1 57 8B 7C 24 1C 8B 9E BC 00 00 00" };


	// UI
	// void (__thiscall*)(UIComponent* this, const WString* text, bool allStates)
	constexpr uint32_t UIComponent_SetText = 0x13B8B40;
	constexpr Signature UIComponent_SetText_Sig{ "80 7C 24 08 00 57 8B F9 75 13" };

	// void (__thiscall*)(UIComponent* this, const WString* text, bool allStates)
	constexpr uint32_t UIComponent_SetTooltipText = 0x13B9A00;
	constexpr Signature UIComponent_SetTooltipText_Sig{ "83 EC 0C 80 7C 24 14 00 57 8B F9 75 14" };

	// WString* (__thiscall*)(WString* this, const wchar_t* str)
	constexpr uint32_t WString_ctor = 0xDFEF0;
	constexpr Signature WString_ctor_Sig{ "56 8B 74 24 08 8B C6 57 8B F9 66 0F 1F 44 00 00" };

	// void (__thiscall*)(WString* this)
	// too small and too common to be unique, resolved from a call in UIComponent_SetTooltipText
	constexpr uint32_t WString_dtor = 0xE0720;
	constexpr Signature WString_dtor_Sig{ "83 EC 0C 80 7C 24 14 00 57 8B F9 75 14 8B 8F B4 00 00 00 FF 74 24 14 83 C1 44 E8 ?? ?? ?? ?? "
	                                      "EB 62 53 FF 74 24 18 8D 8F 18 01 00 00 E8 ?? ?? ?? ?? 8B 9F B0 00 00 00 8B 87 AC 00 00 00 "
	                                      "8D 04 83 3B D8 74 3E 56 8B 33 8D 4C 24 0C 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8D 44 24 0C 50 8D 4E "
	                                      "44 E8 ?? ?? ?? ?? 8D 4C 24 0C E8 ?? ?? ?? ??",
	                                      SignatureType::Rel32, 103 };

} // namespace sdk::Attila::Addresses
