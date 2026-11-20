import re, colorsys, zipfile, struct, zlib, sys

SRC = "/home/claude/night/colors.tdesktop-theme"
OUT_DIR = "/home/claude/work/theme_out"
import os; os.makedirs(OUT_DIR, exist_ok=True)

HEX = re.compile(r'#([0-9a-fA-F]{8}|[0-9a-fA-F]{6})\b')

ACCENT_HUE = 17/360.0   # ember orange

def conv(m):
    h = m.group(1)
    r, g, b = int(h[0:2],16)/255, int(h[2:4],16)/255, int(h[4:6],16)/255
    a = h[6:8] if len(h) == 8 else ""
    hh, l, s = colorsys.rgb_to_hls(r, g, b)
    deg = hh*360
    blue = 190 <= deg <= 245
    if blue and s > 0.34 and l >= 0.40:
        # accent family (buttons, links, active text) -> ember orange
        ns = min(0.92, s*1.35)
        nl = l
        # keep light accents readable on black
        if l > 0.6: nl = min(0.72, l)
        r2, g2, b2 = colorsys.hls_to_rgb(ACCENT_HUE, nl, ns)
    elif blue and s > 0.34 and l >= 0.22:
        # dark tinted accent (outgoing bubbles etc.) -> deep ember
        ns = min(0.75, s*1.3)
        r2, g2, b2 = colorsys.hls_to_rgb(ACCENT_HUE, l*1.0, ns)
    else:
        # neutral / tinted surface or text -> pure gray
        if l < 0.40:
            nl = max(0.0, (l - 0.10)) * 0.80
        else:
            nl = l
        r2, g2, b2 = colorsys.hls_to_rgb(0, nl, 0)
    return "#%02x%02x%02x%s" % (round(r2*255), round(g2*255), round(b2*255), a)

# hard overrides: the base surfaces of the app
OVERRIDES = {
    "windowBg": "#000000",
    "windowBgOver": "#111111",
    "windowBgRipple": "#1a1a1a",
    "windowShadowFgFallback": "#000000",
    "windowFg": "#f2f2f2",
    "windowBoldFg": "#f2f2f2",
    "windowSubTextFg": "#8a8a8a",
    "windowSubTextFgOver": "#9a9a9a",
    "windowBgActive": "#d9480f",
    "windowFgActive": "#ffffff",
    "windowActiveTextFg": "#ff8a57",
    "activeButtonBg": "#d9480f",
    "activeButtonBgOver": "#e5531a",
    "activeButtonBgRipple": "#ef6630",
    "lightButtonBg": "#000000",
    "lightButtonFg": "#ff8a57",
    "outlineButtonBg": "#000000",
    "menuBg": "#0d0d0d",
    "inputBorderFg": "#2a2a2a",
    "msgInBg": "#161616",
    "msgInBgSelected": "#242424",
    "sideBarBg": "#000000",
    "historyComposeAreaBg": "#000000",
    "boxBg": "#0a0a0a",
}

out = []
seen = {}
for line in open(SRC, encoding="utf-8"):
    mm = re.match(r'^(\w+):\s*([^;]*);(.*)$', line.rstrip("\n"))
    if not mm:
        out.append(line.rstrip("\n")); continue
    name, val, tail = mm.groups()
    if name in OVERRIDES:
        val = OVERRIDES[name]
    else:
        val = HEX.sub(conv, val)
    out.append(f"{name}: {val};{tail}")
    seen[name] = val

open(OUT_DIR + "/colors.tdesktop-theme", "w", encoding="utf-8").write("\n".join(out) + "\n")

# 1x1 pure black wallpaper (RGBA)
def png(rgba):
    raw = b"\x00" + bytes(rgba)
    def chunk(t, d):
        c = struct.pack(">I", len(d)) + t + d
        return c + struct.pack(">I", zlib.crc32(t + d) & 0xffffffff)
    return (b"\x89PNG\r\n\x1a\n"
            + chunk(b"IHDR", struct.pack(">IIBBBBB", 1, 1, 8, 6, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b""))
open(OUT_DIR + "/background.png", "wb").write(png((0, 0, 0, 255)))

for k in ["windowBg","windowBgOver","msgInBg","msgOutBg","dialogsBg","dialogsBgActive","historyComposeAreaBg",
          "activeButtonBg","windowActiveTextFg","dialogsBgOver","msgInBgSelected","msgOutBgSelected","historyTextInFg","msgOutReplyBarColor","dialogsUnreadBg","menuBgOver","boxDividerBg","boxBg","sideBarBg","titleBg"]:
    print(k, seen.get(k))
print("lines:", len(out))
