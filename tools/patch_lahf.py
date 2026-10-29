"""Patch the generated code's x87 `lahf` comments (lifter.py _lahf_value fix,
2026-10-28) without a 22-minute regen: a lahf two lines after a fucompi
(with `fstp st(0)` between) becomes the AH byte built from g_fp_cmp.
    python tools/patch_lahf.py iceage2-pc/src/recomp/gen
"""
import glob
import os
import sys

COMMENT = "/* lahf - load AH from flags (used in FPU compare idiom) */"
AH = ("SET_HI8(eax, (uint8_t)(0x02 | ((g_fp_cmp == 0 || g_fp_cmp == 2) ? 0x40 : 0)"
      " | (g_fp_cmp == 2 ? 0x04 : 0) | ((g_fp_cmp < 0 || g_fp_cmp == 2) ? 0x01 : 0)));"
      " /* lahf */")

total = 0
for path in sorted(glob.glob(os.path.join(sys.argv[1], "*.c"))):
    with open(path, encoding="utf-8") as f:
        lines = f.readlines()
    n = 0
    for i, line in enumerate(lines):
        if line.strip() != COMMENT or i < 2:
            continue
        if "/* fucompi */" in lines[i - 2] and "/* fstp st(0) */" in lines[i - 1]:
            indent = line[:len(line) - len(line.lstrip())]
            lines[i] = indent + AH + "\n"
            n += 1
    if n:
        with open(path, "w", encoding="utf-8", newline="") as f:
            f.writelines(lines)
        print(f"{os.path.basename(path)}: {n}")
        total += n
print(f"total {total}")
