#!/usr/bin/env python3
"""Render twenty independent 296x152, one-bit clock design proposals."""
from __future__ import annotations

import argparse
from dataclasses import dataclass
from functools import lru_cache
import gzip
import hashlib
import json
from pathlib import Path
import shutil

from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont
import matplotlib

ROOT = Path(__file__).resolve().parents[1]
FONTS = Path(matplotlib.get_data_path()) / "fonts/ttf"
W, H = 296, 152
BAYER = (
    (0, 48, 12, 60, 3, 51, 15, 63),
    (32, 16, 44, 28, 35, 19, 47, 31),
    (8, 56, 4, 52, 11, 59, 7, 55),
    (40, 24, 36, 20, 43, 27, 39, 23),
    (2, 50, 14, 62, 1, 49, 13, 61),
    (34, 18, 46, 30, 33, 17, 45, 29),
    (10, 58, 6, 54, 9, 57, 5, 53),
    (42, 26, 38, 22, 41, 25, 37, 21),
)
FONT_FILES = {
    "sans": "DejaVuSans.ttf", "bold": "DejaVuSans-Bold.ttf",
    "serif": "DejaVuSerif.ttf", "serif_bold": "DejaVuSerif-Bold.ttf",
    "italic": "DejaVuSerif-Italic.ttf", "mono": "DejaVuSansMono.ttf",
    "mono_bold": "DejaVuSansMono-Bold.ttf", "slant": "DejaVuSans-BoldOblique.ttf",
}
PIXEL_DIGITS = (
    "01110100011001110101110011000101110", "00100011000010000100001000010001110",
    "01110100010000100010001000100011111", "11110000010000101110000010000111110",
    "00010001100101010010111110001000010", "11111100001000011110000010000111110",
    "01110100001000011110100011000101110", "11111000010001000100010000100001000",
    "01110100011000101110100011000101110", "01110100011000101111000010000101110",
)


@dataclass(frozen=True)
class Sample:
    time: str = "14:36"
    date: str = "2026.10.20"
    weekday: str = "星期二"
    temperature: str = "26°C"


@dataclass(frozen=True)
class Theme:
    number: int
    name: str
    english: str
    group: str
    font: str
    effect: str


THEMES = [
    Theme(1, "朋克·错版", "MISREGISTER", "朋克实验", "粗斜体 + 等宽小字", "错版网点 / 偏移套印"),
    Theme(2, "朋克·钢印", "STENCIL", "朋克实验", "镂空钢印数字", "喷墨颗粒 / 金属压纹"),
    Theme(3, "朋克·撕纸", "RIPPED PAPER", "朋克实验", "拼贴衬线 + 黑体", "撕纸边 / 复印机网点"),
    Theme(4, "赛博·终端", "AFTER HOURS", "朋克实验", "等宽终端数字", "扫描线 / 有序点阵"),
    Theme(5, "工业·警戒", "CAUTION", "朋克实验", "窄体钢印数字", "斜纹警戒带 / 铆点"),
    Theme(6, "简约·瑞士", "SWISS", "简约排版", "清晰无衬线数字", "细网点侧标 / 非对称网格"),
    Theme(7, "简约·书卷", "BOOKISH", "简约排版", "高对比衬线数字", "轻纸纹 / 细线层级"),
    Theme(8, "简约·呼吸", "BREATH", "简约排版", "轻量无衬线数字", "稀疏抖动日进度"),
    Theme(9, "简约·纵向", "VERTICAL", "简约排版", "上下排列的等宽数字", "网点信息栏 / 大块留白"),
    Theme(10, "简约·负片", "NEGATIVE", "简约排版", "纯白几何数字", "暗底 / 低密度星点"),
    Theme(11, "报刊·头版", "DAILY EDITION", "复古印刷", "报头黑体 + 正文衬线", "报纸半色调 / 栏线"),
    Theme(12, "复古·打字机", "TYPEWRITER", "复古印刷", "等宽机械打字字形", "纸张颗粒 / 印章"),
    Theme(13, "像素·点阵", "DOT MATRIX", "复古印刷", "圆点矩阵数字", "点阵发光孔 / 阶梯点阵"),
    Theme(14, "复古·液晶", "LCD 1984", "复古印刷", "原创七段液晶字", "未点亮段抖动 / 液晶底纹"),
    Theme(15, "制版·半色调", "RISOGRAPH", "复古印刷", "衬线斜体 + 反白数字", "粗网点 / 套色偏移阴影"),
    Theme(16, "装饰·几何", "DECO", "几何工业", "几何衬线数字", "阶梯装饰 / 扇形排线"),
    Theme(17, "现代·轮廓", "OUTLINE", "几何工业", "空心与实心数字混排", "轮廓线 / 斜向抖动"),
    Theme(18, "网点·日牌", "DAY TICKET", "几何工业", "巨型日期 + 紧凑时间", "日票冲孔 / 粗细网点"),
    Theme(19, "工业·仪表", "INSTRUMENT", "几何工业", "七段数字 + 等宽标尺", "仪表刻度 / 分级抖动"),
    Theme(20, "现代·织纹", "WOVEN", "几何工业", "黑体数字 + 像素标签", "织纹交叉排线 / 悬浮反白"),
]


