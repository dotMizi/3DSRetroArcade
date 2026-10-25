"""Convert the supplied splash image to the 3DS BGR8 framebuffer layout."""
from pathlib import Path
from PIL import Image
root = Path(__file__).resolve().parent.parent
im = Image.open(root/'assets/arcade-original.png').convert('RGB').resize((400,240),Image.Resampling.LANCZOS)
im.save(root/'assets/splash-preview.png')
(root/'data').mkdir(exist_ok=True)
(root/'data/splash.bin').write_bytes(bytes(c for x in range(400) for y in range(239,-1,-1) for c in im.getpixel((x,y))[::-1]))
