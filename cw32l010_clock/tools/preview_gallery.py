from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
root=Path(__file__).resolve().parents[1]/'design'
f=ImageFont.truetype('/System/Library/Fonts/STHeiti Light.ttc',24)
small=ImageFont.truetype('/System/Library/Fonts/STHeiti Light.ttc',16)
for p in root.glob('*.pgm'):
 Image.open(p).resize((888,456),Image.Resampling.NEAREST).save(p.with_suffix('.png'))
canvas=Image.new('RGB',(1260,1254),'#e9e8e4');d=ImageDraw.Draw(canvas)
d.text((34,20),'小钟 · 四级灰度主题',font=f,fill='#101010')
d.text((34,57),'296 × 152 像素 | 左：四灰设计效果  右：黑白点阵模拟灰度',font=small,fill='#444444')
for i,name in enumerate(['留白 · 清晰大字','翻页 · 轻复古','日历 · 日期优先']):
 y=110+i*375
 d.text((34,y),f'{i+1:02d}  {name}',font=f,fill='#101010')
 for col,suffix in enumerate(['','-dither']):
  im=Image.open(root/f'theme-{i+1}{suffix}.pgm').convert('RGB').resize((592,304),Image.Resampling.NEAREST)
  canvas.paste(im,(34+col*620,y+46))
canvas.save(root/'themes-comparison.png')
canvas=Image.new('RGB',(1260,1285),'#e9e8e4');d=ImageDraw.Draw(canvas)
for i,(file,name) in enumerate([('menu-1','设置菜单'),('menu-2','时间设置'),('menu-3','日期设置'),('menu-4','12/24 小时制'),('menu-5','定时反色'),('menu-6','主题选择')]):
 x=34+(i%2)*620;y=25+(i//2)*420
 d.text((x,y),name,font=f,fill='#111111')
 im=Image.open(root/(file+'.pgm')).convert('RGB').resize((592,304),Image.Resampling.NEAREST)
 canvas.paste(im,(x,y+42))
canvas.save(root/'menus-comparison.png')
