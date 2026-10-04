#!/usr/bin/env python3
"""Render a pixel-exact contact sheet from the production C renderer."""
from pathlib import Path
import subprocess
from PIL import Image, ImageDraw
ROOT=Path(__file__).resolve().parents[1]
(ROOT/'design').mkdir(exist_ok=True)
exe=ROOT/'build/clock/preview'
subprocess.run(['cc','-std=c11','-O2','-Iuser/inc','tools/preview.c','user/src/clock_core.c','user/src/clock_render.c','-o',str(exe)],cwd=ROOT,check=True)
subprocess.run([str(exe)],cwd=ROOT,check=True)
items=[('theme-1.pgm','01  MINIMAL / CHIP TEMPERATURE'),('theme-2-dither.pgm','02  DITHERED SPLIT-FLAP'),('theme-3.pgm','03  DAY CARD / DAILY PROGRESS'),('theme-2-night-dither.pgm','04  FLIP / EXACT PIXEL INVERSION'),('menu-7.pgm','05  FOCUS TIMER'),('focus-done.pgm','06  FOCUS COMPLETE')]
canvas=Image.new('L',(1848,1554),255)
draw=ImageDraw.Draw(canvas)
draw.text((24,12),'DESKTOP CLOCK V4 - production C renderer / preview, not a device photograph',fill=0)
for i,(name,label) in enumerate(items):
    im=Image.open(ROOT/'design'/name)
    assert set(im.tobytes()) <= {0,255}, 'UI has unexpected gray/antialiasing'
    im=im.resize((888,456),Image.Resampling.NEAREST)
    x=24+(i%2)*912;y=54+(i//2)*500
    draw.text((x,y-18),label,fill=0)
    canvas.paste(im,(x,y))
    draw.rectangle((x-1,y-1,x+888,y+456),outline=0)
canvas.save(ROOT/'design/desktop-v4.png')
print('PASS: all contact-sheet pixels are strictly black/white before integer scaling')
