#!/usr/bin/env python3
# Copyright (C) 2026 Erica Synths
# SPDX-License-Identifier: GPL-3.0-only
"""Print the memory usage of a firmware ELF per memory region.

Reads the memory regions from the linker map ("Memory Configuration") and the
sections and symbols from the ELF. Sections count towards the region of their
run address (VMA); sections with contents that are loaded from elsewhere
(.data, .dtcm, .itcm_text) also count towards the region of their load
address (LMA), i.e. their initial values in FLASH.

Besides the table it shows:
  - the change since the previous build (kept in <elf>.memory.json and
    <elf>.memory.prev.json; rebuilding an unchanged ELF keeps the comparison),
  - the change against a baseline report (--baseline), e.g. the stock firmware,
  - with --top N, the N largest symbols per region.

Only the standard library is used, so it runs wherever Python 3 does.
"""

import argparse
import hashlib
import json
import os
import re
import struct
import sys

SHF_ALLOC = 0x2
SHT_NOBITS = 8
SHT_SYMTAB = 2
PT_LOAD = 1
STT_OBJECT = 1
STT_FUNC = 2

# Region usage from which the table marks a region as nearly full.
WARN_PERCENT = 90


# --- ELF ----------------------------------------------------------------------

class Elf:
    def __init__(self, path):
        with open(path, "rb") as f:
            self.data = data = f.read()
        if data[:4] != b"\x7fELF" or data[4] != 1 or data[5] != 1:
            sys.exit(f"{path}: not a 32-bit little-endian ELF file")
        (self.phoff, self.shoff, _flags, _ehsize, self.phentsize, self.phnum,
         self.shentsize, self.shnum, self.shstrndx) = struct.unpack_from("<IIIHHHHHH", data, 0x1C)
        self.sha256 = hashlib.sha256(data).hexdigest()

        self.segments = []
        for i in range(self.phnum):
            p_type, _off, vaddr, paddr, filesz, memsz, _flg, _align = struct.unpack_from(
                "<IIIIIIII", data, self.phoff + i * self.phentsize)
            if p_type == PT_LOAD:
                self.segments.append((vaddr, paddr, filesz, memsz))

        raw = [struct.unpack_from("<IIIIIIIIII", data, self.shoff + i * self.shentsize)
               for i in range(self.shnum)]
        names = raw[self.shstrndx]
        self.sections = []
        for (name, sh_type, flags, addr, offset, size, link, _info, _align, entsize) in raw:
            self.sections.append(dict(
                name=self._str(names[4], name), type=sh_type, flags=flags,
                addr=addr, offset=offset, size=size, link=link, entsize=entsize))

    def _str(self, table_offset, index):
        start = table_offset + index
        return self.data[start:self.data.index(b"\0", start)].decode("ascii", "replace")

    def section(self, name):
        return next((s for s in self.sections if s["name"] == name), None)

    def lma(self, sec):
        """Load address of an allocated section, or None if it isn't loaded."""
        if sec["type"] == SHT_NOBITS:
            return None
        for vaddr, paddr, filesz, _memsz in self.segments:
            if vaddr <= sec["addr"] and sec["addr"] + sec["size"] <= vaddr + filesz:
                return sec["addr"] - vaddr + paddr
        return sec["addr"]

    def symbols(self):
        """(name, value, size, type) of all symbols."""
        symtab = next((s for s in self.sections if s["type"] == SHT_SYMTAB), None)
        if not symtab:
            return []
        strtab = self.sections[symtab["link"]]
        result = []
        for i in range(symtab["size"] // symtab["entsize"]):
            name, value, size, info, _other, _shndx = struct.unpack_from(
                "<IIIBBH", self.data, symtab["offset"] + i * symtab["entsize"])
            result.append((self._str(strtab["offset"], name), value, size, info & 0xF))
        return result

    def compiler(self):
        sec = self.section(".comment")
        if not sec:
            return ""
        text = self.data[sec["offset"]:sec["offset"] + sec["size"]].decode("ascii", "replace")
        return next((s for s in text.split("\0") if s), "").removeprefix("GCC: ").strip()


# --- Linker map ---------------------------------------------------------------

def read_regions(map_path):
    """Memory regions as [(name, origin, length)] from the linker map."""
    regions = []
    with open(map_path, encoding="utf-8", errors="replace") as f:
        lines = iter(f)
        for line in lines:
            if line.startswith("Memory Configuration"):
                break
        for line in lines:
            if line.startswith("Linker script and memory map"):
                break
            m = re.match(r"(\S+)\s+0x([0-9a-fA-F]+)\s+0x([0-9a-fA-F]+)", line)
            if m and m.group(1) != "*default*":
                regions.append((m.group(1), int(m.group(2), 16), int(m.group(3), 16)))
    if not regions:
        sys.exit(f"{map_path}: no memory regions found")
    return regions


# --- Report -------------------------------------------------------------------

def region_of(regions, addr, size):
    for name, origin, length in regions:
        if origin <= addr and addr + size <= origin + length:
            return name
    return None


def build_report(elf, regions):
    report = {
        "sha256": elf.sha256,
        "compiler": elf.compiler(),
        "regions": {name: {"origin": origin, "size": length, "used": 0, "sections": {}}
                    for name, origin, length in regions},
        "symbols": {},
    }
    ends = {}

    def add(region, label, addr, size):
        if region and size:
            r = report["regions"][region]
            r["sections"][label] = r["sections"].get(label, 0) + size
            ends[region] = max(ends.get(region, 0), addr + size)


    for sec in elf.sections:
        if not sec["flags"] & SHF_ALLOC or not sec["size"]:
            continue
        run_region = region_of(regions, sec["addr"], sec["size"])
        add(run_region, sec["name"], sec["addr"], sec["size"])
        lma = elf.lma(sec)
        if lma is not None and lma != sec["addr"]:
            add(region_of(regions, lma, sec["size"]), f"{sec['name']} (initial values)", lma, sec["size"])

    # Like ld --print-memory-usage: used = end of the last section - region start,
    # so alignment gaps between sections count as used.
    for name, r in report["regions"].items():
        if name in ends:
            r["used"] = ends[name] - r["origin"]
            gaps = r["used"] - sum(r["sections"].values())
            if gaps:
                r["sections"]["(alignment gaps)"] = gaps

    syms = {name: value for name, value, _size, _type in elf.symbols()}
    veneers = {}
    for name, value, size, sym_type in elf.symbols():
        if sym_type not in (STT_FUNC, STT_OBJECT) or not size:
            continue
        region = region_of(regions, value & ~1, size)
        if not region:
            continue
        if name.endswith("_veneer"):
            v = veneers.setdefault(region, {"count": 0, "bytes": 0})
            v["count"] += 1
            v["bytes"] += size
        report["symbols"].setdefault(region, {})[name] = size
    for region, v in veneers.items():
        report["regions"][region]["veneers"] = v

    # Stack at the top of the DTCM, growing down towards the end of the DTCM data.
    if "_dtcm_stack" in syms and "_sccmstack" in syms:
        region = region_of(regions, syms["_sccmstack"], 0)
        if region:
            report["regions"][region]["stack"] = {
                "available": syms["_dtcm_stack"] - syms["_sccmstack"],
                "minimum": syms.get("_Min_Stack_Size", 0),
            }
    return report


def fmt_delta(new, old):
    if old is None:
        return ""
    d = new - old
    return "0" if d == 0 else f"{d:+,}"


def bar(used, size, width=20):
    filled = min(width, round(width * used / size)) if size else 0
    return "[" + "#" * filled + "." * (width - filled) + "]"


def print_report(report, title, prev=None, baseline=None, top=0):
    def used_of(rep, region):
        return rep["regions"].get(region, {}).get("used") if rep else None

    header = f"{'Region':<9} {'Used':>10} {'Size':>10} {'Free':>10} {'Use':>6}  {'':22}"
    if prev:
        header += f" {'vs. last':>9}"
    if baseline:
        header += f" {'vs. base':>9}"
    print()
    print(title)
    print(header)
    print("-" * len(header))
    for name, r in report["regions"].items():
        pct = 100.0 * r["used"] / r["size"] if r["size"] else 0.0
        line = (f"{name:<9} {r['used']:>10,} {r['size']:>10,} {r['size'] - r['used']:>10,} "
                f"{pct:>5.1f}%  {bar(r['used'], r['size'])}{'!' if pct >= WARN_PERCENT else ' '}")
        if prev:
            line += f" {fmt_delta(r['used'], used_of(prev, name)):>9}"
        if baseline:
            line += f" {fmt_delta(r['used'], used_of(baseline, name)):>9}"
        print(line)

        for label, size in sorted(r["sections"].items(), key=lambda kv: -kv[1]):
            print(f"  {label:<28} {size:>10,}")
        if "veneers" in r:
            v = r["veneers"]
            print(f"  {'(of which linker veneers)':<28} {v['bytes']:>10,}  ({v['count']} long-branch stubs)")
        if "stack" in r:
            s = r["stack"]
            note = "" if s["available"] >= 4 * s["minimum"] else "  LOW"
            print(f"  {'stack (the free space)':<28} {s['available']:>10,}  (linker minimum {s['minimum']:,}){note}")

    if baseline and baseline.get("compiler") != report.get("compiler"):
        print(f"Note: baseline was built with {baseline.get('compiler') or 'an unknown compiler'},")
        print(f"      this build with {report.get('compiler') or 'an unknown compiler'}.")

    if top:
        for name, syms in report["symbols"].items():
            biggest = sorted(syms.items(), key=lambda kv: -kv[1])[:top]
            if not biggest:
                continue
            print()
            print(f"Largest symbols in {name}:")
            for sym, size in biggest:
                delta = ""
                if baseline:
                    old = baseline.get("symbols", {}).get(name, {}).get(sym)
                    delta = "(new)" if old is None else fmt_delta(size, old)
                print(f"  {size:>8,}  {sym:<48} {delta}")
    print()


def load_json(path):
    try:
        with open(path, encoding="utf-8") as f:
            return json.load(f)
    except (OSError, ValueError):
        return None


def save_json(path, data):
    tmp = path + ".tmp"
    with open(tmp, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=1, sort_keys=True)
        f.write("\n")
    os.replace(tmp, path)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("elf", help="firmware ELF file")
    parser.add_argument("--map", help="linker map (default: <elf without .elf>.map)")
    parser.add_argument("--baseline", help="compare against this saved report (ignored if missing)")
    parser.add_argument("--save-baseline", metavar="FILE", help="write this build's report to FILE and exit")
    parser.add_argument("--top", type=int, default=0, help="also list the N largest symbols per region")
    parser.add_argument("--no-history", action="store_true", help="don't compare with or record the previous build")
    args = parser.parse_args()

    elf_path = args.elf
    map_path = args.map or os.path.splitext(elf_path)[0] + ".map"
    elf = Elf(elf_path)
    report = build_report(elf, read_regions(map_path))

    if args.save_baseline:
        save_json(args.save_baseline, report)
        print(f"Memory baseline written to {args.save_baseline}")
        return

    prev = None
    if not args.no_history:
        cur_path = elf_path + ".memory.json"
        prev_path = elf_path + ".memory.prev.json"
        last = load_json(cur_path)
        if last and last.get("sha256") == report["sha256"]:
            prev = load_json(prev_path)  # same ELF as last time: keep showing its change
        else:
            prev = last
            if last:
                save_json(prev_path, last)
            save_json(cur_path, report)

    baseline = load_json(args.baseline) if args.baseline else None
    title = f"Memory usage of {os.path.basename(elf_path)}"
    if report["compiler"]:
        title += f", {report['compiler']}"
    print_report(report, title, prev=prev, baseline=baseline, top=args.top)


if __name__ == "__main__":
    main()
