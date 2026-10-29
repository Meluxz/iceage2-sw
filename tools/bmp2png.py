"""Convert a 24-bit BMP (the renderer's RECOMP_GL_DUMP output) to PNG, optionally brightened.
usage: bmp2png.py <in.bmp> <out.png> [gain]
"""
import struct, sys, zlib

d = open(sys.argv[1], "rb").read()
off = struct.unpack_from("<I", d, 10)[0]
w, h = struct.unpack_from("<ii", d, 18)
gain = float(sys.argv[3]) if len(sys.argv) > 3 else 1.0
stride = (w * 3 + 3) & ~3
rows = []
for y in range(abs(h)):
    sy = abs(h) - 1 - y if h > 0 else y
    r = d[off + sy * stride: off + sy * stride + w * 3]
    px = bytearray(b"\0")
    for x in range(w):
        b, g, rr = r[3 * x], r[3 * x + 1], r[3 * x + 2]
        px += bytes(min(255, int(v * gain)) for v in (rr, g, b))
    rows.append(bytes(px))


def chunk(t, data):
    c = struct.pack(">I", len(data)) + t + data
    return c + struct.pack(">I", zlib.crc32(t + data) & 0xFFFFFFFF)


png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, abs(h), 8, 2, 0, 0, 0))
png += chunk(b"IDAT", zlib.compress(b"".join(rows), 6)) + chunk(b"IEND", b"")
open(sys.argv[2], "wb").write(png)
