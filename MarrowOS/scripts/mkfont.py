#!/usr/bin/env python3
from PIL import Image, ImageFont, ImageDraw
from pathlib import Path
out=Path(__file__).resolve().parents[1]/'assets/font.h'
with out.open('w') as f:
 f.write('/* Rasterized DejaVu Sans; see assets/FONT-LICENSE. */\n#include <stdint.h>\ntypedef struct {int width,height; const uint8_t *data; const uint16_t *advance;} Font;\n')
 for size in [12,14,16,18,24,36]:
  font=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf',size)
  w=size+6; h=size+9; data=[]; adv=[]
  for c in range(32,127):
   im=Image.new('L',(w,h)); ImageDraw.Draw(im).text((0,0),chr(c),font=font,fill=255,stroke_width=0)
   data.extend(im.tobytes()); adv.append(round(font.getlength(chr(c))))
  f.write(f'static const uint8_t fontdata{size}[]={{'+','.join(map(str,data))+'};\n')
  f.write(f'static const uint16_t fontadv{size}[]={{'+','.join(map(str,adv))+'};\n')
  f.write(f'static const Font font{size}={{{w},{h},fontdata{size},fontadv{size}}};\n')
