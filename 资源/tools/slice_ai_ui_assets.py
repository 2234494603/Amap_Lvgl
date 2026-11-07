"""
从 AI 第 2 页（导航主界面）切出右侧/底部 UI 小图标。

输入：
  assets/ai_crops/page2_zoom12.png  （AI 第 2 页，3742x1906）

输出（assets/gui_guider/）：
  icon_nav_ai_64.png / icon_fault_ai_64.png / icon_assistant_ai_64.png
  icon_cast_ai_64.png / icon_settings_ai_64.png
  icon_seat_ai_48.png
  album_art_ai_48.png
  speed_ticks_ai_460x260.png
  icon_bluetooth_ai_28.png
  icon_bolt_ai_28.png

说明：
  - 蓝色图标：按 B-max(R,G) 提取透明通道；
  - 白色图标：按最小通道亮度提取；
  - 灰色刻度：按最大通道亮度提取；
  - 坐标基于 page2_zoom12.png，改分辨率时按比例缩放。
"""
from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageFilter

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "assets" / "ai_crops" / "page2_zoom12.png"
AI = ROOT / "电动车智能显示屏修图-Q.ai"
OUT = ROOT / "assets" / "gui_guider"
OUT.mkdir(parents=True, exist_ok=True)


def load_page() -> Image.Image:
    """优先用已渲染的高清页；没有就直接从 .ai 第 2 页渲染。"""
    if SRC.exists():
        return Image.open(SRC).convert("RGB")

    import pymupdf

    doc = pymupdf.open(str(AI))
    pix = doc[2].get_pixmap(matrix=pymupdf.Matrix(12, 12), alpha=False)
    return Image.frombytes("RGB", (pix.width, pix.height), pix.samples)


img = load_page()
print("source", img.size)


def alpha_blue(im: Image.Image) -> Image.Image:
    r, g, b = im.split()
    blue = ImageChops.subtract(b, ImageChops.lighter(r, g))
    blue = blue.point(lambda v: min(255, max(0, (v - 18) * 7)))
    min_rgb = ImageChops.darker(r, ImageChops.darker(g, b))
    white = min_rgb.point(lambda v: min(255, max(0, (v - 170) * 4)))
    return ImageChops.lighter(blue, white).filter(ImageFilter.GaussianBlur(0.4))


def alpha_white(im: Image.Image) -> Image.Image:
    r, g, b = im.split()
    min_rgb = ImageChops.darker(r, ImageChops.darker(g, b))
    return min_rgb.point(lambda v: min(255, max(0, (v - 160) * 4))).filter(ImageFilter.GaussianBlur(0.4))


def alpha_gray(im: Image.Image) -> Image.Image:
    r, g, b = im.split()
    mx = ImageChops.lighter(r, ImageChops.lighter(g, b))
    return mx.point(lambda v: min(255, max(0, (v - 45) * 4))).filter(ImageFilter.GaussianBlur(0.3))


def save_rgba(im: Image.Image, alpha: Image.Image, size, name: str):
    out = im.convert("RGBA")
    out.putalpha(alpha)
    out.resize(size, Image.LANCZOS).save(OUT / name)


def cut(box, alpha_fn, size, name):
    c = img.crop(box)
    save_rgba(c, alpha_fn(c), size, name)


# 底部导航图标（page2_zoom12 坐标，按蓝色/白色像素实测）
cut((400, 1600, 545, 1730), alpha_white, (64, 64), "icon_nav_ai_64.png")
cut((1100, 1590, 1235, 1735), alpha_blue, (64, 64), "icon_fault_ai_64.png")
cut((1790, 1585, 1945, 1740), alpha_blue, (64, 64), "icon_assistant_ai_64.png")
cut((2490, 1585, 2615, 1740), alpha_blue, (64, 64), "icon_cast_ai_64.png")
cut((3140, 1585, 3310, 1740), alpha_blue, (64, 64), "icon_settings_ai_64.png")

# 座椅图标
cut((2160, 1250, 2300, 1420), alpha_white, (48, 48), "icon_seat_ai_48.png")

# 蓝牙/闪电不切，直接用 LVGL 符号 LV_SYMBOL_BLUETOOTH / LV_SYMBOL_CHARGE

# 专辑封面（矩形，不需要透明）
album = img.crop((2820, 1200, 2940, 1320)).resize((48, 48), Image.LANCZOS)
album.save(OUT / "album_art_ai_48.png")

# 速度刻度：裁整个环，中心区域（03 / KM/H）挖掉
tick = img.crop((2100, 150, 3420, 780))
ta = alpha_gray(tick).point(lambda v: 255 if v > 18 else 0)
mask = Image.new("L", tick.size, 255)
d = ImageDraw.Draw(mask)
# 中心数字区域 -> 透明
d.rectangle((int(tick.width * 0.30), int(tick.height * 0.26),
             int(tick.width * 0.70), int(tick.height * 0.88)), fill=0)
# KM/H 区域（右下）
d.rectangle((int(tick.width * 0.64), int(tick.height * 0.72),
             int(tick.width * 0.91), int(tick.height * 1.0)), fill=0)
# 底部 ECO/D/S 按钮顶边
d.rectangle((int(tick.width * 0.22), int(tick.height * 0.88),
             int(tick.width * 0.68), int(tick.height * 1.0)), fill=0)
ta = ImageChops.multiply(ta, mask)
save_rgba(tick, ta, (460, 260), "speed_ticks_ai_460x260.png")

print("done ->", OUT)
