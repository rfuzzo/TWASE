#!/usr/bin/env python3
"""Generates the Attila campaign API reference (docs/reference/) from:

- data/api_dump_macos.tsv: what the game's Lua offers in a campaign, written by dump.lua in the game
- data/lunar_types_macos.json: the script interfaces' method tables in the Mac binary (handler, arguments, return type),
  typed from the decompiled handlers in Binary Ninja (the local-llm-re experiments, 2026-10-06)
- the game's own script library, from an extracted data.pack (script/_lib, lua_scripts, campaigns/<campaign>/*.lua),
  for the library functions' parameters and where they're defined

usage: apigen.py <extracted data.pack folder> [--out docs/reference]

Only what the vanilla game defines is kept: the dump is taken in a campaign with mods, so Lua globals that no vanilla
script defines are dropped. Python 3 standard library only.
"""

import argparse
import collections
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))

STANDARD_LIBRARIES = {"bit", "coroutine", "debug", "io", "math", "os", "package", "string", "table"}
# global tables of C functions the engine provides, besides the script interfaces
ENGINE_TABLES = {"CampaignUI", "system", "vfs"}


def read_dump(path):
    """classes: {name: set(methods)}, globals: {env: {name: type}}, members: {table: {key: (type, what)}}, events"""
    classes = collections.defaultdict(set)
    globals_ = collections.defaultdict(dict)
    members = collections.defaultdict(dict)
    events = []
    with open(path, encoding="utf-8") as f:
        for line in f:
            fields = line.rstrip("\n").split("\t")
            kind = fields[0]
            if kind == "class":
                classes[fields[1]].add(fields[2])
            elif kind == "global":
                globals_[fields[4] if len(fields) > 4 else "_G"][fields[1]] = fields[2]
            elif kind == "member":
                members[fields[1]][fields[2]] = (fields[3], fields[4] if len(fields) > 4 else "")
            elif kind == "event":
                events.append(fields[1])
    return classes, globals_, members, events


def is_metamethod(name):
    return name.startswith("__")


def match_tables(classes, types):
    """The binary's method table for each class, by the largest overlap of method names"""
    matched = {}
    for address, methods in types.items():
        names = {m["method"] for m in methods}
        best, overlap = None, 0
        for name, class_methods in classes.items():
            common = len(names & class_methods)
            if common > overlap:
                best, overlap = name, common
        # a real match covers (nearly) the whole table; battle tables share only a few common names
        if best and overlap >= max(3, len(names) * 0.8):
            if best not in matched or overlap > matched[best][1]:
                matched[best] = (address, overlap)
    return {name: address for name, (address, _) in matched.items()}


FUNCTION_RE = re.compile(r"^function\s+([A-Za-z_][\w]*)(?:([:.])([A-Za-z_]\w*))?\s*\(([^)]*)\)")


def read_library(data_dir):
    """{(table or None, name): (params, file, line)} for every top-level function definition in the vanilla scripts"""
    roots = ["script/_lib", "lua_scripts"]
    campaigns = os.path.join(data_dir, "campaigns")
    if os.path.isdir(campaigns):
        roots += [os.path.join("campaigns", c) for c in sorted(os.listdir(campaigns))]
    definitions = {}
    for root in roots:
        folder = os.path.join(data_dir, root)
        if not os.path.isdir(folder):
            continue
        for file in sorted(os.listdir(folder)):
            if not file.endswith(".lua"):
                continue
            rel = f"{root}/{file}"
            with open(os.path.join(folder, file), encoding="latin-1") as f:
                for number, line in enumerate(f, 1):
                    m = FUNCTION_RE.match(line)
                    if not m:
                        continue
                    first, sep, second, params = m.groups()
                    key = (first, second) if sep else (None, first)
                    params = ", ".join(p.strip() for p in params.split(",") if p.strip())
                    definitions.setdefault(key, (params, rel, number))
    return definitions


def clean_type(text, kind):
    if text.startswith("?") or text == "-":
        return ""
    text = text.replace("number (from signed int)", "number (int)").replace("number (from unsigned int)", "number (uint)")
    text = re.sub(r"object: (\w+) \(wraps \w+\)", r"\1", text)
    text = text.replace(" (types not checked)", " values")
    if kind == "args" and text == "none":
        return ""
    return text


