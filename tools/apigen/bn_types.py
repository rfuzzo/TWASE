#!/usr/bin/env python3
"""Types the script interfaces' methods in the Mac binary through Binary Ninja's MCP server.

Reads the method tables in data/lunar_types_macos.json (table address -> methods with their handler), decompiles each
handler, and rewrites the file with the arguments and return types found through the helpers below. Needs Binary Ninja
with the game's bndb open and its MCP server started (http://127.0.0.1:24642/mcp).

usage: bn_types.py [--cache <folder for the decompiled handlers>]

The helpers (Mac build, addresses unslid):
- Campaign methods get a context from LuaGetLookup(L) (ctx[0] = lua_State*, byte at ctx + 8 = still valid) and pop
  their arguments from the top of the stack, so the Lua argument order is the reverse of the pops.
- Faction and character arguments are read by stack index with <Class>ScriptInterface_GetArg(L, index, &out).
- Results are pushed with the LuaPush* helpers, objects with the per-class Lunar push (L, object, flag), which looks
  up the class's metatable by name.
"""

import argparse
import json
import os
import re
import struct
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
TYPES = os.path.join(HERE, "data", "lunar_types_macos.json")
MCP = "http://127.0.0.1:24642/mcp"

# helper -> Lua type, by name (as named in the bndb) and by address (sub_<address>, for unnamed copies)
POP = {
    "LuaPopString": "string", "sub_1038affd8": "string",
    "LuaPopBool": "boolean", "sub_1038b23d0": "boolean",
    "LuaPopNumber": "number", "sub_1038b1ae0": "number",
    "LuaPopInt": "number (int)", "sub_1038b0a68": "number (int)",
    "LuaPopInt2": "number (int)", "sub_1038b2318": "number (int)",
    "LuaPopTable": "table", "sub_1038b251c": "table",
    "LuaPopLightUserdata": "userdata", "sub_1010de910": "userdata",
}
GET_ARG = {
    "FactionScriptInterface_GetArg": "FACTION_SCRIPT_INTERFACE", "sub_101d5c640": "FACTION_SCRIPT_INTERFACE",
    "CharacterScriptInterface_GetArg": "CHARACTER_SCRIPT_INTERFACE", "sub_101d5bd18": "CHARACTER_SCRIPT_INTERFACE",
}
PUSH = {
    "LuaPushString": "string", "sub_1038b0b58": "string", "lua_pushstring": "string",
    "LuaPushInt": "number (int)", "sub_1038b0ddc": "number (int)",
    "LuaPushUInt": "number (uint)", "sub_1038b0da0": "number (uint)",
    "LuaPushFloat": "number", "sub_1038b1b68": "number", "lua_pushnumber": "number", "sub_103f28c94": "number",
    "LuaPushBool": "boolean", "sub_1038b00a8": "boolean", "lua_pushboolean": "boolean",
    "LuaPushNewTable": "table", "sub_1038b1fa0": "table",
    "LuaPushComponentPointer": "Pointer<Component>", "sub_1014a2224": "Pointer<Component>",
}
# the per-class Lunar push, named LunarPush_<CLASS> in the bndb (2026-10-13)
NAMED_PUSH = re.compile(r"\bLunarPush_(\w+)\(")
TAG = {1: "boolean", 3: "number", 4: "string", 5: "table", 6: "function", 7: "userdata"}


class Mcp:
    def __init__(self):
        self.session = None
        _, self.session = self.rpc("initialize", {"protocolVersion": "2025-03-26", "capabilities": {},
                                                  "clientInfo": {"name": "apigen", "version": "1"}})

    def rpc(self, method, params):
        headers = {"Content-Type": "application/json", "Accept": "application/json, text/event-stream"}
        if self.session:
            headers["Mcp-Session-Id"] = self.session
        body = json.dumps({"jsonrpc": "2.0", "id": 1, "method": method, "params": params}).encode()
        response = urllib.request.urlopen(urllib.request.Request(MCP, body, headers), timeout=120)
        text = response.read().decode()
        if "data:" in text:
            text = [line[5:] for line in text.splitlines() if line.startswith("data:")][0]
        return json.loads(text), response.headers.get("Mcp-Session-Id")

    def call(self, name, **args):
        result, _ = self.rpc("tools/call", {"name": name, "arguments": args})
        return "".join(c.get("text", "") for c in result["result"]["content"])

    def decompile(self, address):
        text = self.call("bn_function_decompile", function=address)
        return text.split("````bn-pseudoc", 1)[-1].rsplit("````", 1)[0]

    def read(self, address, length):
        return bytes.fromhex(json.loads(self.call("bn_memory_read", address=hex(address), length=length))["hex"])


def lua_state_calls(code, state):
    """The sub_* functions called with the Lua state (`state`, a copy of it or its LuaGetLookup context) first"""
    aliases = {state} | set(re.findall(rf"\b(\w+) = LuaGetLookup\({state}\)", code))
    for _ in range(3):
        for var, source in re.findall(r"\b(\w+) = (\w+);", code) + re.findall(r"\b(\w+) = sub_\w+\((\w+), ", code):
            if source in aliases:
                aliases.add(var)
    calls = set()
    for function, first in re.findall(r"\b(sub_10[0-9a-f]{7})\((\w+)[,)]", code):
        if first in aliases and function not in POP and function not in PUSH and function not in GET_ARG:
            calls.add(function)
    return calls


