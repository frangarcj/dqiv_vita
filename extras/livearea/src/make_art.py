#!/usr/bin/env python3
"""Regenerates icon0/pic0/bg0/startup.png from the game's own launcher icon (APK res/drawable-xxxhdpi-v4/app_icon.png, 192x192).
   python3 make_art.py <app_icon.png | DQ4.apk>        (needs Pillow; run from anywhere, writes next to this script's parent)
The artwork belongs to Square Enix / ARMOR PROJECT; replace these files if you redistribute the VPK."""
import io, os, sys, zipfile
from PIL import Image, ImageDraw, ImageFilter, ImageFont

OUT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FONTS = ["/System/Library/Fonts/Supplemental/Georgia Bold.ttf", "/Library/Fonts/Georgia Bold.ttf", "C:/Windows/Fonts/georgiab.ttf",
         "/usr/share/fonts/truetype/dejavu/DejaVuSerif-Bold.ttf", "/usr/share/fonts/dejavu/DejaVuSerif-Bold.ttf"]

def load_icon(p):
    if p.lower().endswith(".apk"):
        with zipfile.ZipFile(p) as z: return Image.open(io.BytesIO(z.read("res/drawable-xxxhdpi-v4/app_icon.png"))).convert("RGB")
    return Image.open(p).convert("RGB")

def font(size):
    for f in FONTS:
        if os.path.exists(f): return ImageFont.truetype(f, size)
    return ImageFont.load_default()

def backdrop(icon, w, h, dark=0.55):
    """icon blown up to cover w x h, blurred and darkened"""
    s = max(w / icon.width, h / icon.height)
    big = icon.resize((int(icon.width * s) + 1, int(icon.height * s) + 1), Image.LANCZOS).filter(ImageFilter.GaussianBlur(18))
    x, y = (big.width - w) // 2, (big.height - h) // 2
    bg = big.crop((x, y, x + w, y + h))
    return Image.blend(bg, Image.new("RGB", (w, h), (8, 6, 24)), dark)

def title(img, text, y, size, color=(255, 214, 90), cx=None):
    d = ImageDraw.Draw(img); f = font(size)
    w = d.textlength(text, font=f)
    x = (img.width if cx is None else 2 * cx) / 2 - w / 2
    d.text((x + 3, y + 3), text, font=f, fill=(20, 10, 40))
    d.text((x, y), text, font=f, fill=color)

def main():
    icon = load_icon(sys.argv[1])
    # icon0: 128x128
    icon.resize((128, 128), Image.LANCZOS).save(os.path.join(OUT, "icon0.png"))
    # bg0: 840x500
    bg = backdrop(icon, 840, 500, 0.45); bg.save(os.path.join(OUT, "bg0.png"))
    # pic0: 960x544 with the icon and the title
    pic = backdrop(icon, 960, 544, 0.5)
    big = icon.resize((300, 300), Image.LANCZOS)
    pic.paste(big, (60, 122))
    title(pic, "DRAGON QUEST IV", 205, 52, cx=660)
    title(pic, "Chapters of the Chosen", 290, 30, (230, 230, 255), cx=660)
    pic.save(os.path.join(OUT, "pic0.png"))
    # startup: 280x158 gate image
    st = backdrop(icon, 280, 158, 0.4)
    st.paste(icon.resize((110, 110), Image.LANCZOS), (14, 24))
    title(st, "DRAGON", 36, 28, cx=205); title(st, "QUEST IV", 72, 28, cx=205)
    st.save(os.path.join(OUT, "startup.png"))
    for f in ("icon0", "bg0", "pic0", "startup"): print(f, Image.open(os.path.join(OUT, f + ".png")).size)

main()
