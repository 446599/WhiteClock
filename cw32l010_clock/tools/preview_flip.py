#!/usr/bin/env python3
"""Pixel-exact split-flap previews decoded from the production panel-row renderer."""
from pathlib import Path
import subprocess
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
DESIGN = ROOT / 'design'
BUILD = ROOT / 'build/clock'
DESIGN.mkdir(exist_ok=True)
BUILD.mkdir(parents=True, exist_ok=True)
exe = BUILD / 'preview'
subprocess.run(['cc', '-std=c11', '-O2', '-Iuser/inc', 'tools/preview.c',
                'user/src/clock_core.c', 'user/src/clock_render.c', '-o', str(exe)],
               cwd=ROOT, check=True)
subprocess.run([str(exe)], cwd=ROOT, check=True)

items = [
    ('theme-2-dither.pgm', '01 / DAY - DITHERED CHARCOAL', 'flip-day.png'),
    ('theme-2-night-dither.pgm', '02 / NIGHT - EXACT PIXEL INVERSE', 'flip-night.png'),
    ('flip-midnight.pgm', '03 / 12-HOUR - MIDNIGHT', 'flip-midnight.png'),
    ('flip-noon-negative.pgm', '04 / NOON - NEGATIVE CHIP TEMPERATURE', 'flip-noon-negative.png'),
    ('flip-unavailable.pgm', '05 / SENSOR UNAVAILABLE', 'flip-unavailable.png'),
    ('flip-focus-paused.pgm', '06 / FOCUS PAUSED', 'flip-focus-paused.png'),
]
# The GUI contains no anti-aliasing; 3x nearest-neighbour only, no filtered resize.
canvas = Image.new('1', (1848, 1548), 1)
draw = ImageDraw.Draw(canvas)
draw.text((24, 12), 'FLIP CLOCK V4.1 / 296 x 152 / production row bytes / not a device photograph', fill=0)
for index, (pgm, label, png) in enumerate(items):
    im = Image.open(DESIGN / pgm).convert('L')
    assert im.size == (296, 152) and set(im.tobytes()) <= {0, 255}
    im.convert('1', dither=Image.Dither.NONE).save(DESIGN / png)
    x, y = 24 + (index % 2)*912, 54 + (index // 2)*500
    draw.text((x, y-18), label, fill=0)
    canvas.paste(im.resize((888, 456), Image.Resampling.NEAREST), (x, y))
    draw.rectangle((x-1, y-1, x+888, y+456), outline=0)
canvas.save(DESIGN / 'flip-clock-v4.1.png')
# A single theme image is useful for comparing with a photograph of the device.
day = Image.open(DESIGN / 'flip-day.png')
day.resize((1184, 608), Image.Resampling.NEAREST).save(DESIGN / 'flip-day-4x.png')
night = Image.open(DESIGN / 'flip-night.png').convert('L')
assert all(a ^ b == 255 for a, b in zip(day.convert('L').tobytes(), night.tobytes()))
print('PASS: flip previews use actual 1-bit row data; six states, integer scaling, exact night complement')
