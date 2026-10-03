#!/usr/bin/env python3
"""Build exact 8x16/16x16 bitmap glyphs; never rasterize host outline fonts.
GNU Unifont 16.0.04, SIL OFL 1.1 (also GPL with font exception).
The downloaded font is a build dependency, not a redistributable artifact here.
"""
import gzip
import hashlib
from pathlib import Path
import re
import urllib.request
ROOT = Path(__file__).resolve().parents[1]
URL = 'https://unifoundry.com/pub/unifont/unifont-16.0.04/font-builds/unifont-16.0.04.hex.gz'
EXPECTED_SHA256 = ''  # pinned by the first verified build
cache = ROOT / 'build/font-cache/unifont-16.0.04.hex.gz'
cache.parent.mkdir(parents=True, exist_ok=True)
if not cache.exists():
    with urllib.request.urlopen(URL, timeout=60) as response:
        cache.write_bytes(response.read())
raw = cache.read_bytes()
digest = hashlib.sha256(raw).hexdigest()
if EXPECTED_SHA256 and digest != EXPECTED_SHA256:
    raise RuntimeError('Pixel font dependency hash mismatch')
print('Unifont 16.0.04 SHA256:', digest)
# Include upstream notices with firmware distributions, not just a web link.
for name in ('OFL-1.1.txt', 'LICENSE.txt'):
    target = ROOT / 'docs' / ('Unifont-' + name)
    if not target.exists():
        with urllib.request.urlopen('https://unifoundry.com/' + name, timeout=60) as response:
            target.write_bytes(response.read())
chars = set(chr(c) for c in range(32, 127)) | {'°'}
for path in (ROOT / 'user/src').glob('*.c'):
    for literal in re.findall(r'"([^"\n]*)"', path.read_text()):
        chars.update(c for c in literal if ord(c) >= 128)
source = {}
for line in gzip.decompress(raw).decode('ascii').splitlines():
    code, bits = line.split(':')
    if chr(int(code, 16)) in chars:
        source[int(code, 16)] = bits
assert all(ord(c) in source for c in chars), 'Missing UI glyph'
out = ['#ifndef UI_FONT_H', '#define UI_FONT_H', '#include <stdint.h>',
       '/* Build-generated Unifont 16.0.04 subset; see docs/FONT-LICENSE.md. */',
       'typedef struct { uint16_t code; uint8_t width; uint16_t rows[16]; } SmallGlyph;',
       'static const SmallGlyph small_glyphs[] = {']
for c in sorted(chars):
    bits = source[ord(c)]
    width = len(bits) // 4
    assert width in (8, 16)
    stride = width // 4
    rows = [int(bits[y*stride:(y+1)*stride], 16) << (16-width) for y in range(16)]
    out.append('{0x%04x,%d,{%s}},' % (ord(c), width, ','.join('0x%04x' % r for r in rows)))
out += ['};', '#define SMALL_GLYPH_COUNT (sizeof small_glyphs / sizeof small_glyphs[0])']
# Original geometric 5x7 digits, scaled only by whole pixels.
digits = ['01110100011001110101110011000101110', '00100011000010000100001000010001110',
          '01110100010000100010001000100011111', '11110000010000101110000010000111110',
          '00010001100101010010111110001000010', '11111100001000011110000010000111110',
          '01110100001000011110100011000101110', '11111000010001000100010000100001000',
          '01110100011000101110100011000101110', '01110100011000101111000010000101110',
          '00000001000010000000001000010000000']
assert all(len(d) == 35 for d in digits)
for name, w, h, scale in [('large', 38, 52, 6), ('medium', 24, 32, 4)]:
    stride = (w+7)//8
    out.append('static const uint8_t %s_digits[11][%d] = {' % (name, stride*h))
    for digit in digits:
        data = [0] * (stride*h)
        for y in range(7*scale):
            for x in range(5*scale):
                if digit[(y//scale)*5+x//scale] == '1':
                    px, py = x+(w-5*scale)//2, y+(h-7*scale)//2
                    data[py*stride+px//8] |= 0x80 >> (px%8)
        out.append('{' + ','.join(map(str, data)) + '},')
    out.append('};')
out.append('#endif')
(ROOT/'user/inc/ui_font.h').write_text('\n'.join(out)+'\n')
print('Generated', len(chars), 'pixel glyphs; 1-bit, no interpolation or antialiasing')
