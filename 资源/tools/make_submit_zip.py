"""打包最终提交物（不含 .ai/PDF/截图等原始素材）。"""
import os
import shutil
import tempfile
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DESKTOP = Path(os.path.expanduser("~/Desktop"))
OUT = DESKTOP / "MCU_LVGL_GUI_Guider_submit.zip"

ITEMS = [
    "README.md",
    "docs",
    "src",
    "assets/gui_guider",
    "assets/ai_pages_1400x700",
    "tools",
]

stage = Path(tempfile.mkdtemp(prefix="mcu_submit_"))
try:
    for item in ITEMS:
        src = ROOT / item
        dst = stage / item
        if src.is_dir():
            shutil.copytree(src, dst)
        elif src.is_file():
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src, dst)

    with zipfile.ZipFile(OUT, "w", zipfile.ZIP_DEFLATED) as z:
        for root, dirs, files in os.walk(stage):
            for f in files:
                p = Path(root) / f
                z.write(p, p.relative_to(stage))
    print("created", OUT, round(OUT.stat().st_size / 1024 / 1024, 2), "MB")
finally:
    shutil.rmtree(stage, ignore_errors=True)
