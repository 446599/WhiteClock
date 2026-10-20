#!/usr/bin/env python3
"""Read live refresh metrics without resetting, halting, or writing the device."""
import argparse
import datetime
import hashlib
import json
from pathlib import Path
import time

from pyocd.core.helpers import ConnectHelper

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--probe', default='C7F18F064CCC')
    parser.add_argument('--frequency', type=int, default=5000)
    parser.add_argument('--seconds', type=float, default=90)
    parser.add_argument('--build', type=Path, default=ROOT / 'build/clock')
    args = parser.parse_args()
    if not 0 < args.seconds <= 600:
        parser.error('--seconds must be between 0 and 600')
    build = args.build.resolve()
    manifest = json.loads((build / 'manifest.json').read_text())
    image = (build / 'clock.bin').read_bytes()
    if hashlib.sha256(image).hexdigest() != manifest['sha256']:
        raise RuntimeError('Local image and manifest differ.')
    symbols = manifest['debug_symbols']
    low, high = min(symbols.values()), max(symbols.values())
    if low < 0x20000000 or high >= 0x20001000:
        raise RuntimeError('Debug symbols are outside device RAM.')
    now = datetime.datetime.now().astimezone()
    output = ROOT.parent / 'evidence' / ('refresh_' + now.strftime('%Y%m%d_%H%M%S') + '.json')
    report = {'started': now.isoformat(), 'image_sha256': manifest['sha256'],
              'mode': 'read-only, CPU left in its existing state', 'samples': []}
    session = ConnectHelper.session_with_chosen_probe(
        unique_id=args.probe, target_override='cw32l010f8',
        pack=str(ROOT / 'pack/WHXY.CW32L010_DFP.1.0.2.pack'),
        frequency=args.frequency, connect_mode='attach', auto_unlock=False,
        resume_on_disconnect=False, options={'cache.enable_memory': False})
    if session is None:
        raise RuntimeError('Debug probe not detected.')
    try:
        with session:
            target = session.target
            part = bytes(target.read_memory_block8(0x001007e0, 16)).rstrip(b'\0')
            if not part.startswith(b'CW32L010') or target.read32(0x001007d8) != 65536:
                raise RuntimeError('Unexpected target identity.')
            live = bytes(target.read_memory_block8(0, len(image)))
            if live != image:
                raise RuntimeError('Device image differs; runtime symbol addresses are unsafe.')
            report['initial_cpu_state'] = str(target.get_state())
            print('MONITOR READY: operate physical keys now.', flush=True)
            start = time.monotonic()
            next_heartbeat = 20
            previous = None
            sample = {}
            while time.monotonic() - start < args.seconds:
                successes_before = target.read32(symbols['epd_successes'])
                words = target.read_memory_block32(low, (high - low) // 4 + 1)
                sample = {name: words[(address - low) // 4] for name, address in symbols.items()}
                successes_after = target.read32(symbols['epd_successes'])
                elapsed = round(time.monotonic() - start, 3)
                if (sample['epd_successes'] == successes_before == successes_after
                        and sample['epd_total_ms'] > 0):
                    signature = (sample['epd_successes'], sample['epd_failures'],
                                 sample.get('clock_key_events'), sample.get('clock_ui_state'))
                    if signature != previous:
                        sample['observed_s'] = elapsed
                        if 'clock_ui_state' in sample:
                            state = sample['clock_ui_state']
                            sample['pages'] = {'ui': state & 15, 'pending': (state >> 8) & 15,
                                               'displayed': (state >> 16) & 15,
                                               'active': bool(state & (1 << 24)),
                                               'have_display': bool(state & (1 << 25)),
                                               'retry': bool(state & (1 << 26))}
                        report['samples'].append(sample)
                        print(json.dumps(sample), flush=True)
                        previous = signature
                if elapsed >= next_heartbeat:
                    print('MONITOR ACTIVE: ' + str(elapsed) + 's', flush=True)
                    next_heartbeat += 20
                time.sleep(0.1)
            report['final_cpu_state'] = str(target.get_state())
            report['last_observed'] = sample
    except Exception as exc:
        report['error'] = str(exc)
        raise
    finally:
        output.parent.mkdir(exist_ok=True)
        output.write_text(json.dumps(report, indent=2) + '\n')
        print('Evidence: ' + str(output), flush=True)


if __name__ == '__main__':
    main()
