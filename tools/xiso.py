"""Minimal XDVDFS (Xbox XISO) reader: list files, extract selected ones.
usage: xiso.py <iso> list <out.tsv>
       xiso.py <iso> extract <outdir> [path-in-iso ...]   (no paths = everything)
"""
import struct, sys, os

SECTOR = 2048
MAGIC = b"MICROSOFT*XBOX*MEDIA"


def find_base(f):
    # Plain XISO: volume descriptor at sector 32. Full Redump image: game partition at 0x18300000.
    for base in (0, 0x18300000, 0x2080000, 0xFD90000):
        f.seek(base + 32 * SECTOR)
        if f.read(20) == MAGIC:
            return base
    raise SystemExit("XDVDFS volume descriptor not found")


def walk(f, base, sector, size, prefix, out):
    f.seek(base + sector * SECTOR)
    data = f.read(size)
    seen = set()
    stack = [0]
    while stack:
        off = stack.pop()
        if off in seen or off + 14 > len(data):
            continue
        seen.add(off)
        left, right, start, fsize, attr, nlen = struct.unpack_from("<HHIIBB", data, off)
        if left == 0xFFFF:
            continue
        name = data[off + 14:off + 14 + nlen].decode("latin-1")
        path = prefix + name
        if attr & 0x10:
            out.append((path + "/", start, 0, True))
            if fsize:
                walk(f, base, start, fsize, path + "/", out)
        else:
            out.append((path, start, fsize, False))
        if left:
            stack.append(left * 4)
        if right:
            stack.append(right * 4)


def main():
    iso, cmd = sys.argv[1], sys.argv[2]
    with open(iso, "rb") as f:
        base = find_base(f)
        f.seek(base + 32 * SECTOR + 20)
        root_sec, root_size = struct.unpack("<II", f.read(8))
        out = []
        walk(f, base, root_sec, root_size, "", out)
        out.sort(key=lambda e: e[0].lower())
        if cmd == "list":
            with open(sys.argv[3], "w", encoding="utf-8") as o:
                o.write(f"# base=0x{base:X}\n")
                for p, s, n, d in out:
                    o.write(f"{p}\t{n}\t{s}\n")
            print(f"base=0x{base:X} entries={len(out)}")
        elif cmd == "extract":
            outdir = sys.argv[3]
            want = {w.lower(): w for w in sys.argv[4:]}
            for p, s, n, d in out:
                if d or (want and p.lower() not in want):
                    continue
                dst = os.path.join(outdir, p.replace("/", os.sep))
                os.makedirs(os.path.dirname(dst) or ".", exist_ok=True)
                f.seek(base + s * SECTOR)
                with open(dst, "wb") as o:
                    left = n
                    while left:
                        chunk = f.read(min(left, 1 << 20))
                        o.write(chunk)
                        left -= len(chunk)
                print(f"extracted {p} ({n} bytes)")


if __name__ == "__main__":
    main()
