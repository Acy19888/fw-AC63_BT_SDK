#!/usr/bin/env python3
"""
patch_bitcode.py — Ensure @delay_nus in LLVM IR has section ".volatile_ram_code".

LTO promotes static delay_nus (pmu_analog.c) to delay_nus.636 with no
section attribute -> lands in .text (Flash). ldo13_on in .volatile_ram_code
(RAM) tries to call it via 23-bit PI32V2 jump -> truncated relocation.
Fix: add section ".volatile_ram_code" to delay_nus in the LLVM IR bitcode
before linking, so delay_nus.636 ends up in RAM alongside ldo13_on.

Usage: python3 patch_bitcode.py <power_hw.ll>
"""
import re
import sys

ll_file = sys.argv[1] if len(sys.argv) > 1 else 'power_hw.ll'

with open(ll_file, 'r') as f:
    lines = f.readlines()

TARGET_SECTION = 'section ".volatile_ram_code"'
patched = 0
out = []

for line in lines:
    # Match any "define ... @delay_nus(...) ... {" line
    if re.search(r'\bdefine\b.*@delay_nus\s*\(', line) and line.rstrip().endswith('{'):
        if TARGET_SECTION not in line:
            # Insert section attribute before the opening brace
            stripped = line.rstrip()
            line = stripped[:-1].rstrip() + ' ' + TARGET_SECTION + ' {\n'
            patched += 1
        else:
            print("  (delay_nus already has volatile_ram_code section -- no change needed)")
    out.append(line)

if patched > 0:
    print("Patched %d delay_nus definition(s)" % patched)
    with open(ll_file, 'w') as f:
        f.writelines(out)
elif not any(re.search(r'\bdefine\b.*@delay_nus\s*\(', l) for l in lines):
    print("No delay_nus definition found -- showing all delay_nus lines:")
    for l in lines:
        if 'delay_nus' in l:
            print("  %s" % l[:120])
    sys.exit(1)
else:
    # Found but already correct -- write unchanged (idempotent)
    print("delay_nus already has correct section attribute, nothing to do.")
    with open(ll_file, 'w') as f:
        f.writelines(out)