def build(data_dir):
    classes, globals_, members, events = read_dump(os.path.join(HERE, "data", "api_dump_macos.tsv"))
    with open(os.path.join(HERE, "data", "lunar_types_macos.json"), encoding="utf-8") as f:
        types = json.load(f)
    library = read_library(data_dir)
    vanilla_events = set()
    events_lua = os.path.join(data_dir, "lua_scripts", "events.lua")
    if os.path.isfile(events_lua):
        with open(events_lua, encoding="latin-1") as f:
            vanilla_events = set(re.findall(r"^(\w+) = \{\}", f.read(), re.MULTILINE))

    tables = match_tables(classes, types)
    api = {"build": "Feral 1.6.1 RC2 (Mac), LC_UUID 392D6F66-9183-329E-8A98-8C90FC828326", "classes": {}, "engine": {},
           "library": {"functions": [], "tables": {}}, "events": []}

    for name in sorted(classes):
        # skip Lua's own and internal registry entries (FILE*, _LOADED, Pointer<...>)
        if not re.fullmatch(r"[A-Za-z]\w*", name) or not any(not is_metamethod(m) for m in classes[name]):
            continue
        typed = {m["method"]: m for m in types.get(tables.get(name), [])}
        methods = []
        for method in sorted(m for m in classes[name] if not is_metamethod(m)):
            t = typed.get(method, {})
            methods.append({"name": method, "args": clean_type(t.get("args", "?"), "args"),
                            "returns": clean_type(t.get("returns", "?"), "returns"), "handler": t.get("handler")})
        api["classes"][name] = {"method_table": tables.get(name), "methods": methods}

    for table in sorted(ENGINE_TABLES):
        functions = sorted(k for k, (kind, what) in members.get(table, {}).items() if kind == "function" and what == "C")
        if functions:
            api["engine"][table] = functions

    lib_env = globals_.get("lib", {})
    for name in sorted(lib_env):
        if lib_env[name] == "function" and (None, name) in library:
            params, file, line = library[(None, name)]
            api["library"]["functions"].append({"name": name, "params": params, "file": file, "line": line})
        elif lib_env[name] == "table" and name not in STANDARD_LIBRARIES and name not in api["classes"]:
            entries = []
            for key, (kind, what) in sorted(members.get(name, {}).items()):
                if kind == "function" and (name, key) in library:
                    params, file, line = library[(name, key)]
                    entries.append({"name": key, "params": params, "file": file, "line": line})
            if entries:
                api["library"]["tables"][name] = entries

    api["events"] = sorted(e for e in events if not vanilla_events or e in vanilla_events)
    return api


def anchor(text):
    return re.sub(r"[^a-z0-9_-]", "", text.lower().replace(" ", "-"))


