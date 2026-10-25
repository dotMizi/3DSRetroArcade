"""Resize the three supplied menu pictures without cropping; embed RGB565 pixels."""
from pathlib import Path
from PIL import Image, ImageOps
root=Path(__file__).resolve().parent.parent
names=['delta-race','sky-shield','rescue-wing']
arrays=[]
for name in names:
    original=Image.open(root/f'assets/menu-icons/{name}-original.png').convert('RGB')
    thumb=ImageOps.contain(original,(88,88),Image.Resampling.LANCZOS)
    tile=Image.new('RGB',(88,88),(0,0,0))
    tile.paste(thumb,((88-thumb.width)//2,(88-thumb.height)//2))
    tile.save(root/f'assets/menu-icons/{name}-88.png')
    pixels=[((r>>3)<<11)|((g>>2)<<5)|(b>>3) for y in range(88) for x in range(88) for r,g,b in [tile.getpixel((x,y))]]
    arrays.append(' {\n'+''.join('  '+','.join(f'0x{v:04x}' for v in pixels[i:i+16])+',\n' for i in range(0,len(pixels),16))+' },\n')
(root/'source/menu_icons.h').write_text('''/* User-supplied menu art, converted by tools/prepare_menu_icons.py. */
#ifndef MENU_ICONS_H
#define MENU_ICONS_H
#define MENU_ICON_SIZE 88
extern const unsigned short menuIcons[3][MENU_ICON_SIZE*MENU_ICON_SIZE];
#endif
''')
(root/'source/menu_icons.c').write_text('/* Generated from supplied artwork. Regenerate with tools/prepare_menu_icons.py. */\n#include "menu_icons.h"\nconst unsigned short menuIcons[3][MENU_ICON_SIZE*MENU_ICON_SIZE] = {\n'+''.join(arrays)+'};\n')
