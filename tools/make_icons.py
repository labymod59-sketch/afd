import math, os
from PIL import Image, ImageDraw

BG = (217, 72, 15, 255)      # ember #d9480f
FG = (0, 0, 0, 255)          # black flame
S = 2048                     # master canvas

def teardrop(d, cx, cy, r, tx, ty, fill):
    """circle + pointed tip (tangent construction)"""
    d.ellipse([cx-r, cy-r, cx+r, cy+r], fill=fill)
    dx, dy = tx-cx, ty-cy
    dist = math.hypot(dx, dy)
    a = math.atan2(dy, dx)
    b = math.acos(r/dist)
    p1 = (cx + r*math.cos(a+b), cy + r*math.sin(a+b))
    p2 = (cx + r*math.cos(a-b), cy + r*math.sin(a-b))
    d.polygon([p1, (tx, ty), p2], fill=fill)

def master(tile="round"):
    im = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    if tile == "round":
        d.rounded_rectangle([0, 0, S-1, S-1], radius=int(S*0.225), fill=BG)
    elif tile == "circle":
        d.ellipse([0, 0, S-1, S-1], fill=BG)
    elif tile == "square":
        d.rectangle([0, 0, S, S], fill=BG)
    u = S/100.0
    # outer flame
    teardrop(d, 52*u, 64*u, 24*u, 66*u, 9*u, FG)
    teardrop(d, 40*u, 56*u, 15*u, 32*u, 27*u, FG)
    # inner ember
    teardrop(d, 52*u, 70*u, 10*u, 49*u, 48*u, BG)
    return im

def save(img, path, size):
    img.resize((size, size), Image.LANCZOS).save(path)

round_m = master("round")
for s in (16, 32, 48, 64, 128, 256, 512):
    save(round_m, f"icon{s}.png", s)
    save(round_m, f"icon{s}@2x.png", s*2)
save(master("circle"), "icon_round512@2x.png", 1024)
save(round_m, "logo_256.png", 256)
save(master("square"), "logo_256_no_margin.png", 256)
save(round_m, "iconbig_green.png", 256)
save(round_m, "icon_green.png", 1024)

ico_sizes = [(16,16),(24,24),(32,32),(48,48),(64,64),(128,128),(256,256)]
big = round_m.resize((256,256), Image.LANCZOS)
big.save("icon256.ico", sizes=ico_sizes)
big.save("ayu/default/app_icon.ico", sizes=ico_sizes)
big.save("ayu/alt/app_icon.ico", sizes=ico_sizes)

import sys
def tangent(cx, cy, r, tx, ty):
    dx, dy = tx-cx, ty-cy
    dist = math.hypot(dx, dy); a = math.atan2(dy, dx); b = math.acos(r/dist)
    return ((cx+r*math.cos(a+b), cy+r*math.sin(a+b)), (cx+r*math.cos(a-b), cy+r*math.sin(a-b)))
def shape(cx,cy,r,tx,ty,fill):
    k=12.0
    a,b=tangent(cx*k,cy*k,r*k,tx*k,ty*k)
    return '<circle cx="%.1f" cy="%.1f" r="%.1f" fill="%s"/>\n<polygon points="%.1f,%.1f %.1f,%.1f %.1f,%.1f" fill="%s"/>\n' % (cx*k,cy*k,r*k,fill,a[0],a[1],tx*k,ty*k,b[0],b[1],fill)
svg='<svg width="1200" height="1200" viewBox="0 0 1200 1200" fill="none" xmlns="http://www.w3.org/2000/svg">\n<rect width="1200" height="1200" rx="270" fill="#D9480F"/>\n'
svg+=shape(52,64,24,66,9,"#000")+shape(40,56,15,32,27,"#000")+shape(52,70,10,49,48,"#D9480F")+'</svg>\n'
open("ayu/default/app.svg","w").write(svg)
open("ayu/alt/app.svg","w").write(svg)
print("ok")