def write_markdown(api, out):
    os.makedirs(out, exist_ok=True)
    header = "<!-- Generated by tools/apigen/apigen.py, don't edit by hand. -->\n\n"

    interfaces = [n for n in api["classes"] if n != "GAME"]
    lines = [header + "# Script interfaces\n",
             "The campaign model's classes as Lua sees them (Lunar bindings), e.g. what `get_cm():model():world()` returns. "
             "Methods are called with `:`. **Arguments** lists what the handler reads with the known helpers: object arguments "
             "(a faction, a character, ...) and some numbers aren't detected yet, so an empty cell doesn't mean the method takes "
             "none. **Returns** is empty where the handler pushes nothing we recognise (usually: nothing).\n",
             "Classes: " + ", ".join(f"[{n}](#{anchor(n)})" for n in interfaces) + "\n"]
    for name in interfaces:
        cls = api["classes"][name]
        lines.append(f"## {name}\n")
        if not cls["method_table"]:
            lines.append("_Method table not found in the binary yet, names only._\n")
        lines.append("| Method | Arguments | Returns |\n|---|---|---|")
        for m in cls["methods"]:
            lines.append(f"| `{m['name']}` | {m['args']} | {m['returns']} |")
        lines.append("")
    write(out, "script-interfaces.md", lines)

    game = api["classes"].get("GAME")
    if game:
        lines = [header + "# Game interface\n",
                 "The engine's campaign commands: the methods of the `GAME` object, which the library keeps as "
                 "`cm:get_game_interface()` (`scripting.game_interface`). Same columns as the [script interfaces](script-interfaces.md).\n",
                 "| Method | Arguments | Returns |\n|---|---|---|"]
        for m in game["methods"]:
            lines.append(f"| `{m['name']}` | {m['args']} | {m['returns']} |")
        lines.append("")
        if api["engine"]:
            lines.append("## Other engine tables\n")
            for table, functions in api["engine"].items():
                lines.append(f"- `{table}`: " + ", ".join(f"`{f}`" for f in functions))
            lines.append("")
        write(out, "game-interface.md", lines)

    lib = api["library"]
    lines = [header + "# Script library\n",
             "The game's own Lua library as loaded in a campaign (`script/_lib`, `lua_scripts`, the campaign's scripts), "
             "with the parameters from its source and where each function is defined in the game's `data.pack`. "
             "`campaign_manager` is the class of `get_cm()`.\n",
             "Tables: " + ", ".join(f"[{t}](#{anchor(t)})" for t in lib["tables"]) + ", [global functions](#global-functions)\n"]
    for table, entries in lib["tables"].items():
        lines.append(f"## {table}\n")
        lines.append("| Function | Defined in |\n|---|---|")
        for e in entries:
            lines.append(f"| `{table}:{e['name']}({e['params']})` | `{e['file']}:{e['line']}` |")
        lines.append("")
    lines.append("## Global functions\n")
    lines.append("| Function | Defined in |\n|---|---|")
    for e in lib["functions"]:
        lines.append(f"| `{e['name']}({e['params']})` | `{e['file']}:{e['line']}` |")
    lines.append("")
    write(out, "script-library.md", lines)

    lines = [header + "# Events\n",
             "The engine's script events (`lua_scripts/events.lua`, the keys of the global `events`). Listen with "
             "`get_cm():add_listener(name, event, condition, callback, persistent)`; the callback gets the event's context.\n"]
    lines += [f"- `{e}`" for e in api["events"]]
    lines.append("")
    write(out, "events.md", lines)

    counts = {
        "classes": len(interfaces),
        "methods": sum(len(c["methods"]) for n, c in api["classes"].items() if n != "GAME"),
        "game": len(game["methods"]) if game else 0,
        "typed": sum(1 for c in api["classes"].values() for m in c["methods"] if m["returns"] or m["args"]),
        "library": sum(len(e) for e in lib["tables"].values()) + len(lib["functions"]),
        "events": len(api["events"]),
    }
    lines = [header + "# Campaign API reference\n",
             f"Generated from a dump of the game's Lua in a campaign, the script interfaces' method tables in the Mac binary "
             f"and the game's own scripts. Build: {api['build']}.\n",
             f"- [Script interfaces](script-interfaces.md): {counts['classes']} classes, {counts['methods']} methods",
             f"- [Game interface](game-interface.md): {counts['game']} methods of the `GAME` object, and the other engine tables",
             f"- [Script library](script-library.md): {counts['library']} functions of the game's Lua library",
             f"- [Events](events.md): {counts['events']} events",
             f"- [attila-api.json](attila-api.json): all of it as data\n",
             f"{counts['typed']} of the {counts['methods'] + counts['game']} engine methods have an argument or return type so far. "
             "To regenerate: see `tools/apigen/README.md`.\n"]
    write(out, "README.md", lines)

    with open(os.path.join(out, "attila-api.json"), "w", encoding="utf-8") as f:
        json.dump(api, f, indent=1, sort_keys=True)
        f.write("\n")
    return counts


def write(out, name, lines):
    with open(os.path.join(out, name), "w", encoding="utf-8") as f:
        f.write("\n".join(lines).rstrip("\n") + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("data", help="folder of the extracted data.pack (with script/_lib, lua_scripts, campaigns)")
    parser.add_argument("--out", default=os.path.join(REPO, "docs", "reference"))
    args = parser.parse_args()
    if not os.path.isdir(os.path.join(args.data, "script", "_lib")):
        sys.exit(f"no script/_lib in {args.data}: pass the folder of the extracted data.pack")
    counts = write_markdown(build(args.data), args.out)
    print(", ".join(f"{k} {v}" for k, v in counts.items()))


if __name__ == "__main__":
    main()
