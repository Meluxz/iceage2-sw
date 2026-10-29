"""Turn recomp_stubs_unresolved.c into seed candidates.

A stub there is an address some code calls or jumps to that the disassembler never made a
function: the generated stub just pops the return address and does nothing, so whatever the
real code did is skipped and a `ret N` callee comes back N bytes short (0x00185EB0, tail-
jumped from 0x00186530, broke the stack of everything above it after frame ~90).

Kept: stubs in a code section that lie in a gap no detected function covers -- an undiscovered
function (0x00185EB0 follows the previous function's jump table, so "what precedes it" is no
test). Not kept: stubs inside a detected function -- a label some other function jumps into;
seeding those would split the real function.
usage: stub_seeds.py <xbe> <recomp_stubs_unresolved.c> <functions.json> <seed_functions.json>
"""
import json, re, struct, sys
import capstone

xbe_path, stubs_path, funcs_path, seeds_path = sys.argv[1:5]
xbe = open(xbe_path, "rb").read()
base = struct.unpack_from("<I", xbe, 0x104)[0]
nsec, shdr = struct.unpack_from("<II", xbe, 0x11C)
CODE = {".text", "D3DX", "XMV", "D3D", "XGRPH", "DSOUND", "XPP"}
secs = []
for i in range(nsec):
    fl, va, vsz, raw, rsz, naddr = struct.unpack_from("<6I", xbe, shdr - base + i * 0x38)
    name = xbe[naddr - base:naddr - base + 16].split(b"\0")[0].decode()
    if name in CODE:
        secs.append((va, raw, rsz))


def read(va, n):
    for sva, raw, rsz in secs:
        if sva <= va < sva + rsz:
            return xbe[raw + va - sva: raw + min(rsz, va - sva + n)]
    return None


def num(v):
    return int(v, 16) if isinstance(v, str) else v


funcs = json.load(open(funcs_path))
funcs = funcs if isinstance(funcs, list) else funcs.get("functions", list(funcs.values()))
spans = sorted((num(f["start"]), num(f["end"])) for f in funcs)
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
addrs = sorted({int(m, 16) for m in re.findall(r"^void sub_([0-9A-Fa-f]{8})\(void\)",
                                               open(stubs_path).read(), re.M)})
seeds = json.load(open(seeds_path))
have = {int(s["start"], 16) for s in seeds}
added = []
for a in addrs:
    if a in have:
        continue
    code = read(a, 48)
    if not code:
        continue
    inside = [s for s, e in spans if s < a < e]
    insns = list(md.disasm(code, a))
    ok_code = len(insns) >= 3 and not any(i.mnemonic in ("int3", "hlt", "in", "out")
                                           for i in insns[:3])
    if inside:
        print(f"  label {a:08X} inside sub_{inside[-1]:08X}")
    elif ok_code:
        seeds.append({"start": f"0x{a:08X}", "note": "Unresolved stub (recomp_stubs_unresolved.c) "
                      "in a gap no detected function covers: an undiscovered function."})
        added.append(a)
        print(f"  + {a:08X}  {insns[0].mnemonic} {insns[0].op_str}")
    else:
        print(f"  ? {a:08X}  in a gap but does not decode as code")
json.dump(seeds, open(seeds_path, "w"), indent=1)
print(f"added {len(added)} of {len(addrs)} stubs")