def push_class(mcp, function, cache, depth=0):
    """The class a function pushes: a per-class Lunar push (L, object, flag), which pushes nil for a null object and
    then looks up the class's metatable, or (one level deep) a helper that calls one with its own Lua state"""
    if function in cache:
        return cache[function]
    cache[function] = None
    body = mcp.decompile("0x" + function[4:])
    m = re.search(r'lua_getfield\(arg1, 0xffffd8f0, "(\w+)"\)', body)
    pointer = re.search(r"State::operator<<<(?:\w+::)*(\w+)>\(UTILITYDLL::LUA::Pointer", body)
    if m and "lua_pushnil(arg1)" in body:
        cache[function] = m.group(1)
    elif pointer:
        # State::operator<<(Pointer<T>): a pointer userdata, e.g. a UI component's address
        cache[function] = f"Pointer<{pointer.group(1)}>"
    elif depth == 0:
        found = set(NAMED_PUSH.findall(body)) | {push_class(mcp, f, cache, 1) for f in lua_state_calls(body, "arg1")}
        found.discard(None)
        # one class, or one class or the null interface (e.g. "faction has no home region")
        if len(found) == 1 or (len(found) == 2 and "NULL_SCRIPT_INTERFACE" in found):
            cache[function] = ", ".join(sorted(found))
    return cache[function]


def push_classes(mcp, code, cache):
    """The classes of the objects `code` pushes"""
    classes = set(NAMED_PUSH.findall(code))
    for function in lua_state_calls(code, "arg2"):
        classes.update(c for c in (push_class(mcp, function, cache) or "").split(", ") if c)
    return classes


def wrapper_class(mcp, code):
    """Objects returned through an OBJECT_WRAPPER_INTERFACE: the wrapped interface from the wrapper's vtable RTTI"""
    for address in re.findall(r"\[1\] = &data_([0-9a-f]+)", code):
        for q in struct.unpack("<6Q", mcp.read(int(address, 16), 48)):
            if 0x100000000 <= q < 0x110000000:
                name = mcp.read(q, 220).split(b"\0")[0].decode("latin1")
                m = re.search(r"OBJECT_WRAPPER_INTERFACEINS0_(\d+)", name)
                if m:
                    start = name.index(m.group(0)) + len(m.group(0))
                    return name[start:start + int(m.group(1))]
    return None


def type_handler(mcp, code, push_cache):
    # arguments: by explicit index (GetArg, lua_type checks), else the pops in reverse order
    indexed = {}
    for function, index in re.findall(r"\b(\w+)\(arg2, (\d+), &", code):
        if function in GET_ARG:
            indexed[int(index)] = GET_ARG[function]
    for index, tag in re.findall(r"lua_type\((?:arg2|\*LuaGetLookup\(arg2\)), (\d+)\) [!=]= (\d+)", code):
        indexed.setdefault(int(index), TAG.get(int(tag), f"tag {tag}"))
    pops = [POP[p] for p in re.findall(r"\b(" + "|".join(POP) + r")\(", code)]
    if indexed:
        args = [indexed[i] for i in sorted(indexed)]
    else:
        args = list(reversed(pops))
    counts = sorted({int(n) for n in re.findall(r"lua_gettop\([^)]*\)\)? [!=]= (\d+)", code)})
    if not args and counts and counts[-1] > 0:
        args = [f"{counts[-1]} values"]

    returns = sorted({PUSH[p] for p in re.findall(r"\b(" + "|".join(sorted(PUSH, key=len, reverse=True)) + r")\(", code)})
    returns += sorted(push_classes(mcp, code, push_cache))
    wrapped = wrapper_class(mcp, code)
    if wrapped and wrapped not in returns:
        returns.append(wrapped)
    return ", ".join(args), ", ".join(returns)


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--cache", help="folder to cache the decompiled handlers in")
    args = parser.parse_args()

    with open(TYPES, encoding="utf-8") as f:
        tables = json.load(f)
    mcp = Mcp()
    push_cache = {}
    typed = total = 0
    for address, methods in tables.items():
        for method in methods:
            if not method.get("handler"):
                continue  # virtual (is_null_interface): no handler to decompile
            path = args.cache and os.path.join(args.cache, method["handler"] + ".c")
            if path and os.path.exists(path):
                code = open(path, encoding="utf-8").read()
            else:
                code = mcp.decompile(method["handler"])
                if path:
                    os.makedirs(args.cache, exist_ok=True)
                    with open(path, "w", encoding="utf-8") as f:
                        f.write(code)
            method["args"], method["returns"] = type_handler(mcp, code, push_cache)
            method.pop("offsets", None)
            total += 1
            typed += bool(method["args"] or method["returns"])
    with open(TYPES, "w", encoding="utf-8") as f:
        json.dump(tables, f, indent=1)
        f.write("\n")
    print(f"typed {typed} of {total} methods; object pushes: {len([c for c in push_cache.values() if c])}")


if __name__ == "__main__":
    main()
