#!/usr/bin/env python3
"""Flash/RAM breakdown of GNU ld map files (read-only analysis, no build).

usage: zephyr_map_breakdown.py OUTDIR LABEL=MAP[,SIZEFILE] [LABEL=MAP[,SIZEFILE] ...]

SIZEFILE is the output of `size -B <elf>` (optional); it is used to reconcile the map totals.
Flash is every input section whose output section is loaded into 0x08000000-0x0A000000
(the load address counts for .data). RAM is every other allocated input section.
"""
import csv
import os
import re
import sys
from collections import defaultdict

FLASH_LO, FLASH_HI = 0x08000000, 0x0A000000
HEX = r"0x[0-9a-fA-F]+"

# first matching rule wins; (category, substrings of the input file path)
CATEGORIES = [
    ("micro-ROS", ("libmicroros", "micro_ros", "microros")),
    ("rosserial/ros_msgs", ("ros_lib", "ros_msgs", "rosserial")),
    ("app", ("app/libapp.a", "/app/", "app.dir", "lexxpluss_apps")),
    ("libc/libgcc", ("libc.a", "libm.a", "libgcc", "libnosys", "libstdc++", "libsupc++", "newlib", "picolibc",
                     "liblib__libc", "libc_nano")),
    ("HAL (stm32cube)", ("hal_stm32", "stm32cube", "modules/hal/stm32", "libmodules__hal")),
    ("Zephyr drivers", ("libdrivers__", "zephyr/drivers")),
    ("Zephyr kernel/arch", ("libkernel", "libarch__", "libisr_tables", "zephyr/arch", "zephyr/kernel", "libsoc")),
    ("Zephyr lib/subsys/core", ("libzephyr.a", "zephyr/CMakeFiles/zephyr.dir", "liblib__", "libsubsys__",
                                "zephyr/lib", "zephyr/subsys", "zephyr_final", "offsets", "fatfs",
                                "modules__fs", "littlefs", "mbedtls", "mcuboot")),
]


def category(path):
    for name, keys in CATEGORIES:
        if any(k in path for k in keys):
            return name
    return "other"


def archive(path):
    m = re.search(r"([^/\s()]+\.a)\(", path)
    if m:
        return m.group(1)
    m = re.search(r"([^/\s()]+\.a)$", path)
    if m:
        return m.group(1)
    return os.path.dirname(path) or path


def parse(path):
    """Return a list of (output_section, flash_resident, input_section, size, file)."""
    rows = []
    started = False
    out = None            # (name, vma, lma or None)
    pending_header = None  # output section name waiting for its address line
    pending_input = None   # input section name waiting for its address line
    with open(path, errors="replace") as f:
        for line in f:
            if not started:
                started = line.startswith("Linker script and memory map")
                continue
            line = line.rstrip("\n")
            if not line.strip():
                pending_header = pending_input = None
                continue
            if line[0] not in " \t":
                pending_input = None
                m = re.match(rf"^(\S+)\s+({HEX})\s+({HEX})(?:\s+load address ({HEX}))?", line)
                if m:
                    out = (m.group(1), int(m.group(2), 16), int(m.group(4), 16) if m.group(4) else None)
                    pending_header = None
                elif re.match(r"^\.\S+\s*$", line):
                    pending_header = line.strip()
                continue
            if pending_header:
                m = re.match(rf"^\s+({HEX})\s+({HEX})(?:\s+load address ({HEX}))?\s*$", line)
                if m:
                    out = (pending_header, int(m.group(1), 16), int(m.group(3), 16) if m.group(3) else None)
                pending_header = None
                continue
            if out is None:
                continue
            m = re.match(rf"^ (\S+)\s+({HEX})\s+({HEX})\s+(\S.*)$", line)
            if m:
                name, size, file = m.group(1), int(m.group(3), 16), m.group(4).strip()
            else:
                m2 = re.match(rf"^ (\S+)\s*$", line)
                if m2:
                    pending_input = m2.group(1)
                    continue
                m3 = re.match(rf"^\s+({HEX})\s+({HEX})\s+(\S.*)$", line)
                if m3 and pending_input:
                    name, size, file = pending_input, int(m3.group(2), 16), m3.group(3).strip()
                else:
                    pending_input = None
                    continue
            pending_input = None
            if size == 0 or file.startswith("*fill*") or name == "*fill*":
                continue
            _, vma, lma = out
            addr = lma if lma is not None else vma
            if vma == 0 and lma is None:
                continue  # not allocated (debug sections)
            flash = FLASH_LO <= addr < FLASH_HI
            rows.append((out[0], flash, name, size, file))
    return rows


