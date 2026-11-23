#!/usr/bin/env python3
"""tga2png.py <in.tga> [out.png]: converts the host's uncompressed 32-bit bottom-left TGA screenshots to PNG (standard library only)."""
import struct, sys, zlib

src = sys.argv[1]
dst = sys.argv[2] if len(sys.argv) > 2 else src.rsplit(".", 1)[0] + ".png"
d = open(src, "rb").read()
w, h = struct.unpack("<HH", d[12:16])
assert d[2] == 2 and d[16] == 32, "uncompressed 32-bit TGA expected"
px = d[18:18 + w * h * 4]
rows = []
for y in range(h - 1, -1, -1):  # bottom-left origin: flip
    r = bytearray(b"\0")
    line = px[y * w * 4:(y + 1) * w * 4]
    for i in range(0, len(line), 4):
        r += bytes((line[i + 2], line[i + 1], line[i], 255))
    rows.append(bytes(r))

def chunk(t, b):
    c = struct.pack(">I", len(b)) + t + b
    return c + struct.pack(">I", zlib.crc32(t + b) & 0xFFFFFFFF)

open(dst, "wb").write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)) +
                      chunk(b"IDAT", zlib.compress(b"".join(rows), 6)) + chunk(b"IEND", b""))
