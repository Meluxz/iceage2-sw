"""Tile 24-bit BMP frames (downscaled by an integer factor) into one BMP contact sheet.
   python3 bmp_sheet.py <out.bmp> <scale> <cols> <in1.bmp> [in2.bmp ...]"""
import struct
import sys


def read_bmp(path):
    d = open(path, "rb").read()
    off = struct.unpack_from("<I", d, 10)[0]
    w, h = struct.unpack_from("<ii", d, 18)
    bpp = struct.unpack_from("<H", d, 28)[0]
    bypp = bpp // 8
    row = (w * bypp + 3) & ~3
    flip = h > 0
    h = abs(h)
    rows = []
    for y in range(h):
        sy = h - 1 - y if flip else y
        o = off + sy * row
        rows.append(d[o:o + w * bypp])
    return w, h, bypp, rows


def main():
    out, scale, cols = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
    files = sys.argv[4:]
    w, h, _, _ = read_bmp(files[0])
    # CROP=x,y,w,h: tile only that part of each frame (top-left origin).
    cx, cy = 0, 0
    crop = __import__("os").environ.get("CROP")
    if crop:
        cx, cy, w, h = (int(v) for v in crop.split(","))
    tw, th = w // scale, h // scale
    nrows = (len(files) + cols - 1) // cols
    W, H = tw * cols, th * nrows
    canvas = [bytearray(W * 3) for _ in range(H)]
    for i, f in enumerate(files):
        fw, fh, b, rows = read_bmp(f)
        ox, oy = (i % cols) * tw, (i // cols) * th
        for y in range(th):
            src = rows[cy + y * scale]
            dst = canvas[oy + y]
            for x in range(tw):
                s = (cx + x * scale) * b
                dst[(ox + x) * 3:(ox + x) * 3 + 3] = src[s:s + 3]
    row = (W * 3 + 3) & ~3
    hdr = struct.pack("<2sIHHIIiiHHIIiiII", b"BM", 54 + row * H, 0, 0, 54, 40, W, -H, 1, 24, 0,
                      row * H, 2835, 2835, 0, 0)
    with open(out, "wb") as fo:
        fo.write(hdr)
        pad = b"\0" * (row - W * 3)
        for r in canvas:
            fo.write(bytes(r) + pad)


main()
