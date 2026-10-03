#!/usr/bin/env python3
"""Inspect CW32L010 by default; --flash writes only the reviewed clock image.
No mass erase, read-protection changes, or automatic unlocking are allowed.
"""
import argparse
import datetime
import hashlib
import json
from pathlib import Path
import struct
import sys
import time

from pyocd.core.helpers import ConnectHelper
from pyocd.core.target import Target
from pyocd.flash.file_programmer import FileProgrammer

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build/clock'
EVIDENCE = ROOT.parent / 'evidence'
ORIGINAL_HASH = '0f92f36672594320223d3a23815bd04f864b1163875ef61d41a2460bd0c80c71'


def digest(data):
    return hashlib.sha256(data).hexdigest()


def snapshot(t):
    part = bytes(t.read_memory_block8(0x001007e0,16)).rstrip(b'\x00').decode('ascii',errors='replace')
    return {
        'part': part, 'pin_count': t.read8(0x001007d5),
        'ram_bytes': t.read16(0x001007d6), 'flash_bytes': t.read32(0x001007d8),
        'security_level': (t.read32(0x40022000) >> 5) & 3,
        'rtc': [hex(x) for x in t.read_memory_block32(0x40004404,17)],
        'sysctrl': [hex(x) for x in t.read_memory_block32(0x40004000,15)],
        'gpiob_idr': hex(t.read32(0x48000150)),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--flash', action='store_true')
    mode.add_argument('--restore', action='store_true', help='restore the verified original 64 KiB backup')
    parser.add_argument('--probe',default='C7F18F064CCC')
    parser.add_argument('--frequency',type=int,default=10000)
    parser.add_argument('--expected-before-sha256', default=ORIGINAL_HASH,
                        help='full flash hash of the known image being replaced')
    args = parser.parse_args()
    original = (ROOT.parent/'cw32l010_original_64k.bin').read_bytes()
    if len(original) != 65536 or digest(original) != ORIGINAL_HASH:
        raise RuntimeError('Original backup hash mismatch; stopping.')
    image = (BUILD/'clock.bin').read_bytes()
    manifest = json.loads((BUILD/'manifest.json').read_text())
    if digest(image) != manifest['sha256'] or not 192 < len(image) <= 64000:
        raise RuntimeError('Image does not match the reviewed manifest; stopping.')
    if args.restore:
        image = original
    EVIDENCE.mkdir(exist_ok=True)
    stamp = datetime.datetime.now().astimezone().strftime('%Y%m%d_%H%M%S')
    report = {'started': stamp, 'mode':'restore' if args.restore else 'flash' if args.flash else 'inspect',
              'image_sha256':digest(image)}
    report_file = EVIDENCE/f'device_{stamp}.json'
    flash_started = False
    verified = False
    session = ConnectHelper.session_with_chosen_probe(
        unique_id=args.probe, target_override='cw32l010f8',
        pack=str(ROOT/'pack/WHXY.CW32L010_DFP.1.0.2.pack'),
        frequency=args.frequency, connect_mode='attach', auto_unlock=False,
        resume_on_disconnect=False, reset_type='sysresetreq',
        options={'cache.enable_memory':False})
    if session is None:
        raise RuntimeError('WCH-Link not detected.')
    try:
        with session:
            t = session.target
            was_running = t.get_state() in (Target.State.RUNNING, Target.State.SLEEPING)
            try:
                t.halt()
                info = snapshot(t)
                report['before'] = info
                print(json.dumps(info,indent=2),flush=True)
                if not info['part'].startswith('CW32L010') or info['flash_bytes'] != 65536 or info['ram_bytes'] != 4096:
                    raise RuntimeError('Unexpected chip identity or memory size; no flash written.')
                if info['security_level'] != 0:
                    raise RuntimeError('Read protection is enabled; automatic unlock is prohibited.')
                current = bytes(t.read_memory_block8(0,65536))
                (EVIDENCE/f'before_{stamp}.bin').write_bytes(current)
                report['before_sha256'] = digest(current)
                if not (args.flash or args.restore):
                    print('READ-ONLY inspection complete.',flush=True)
                    return
                if args.flash and digest(current) != args.expected_before_sha256:
                    raise RuntimeError('Live firmware differs from the original backup; inspect saved readback before flashing.')
                # Supply entire 512-byte erase pages, preserving original tail bytes.
                end = (len(image)+511)//512*512
                payload = image + current[len(image):end]
                candidate = EVIDENCE/f'program_pages_{stamp}.bin'
                candidate.write_bytes(payload)
                expected = payload + current[end:]
                print(f'Programming {end//512} pages at 0x00000000; keep contact until VERIFY PASS.',flush=True)
                flash_started = True
                report['flash_started'] = True
                report_file.write_text(json.dumps(report,indent=2)+'\n')
                # Clear exception state and inherited application interrupts before
                # executing RAM algorithms (the first erased page holds vectors).
                t.reset_and_halt(reset_type=Target.ResetType.SYSRESETREQ)
                t.write32(0xe000e010, 0)          # SysTick CTRL
                t.write32(0xe000e180, 0xffffffff) # NVIC ICER
                t.write32(0xe000e280, 0xffffffff) # NVIC ICPR
                t.write32(0xe000ed04, 0x0a000000) # pending SysTick/PendSV clear
                t.write_core_register('primask', 1)
                FileProgrammer(session,chip_erase='sector',smart_flash=False,trust_crc=False,
                    keep_unwritten=True).program(str(candidate),base_address=0)
                t.halt()
                readback = bytes(t.read_memory_block8(0,65536))
                (EVIDENCE/f'after_{stamp}.bin').write_bytes(readback)
                report['after_sha256'] = digest(readback)
                if readback != expected:
                    raise RuntimeError('Full 64 KiB readback mismatch; CPU left halted. Keep the clamp attached for recovery.')
                verified = True
                report['verified'] = True
                print('VERIFY PASS: programmed image and all unchanged flash bytes match.',flush=True)
                t.reset(reset_type=Target.ResetType.SYSRESETREQ)
                # The programming algorithm can leave reset vector catch set.
                t.resume()
                if args.restore:
                    print('Original firmware restored and full readback verified.',flush=True)
                    return
                time.sleep(2)
                samples=[]
                for _ in range(20):
                    sample={n:t.read32(a) for n,a in manifest['debug_symbols'].items()}
                    sample['rtc_time']=hex(t.read32(0x40004418))
                    sample['cpu_state']=str(t.get_state())
                    samples.append(sample)
                    if sample['epd_successes'] or sample['epd_failures'] or sample['clock_stage']==255:
                        break
                    time.sleep(1)
                report['runtime_samples']=samples
                print(json.dumps(samples[-1],indent=2),flush=True)
                print('Flash verification finished. Screen appearance still requires visual confirmation.',flush=True)
            finally:
                if not flash_started and was_running:
                    t.resume()
    except Exception as exc:
        report['error']=str(exc)
        print(f'ERROR: {exc}',file=sys.stderr,flush=True)
        if flash_started and not verified:
            print('Programming was started; keep the connection for readback/recovery.',file=sys.stderr,flush=True)
        raise
    finally:
        report_file.write_text(json.dumps(report,indent=2)+'\n')

if __name__ == '__main__':
    try:
        main()
    except Exception:
        raise SystemExit(1)
