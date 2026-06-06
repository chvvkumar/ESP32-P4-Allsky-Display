# tools/bake_moon_texture.py
# Downsamples ESP32-P4-NINA-Display/main/moon_texture.png (500x500 RGBA)
# to 256x256 luminance+alpha and emits a C header (128 KB flash).
from PIL import Image
import sys

SRC = sys.argv[1] if len(sys.argv) > 1 else \
    r"C:\Users\Kumar\git\ESP32-P4-NINA-Display\main\moon_texture.png"
SIZE = 256
img = Image.open(SRC).convert("RGBA").resize((SIZE, SIZE), Image.LANCZOS)
px = img.load()
out = ["#pragma once", "#include <stdint.h>",
       f"#define MOON_TEX_SIZE {SIZE}",
       "// luminance,alpha pairs, row-major, MOON_TEX_SIZE^2 entries",
       "static const uint8_t MOON_TEX_LA[MOON_TEX_SIZE*MOON_TEX_SIZE*2] = {"]
vals = []
for y in range(SIZE):
    for x in range(SIZE):
        r, g, b, a = px[x, y]
        lum = (r*30 + g*59 + b*11) // 100
        vals.append(str(lum)); vals.append(str(a))
out.append(",".join(vals))
out.append("};")
open("moon_texture_data.h", "w").write("\n".join(out))
print("wrote moon_texture_data.h", SIZE, "x", SIZE)
