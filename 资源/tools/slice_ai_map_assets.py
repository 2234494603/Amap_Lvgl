"""
从 AI 导出的高分辨率地图卡片中切出 GUI Guider 可用的分层资源。

输入：
  assets/ai_crops/map_card_test.png   （由 PyMuPDF 渲染 AI 第 2 页、裁出的地图卡片，约 1490x1236）

输出（assets/gui_guider/）：
  map_card_ai_600x465.png      整张卡片（参考/最快占位）
  map_bg_clean_600x465.png     去掉路径/Pin/箭头/转向卡后的底图（近似）
  route_ai_600x465.png         透明路径（含发光）
  pin_ai_40x52.png             透明 Pin
  arrow_ai_56x56.png           透明箭头
  turn_card_ai_300x100.png     转向卡片（圆角）
  turn_icon_ai_48x56.png       转向白色箭头（透明）

说明：
  - 从 AI 渲染图切图，比从 1400x700 截图切更清晰；
  - 路径/Pin/箭头的透明通道用“蓝色/白色亮度”提取，深色地图自动变透明；
  - 地图底图的“去路径”用高斯模糊填充，路径/转向卡会重新盖上去；
  - 需要 1:1 完美底图时，应在 AI 里隐藏路径/转向卡/Pin/箭头后单独导出。
"""
from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageFilter

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "assets" / "ai_crops" / "map_card_test.png"
AI = ROOT / "电动车智能显示屏修图-Q.ai"
OUT = ROOT / "assets" / "gui_guider"
OUT.mkdir(parents=True, exist_ok=True)


def load_card() -> Image.Image:
    """优先用已裁好的高清卡片；没有就直接从 .ai 第 2 页渲染并裁。"""
    if SRC.exists():
        return Image.open(SRC).convert("RGB")

    import pymupdf

    doc = pymupdf.open(str(AI))
    pix = doc[2].get_pixmap(matrix=pymupdf.Matrix(12, 12), alpha=False)
    page = Image.frombytes("RGB", (pix.width, pix.height), pix.samples)
    return page.crop((190, 350, 1680, 1586))

# 在高分辨率卡片中的裁剪框（左, 上, 右, 下）
BOX_TURN_CARD = (50, 15, 1035, 345)
BOX_TURN_ICON = (120, 55, 265, 280)
BOX_PIN = (1145, 70, 1295, 265)
BOX_ARROW = (650, 805, 890, 1000)

# 路径要排除的区域，避免把 Pin/箭头/转向卡一起抠进去
ROUTE_EXCLUDE = [BOX_TURN_CARD, BOX_PIN, BOX_ARROW]


def color_alpha(img: Image.Image) -> Image.Image:
    """按“蓝色优势 + 白色亮度”生成透明度。深色地图 -> 0，亮蓝/白 -> 255。

    关键：地图底色大约 (9,11,23)~(14,16,29)，B-max(R,G) 只有 9~15；
    路径蓝色大约 (24,111,252)，B-max(R,G) 约 140。
    所以阈值取 22，既能去掉地图，又能保留路径外发光。
    """
    r, g, b = img.split()
    max_rg = ImageChops.lighter(r, g)
    blue = ImageChops.subtract(b, max_rg)          # B - max(R,G)
    blue = blue.point(lambda v: min(255, max(0, (v - 22) * 6)))

    min_rgb = ImageChops.darker(r, ImageChops.darker(g, b))
    white = min_rgb.point(lambda v: min(255, max(0, (v - 140) * 4)))

    alpha = ImageChops.lighter(blue, white)
    alpha = alpha.filter(ImageFilter.GaussianBlur(0.5))
    return alpha


def rounded_alpha(size: tuple[int, int], radius: int) -> Image.Image:
    mask = Image.new("L", size, 0)
    d = ImageDraw.Draw(mask)
    d.rounded_rectangle((0, 0, size[0] - 1, size[1] - 1), radius=radius, fill=255)
    return mask


def crop_resize(box, size):
    return card.crop(box).resize(size, Image.LANCZOS)


def save_rgba(img: Image.Image, alpha: Image.Image, size, name: str):
    out = img.convert("RGBA")
    out.putalpha(alpha)
    out = out.resize(size, Image.LANCZOS)
    out.save(OUT / name)


card = load_card()
print("source", card.size)

# 1) 整张卡片
card.resize((600, 465), Image.LANCZOS).save(OUT / "map_card_ai_600x465.png")

# 2) 路径层
route_alpha = color_alpha(card)
zero = Image.new("L", card.size, 0)
for box in ROUTE_EXCLUDE:
    route_alpha.paste(0, box)
# 轻微膨胀，保留发光边缘
route_alpha = route_alpha.filter(ImageFilter.MaxFilter(3))
save_rgba(card, route_alpha, (600, 465), "route_ai_600x465.png")

# 3) Pin
pin_img = card.crop(BOX_PIN)
pin_alpha = color_alpha(pin_img)
pin_alpha = pin_alpha.point(lambda v: 255 if v > 24 else 0)
pin_alpha = pin_alpha.filter(ImageFilter.GaussianBlur(0.6))
save_rgba(pin_img, pin_alpha, (40, 52), "pin_ai_40x52.png")

# 4) 箭头
arrow_img = card.crop(BOX_ARROW)
arrow_alpha = color_alpha(arrow_img)
arrow_alpha = arrow_alpha.point(lambda v: 255 if v > 24 else 0)
arrow_alpha = arrow_alpha.filter(ImageFilter.GaussianBlur(0.6))
save_rgba(arrow_img, arrow_alpha, (56, 56), "arrow_ai_56x56.png")

# 5) 转向卡片（圆角，不带透明边缘的地图）
turn_img = card.crop(BOX_TURN_CARD)
turn_alpha = rounded_alpha(turn_img.size, 34)
save_rgba(turn_img, turn_alpha, (300, 100), "turn_card_ai_300x100.png")
# 按 AI 卡片原始比例（600x465 卡片内约 397x124）
save_rgba(turn_img, turn_alpha, (397, 124), "turn_card_ai_397x124.png")

# 6) 转向白色箭头（只要图标，不要卡片底）
icon_img = card.crop(BOX_TURN_ICON)
icon_alpha = color_alpha(icon_img)
icon_alpha = icon_alpha.point(lambda v: 255 if v > 20 else 0)
icon_alpha = icon_alpha.filter(ImageFilter.GaussianBlur(0.5))
save_rgba(icon_img, icon_alpha, (48, 56), "turn_icon_ai_48x56.png")

# 7) 干净底图：把路径/Pin/箭头/转向卡区域填掉
# 只处理“强路径/强发光”像素，避免把地图里淡淡的蓝色调也挖掉
mask = route_alpha.point(lambda v: 255 if v > 80 else 0)
mask = mask.filter(ImageFilter.MaxFilter(15))
for box in (BOX_PIN, BOX_ARROW, BOX_TURN_CARD):
    mask.paste(255, box)

# 用纯深色填掉路径/Pin/箭头区域。路径层会重新盖在上面；
# 这样不会留下蓝色残影（比 blur/inpaint 更可控）。
flat = Image.new("RGB", card.size, (9, 13, 22))
clean = Image.composite(flat, card, mask)

# 转向卡区域再压暗一点，避免亮色透出（实际会被 turn_card 图片盖住）
d = ImageDraw.Draw(clean)
d.rounded_rectangle(BOX_TURN_CARD, radius=34, fill=(9, 13, 21))
clean.resize((600, 465), Image.LANCZOS).save(OUT / "map_bg_clean_600x465.png")

print("done ->", OUT)
