#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
按“图层名”或“画板名”整理 Illustrator 导出的 PNG。

文件名假设为：
    图层名_画板名.png
例如：
    地图_画板1.png      -> 地图/地图_画板1.png
    路径_画板2.png      -> 路径/路径_画板2.png
    图标_导航_画板3.png -> 图标_导航/图标_导航_画板3.png   （默认按最后一个下划线切）

如果文件名只有画板名、没有“图层名_”前缀（如“画板 1.png”），
用 --by artboard，脚本会把整个文件名当作画板名继续处理。

只用 Python 标准库：os、shutil、pathlib、argparse、sys。
无需 pip 安装任何包。

用法：
    python sort_ai_pngs.py
    python sort_ai_pngs.py "D:\\导出\\PNG"
    python sort_ai_pngs.py "D:\\导出\\PNG" --dest "D:\\整理后"
    python sort_ai_pngs.py "D:\\导出\\PNG" --split first
    python sort_ai_pngs.py "D:\\导出\\PNG" --dry-run

    # 按画板名分组（AI 里只有 1 个图层时很有用）
    python sort_ai_pngs.py "D:\\导出\\PNG" --by artboard
    # 再把画板名映射成页面名：导航/故障/助手/投屏/设置...
    python sort_ai_pngs.py "D:\\导出\\PNG" --by artboard --map artboard_page_map.txt
