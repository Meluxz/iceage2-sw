"""Dump the useful bits of an XBE header: certificate, sections, XDK libraries."""
import struct, sys, datetime, hashlib

d = open(sys.argv[1], "rb").read()
assert d[:4] == b"XBEH", "not an XBE"
u32 = lambda o: struct.unpack_from("<I", d, o)[0]
base = u32(0x104)
va = lambda a: a - base

def ts(t):
    return datetime.datetime.fromtimestamp(t, datetime.UTC).strftime("%Y-%m-%d %H:%M:%S UTC")

print("sha1", hashlib.sha1(d).hexdigest())
print(f"base 0x{base:08X} image size 0x{u32(0x10C):X} built {ts(u32(0x114))}")
ep = u32(0x128)
for kind, ek, tk in (("retail", 0xA8FC57AB, 0x5B6D40B6), ("debug", 0x94859D4B, 0xEFB1F152)):
    e = ep ^ ek
    if base <= e < base + u32(0x10C):
        print(f"{kind} XBE: entry 0x{e:08X} kernel thunk 0x{u32(0x158) ^ tk:08X}")

c = va(u32(0x118))
tid = u32(c + 8)
name = d[c + 0xC:c + 0xC + 0x50].decode("utf-16-le").split("\0")[0]
region = u32(c + 0xA0)
regions = [n for b, n in ((1, "NTSC-U/Canada"), (2, "NTSC-J"), (4, "PAL/rest of world"), (0x80000000, "debug")) if region & b]
pub = (tid >> 16) & 0xFFFF
print(f"title '{name}' id {tid:08X} ({chr(pub >> 8)}{chr(pub & 0xFF)}-{tid & 0xFFFF:03d})")
print(f"cert built {ts(u32(c + 4))} region 0x{region:X} {regions} ratings 0x{u32(c + 0xA4):X} disk {u32(c + 0xA8)} version 0x{u32(c + 0xAC):X}")
print(f"allowed media 0x{u32(c + 0x9C):X}")

print("sections:")
sh = va(u32(0x120))
for i in range(u32(0x11C)):
    o = sh + i * 0x38
    fl, vaddr, vsz, raddr, rsz, naddr = struct.unpack_from("<6I", d, o)
    n = d[va(naddr):va(naddr) + 16].split(b"\0")[0].decode()
    print(f"  {n:10s} va 0x{vaddr:08X} vsize 0x{vsz:07X} raw 0x{rsz:07X} flags 0x{fl:X}")

print("XDK libraries:")
lo = va(u32(0x164))
for i in range(u32(0x160)):
    o = lo + i * 16
    n = d[o:o + 8].split(b"\0")[0].decode()
    ma, mi, bu, fl = struct.unpack_from("<4H", d, o + 8)
    print(f"  {n:8s} {ma}.{mi}.{bu} flags 0x{fl:X}")
