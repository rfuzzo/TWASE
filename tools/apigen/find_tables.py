#!/usr/bin/env python3
"""Finds the Lunar method tables in the Mac game binary, without Binary Ninja.

A method table is a run of 24-byte entries { const char* name, member function, this adjustment } ending at a null
name. In the arm64 Mach-O the pointers in __DATA / __DATA_CONST are chained fixups (DYLD_CHAINED_PTR_64_OFFSET):
a rebase's target is 0x100000000 + (raw & 0xFFFFFFFFF). Template methods (e.g. the list classes'
LIST_SCRIPT_INTERFACE<...>::item_at) are weak definitions, so their pointers are binds (bit 63, import ordinal in the
low 24 bits) to the binary's own symbol, resolved here through the fixups' import table and the symbol table.

usage: find_tables.py <game executable> [--write]
  prints the tables (address, size, first and last method); --write replaces the tables in data/lunar_types_macos.json
  with the ones found, keeping the types of handlers it already has (bn_types.py types the rest)
"""

import argparse
import json
import os
import re
import struct

HERE = os.path.dirname(os.path.abspath(__file__))
TYPES = os.path.join(HERE, "data", "lunar_types_macos.json")
BASE = 0x100000000
NAME = re.compile(rb"[A-Za-z_][A-Za-z0-9_]{0,120}\0")


def load_commands(binary):
    magic, _, _, _, ncmds, _, _, _ = struct.unpack_from("<IiiIIIII", binary, 0)
    assert magic == 0xFEEDFACF, "not a 64-bit Mach-O (thin arm64 expected)"
    offset = 32
    for _ in range(ncmds):
        cmd, size = struct.unpack_from("<II", binary, offset)
        yield cmd, offset
        offset += size


def segments(binary):
    result = []
    for cmd, offset in load_commands(binary):
        if cmd == 0x19:  # LC_SEGMENT_64
            name = binary[offset + 8:offset + 24].rstrip(b"\0").decode()
            vmaddr, vmsize, fileoff, filesize = struct.unpack_from("<QQQQ", binary, offset + 24)
            result.append((name, vmaddr, vmsize, fileoff, filesize))
    return result


def bind_targets(binary):
    """import ordinal -> address of the binary's own definition of that symbol, and address -> symbol name"""
    imports, symbols = [], {}
    for cmd, offset in load_commands(binary):
        if cmd == 0x80000034:  # LC_DYLD_CHAINED_FIXUPS
            dataoff, _ = struct.unpack_from("<II", binary, offset + 8)
            _, _, imports_offset, symbols_offset, count, imports_format, _ = struct.unpack_from("<7I", binary, dataoff)
            # DYLD_CHAINED_IMPORT (4 bytes), _ADDEND (8), _ADDEND64 (16): the name offset after the ordinal and weak bit
            size = {1: 4, 2: 8, 3: 16}[imports_format]
            for i in range(count):
                entry = dataoff + imports_offset + size * i
                if imports_format == 3:
                    name_offset = struct.unpack_from("<Q", binary, entry)[0] >> 32
                else:
                    name_offset = struct.unpack_from("<I", binary, entry)[0] >> 9
                start = dataoff + symbols_offset + name_offset
                imports.append(binary[start:binary.index(b"\0", start)])
        elif cmd == 0x2:  # LC_SYMTAB
            symoff, nsyms, stroff, _ = struct.unpack_from("<IIII", binary, offset + 8)
            for i in range(nsyms):
                strx, ntype, _, _, value = struct.unpack_from("<IBBHQ", binary, symoff + 16 * i)
                if ntype & 0x0E == 0x0E and value:  # N_SECT: defined here
                    start = stroff + strx
                    symbols.setdefault(binary[start:binary.index(b"\0", start)], value)
    names = {value: name.decode("latin-1") for name, value in symbols.items()}
    return {i: symbols[name] for i, name in enumerate(imports) if name in symbols}, names


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("binary")
    parser.add_argument("--write", action="store_true")
    args = parser.parse_args()

    binary = open(args.binary, "rb").read()
    segs = segments(binary)
    text = next(s for s in segs if s[0] == "__TEXT")

    def in_text(va):
        return text[1] <= va < text[1] + text[2]

    binds, symbol_names = bind_targets(binary)

    def target(raw):
        if raw >> 63:
            return binds.get(raw & 0xFFFFFF, 0)
        return BASE + (raw & 0xFFFFFFFFF) if raw else 0

    def cstring(va):
        off = va - text[1] + text[3]
        m = NAME.match(binary, off)
        return m.group(0)[:-1].decode() if m else None

    tables = {}
    for name, vmaddr, vmsize, fileoff, filesize in segs:
        if not name.startswith("__DATA"):
            continue
        offset, end = fileoff, fileoff + filesize - 24
        while offset < end:
            run, position = [], offset
            while position < end:
                raw_name, raw_fn, adjust = struct.unpack_from("<QQQ", binary, position)
                name_va, fn_va = target(raw_name), target(raw_fn)
                method = cstring(name_va) if in_text(name_va) else None
                # a member function pointer: { function, adjustment }; on arm64 a virtual one has bit 0 of the
                # adjustment set and the vtable offset in place of the function (e.g. is_null_interface)
                virtual = adjust & 1 and not raw_fn >> 63 and raw_fn < 0x10000
                if not method or adjust > 0x201 or not (virtual or in_text(fn_va)):
                    break
                if virtual:
                    entry = {"method": method, "handler": None, "vtable_offset": hex(raw_fn)}
                else:
                    entry = {"method": method, "handler": hex(fn_va)}
                    if fn_va in symbol_names:
                        entry["symbol"] = symbol_names[fn_va]
                run.append(entry)
                position += 24
            # a table ends at a null entry
            if len(run) >= 2 and binary[position:position + 8] == b"\0" * 8:
                tables[hex(vmaddr + offset - fileoff)] = run
                offset = position + 24
            else:
                offset += 8

    for address, methods in sorted(tables.items(), key=lambda t: int(t[0], 16)):
        print(f"{address} {len(methods):4}  {methods[0]['method']} … {methods[-1]['method']}")
    print(len(tables), "tables,", sum(len(m) for m in tables.values()), "methods")

    if args.write:
        with open(TYPES, encoding="utf-8") as f:
            known = {m["handler"]: m for methods in json.load(f).values() for m in methods if m.get("handler")}
        for methods in tables.values():
            for m in methods:
                previous = known.get(m["handler"], {})
                m["args"], m["returns"] = previous.get("args", ""), previous.get("returns", "")
        with open(TYPES, "w", encoding="utf-8") as f:
            json.dump(tables, f, indent=1)
            f.write("\n")
        print("wrote", len(tables), "tables to", TYPES)

if __name__ == "__main__":
    main()