"""

from __future__ import annotations

import argparse
import os
import shutil
import sys
from pathlib import Path
from typing import Dict, List, Optional, Tuple

# ============================================================================
# 在这里改默认路径，方便直接双击/运行
# ============================================================================

# AI 导出的 PNG 所在文件夹。留空字符串则使用“脚本所在目录”。
# 当前指向你这次 1x 导出所在的目录；以后换目录改这一行，或用命令行参数。
DEFAULT_SOURCE_DIR = r"C:\Users\Administrator\Downloads\1x"

# 文件名里“图层名”和“画板名”的分隔符
SEPARATOR = "_"

# 切割方式：
#   "first" = 第一个下划线：图层名_剩余部分.png
#   "last"  = 最后一个下划线：前面整体_画板名.png
# 默认 last：图层名里带下划线（如“地图_底图_画板1”）时，
# 会得到“地图_底图”，不会把图层名截断。
# 如果画板名本身也带下划线，改用 first。
SPLIT_MODE = "last"

# 默认分组方式：
#   "layer"    = 按图层名（分隔符前的那一段）
#   "artboard" = 按画板名（分隔符后的那一段）
# 命令行 --by 可临时覆盖。
GROUP_BY = "layer"

# 画板名 -> 页面名 的映射文件；放在脚本旁边时，--by artboard 会自动读取。
DEFAULT_MAP_FILE = "artboard_page_map.txt"

PNG_SUFFIXES = {".png"}  # 大小写不敏感

# Windows 文件名里不能出现的字符
INVALID_CHARS = '<>:"/\\|?*'

# Windows 保留设备名（防止创建 CON、PRN 等文件夹）
WINDOWS_RESERVED = {
    "CON", "PRN", "AUX", "NUL",
    *(f"COM{i}" for i in range(1, 10)),
    *(f"LPT{i}" for i in range(1, 10)),
}


def sanitize_folder_name(name: str) -> str:
    """把图层名变成 Windows 可用的文件夹名；中文原样保留。"""
    # 非法字符和控制字符 -> 下划线
    name = "".join(
        "_" if (ch in INVALID_CHARS or ord(ch) < 32) else ch
        for ch in name
    )

    name = name.strip().rstrip(" .")  # Windows 不允许结尾空格/点
    if not name:
        return ""

    # CON、CON.txt、PRN.1 之类都算保留设备名
    if name.split(".", 1)[0].upper() in WINDOWS_RESERVED:
        name = "_" + name

    return name


def configure_output() -> None:
    """保证中文输出不因控制台/重定向编码而报错。

    - 真实控制台：保留系统编码，只把无法编码的字符替换掉；
    - 管道/重定向：统一输出 UTF-8（日志文件、CI、编辑器都不会乱码）。
    """
    for stream in (sys.stdout, sys.stderr):
        if stream is None:
            continue
        try:
            if stream.isatty():
                stream.reconfigure(errors="replace")
            else:
                stream.reconfigure(encoding="utf-8", errors="replace")
        except (AttributeError, ValueError, OSError):
            pass


def normalize_path_arg(value: str) -> str:
    """去掉资源管理器“复制为路径”带上的引号，并展开环境变量。"""
    value = value.strip()
    if len(value) >= 2 and value[0] == value[-1] and value[0] in "\"'":
        value = value[1:-1]
    return os.path.expandvars(value)


def load_name_map(path: Path) -> Dict[str, str]:
    """读取“原名=新名”映射文件。

    每行一个映射，支持：
        画板 1 副本 3=导航
        画板 1 副本 3<TAB>导航
    空行和 # 开头的行会被忽略；utf-8-sig 兼容记事本保存的 BOM。
    """
    mapping: Dict[str, str] = {}
    with path.open("r", encoding="utf-8-sig") as fh:
        for lineno, line in enumerate(fh, 1):
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            if "\t" in line:
                key, value = line.split("\t", 1)
            elif "=" in line:
                key, value = line.split("=", 1)
            else:
                print(
                    f"警告：映射文件第 {lineno} 行缺少 '=' 或 Tab，已忽略：{line}",
                    file=sys.stderr,
                )
                continue
            key = key.strip()
            value = value.strip()
            if not key or not value:
                print(
                    f"警告：映射文件第 {lineno} 行原名为空或新名为空，已忽略：{line}",
                    file=sys.stderr,
                )
                continue
            mapping[key] = value
    return mapping


def split_name(stem: str, separator: str, split_mode: str) -> Optional[Tuple[str, str]]:
    """把 图层名_画板名 拆成 (图层名, 画板名)。失败返回 None。"""
    if separator not in stem:
        return None

    if split_mode == "last":
        layer, artboard = stem.rsplit(separator, 1)
    else:
        layer, artboard = stem.split(separator, 1)

    layer = layer.strip()
    artboard = artboard.strip()
    if not layer or not artboard:
        return None

    return layer, artboard


def find_pngs(source_dir: Path) -> List[Path]:
    """只处理源文件夹顶层 PNG，不递归，避免误动子文件夹。"""
    result: List[Path] = []
    with os.scandir(source_dir) as entries:
        for entry in entries:
            try:
                if not entry.is_file():
                    continue
            except OSError:
                continue
            if Path(entry.name).suffix.lower() in PNG_SUFFIXES:
                result.append(Path(entry.path))
    return sorted(result, key=lambda p: p.name.lower())


def main() -> int:
    configure_output()
    default_src = DEFAULT_SOURCE_DIR or str(Path(__file__).resolve().parent)

    parser = argparse.ArgumentParser(
        description="按“图层名”或“画板名”把 Illustrator 导出的 PNG 移动到对应子文件夹。",
    )
    parser.add_argument(
        "source",
        nargs="?",
        default=default_src,
        help=f"PNG 所在文件夹，默认：{default_src}",
    )
    parser.add_argument(
        "--dest",
        default=None,
        help="整理后的根目录，默认与源文件夹相同",
    )
    parser.add_argument(
        "--sep",
        default=SEPARATOR,
        help=f"图层名/画板名分隔符，默认：{SEPARATOR!r}",
    )
    parser.add_argument(
        "--split",
        choices=("first", "last"),
        default=SPLIT_MODE,
        help=f"按第一个还是最后一个分隔符切分，默认：{SPLIT_MODE}",
    )
    parser.add_argument(
        "--by",
        choices=("layer", "artboard"),
        default=GROUP_BY,
        help="按哪一段分组：layer=图层名（默认），artboard=画板名；文件名没有分隔符时整体当画板名",
    )
    parser.add_argument(
        "--map",
        dest="map_file",
        default=None,
        help=(
            "名称映射文件，一行“原名=新名”；"
            f"按画板分组时默认读取脚本旁的 {DEFAULT_MAP_FILE}"
        ),
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="只打印将要执行的操作，不实际移动文件",
    )
    parser.add_argument(
        "--verbose",
        action="store_true",
        help="打印每个文件的移动结果",
    )
    args = parser.parse_args()

    source_dir = Path(normalize_path_arg(args.source)).expanduser()
    dest_root = (
        Path(normalize_path_arg(args.dest)).expanduser()
        if args.dest
        else source_dir
    )

    if not source_dir.exists():
        print(f"错误：源文件夹不存在：{source_dir}", file=sys.stderr)
        return 1
    if not source_dir.is_dir():
        print(f"错误：源路径不是文件夹：{source_dir}", file=sys.stderr)
        return 1

    name_map: Dict[str, str] = {}
    map_path: Optional[Path] = None
    if args.map_file:
        map_path = Path(normalize_path_arg(args.map_file)).expanduser()
        if not map_path.is_file():
            print(f"错误：映射文件不存在：{map_path}", file=sys.stderr)
            return 1
        try:
            name_map = load_name_map(map_path)
        except (OSError, UnicodeError) as exc:
            print(f"错误：读取映射文件失败：{map_path}：{exc}", file=sys.stderr)
            return 1
    elif args.by == "artboard":
        auto_map = Path(__file__).resolve().with_name(DEFAULT_MAP_FILE)
        if auto_map.is_file():
            map_path = auto_map
            try:
                name_map = load_name_map(auto_map)
            except (OSError, UnicodeError) as exc:
                print(f"警告：读取默认映射文件失败，将使用画板原名：{exc}", file=sys.stderr)

    pngs = find_pngs(source_dir)
    if not pngs:
        print(f"源文件夹里没有找到 PNG：{source_dir}")
        return 0

    moved = 0
    skipped_existing = 0
    skipped_invalid = 0
    failed = 0

    for src in pngs:
        parsed = split_name(src.stem, args.sep, args.split)
        if parsed is None:
            if args.by == "artboard" and args.sep not in src.stem:
                # 没有“图层名_”前缀的导出（如“画板 1.png”），整个文件名就是画板名
                layer_name, artboard_name = "", src.stem.strip()
            else:
                skipped_invalid += 1
                print(f"跳过（文件名不符合“图层名{args.sep}画板名”）：{src.name}")
                continue
        else:
            layer_name, artboard_name = parsed
        group_name = layer_name if args.by == "layer" else artboard_name
        mapped_name = name_map.get(group_name, group_name)
        folder_name = sanitize_folder_name(mapped_name)
        if not folder_name:
            skipped_invalid += 1
            print(f"跳过（分组名为空/非法）：{src.name}")
            continue

        target_dir = dest_root / folder_name
        target = target_dir / src.name

        if target.exists():
            skipped_existing += 1
            if args.verbose:
                print(f"跳过（已存在）：{target}")
            continue

        if args.dry_run:
            moved += 1
            print(f"[dry-run] 会移动：{src.name}  ->  {folder_name}/")
            continue

        try:
            target_dir.mkdir(parents=True, exist_ok=True)
            shutil.move(str(src), str(target))
            moved += 1
            if args.verbose:
                print(f"移动：{src.name}  ->  {folder_name}/")
        except OSError as exc:
            failed += 1
            print(f"失败：{src.name}：{exc}", file=sys.stderr)

    print()
    print("=" * 56)
    print(f"源文件夹：{source_dir}")
    print(f"目标根目录：{dest_root}")
    print(f"分组方式：{'图层名' if args.by == 'layer' else '画板名'}")
    print(f"名称映射：{map_path if map_path else '无'}")
    if args.dry_run:
        print(f"将移动（dry-run）：{moved}")
    else:
        print(f"移动：{moved}")
    skipped_total = skipped_existing + skipped_invalid
    print(
        f"跳过：{skipped_total}"
        f"（已存在 {skipped_existing}，文件名不符合 {skipped_invalid}）"
    )
    print(f"失败：{failed}")
    if skipped_invalid and args.by == "layer":
        print("提示：如果文件名只有画板名（如“画板 1.png”），请加 --by artboard。")
    print("=" * 56)

    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
