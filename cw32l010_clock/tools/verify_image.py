#!/usr/bin/env python3
"""Validate the linked image and publish a local build manifest (no device access)."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build', type=Path, default=ROOT / 'build/clock')
args = parser.parse_args()
BUILD = args.build.resolve()
image = (BUILD / 'clock.bin').read_bytes()
original = (ROOT.parent / 'cw32l010_original_64k.bin').read_bytes()
original_hash = '0f92f36672594320223d3a23815bd04f864b1163875ef61d41a2460bd0c80c71'
assert len(original) == 65536 and hashlib.sha256(original).hexdigest() == original_hash
assert 192 < len(image) <= 64000 and len(image) % 4 == 0
vectors = struct.unpack('<48I', image[:192])
assert vectors[0] == 0x20001000
for address in vectors[1:]:
    assert address & 1 and 192 <= (address & ~1) < len(image)
assert not subprocess.check_output(['arm-none-eabi-nm','-u',str(BUILD/'clock.elf')]).strip()
symbols = {}
for line in subprocess.check_output(['arm-none-eabi-nm','-n',str(BUILD/'clock.elf')],text=True).splitlines():
    items = line.split()
    if len(items) == 3:
        symbols[items[2]] = int(items[0],16)
assert symbols['_estack'] == vectors[0]
assert symbols['Reset_Handler'] == vectors[1] & ~1
assert symbols['SysTick_Handler'] == vectors[15] & ~1
assert symbols['_ebss'] + 1024 <= symbols['_estack']
debug_symbols = {n:symbols[n] for n in [
    'clock_stage','epd_successes','epd_failures','rtc_failures','key_overflows',
    'main_loop_max_ms','epd_poll_max_ms','clock_ui_state','clock_key_events','clock_last_key_ms'
]}
# EpdMetrics is a contiguous record of uint32_t fields, in epd.h order.
for i, name in enumerate([
    'setup_ms','old_ms','new_ms','busy_ms','finish_ms','total_ms',
    'image_bytes','rendered_rows','max_render_ms','refreshed'
]):
    debug_symbols[f'epd_{name}'] = symbols['epd_metrics'] + 4*i
manifest = {
    'image': 'clock.bin', 'sha256': hashlib.sha256(image).hexdigest(),
    'bytes': len(image), 'flash_origin': 0,
    'static_ram_bytes': symbols['_ebss'] - 0x20000000,
    'minimum_stack_bytes': 1024,
    'original_sha256': original_hash,
    'hardware_status': 'Built; NOT flashed or hardware-validated',
    'debug_symbols': debug_symbols
}
(BUILD/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print(json.dumps(manifest,indent=2))