@lru_cache(maxsize=64)
def font(kind: str, size: int):
    return ImageFont.truetype(str(FONTS / FONT_FILES[kind]), size)


@lru_cache(maxsize=1)
def unifont():
    data = (ROOT / "build/font-cache/unifont-16.0.04.hex.gz").read_bytes()
    assert hashlib.sha256(data).hexdigest() == "f9c8c7802453f47be02677176aeac2342ee96d354fad7a26cedcce48e68e1d9f"
    return {int(line.split(":")[0], 16): line.split(":")[1]
            for line in gzip.decompress(data).decode("ascii").splitlines()}


@lru_cache(maxsize=256)
def bitmap_label(text: str):
    glyphs = []
    for character in text:
        raw = unifont()[ord(character)]
        width = len(raw) // 4
        glyph = Image.frombytes("1", (width, 16), bytes.fromhex(raw)).convert("L")
        glyphs.append(glyph)
    output = Image.new("L", (sum(g.width for g in glyphs), 16), 0)
    x = 0
    for glyph in glyphs:
        output.paste(glyph, (x, 0))
        x += glyph.width
    return output


def text_mask(text: str, kind="sans", size=12):
    f = font(kind, size)
    box = f.getbbox(text)
    mask = Image.new("L", (max(1, box[2] - box[0]), max(1, box[3] - box[1])), 0)
    ImageDraw.Draw(mask).text((-box[0], -box[1]), text, font=f, fill=255)
    return mask.point(lambda p: 255 if p >= 120 else 0)


def digits(text: str, kind="sans", size=64, condensed=1.0):
    mask = text_mask(text, kind, size)
    if condensed != 1:
        mask = mask.resize((round(mask.width * condensed), mask.height), Image.Resampling.NEAREST)
    return mask


