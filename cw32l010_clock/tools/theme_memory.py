#!/usr/bin/env python3
"""Measure design-study asset budgets; never modify or flash clock firmware."""
from __future__ import annotations

import argparse
from dataclasses import dataclass
import hashlib
import json
from pathlib import Path
import struct
import subprocess
from unittest.mock import patch

from PIL import Image

import theme_study as study

ROOT = Path(__file__).resolve().parents[1]
APPLICATION_LIMIT = 64000
RAM_LIMIT = 4096
COMMON_CODE_ALLOWANCE = 4096
THEME_CODE_ALLOWANCE = 256
FLASH_HEADROOM = 2048
SAMPLE = study.Sample()

# Fixed sample strings that will become dynamic date/weekday/temperature fields.
DYNAMIC_LABELS = {
    1: {SAMPLE.date, SAMPLE.temperature, "TUE"},
    2: {SAMPLE.date + "  TUE", SAMPLE.temperature},
    3: {SAMPLE.date + "  " + SAMPLE.temperature + "   SET/BACK"},
    4: {SAMPLE.date + "  TUE", SAMPLE.temperature},
    5: {SAMPLE.date + "  TUE", SAMPLE.temperature},
    6: {SAMPLE.date, SAMPLE.weekday, SAMPLE.temperature},
    7: {"T U E S D A Y", SAMPLE.date + "  /  " + SAMPLE.temperature},
    8: {SAMPLE.date, SAMPLE.weekday, SAMPLE.temperature},
    9: {"14", "36", "TUE", "2026", "10.20", SAMPLE.temperature},
    10: {SAMPLE.date, SAMPLE.weekday, SAMPLE.temperature},
    11: {SAMPLE.date, "TUESDAY / NO. 293", SAMPLE.temperature,
         "DAY 293 / A MOMENT TO KEEP"},
    12: {"TUESDAY, OCTOBER 20", "2026", "-  " + SAMPLE.temperature + "  -"},
    13: {SAMPLE.date, SAMPLE.date + "  TUE", SAMPLE.temperature},
    14: {"TUE 20", SAMPLE.date + "  TUE", SAMPLE.temperature},
    15: {SAMPLE.date, "TUESDAY / " + SAMPLE.temperature},
    16: {"O C T   2 0", "T U E S D A Y", SAMPLE.temperature},
    17: {SAMPLE.date, "TUE", "36", SAMPLE.temperature},
    18: {"OCT", "20", "2026", SAMPLE.weekday, SAMPLE.temperature},
    19: {SAMPLE.date + "  TUE", SAMPLE.temperature},
    20: {SAMPLE.date + "   TUESDAY", SAMPLE.temperature},
}
WEEKDAYS = "MONDAY TUESDAY WEDNESDAY THURSDAY FRIDAY SATURDAY SUNDAY"
MONTHS = "JANUARY FEBRUARY MARCH APRIL MAY JUNE JULY AUGUST SEPTEMBER OCTOBER NOVEMBER DECEMBER"


def align4(value):
    return (value + 3) & ~3


def encode_bitmap(image):
    """Random-access raw or 8x8 tile dictionary, whichever is smaller."""
    image = image.convert("1")
    width, height = image.size
    raw = struct.pack("<BHH", 0, width, height) + image.tobytes()
    tiles = []
    lookup = {}
    indices = []
    for y in range(0, height, 8):
        for x in range(0, width, 8):
            tile = Image.new("1", (8, 8), 0)
            tile.paste(image.crop((x, y, min(x + 8, width), min(y + 8, height))), (0, 0))
            value = tile.tobytes()
            if value not in lookup:
                lookup[value] = len(tiles)
                tiles.append(value)
            indices.append(lookup[value])
    index_bytes = 1 if len(tiles) <= 256 else 2
    mapping = (bytes(indices) if index_bytes == 1 else
               struct.pack("<" + "H" * len(indices), *indices))
    packed = struct.pack("<BHHH", index_bytes, width, height, len(tiles)) + b"".join(tiles) + mapping
    return min((raw, packed), key=len)


