#!/usr/bin/env python3
"""Render a pixel-exact contact sheet from the production C renderer."""
from pathlib import Path
import argparse
import subprocess
from PIL import Image, ImageDraw
ROOT=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output-dir',type=Path,default=ROOT/'design')
args=parser.parse_args()
output=args.output_dir.resolve()
output.mkdir(parents=True,exist_ok=True)
exe=ROOT/'build/clock/preview'
exe.parent.mkdir(parents=True,exist_ok=True)
subprocess.run(['cc','-std=c11','-O2','-Iuser/inc','tools/preview.c','user/src/clock_core.c','user/src/clock_render.c','user/src/clock_theme.c','-o',str(exe)],cwd=ROOT,check=True)
subprocess.run([str(exe),str(output)],cwd=ROOT,check=True)
names=['01  OFFBEAT','03  PAPER COLLAGE','11  FRONT PAGE','15  HALFTONE','16  GEOMETRIC','18  DAY TICKET','20  WOVEN']
themes=sorted(output.glob('theme-*-dither.pgm'),key=lambda p:int(p.stem.split('-')[1]))
assert len(themes)==len(names), 'Update gallery names when adding a theme'
items=[(p.name,f'{i+1:02d}  {names[i]}') for i,p in enumerate(themes)]
def sheet(items,filename):
    canvas=Image.new('L',(1256,54+((len(items)+1)//2)*344),255)
    draw=ImageDraw.Draw(canvas)
    draw.text((24,12),'WhiteClock V1.0 - production C renderer / preview, not a device photograph',fill=0)
    for i,(name,label) in enumerate(items):
        with Image.open(output/name) as source:
            assert source.size==(296,152)
            assert set(source.getdata()) <= {0,85,170,255}, 'Unexpected gray/antialiasing'
            im=source.resize((592,304),Image.Resampling.NEAREST)
        x=24+(i%2)*616;y=54+(i//2)*344
        draw.text((x,y-18),label,fill=0)
        canvas.paste(im,(x,y))
        draw.rectangle((x-1,y-1,x+592,y+304),outline=0)
    canvas.save(output/filename)
sheet(items,'themes.png')
sheet(items+[('theme-4-night.pgm','SCHEDULED DARK MODE'),('menu-7.pgm','FOCUS TIMER')],'desktop-v4.png')
sheet([(f'theme-{i+1}-select.pgm',name) for i,name in enumerate(names)],'theme-selection.png')
sheet([('menu-1.pgm','SETTINGS'),('menu-last.pgm','READ-ONLY VERSION'),
       ('menu-7.pgm','FOCUS TIMER'),('focus-done.pgm','FOCUS COMPLETE')],'menus.png')
print('PASS: contact-sheet pixels are limited to four logical gray levels before integer scaling')