def size_total(sizefile):
    """text+data of `size -B` (the last line with four numbers)."""
    if not sizefile or not os.path.exists(sizefile):
        return None
    for line in reversed(open(sizefile).read().splitlines()):
        m = re.match(r"^\s*(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+", line)
        if m:
            return int(m.group(1)), int(m.group(2)), int(m.group(3))
    return None


def main():
    outdir = sys.argv[1]
    os.makedirs(outdir, exist_ok=True)
    targets = []
    for a in sys.argv[2:]:
        label, rest = a.split("=", 1)
        mp, _, sz = rest.partition(",")
        targets.append((label, mp, sz or None))

    cat_tot = {}   # label -> {category: [flash, ram]}
    summary = []
    for label, mp, sz in targets:
        rows = parse(mp)
        cats = defaultdict(lambda: [0, 0])
        libs = defaultdict(lambda: [0, 0, ""])
        members = []
        for outsec, flash, name, size, file in rows:
            c = category(file)
            cats[c][0 if flash else 1] += size
            a = archive(file)
            libs[a][0 if flash else 1] += size
            libs[a][2] = c
            if flash:
                members.append((size, f"{name} {file}"))
        cat_tot[label] = cats
        flash_total = sum(v[0] for v in cats.values())
        ram_total = sum(v[1] for v in cats.values())
        st = size_total(sz)
        summary.append((label, flash_total, ram_total, st))
        with open(os.path.join(outdir, f"{label}_by_archive.csv"), "w", newline="") as f:
            w = csv.writer(f)
            w.writerow(["archive_or_dir", "category", "flash_bytes", "ram_bytes"])
            for a, (fl, ra, c) in sorted(libs.items(), key=lambda kv: -kv[1][0]):
                w.writerow([a, c, fl, ra])
        with open(os.path.join(outdir, f"{label}_top_flash_members.csv"), "w", newline="") as f:
            w = csv.writer(f)
            w.writerow(["flash_bytes", "input_section file"])
            for size, desc in sorted(members, reverse=True)[:200]:
                w.writerow([size, desc])
        print(f"\n## {label}\n")
        print("| Category | Flash (B) | RAM (B) |\n|---|---:|---:|")
        for c, (fl, ra) in sorted(cats.items(), key=lambda kv: -kv[1][0]):
            print(f"| {c} | {fl:,} | {ra:,} |")
        print(f"| **total** | **{flash_total:,}** | **{ram_total:,}** |")
        print("\nTop 12 archives by Flash:\n\n| Archive / dir | Category | Flash (B) | RAM (B) |\n|---|---|---:|---:|")
        for a, (fl, ra, c) in sorted(libs.items(), key=lambda kv: -kv[1][0])[:12]:
            print(f"| {a} | {c} | {fl:,} | {ra:,} |")
        print("\nTop 12 Flash input sections:\n\n| Size (B) | Section and file |\n|---:|---|")
        for size, desc in sorted(members, reverse=True)[:12]:
            print(f"| {size:,} | {desc[:110]} |")

    print("\n## Reconciliation with `size -B` (text + data)\n")
    print("| Label | Map Flash total (B) | size text+data (B) | Difference (B) | Map RAM total (B) | size data+bss (B) |")
    print("|---|---:|---:|---:|---:|---:|")
    for label, fl, ra, st in summary:
        if st:
            text, data, bss = st
            print(f"| {label} | {fl:,} | {text + data:,} | {fl - (text + data):,} | {ra:,} | {data + bss:,} |")
        else:
            print(f"| {label} | {fl:,} | n/a | n/a | {ra:,} | n/a |")

    if len(targets) > 1:
        cats_all = sorted({c for v in cat_tot.values() for c in v})
        print("\n## Flash by category, side by side (B)\n")
        print("| Category | " + " | ".join(l for l, _, _ in targets) + " |")
        print("|---|" + "---:|" * len(targets))
        for c in cats_all:
            print(f"| {c} | " + " | ".join(f"{cat_tot[l].get(c, [0, 0])[0]:,}" for l, _, _ in targets) + " |")


if __name__ == "__main__":
    main()