def decode_bitmap(data):
    kind, width, height = struct.unpack_from("<BHH", data)
    if not kind:
        return Image.frombytes("1", (width, height), data[5:])
    count, = struct.unpack_from("<H", data, 5)
    tiles = data[7:7 + count * 8]
    mapping = data[7 + count * 8:]
    result = Image.new("1", (width, height), 0)
    position = 0
    for y in range(0, height, 8):
        for x in range(0, width, 8):
            index = mapping[position] if kind == 1 else struct.unpack_from("<H", mapping, position)[0]
            tile = Image.frombytes("1", (8, 8), tiles[index * 8:(index + 1) * 8])
            result.paste(tile, (x, y))
            position += kind
    return result


def verified_bitmap(image):
    image = image.convert("1")
    encoded = encode_bitmap(image)
    decoded = decode_bitmap(encoded)
    assert decoded.size == image.size and decoded.tobytes() == image.tobytes()
    return encoded


@dataclass
class Asset:
    name: str
    data: bytes

    @property
    def size(self):
        return align4(len(self.data))


def atlas(kind, size, condensed, chars):
    glyphs = [(ch, study.digits(ch, kind, size, condensed)) for ch in sorted(chars)]
    cell_w = (max(g.width for _, g in glyphs) + 7) & ~7
    cell_h = (max(g.height for _, g in glyphs) + 7) & ~7
    columns = min(10, len(glyphs))
    rows = (len(glyphs) + columns - 1) // columns
    image = Image.new("1", (columns * cell_w, rows * cell_h), 0)
    metrics = bytearray(struct.pack("<HHHHH", len(glyphs), cell_w, cell_h, columns, rows))
    f = study.font(kind, size)
    for i, (ch, glyph) in enumerate(glyphs):
        image.paste(glyph.convert("1"), ((i % columns) * cell_w, (i // columns) * cell_h))
        box = f.getbbox(ch)
        metrics.extend(struct.pack("<HBBhhH", ord(ch), glyph.width, glyph.height,
                                   box[0], box[1], round(f.getlength(ch) * condensed * 64)))
    return bytes(metrics) + verified_bitmap(image)


def capture_theme(number):
    requested = {}
    original_digits = study.digits
    original_segments = study.seven_segment

    def request(kind, size, condensed, chars):
        requested.setdefault((kind, size, condensed), set()).update(chars)

    def blank_digits(text, kind="sans", size=64, condensed=1.0):
        mask = original_digits(text, kind, size, condensed)
        if text == ":":
            return mask
        request(kind, size, condensed, "0123456789:")
        return Image.new("L", mask.size, 0)

    def blank_segments(*args, **kwargs):
        return Image.new("L", original_segments(*args, **kwargs).size, 0)

    original_text = study.Canvas.text

    def record_text(canvas, text, x, y, kind="mono", size=10, color=0, align="left", native=False):
        if text not in DYNAMIC_LABELS[number]:
            return original_text(canvas, text, x, y, kind, size, color, align, native)
        if native:
            return None  # The installed firmware already contains all seven Chinese weekdays.
        chars = set(text) | set("0123456789-./ :")
        if any(word in text for word in ("TUE", "T U E", "OCT", "O C T")):
            chars.update(WEEKDAYS)
        if "OCT" in text or "O C T" in text:
            chars.update(MONTHS)
        request(kind, size, 1.0, chars)
        return None

    with patch.object(study, "digits", blank_digits), \
         patch.object(study.Canvas, "text", record_text), \
         patch.object(study, "PIXEL_DIGITS", tuple("0" * 35 for _ in range(10))), \
         patch.object(study, "seven_segment", blank_segments):
        background = study.render(number, SAMPLE)
    return background, requested


def subset_budget(rows, assets, budget):
    keys = sorted(assets)
    positions = {key: i for i, key in enumerate(keys)}
    weights = [assets[key].size for key in keys]
    theme_masks = [sum(1 << positions[key] for key in row["asset_keys"]) for row in rows]
    count = 1 << len(rows)
    unions = [0] * count
    costs = [0] * count
    best = {0: (0, 0)}
    worst_by_count = {0: 0}
    for mask in range(1, count):
        bit = mask & -mask
        index = bit.bit_length() - 1
        previous = mask ^ bit
        added = theme_masks[index] & ~unions[previous]
        cost = costs[previous] + THEME_CODE_ALLOWANCE
        while added:
            asset_bit = added & -added
            cost += weights[asset_bit.bit_length() - 1]
            added ^= asset_bit
        unions[mask] = unions[previous] | theme_masks[index]
        costs[mask] = cost
        size = mask.bit_count()
        worst_by_count[size] = max(worst_by_count.get(size, 0), cost)
        if cost <= budget and (size not in best or cost < best[size][0]):
            best[size] = (cost, mask)
    maximum = max(best)
    cost, mask = best[maximum]
    return {"max_new_themes_under_model": maximum,
            "cheapest_maximum_selection": [row["number"] for i, row in enumerate(rows) if mask & (1 << i)],
            "selected_assets_and_theme_allowances_bytes": cost,
            "any_selection_new_theme_count_under_model": max(
                size for size, maximum_cost in worst_by_count.items() if maximum_cost <= budget),
            "all_twenty_assets_and_theme_allowances_bytes": costs[-1],
            "budget_after_common_code_and_headroom_bytes": budget}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "design/theme-study-20261020/memory")
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    release = ROOT / "releases/uc8251-six-themes-20261020"
    firmware = (release / "clock.bin").read_bytes()
    manifest = json.loads((release / "manifest.json").read_text())
    assert len(firmware) == manifest["bytes"]
    assert hashlib.sha256(firmware).hexdigest() == manifest["sha256"]
    size_text = subprocess.check_output(["arm-none-eabi-size", "-A", str(release / "clock.elf")], text=True)
    sections = {line.split()[0]: int(line.split()[1]) for line in size_text.splitlines()
                if line.startswith(".")}
    static_ram = sections[".data"] + sections[".bss"]
    assert static_ram == manifest["static_ram_bytes"]
    assets = {}
    requests = []
    backgrounds = []
    for theme in study.THEMES:
        background, fonts = capture_theme(theme.number)
        backgrounds.append(background)
        requests.append(fonts)
    # The same style/size/scale uses one union of all needed characters.
    shared_fonts = {}
    for fonts in requests:
        for key, chars in fonts.items():
            shared_fonts.setdefault(key, set()).update(chars)
    for key, chars in shared_fonts.items():
        name = f"font-{key[0]}-{key[1]}-{key[2]:g}"
        assets[name] = Asset(name, atlas(*key, chars))
    rows = []
    for theme, background, fonts in zip(study.THEMES, backgrounds, requests):
        name = f"background-{theme.number:02d}"
        assets[name] = Asset(name, verified_bitmap(background))
        keys = [name] + [f"font-{k[0]}-{k[1]}-{k[2]:g}" for k in fonts]
        # Original 5x7 and seven-segment numerals are generated, not bitmap atlases.
        if theme.number == 13:
            key = "pixel-digit-patterns"
            assets[key] = Asset(key, b"".join(int(value, 2).to_bytes(5, "little") for value in study.PIXEL_DIGITS))
            keys.append(key)
        if theme.number in (14, 19):
            key = "seven-segment-patterns"
            assets[key] = Asset(key, bytes((0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f)))
            keys.append(key)
        size = sum(assets[key].size for key in keys)
        rows.append({"number": theme.number, "name": theme.name,
                     "background_bytes": assets[name].size,
                     "font_and_digit_bytes": size - assets[name].size,
                     "asset_bytes": size, "theme_code_allowance_bytes": THEME_CODE_ALLOWANCE,
                     "standalone_budget_bytes": size + THEME_CODE_ALLOWANCE,
                     "asset_keys": keys})
        background.save(output / f"background-{theme.number:02d}.png")
    free = APPLICATION_LIMIT - len(firmware)
    capacity = subset_budget(rows, assets, free - COMMON_CODE_ALLOWANCE - FLASH_HEADROOM)
    for asset in assets.values():
        (output / (asset.name + ".bin")).write_bytes(asset.data + b"\0" * (asset.size - len(asset.data)))
    result = {
        "kind": "asset-measurement-plus-code-estimate-not-linked-firmware",
        "firmware_modified": False,
        "baseline_sha256": manifest["sha256"], "baseline_flash_bytes": len(firmware),
        "flash_total_bytes": 65536, "application_limit_bytes": APPLICATION_LIMIT,
        "reserved_settings_security_bytes": 1536, "free_application_bytes": free,
        "static_ram_bytes": static_ram, "ram_total_bytes": RAM_LIMIT,
        "minimum_stack_bytes": 1024,
        "ram_after_static_and_minimum_stack_bytes": RAM_LIMIT - static_ram - 1024,
        "full_uncompressed_frame_bytes": study.W * study.H // 8,
        "common_code_allowance_bytes": COMMON_CODE_ALLOWANCE,
        "flash_headroom_bytes": FLASH_HEADROOM,
        "method": "Dynamic fields removed from static art; raw or 8x8 dictionary bitmap; subset font atlases and metrics; four-byte asset alignment; shared font union; no PNG or desktop TTF included.",
        "limitations": [
            "Resources are measured; common renderer and per-theme code are allowances, not ARM-linked measurements.",
            "Bitmap backgrounds retain sampled-size cutout rectangles; final layout and glyph placement may adjust slightly.",
            "Characterwise condensed glyphs can differ by a pixel from whole-string rasterization.",
            "No cross-theme background tile dictionary or procedural texture savings are credited.",
            "Shared font subsets contain the union of characters needed across all twenty themes, even for smaller selections.",
            "RAM is not per-theme cumulative: keep assets in const Flash and stream scanlines; final stack/runtime must be tested.",
        ],
        "assets": {key: asset.size for key, asset in assets.items()},
        "themes": rows, "capacity": capacity,
    }
    (output / "report.json").write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n")
    lines = ["# 20 款主题容量预算", "", "资源包为实测字节数，代码为预留估算；并非已编译的主题固件。", "",
             f"当前固件 {len(firmware):,} B；应用区可用余量 {free:,} B；设置与安全页另保留 1,536 B。",
             f"静态 RAM {static_ram:,} / 4,096 B；至少保留栈 1,024 B 后，余量 {RAM_LIMIT-static_ram-1024:,} B。", "",
             "|编号|主题|背景 B|字模 B|保守单款预算 KiB|", "|---|---|---:|---:|---:|"]
    for row in rows:
        lines.append(f"|{row['number']:02d}|{row['name']}|{row['background_bytes']:,}|{row['font_and_digit_bytes']:,}|{row['standalone_budget_bytes']/1024:.2f}|")
    lines += ["", "单款预算包含 256 B 布局/效果代码预留，不含一次性的 4,096 B 公共绘制代码预留。",
              "同字模共享，不能把各行直接相加。计量使用二进制 KiB = 1,024 B。", "",
              "表中共享字模按全部 20 款所需字符的并集计量，小组合继续裁剪字符还能略省。", "",
              f"全部 20 款资源和单款代码预算（共享后）：{capacity['all_twenty_assets_and_theme_allowances_bytes']:,} B。",
              f"另保留公共代码 4,096 B、后续功能余量 2,048 B 后，资源预算为 {capacity['budget_after_common_code_and_headroom_bytes']:,} B。",
              f"在这个模型下，保留现有 6 款最多再放 {capacity['max_new_themes_under_model']} 款新主题。",
              f"任意挑选 {capacity['any_selection_new_theme_count_under_model']} 款也不超这个模型的预算；更多数量取决于组合。",
              "达到该数量的最低占用组合：" + "、".join(f"{n:02d}" for n in capacity['cheapest_maximum_selection']) + "。", "",
              "## 边界", "",
              "这不是硬件极限或烧录承诺。最终准确数量必须完成所选主题的 ARM 编译并检查链接映射、栈和实机刷新。",
              "本次未改固件、未连接设备、未烧录。", "",
              "运行内存不按主题数量累加。推荐 Flash 常量资源与逐行绘制，不分配整屏缓冲。",
              "整屏一位图为 5,624 B，大于全部 4 KiB RAM；反色可现场取反，不复制字模和背景。",
              "抖动纹理如改为程序生成、近似字号共用以及跨背景字典共享，仍有进一步缩减空间。", ""]
    (output / "report.md").write_text("\n".join(lines))
    print("PASS: all encoded background/font bitmaps round-trip exactly; baseline unchanged.")
    print(json.dumps({"flash_free": free, "static_ram": static_ram, **capacity}, ensure_ascii=False, indent=2))
    print(output / "report.md")


if __name__ == "__main__":
    main()
