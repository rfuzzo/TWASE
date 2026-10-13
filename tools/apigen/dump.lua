-- Dumps what the game's Lua offers to campaign scripts, for tools/apigen.
--
-- Run it in a campaign's Lua environment (EpisodicScriptingEnv), either from the TWASE console or as a script mod:
-- campaigns/<campaign>/mods/zz_apidump/scripting.lua with the content of this file. It writes TWASE/api_dump.tsv in the
-- game folder (the working directory), tab separated:
--
--   class    <registry key>  <method>  <C or Lua>  <first upvalue>   script interfaces (Lunar metatables) and their methods
--   global   <name>          <type>    <C or Lua>  <environment>     globals (_G, and "lib": the scripts' environment)
--   member   <global table>  <key>     <type>      <C or Lua>  <env> one level into global tables (script libraries)
--   info     ...                                                     how the environments are set up
--   event    <name>                                                  the engine's events (keys of the global `events`)
--
-- The first upvalue is recorded in case a binding keeps one; Attila's Lunar methods have none (2026-10-13), so apigen
-- matches the classes to the binary's method tables by their method names.

local lines = {};

local function add(...)
	lines[#lines + 1] = table.concat({ ... }, "\t");
end;

local function what(f)
	local info = debug.getinfo(f, "S");
	return info and info.what or "?";
end;

local function upvalue(f)
	local name, value = debug.getupvalue(f, 1);
	return value ~= nil and tostring(value) or "";
end;

local function sorted_keys(t)
	local keys = {};
	for k in pairs(t) do
		if type(k) == "string" then keys[#keys + 1] = k end;
	end;
	table.sort(keys);
	return keys;
end;

-- classes: registry tables with functions, and their __index table if it is a different one
local registry = debug.getregistry();
for _, key in ipairs(sorted_keys(registry)) do
	local mt = registry[key];
	if type(mt) == "table" then
		local sources = { mt };
		if type(mt.__index) == "table" and mt.__index ~= mt then sources[2] = mt.__index end;
		local seen = {};
		for _, source in ipairs(sources) do
			for _, method in ipairs(sorted_keys(source)) do
				local f = source[method];
				if type(f) == "function" and not seen[method] then
					seen[method] = true;
					add("class", key, method, what(f), upvalue(f));
				end;
			end;
		end;
	end;
end;

-- globals, and one level into global tables. The campaign's scripting.lua and the libraries it requires run in an
-- environment of their own (not _G), found through a library function.
local function dump_env(env_name, env)
	for _, name in ipairs(sorted_keys(env)) do
		local v = env[name];
		add("global", name, type(v), type(v) == "function" and what(v) or "", env_name);
		if type(v) == "table" and v ~= env and v ~= _G and name ~= "package" and name ~= "_LOADED" and name ~= "events" then
			for _, key in ipairs(sorted_keys(v)) do
				local m = v[key];
				add("member", name, key, type(m), type(m) == "function" and what(m) or "", env_name);
			end;
		end;
	end;
end;

dump_env("_G", _G);

local mt = getmetatable(_G);
add("info", "_G metatable", tostring(mt), mt and type(mt.__index) or "", mt and tostring(mt.__index) or "");

local ok, lib_function = pcall(function() return get_cm end);
local lib_env = ok and type(lib_function) == "function" and getfenv(lib_function) or nil;
add("info", "get_cm", ok and type(lib_function) or "error", tostring(lib_env));
add("info", "this chunk's environment", getfenv(1) == _G and "_G" or tostring(getfenv(1)));
if type(lib_env) == "table" and lib_env ~= _G then
	dump_env("lib", lib_env);
end;

-- events
if type(events) == "table" then
	for _, name in ipairs(sorted_keys(events)) do
		add("event", name);
	end;
end;

local path = "TWASE/api_dump.tsv";
local file = io and io.open(path, "w");
if file then
	file:write(table.concat(lines, "\n"), "\n");
	file:close();
	print("[apidump] wrote " .. #lines .. " lines to " .. path);
else
	for _, line in ipairs(lines) do print("[apidump] " .. line) end;
	print("[apidump] could not open " .. path .. ", printed " .. #lines .. " lines instead");
end;
