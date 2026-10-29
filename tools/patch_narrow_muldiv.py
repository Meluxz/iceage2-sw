"""Patch the generated code's one-operand mul/imul/div/idiv on 8/16-bit
operands (lifter.py _narrow_muldiv fix, 2026-10-28) without a regen.
    python tools/patch_narrow_muldiv.py iceage2-pc/src/recomp/gen [--dry]
"""
import glob
import os
import re
import sys

W8 = ("LO8(", "HI8(", "MEM8(", "SMEM8(")
W16 = ("LO16(", "MEM16(", "SMEM16(")

PATS = {
    "imul": re.compile(r"^(\s*)\{ int64_t _r = \(int64_t\)\(int32_t\)eax \* \(int64_t\)\(int32_t\)(.+);$"),
    "mul": re.compile(r"^(\s*)\{ uint64_t _r = \(uint64_t\)eax \* \(uint64_t\)(.+);$"),
    "div": re.compile(r"^(\s*)\{ uint64_t _dividend = \(\(uint64_t\)edx << 32\) \| eax;$"),
    "idiv": re.compile(r"^(\s*)\{ int64_t _dividend = \(\(int64_t\)\(int32_t\)edx << 32\) \| eax;$"),
}
DIV_SRC = {
    "div": re.compile(r"^\s*eax = \(uint32_t\)\(_dividend / \(uint32_t\)(.+)\);$"),
    "idiv": re.compile(r"^\s*eax = \(uint32_t\)\(\(int32_t\)\(_dividend / \(int32_t\)(.+)\)\);$"),
}


def width(src):
    if src.startswith(W8):
        return 1
    if src.startswith(W16):
        return 2
    return 4


def narrow(m, src, w, ind):
    sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "xboxrecomp-b3"))
    from tools.recomp.lifter import _narrow_muldiv
    return [ind + l for l in _narrow_muldiv(m, src, w)]


def main():
    dry = "--dry" in sys.argv
    total = 0
    for path in sorted(glob.glob(os.path.join(sys.argv[1], "*.c"))):
        lines = open(path, encoding="utf-8").read().split("\n")
        out, i, n = [], 0, 0
        while i < len(lines):
            l = lines[i]
            done = False
            for m in ("imul", "mul"):
                g = PATS[m].match(l)
                if g and i + 1 < len(lines) and "eax = (uint32_t)_r; edx = (uint32_t)(_r >> 32); }" in lines[i + 1]:
                    src, w = g.group(2), width(g.group(2))
                    if w < 4:
                        out += narrow(m, src, w, g.group(1))
                        i += 2; n += 1; done = True
                    break
            if not done:
                for m in ("div", "idiv"):
                    g = PATS[m].match(l)
                    if g and i + 2 < len(lines):
                        s = DIV_SRC[m].match(lines[i + 1])
                        if s and width(s.group(1)) < 4:
                            out += narrow(m, s.group(1), width(s.group(1)), g.group(1))
                            i += 3; n += 1; done = True
                        break
            if not done:
                out.append(l); i += 1
        if n:
            print(f"{os.path.basename(path)}: {n}")
            total += n
            if not dry:
                open(path, "w", encoding="utf-8", newline="").write("\n".join(out))
    print("total", total)


main()
