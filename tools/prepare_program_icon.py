"""Resize the supplied arcade cabinet artwork for the 3DS SMDH icon."""
from pathlib import Path
from PIL import Image
root = Path(__file__).resolve().parent.parent
with Image.open(root / 'assets/program-icon/original.png') as source:
    source.convert('RGB').resize((48, 48), Image.Resampling.LANCZOS).save(root / 'icon.png')
