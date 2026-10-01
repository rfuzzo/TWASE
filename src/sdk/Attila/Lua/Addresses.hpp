#pragma once

#include "../Signature.hpp"

namespace sdk::Attila::Lua
{
	// lua API function RVAs (called directly, not hooked), each with a *_Sig signature (not used yet)
	constexpr uint32_t luaL_loadbuffer = 0x012C7E20;
	constexpr Signature luaL_loadbuffer_Sig{ "83 EC 08 8B 44 24 10 FF 74 24 18" };

	constexpr uint32_t Lua_tolstring = 0x012C7380;
	constexpr Signature Lua_tolstring_Sig{ "56 FF 74 24 0C 8B 74 24 0C 56 E8 ?? ?? ?? ?? 8B C8" };

	constexpr uint32_t Lua_settop = 0x012C7210;
	constexpr Signature Lua_settop_Sig{ "8B 4C 24 08 8B 44 24 04 56" };

	constexpr uint32_t Lua_gettop = 0x012C67B0;
	constexpr Signature Lua_gettop_Sig{ "8B 4C 24 04 8B 41 08 2B 41 0C" };

	constexpr uint32_t Lua_getfield = 0x012C66D0;
	constexpr Signature Lua_getfield_Sig{ "83 EC 08 53 56 8B 74 24 14 57 FF 74 24 1C 56 E8 ?? ?? ?? ?? 8B 54 24 28 83 C4 08 8B CA 8B F8 "
	                                      "8D 59 01 8A 01 41 84 C0 75 F9 2B CB 51 52 56 E8 ?? ?? ?? ?? FF 76 08" };

	constexpr uint32_t Lua_pcall = 0x012C6B00;
	constexpr Signature Lua_pcall_Sig{ "8B 44 24 10 83 EC 08 53" };

	constexpr uint32_t Lua_next = 0x012C6A50;
	constexpr Signature Lua_next_Sig{ "56 8B 74 24 08 57 FF 74 24 10 56 E8 ?? ?? ?? ?? 8B 4E 08 83 E9 08" };

	constexpr uint32_t Lua_pushnil = 0x012C6CE0;
	constexpr Signature Lua_pushnil_Sig{ "8B 4C 24 04 8B 41 08 C7 40 04 00 00 00 00" };

	constexpr uint32_t Lua_pushvalue = 0x012C6D80;
	constexpr Signature Lua_pushvalue_Sig{ "56 FF 74 24 0C 8B 74 24 0C 56 E8 ?? ?? ?? ?? 8B 56 08 83 C4 08" };

	constexpr uint32_t Lua_type = 0x012C74E0;
	constexpr Signature Lua_type_Sig{ "FF 74 24 08 FF 74 24 08 E8 ?? ?? ?? ?? 83 C4 08 3D" };

	constexpr uint32_t Lua_getmetatable = 0x012C6730;
	constexpr Signature Lua_getmetatable_Sig{ "56 FF 74 24 0C 8B 74 24 0C 56 E8 ?? ?? ?? ?? 83 C4 08 8B 50 04" };

	constexpr uint32_t Lua_remove = 0x012C6F60;
	constexpr Signature Lua_remove_Sig{ "56 FF 74 24 0C 8B 74 24 0C 56 E8 ?? ?? ?? ?? 8B 4E 08 83 C0 08" };

} // namespace sdk::Attila::Lua
