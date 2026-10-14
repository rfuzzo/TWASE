-- Records what the context object of each engine event offers, for docs/scripting/events.md.
--
-- Run it as a script mod: copy it to data/campaigns/main_attila/mods/zz_ctxprobe/scripting.lua, load a campaign and
-- play: end turns, select characters and settlements, open panels, fight a battle, trigger what you want recorded.
-- It listens to every event in the global `events` and, the first time each one fires, probes its context. The result
-- is written to TWASE/context_probe.tsv in the game folder (the working directory) each time an event is seen for
-- the first time, and again whenever `ctxprobe_write()` is called from the console (use that before quitting).
--
-- Tab separated, one line per finding:
--
--   <event> <count> meta   <key>        <type>  <where>        keys of the context's metatable (and its __index table)
--   <event> <count> method <name>       <result type or class>  <value>   a candidate accessor that exists and was called
--   <event> <count> field  <name>       <type or class>         <value>   a candidate name that is a plain field
--   <event> <count> info   <what>       <value>
--
-- <count> is how often the event had fired when the file was written. The probe calls the candidates under pcall with
-- the context as the only argument; a candidate that isn't there (nil) is not listed. Extend `candidates` for more.
-- It only reads: it doesn't change the game's state. Remove the folder again when done (the listeners are cheap but
-- there are hundreds of them).

local OUTPUT = "TWASE/context_probe.tsv";
local MAX_EVENTS_PER_WRITE = 0;		-- 0: write after every new event

local candidates = {
	-- game objects
	"character", "faction", "other_faction", "target_faction", "liberating_character", "target_character",
	"region", "target_region", "settlement", "garrison_residence", "military_force", "force", "army", "navy",
	"pending_battle", "mission", "world", "model", "province", "building", "slot", "unit", "agent", "ancillary",
	"trait", "effect", "effect_bundle", "bundle", "technology", "disaster", "trade_route", "treaty", "area",
	"attacker", "defender", "winner", "loser", "caster", "target",
	-- values
	"string", "component", "dilemma", "choice", "incident", "key", "name", "id", "type", "value", "number", "amount",
	"turn", "success", "result", "text", "message", "position", "x", "y", "cqi", "faction_key", "region_key",
	"building_key", "unit_key", "mission_key", "mission_record_key", "dilemma_key", "stance", "direction", "level",
	"is_human", "is_player", "was_attacker", "player_involved",
};

-- ----------------------------------------------------------------------------------------------------------------

local registry = debug.getregistry();
local class_of = {};		-- metatable -> registry key
for key, mt in pairs(registry) do
	if type(key) == "string" and type(mt) == "table" then class_of[mt] = key end;
end;

local seen = {};			-- event -> true
local counts = {};			-- event -> number of times it fired
local rows = {};			-- event -> list of lines
local order = {};			-- events in the order they were first seen

local function clean(s)
	s = tostring(s);
	s = string.gsub(s, "[\t\r\n]", " ");
	if #s > 120 then s = string.sub(s, 1, 120) .. "..." end;
	return s;
end;

local function describe(v)
	local t = type(v);
	if t == "string" or t == "number" or t == "boolean" or t == "nil" then
		return t, clean(v);
	end;
	local class = "";
	local mt = getmetatable(v);
	if type(mt) == "table" and class_of[mt] then class = class_of[mt] end;
	local extra = {};
	-- the usual identifying accessors of script objects, each under pcall
	for _, m in ipairs({ "name", "cqi", "key", "mission_record_key", "region_key", "turn_number" }) do
		local ok, r = pcall(function() return v[m](v) end);
		if ok and (type(r) == "string" or type(r) == "number") then
			extra[#extra + 1] = m .. "=" .. clean(r);
		end;
	end;
	return t .. (class ~= "" and (":" .. class) or ""), table.concat(extra, " ");
end;

local function keys_of(tbl)
	local list = {};
	for k, v in pairs(tbl) do
		if type(k) == "string" then list[#list + 1] = { k, type(v) } end;
	end;
	table.sort(list, function(a, b) return a[1] < b[1] end);
	return list;
end;

local function probe(event, context)
	local out = {};
	local function add(kind, name, a, b)
		out[#out + 1] = { kind, name, a or "", b or "" };
	end;

	add("info", "type", type(context), clean(context));

	local mt = getmetatable(context);
	if type(mt) == "table" then
		add("info", "metatable", class_of[mt] or "unregistered", "");
		for _, kv in ipairs(keys_of(mt)) do add("meta", kv[1], kv[2], "metatable") end;
		if type(mt.__index) == "table" and mt.__index ~= mt then
			for _, kv in ipairs(keys_of(mt.__index)) do add("meta", kv[1], kv[2], "__index") end;
		end;
	end;
	if type(context) == "table" then
		for _, kv in ipairs(keys_of(context)) do add("meta", kv[1], kv[2], "table") end;
	end;

	for _, name in ipairs(candidates) do
		local ok, v = pcall(function() return context[name] end);
		if ok and v ~= nil then
			if type(v) == "function" then
				local ok2, r = pcall(v, context);
				if ok2 then
					local t, d = describe(r);
					add("method", name, t, d);
				else
					add("method", name, "error", clean(r));
				end;
			else
				local t, d = describe(v);
				add("field", name, t, d);
			end;
		end;
	end;
	return out;
end;

local function write()
	local lines = {};
	for _, event in ipairs(order) do
		for _, r in ipairs(rows[event]) do
			lines[#lines + 1] = table.concat({ event, tostring(counts[event]), r[1], r[2], r[3], r[4] }, "\t");
		end;
	end;
	local file = io and io.open(OUTPUT, "w");
	if file then
		file:write(table.concat(lines, "\n"), "\n");
		file:close();
		print("[ctxprobe] wrote " .. #lines .. " lines for " .. #order .. " events to " .. OUTPUT);
	else
		print("[ctxprobe] could not open " .. OUTPUT .. ", printing instead");
		for _, line in ipairs(lines) do print("[ctxprobe] " .. line) end;
	end;
end;

function ctxprobe_write()
	write();
end;

local function on_event(event, context)
	counts[event] = (counts[event] or 0) + 1;
	if seen[event] then return end;
	seen[event] = true;
	local ok, result = pcall(probe, event, context);
	rows[event] = ok and result or { { "info", "probe error", clean(result), "" } };
	order[#order + 1] = event;
	if MAX_EVENTS_PER_WRITE == 0 or #order % MAX_EVENTS_PER_WRITE == 0 then
		pcall(write);
	end;
end;

local cm = get_cm();
local names = {};
for name, v in pairs(events) do
	if type(name) == "string" and type(v) == "table" then names[#names + 1] = name end;
end;
table.sort(names);

for _, name in ipairs(names) do
	cm:add_listener(
		"zz_ctxprobe_" .. name,
		name,
		true,
		function(context) on_event(name, context) end,
		true
	);
end;

print("[ctxprobe] listening to " .. #names .. " events, results in " .. OUTPUT .. " (ctxprobe_write() writes it now)");
