#!/usr/bin/env python3
"""
patch_bitcode.py — Add section ".volatile_ram_code" to @delay_nus in LLVM IR.

LTO promotes static delay_nus (pmu_analog.c) to delay_nus.636 with no
section attribute → lands in .text (Flash). ldo13_on in .volatile_ram_code
(RAM) tries to call it via 23-bit PI32V2 jump → truncated relocation.
Fix: add section ".volatile_ram_code" to delay_nus in the LLVM IR bitcode
before linking, so delay_nus.636 ends up in RAM alongside ldo13_on.

Usage: python3 patch_bitcode.py <power_hw.ll>
"""
import re
import sys

ll_file = sys.argv[1] if len(sys.argv) > 1 else 'power_hw.ll'

with open(ll_file, 'r') as f:
    content = f.read()

orig = content

# LLVM IR function def pattern:
#   define [linkage] [type] @delay_nus(params) [attrs] {
# We need to insert: section ".volatile_ram_code"
# BEFORE the opening brace (if not already present)
pattern = r'(define\b[^@]*@delay_nus\s*\([^)]*\)(?:\s+(?!section)[^\s{]+)*)'

def add_section(m):
    s = m.group(0)
    if 'section' not in s:
        s = s.rstrip() + ' section ".volatile_ram_code"'
    return s

content, n = re.subn(pattern, add_section, content, flags=re.MULTILINE)
if n > 0:
    print(f"Patched {n} delay_nus definition(s)")
    with open(ll_file, 'w') as f:
        f.write(content)
else:
    print("No delay_nus definition found — showing all delay_nus lines:")
    for line in orig.split('\n'):
        if 'delay_nus' in line:
            print(f"  {line[:120]}")
    sys.exit(1)