def patterned(size, density=0.25, style="bayer", origin=(0, 0)):
    output = Image.new("L", size, 255)
    pixels = output.load()
    for y in range(size[1]):
        for x in range(size[0]):
            u, v = x + origin[0], y + origin[1]
            if style == "bayer":
                black = BAYER[v % 8][u % 8] < density * 64
            elif style == "coarse":
                black = BAYER[(v // 2) % 8][(u // 2) % 8] < density * 64
            elif style == "noise":
                black = ((u * 374761393 + v * 668265263) ^ ((u + 31) * (v + 71) * 1274126177)) % 1024 < density * 1024
            elif style == "weave":
                black = ((u // 4 + v // 4) % 2 == 0 and v % 4 == 0) or ((u // 4 + v // 4) % 2 == 1 and u % 4 == 0)
            elif style == "diagonal":
                black = (u + v) % 8 < max(1, round(density * 8))
            elif style == "dots":
                black = (u % 6 - 2) ** 2 + (v % 6 - 2) ** 2 <= max(1, density * 12)
            else:
                raise ValueError(style)
            if black:
                pixels[x, y] = 0
    return output


class Canvas:
    def __init__(self, black=False):
        self.image = Image.new("L", (W, H), 0 if black else 255)
        self.draw = ImageDraw.Draw(self.image)
        self.labels = []
        self.label_halo = 0

    def rect(self, box, color=0, outline=None, width=1):
        self.draw.rectangle(box, fill=color, outline=outline, width=width)

    def line(self, points, color=0, width=1):
        self.draw.line(points, fill=color, width=width)

    def texture(self, box, density=0.25, style="bayer", inverse=False):
        x0, y0, x1, y1 = box
        patch = patterned((x1 - x0, y1 - y0), density, style, (x0, y0))
        if inverse:
            patch = ImageChops.invert(patch)
        self.image.paste(patch, (x0, y0))

    def mask(self, mask, x, y, color=0, halo=0):
        assert 0 <= x and 0 <= y and x + mask.width <= W and y + mask.height <= H, (x, y, mask.size)
        if halo:
            expanded = Image.new("L", (mask.width + 2 * halo, mask.height + 2 * halo), 0)
            expanded.paste(mask, (halo, halo))
            expanded = expanded.filter(ImageFilter.MaxFilter(2 * halo + 1))
            self.image.paste(255 - color, (x - halo, y - halo), expanded)
        self.image.paste(color, (x, y), mask)

    def text(self, text, x, y, kind="mono", size=10, color=0, align="left", native=False):
        mask = bitmap_label(text) if native else text_mask(text, kind, size)
        if align == "center":
            x -= mask.width // 2
        elif align == "right":
            x -= mask.width
        self.mask(mask, x, y, color, halo=self.label_halo)
        self.labels.append((text, (x, y, x + mask.width, y + mask.height)))

    def footer(self, black=False, x=148):
        self.text("SET / BACK", x, 133, native=True, color=255 if black else 0, align="center")

    def time(self, sample, kind="sans", size=64, x=None, y=42, color=0, condensed=1, halo=0):
        mask = digits(sample.time, kind, size, condensed)
        self.mask(mask, (W - mask.width) // 2 if x is None else x, y, color, halo)
        return mask


def standard_footer(c, s, black=False, y=116):
    color = 255 if black else 0
    c.text(s.date + "  TUE", 12, y, size=10, color=color)
    c.text(s.temperature, 284, y, size=11, color=color, align="right")
    c.footer(black)


def seven_segment(text, height=50, thickness=4, dormant=False):
    width = round(height * 0.51)
    cell = width + 7
    positions = []
    total = 0
    for ch in text:
        positions.append(total)
        total += cell if ch != ":" else thickness + 10
    output = Image.new("L", (total - 7, height), 0)
    draw = ImageDraw.Draw(output)
    maps = ("abcedf", "bc", "abged", "abgcd", "fgbc", "afgcd", "afgecd", "abc", "abcdefg", "abfgcd")
    for ch, x in zip(text, positions):
        if ch == ":":
            for y in (height // 3, 2 * height // 3):
                draw.rectangle((x + 2, y, x + 2 + thickness - 1, y + thickness - 1), fill=255)
            continue
        middle = height // 2
        bars = {
            "a": (x + thickness, 0, x + width - thickness, thickness - 1),
            "g": (x + thickness, middle - thickness // 2, x + width - thickness, middle + (thickness - 1) // 2),
            "d": (x + thickness, height - thickness, x + width - thickness, height - 1),
            "f": (x, thickness, x + thickness - 1, middle - 2),
            "b": (x + width - thickness, thickness, x + width - 1, middle - 2),
            "e": (x, middle + 2, x + thickness - 1, height - thickness - 1),
            "c": (x + width - thickness, middle + 2, x + width - 1, height - thickness - 1),
        }
        for key, box in bars.items():
            if dormant or key in maps[int(ch)]:
                draw.rectangle(box, fill=255)
    return output


def render(number, s):
    c = Canvas(black=number in (4, 10))
    if number in (1, 2, 4, 10, 12, 15, 18):
        c.label_halo = 1
    if number == 1:
        c.texture((0, 0, W, H), .13, "coarse")
        c.rect((8, 8, 158, 25))
        c.text("OFFBEAT / 20.10", 14, 12, size=11, color=255)
        c.text("TUE", 280, 11, kind="bold", size=12, align="right")
        m = digits(s.time, "slant", 69, .91)
        x, y = (W - m.width) // 2, 43
        c.mask(m, x + 3, y + 4, halo=1)
        c.mask(m, x, y, halo=1)
        for yy, xx, length in ((62, 12, 29), (79, 256, 26), (95, 22, 12)):
            c.rect((xx, yy, xx + length, yy + 1))
        c.rect((8, 111, 287, 133), 255)
        c.text(s.date, 14, 115, kind="bold", size=12)
        c.text(s.temperature, 282, 115, size=12, align="right")
        c.footer()
    elif number == 2:
        c.texture((0, 0, W, H), .055, "noise")
        c.line((12, 30, 284, 30), width=2)
        c.text("TIME IS A WEAPON", 14, 11, kind="bold", size=13)
        c.text("0020", 282, 13, size=10, align="right")
        m = digits(s.time, "bold", 70, .91)
        md = ImageDraw.Draw(m)
        for xx in range(19, m.width, 35):
            md.rectangle((xx, 0, xx + 2, m.height), fill=0)
        c.mask(m, (W - m.width) // 2, 45, halo=1)
        c.texture((14, 103, 282, 108), .4, "diagonal")
        standard_footer(c, s)
    elif number == 3:
        c.texture((0, 0, W, H), .5, "coarse")
        polygon = [(8, 9), (282, 13), (285, 114), (271, 111), (257, 118), (244, 112),
                   (228, 118), (210, 114), (192, 121), (174, 113), (152, 119), (133, 114),
                   (113, 121), (96, 116), (78, 121), (61, 114), (44, 119), (25, 115), (11, 118)]
        c.draw.polygon(polygon, fill=255)
        c.text("THE MIDNIGHT CLUB", 22, 19, kind="mono_bold", size=11)
        for i, ch in enumerate(s.time):
            kind = ("serif_bold", "bold", "mono", "italic", "bold")[i]
            m = digits(ch, kind, 61)
            x, y = 18 + i * 51, 46 + (i % 2) * 3
            if i in (1, 4):
                c.rect((x - 2, y - 3, x + m.width + 3, y + m.height + 3))
                c.mask(m, x, y, 255)
            else:
                c.mask(m, x, y)
        c.rect((16, 129, 280, 145))
        c.text(s.date + "  " + s.temperature + "   SET/BACK", 24, 133, size=9, color=255)
    elif number == 4:
        c.texture((0, 0, W, H), .055, "bayer", inverse=True)
        for y in range(28, 110, 4):
            c.line((10, y, 285, y), 255)
        c.rect((10, 32, 285, 103))
        c.text("> CLOCK.LOCAL", 12, 10, size=12, color=255)
        c.text("24 HOUR", 284, 12, size=9, color=255, align="right")
        c.time(s, "mono_bold", 67, y=44, color=255)
        c.text("_", 274, 91, size=16, color=255)
        standard_footer(c, s, black=True)
    elif number == 5:
        c.texture((10, 8, 286, 21), .5, "diagonal")
        c.rect((88, 7, 208, 22), 255)
        c.text("LOCAL TIME", 148, 10, size=10, align="center")
        c.time(s, "bold", 78, y=41, condensed=.83)
        c.line((12, 109, 284, 109))
        for x in (8, 287):
            for y in (30, 116):
                c.draw.ellipse((x - 2, y - 2, x + 2, y + 2), outline=0)
        standard_footer(c, s)
    elif number == 6:
        c.rect((14, 12, 20, 22))
        c.text(s.date, 30, 12, size=10)
        c.text(s.weekday, 282, 8, align="right", native=True)
        c.time(s, "bold", 73, x=10, y=42)
        c.texture((267, 45, 283, 98), .2)
        c.text("24H", 14, 112, size=10)
        c.text(s.temperature, 282, 111, size=12, align="right")
        c.footer()
    elif number == 7:
        c.texture((14, 11, 282, 13), .25)
        c.text("T U E S D A Y", 148, 20, kind="serif", size=10, align="center")
        c.time(s, "serif", 68, y=45)
        c.text(s.date + "  /  " + s.temperature, 148, 111, kind="serif", size=12, align="center")
        c.footer()
    elif number == 8:
        c.text(s.date, 148, 14, size=10, align="center")
        c.time(s, "sans", 62, y=44)
        c.text(s.weekday, 104, 103, native=True)
        c.text(s.temperature, 192, 106, size=11)
        c.texture((111, 129, 185, 132), .2)
        c.rect((111, 129, 151, 131))
        c.footer()
    elif number == 9:
        c.text(s.time[:2], 18, 8, kind="mono", size=66)
        c.text(s.time[3:], 18, 71, kind="mono", size=66)
        c.texture((115, 13, 119, 120), .3)
        c.text("TUE", 145, 15, kind="bold", size=11)
        c.text(s.date[:4], 145, 44, size=11)
        c.text(s.date[5:], 145, 65, size=15)
        c.text(s.temperature, 145, 103, size=13)
        c.footer()
    elif number == 10:
        c.texture((0, 0, W, 27), .06, "noise", inverse=True)
        c.text(s.date, 12, 11, size=10, color=255)
        c.text(s.weekday, 284, 8, native=True, color=255, align="right")
        c.time(s, "sans", 69, y=47, color=255)
        c.text(s.temperature, 148, 112, size=12, color=255, align="center")
        c.footer(black=True)
    elif number == 11:
        c.text("THE DAILY CLOCK", 148, 8, kind="serif_bold", size=21, align="center")
        c.line((10, 33, 286, 33), width=2)
        c.text(s.date, 12, 40, kind="serif", size=10)
        c.text("TUESDAY / NO. 293", 284, 40, kind="serif", size=10, align="right")
        c.time(s, "serif_bold", 66, x=13, y=60, condensed=.86)
        c.texture((230, 58, 284, 100), .3, "dots")
        c.rect((237, 69, 277, 88), 255)
        c.text(s.temperature, 257, 73, kind="serif_bold", size=11, align="center")
        c.line((12, 119, 284, 119))
        c.text("DAY 293 / A MOMENT TO KEEP", 12, 125, kind="serif", size=10)
        c.text("SET/BACK", 284, 133, native=True, align="right")
    elif number == 12:
        c.texture((0, 0, W, H), .018, "noise")
        c.text("TUESDAY, OCTOBER 20", 14, 13, size=11)
        c.text("2026", 282, 13, size=11, align="right")
        c.time(s, "mono", 66, y=47, halo=1)
        c.text("-  " + s.temperature + "  -", 148, 111, size=12, align="center")
        c.line((12, 132, 46, 132))
        c.footer()
    elif number == 13:
        c.text("DOT / CLOCK", 12, 10, size=11)
        c.text(s.date, 283, 10, size=10, align="right")
        x = 34
        for ch in s.time:
            if ch == ":":
                for y in (59, 83):
                    c.draw.ellipse((x + 1, y, x + 5, y + 4), fill=0)
                x += 18
                continue
            for gy in range(7):
                for gx in range(5):
                    if PIXEL_DIGITS[int(ch)][gy * 5 + gx] == "1":
                        xx, yy = x + gx * 9, 41 + gy * 9
                        c.draw.ellipse((xx, yy, xx + 5, yy + 5), fill=0)
            x += 56
        c.texture((12, 112, 284, 115), .2)
        standard_footer(c, s, y=121)
    elif number == 14:
        c.texture((8, 29, 288, 108), .05)
        c.rect((8, 29, 288, 108), None, outline=0)
        c.text("QUARTZ", 12, 9, kind="bold", size=12)
        c.text("TUE 20", 284, 11, size=10, align="right")
        m = seven_segment(s.time, 54, 5)
        ghost = seven_segment(s.time, 54, 5, dormant=True)
        x = (W - m.width) // 2
        pattern_mask = ImageChops.invert(patterned(ghost.size, .14, origin=(x, 42)))
        c.mask(ImageChops.multiply(ghost, pattern_mask), x, 42)
        c.mask(m, x, 42)
        standard_footer(c, s)
    elif number == 15:
        c.texture((180, 0, 296, 152), .24, "dots")
        c.text("RISO / 026", 12, 10, kind="mono_bold", size=11)
        c.text(s.date, 12, 29, size=9)
        m = digits(s.time, "italic", 72, .97)
        x = (W - m.width) // 2
        shadow = ImageChops.multiply(m, ImageChops.invert(patterned(m.size, .5, "coarse", (x + 4, 51))))
        c.mask(shadow, x + 4, 51)
        c.mask(m, x, 46, halo=2)
        c.rect((12, 113, 173, 132))
        c.text("TUESDAY / " + s.temperature, 20, 118, size=10, color=255)
        c.footer()
    elif number == 16:
        for offset in (0, 4, 8):
            c.line((12 + offset, 33 - offset, 12 + offset, 13 + offset, 86, 13 + offset))
            c.line((284 - offset, 33 - offset, 284 - offset, 13 + offset, 210, 13 + offset))
        c.text("O C T   2 0", 148, 17, kind="serif", size=10, align="center")
        c.time(s, "serif", 69, y=46, condensed=.95)
        for x in (12, 273):
            for offset in range(0, 12, 3):
                c.line((x + offset, 107, x + offset, 121 + offset // 3))
        c.text("T U E S D A Y", 148, 108, kind="serif", size=9, align="center")
        c.text(s.temperature, 148, 123, kind="serif", size=10, align="center")
        c.footer()
    elif number == 17:
        c.texture((254, 0, W, H), .18, "diagonal")
        c.text(s.date, 12, 11, size=10)
        c.text("TUE", 240, 11, size=10, align="right")
        m = digits(s.time[:2], "bold", 77, .87)
        padded = Image.new("L", (m.width + 6, m.height + 6), 0)
        padded.paste(m, (3, 3))
        m = padded
        inner = m.filter(ImageFilter.MinFilter(5))
        c.mask(ImageChops.subtract(m, inner), 6, 40)
        c.text(":", 112, 57, kind="bold", size=56)
        c.text(s.time[3:], 143, 43, kind="bold", size=77)
        c.text("HOURS / MINUTES", 12, 111, size=9)
        c.text(s.temperature, 242, 111, size=11, align="right")
        c.footer(x=126)
    elif number == 18:
        c.texture((10, 9, 96, 129), .27, "dots")
        c.rect((18, 34, 88, 107), 255)
        c.text("OCT", 53, 15, kind="bold", size=11, align="center")
        c.text("20", 53, 49, kind="bold", size=48, align="center")
        c.text("2026", 53, 113, size=10, align="center")
        for y in range(13, 128, 7):
            c.line((108, y, 108, y + 2))
        c.text(s.weekday, 128, 15, native=True)
        c.time(s, "bold", 47, x=124, y=49, condensed=.96)
        c.text(s.temperature, 128, 101, size=12)
        c.footer()
    elif number == 19:
        c.text("TIME / PRECISION", 12, 11, size=10)
        c.text("24H", 284, 11, size=10, align="right")
        for i in range(25):
            x = 16 + i * 11
            height = 9 if i % 6 == 0 else 4
            c.line((x, 34 - height, x, 34))
        m = seven_segment(s.time, 48, 4)
        c.mask(m, (W - m.width) // 2, 47)
        c.texture((16, 104, 280, 108), .22)
        c.rect((16, 104, 176, 107))
        standard_footer(c, s)
    elif number == 20:
        c.texture((0, 0, W, 152), style="weave")
        c.rect((10, 11, 285, 128), 255)
        c.rect((16, 17, 279, 38))
        c.text(s.date + "   TUESDAY", 26, 23, size=10, color=255)
        c.time(s, "bold", 68, y=51, condensed=.95)
        c.text(s.temperature, 272, 110, size=12, align="right")
        c.text("LOCAL / 24H", 23, 111, size=9)
        c.rect((98, 130, 198, 151), 255)
        c.footer()
    else:
        raise ValueError(number)
    assert set(c.image.getdata()) <= {0, 255}
    return c.image.convert("1")


def make_sheet(themes, output, filename, columns=2, scale=2):
    cell_w, cell_h = W * scale + 36, H * scale + 76
    rows = (len(themes) + columns - 1) // columns
    sheet = Image.new("RGB", (columns * cell_w + 24, rows * cell_h + 72), "#f2f4f3")
    draw = ImageDraw.Draw(sheet)
    label_font = ImageFont.truetype("/System/Library/Fonts/STHeiti Light.ttc", 20)
    small_font = font("mono", 12)
    draw.text((24, 16), "小钟 / 20 个主题设计提案", font=label_font, fill="#161b18")
    draw.text((24, 44), "296 x 152 / 1-BIT / PIXEL-EXACT / 2026.10.20", font=small_font, fill="#435047")
    for i, theme in enumerate(themes):
        x, y = 24 + i % columns * cell_w, 82 + i // columns * cell_h
        draw.text((x, y), f"{theme.number:02d}  {theme.name}", font=label_font, fill="#161b18")
        native = Image.open(output / f"{theme.number:02d}.png")
        image = native.resize((W * scale, H * scale), Image.Resampling.NEAREST).convert("RGB")
        sheet.paste(image, (x, y + 33))
        draw.rectangle((x - 1, y + 32, x + image.width, y + 33 + image.height), outline="#b3bdb7")
        draw.text((x, y + 43 + image.height), theme.english, font=small_font, fill="#435047")
    sheet.save(output / filename)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "design/theme-study-20261020")
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    sample = Sample()
    metadata = []
    for theme in THEMES:
        image = render(theme.number, sample)
        image.save(output / f"{theme.number:02d}.png")
        ImageChops.invert(image.convert("L")).convert("1").save(output / f"{theme.number:02d}-inverse.png")
        digest = hashlib.sha256(image.tobytes()).hexdigest()
        for value in ("08:08", "23:59", "00:00"):
            render(theme.number, Sample(time=value))
        metadata.append({**theme.__dict__, "sha256": digest,
                         "bytes_full_frame": len(image.tobytes()), "size": [W, H]})
    assert len({item["sha256"] for item in metadata}) == 20
    make_sheet(THEMES, output, "overview.png", columns=4, scale=2)
    for start in range(0, 20, 5):
        make_sheet(THEMES[start:start + 5], output, f"group-{start // 5 + 1}.png")
    (output / "manifest.json").write_text(json.dumps({
        "kind": "design-only", "firmware_modified": False, "sample": sample.__dict__,
        "dither": "deterministic, screen-anchored; black and white only",
        "font_sources": "DejaVu + original pixel/seven-segment numerals + Unifont 16.0.04",
        "themes": metadata,
    }, ensure_ascii=False, indent=2) + "\n")
    shutil.copyfile(FONTS / "LICENSE_DEJAVU", output / "LICENSE-DEJAVU.txt")
    shutil.copyfile(ROOT / "docs/Unifont-OFL-1.1.txt", output / "LICENSE-UNIFONT.txt")
    template = (ROOT / "tools/theme_study_gallery.html").read_text()
    cards = []
    for theme in THEMES:
        n = f"{theme.number:02d}"
        cards.append(f'''<article class="sample" data-number="{n}" data-group="{theme.group}">
  <div class="caption"><label><input type="checkbox" value="{n}" aria-label="选择 {n} {theme.name}">
  <span class="number">{n}</span> <span>{theme.name}</span></label>
  <button class="zoom" type="button" data-number="{n}" title="放大 {n} {theme.name}" aria-label="放大 {n} {theme.name}">查看</button></div>
  <img src="{n}.png" width="296" height="152" alt="{n} {theme.name}，296×152 黑白点阵预览" data-number="{n}">
  <p>{theme.font}<span> · </span>{theme.effect}</p>
</article>''')
    (output / "index.html").write_text(template.replace("<!-- THEME_CARDS -->", "\n".join(cards)))
    print(f"PASS: 20 unique one-bit samples; all text in bounds at 14:36, 08:08, 23:59 and 00:00.\n{output}")


if __name__ == "__main__":
    main()
