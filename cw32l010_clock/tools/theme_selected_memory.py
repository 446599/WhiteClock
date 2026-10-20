#!/usr/bin/env python3
"""Audit the selected replacement themes without changing firmware sources."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import tempfile

import theme_memory as memory

ROOT = memory.ROOT
SELECTED = (1, 3, 11, 15, 16, 18, 20)
BLOCK_BYTES = 512


def runs_encode(data):
    """PackBits: literal or repeated-byte runs, each capped at 128 bytes."""
    output = bytearray()
    position = 0
    while position < len(data):
        end = position + 1
        while end < min(position + 128, len(data)) and data[end] == data[position]:
            end += 1
        if end - position >= 3:
            output.extend((257 - (end - position), data[position]))
            position = end
            continue
        first = position
        position = end
        while position < min(first + 128, len(data)):
            end = position + 1
            while end < min(position + 128, len(data)) and data[end] == data[position]:
                end += 1
            if end - position >= 3:
                break
            position = min(end, first + 128)
        output.append(position - first - 1)
        output.extend(data[first:position])
    return bytes(output)


def runs_decode(data):
    output = bytearray()
    position = 0
    while position < len(data):
        control = data[position]
        position += 1
        if control <= 127:
            count = control + 1
            output.extend(data[position:position + count])
            position += count
        elif control >= 129:
            output.extend([data[position]] * (257 - control))
            position += 1
        else:
            raise ValueError("Reserved PackBits control")
    return bytes(output)


def difference(data, stride):
    return bytes(value ^ (data[i - stride] if stride and i >= stride else 0)
                 for i, value in enumerate(data))


def undo_difference(data, stride):
    output = bytearray(data)
    if stride:
        for i in range(stride, len(output)):
            output[i] ^= output[i - stride]
    return bytes(output)


def block_encode(data, row_bytes):
    blocks = []
    for start in range(0, len(data), BLOCK_BYTES):
        raw = data[start:start + BLOCK_BYTES]
        candidates = [b"\0" + raw]
        for stride in {0, 1, row_bytes}:
            if stride <= 127:
                packed = runs_encode(difference(raw, stride))
                candidates.append(bytes((stride + 1,)) + packed)
        blocks.append(min(candidates, key=len))
    offsets = [0]
    for block in blocks:
        offsets.append(offsets[-1] + len(block))
    return (struct.pack("<HH", len(data), len(blocks)) +
            struct.pack("<" + "H" * len(offsets), *offsets) + b"".join(blocks))


def block_decode(encoded):
    length, count = struct.unpack_from("<HH", encoded)
    offsets = struct.unpack_from("<" + "H" * (count + 1), encoded, 4)
    base = 4 + 2 * (count + 1)
    output = bytearray()
    for i in range(count):
        block = encoded[base + offsets[i]:base + offsets[i + 1]]
        mode = block[0]
        decoded = block[1:] if mode == 0 else undo_difference(runs_decode(block[1:]), mode - 1)
        assert len(decoded) <= BLOCK_BYTES
        output.extend(decoded)
    assert len(output) == length
    return bytes(output)


def repack_image(encoded_bitmap, prefix=b""):
    image = memory.decode_bitmap(encoded_bitmap)
    options = []
    for rotation in (None, memory.study.Image.Transpose.ROTATE_90):
        current = image if rotation is None else image.transpose(rotation)
        raw = current.tobytes()
        encoded = block_encode(raw, (current.width + 7) // 8)
        assert block_decode(encoded) == raw
        # One format/rotation byte, native dimensions and metrics are retained.
        options.append(memory.align4(len(prefix) + 5 + len(encoded)))
    existing = memory.align4(len(prefix) + len(encoded_bitmap))
    return min(existing, *options)


def without_old_home_baseline(release):
    """Compile a non-runnable blank-home comparison in a temporary directory."""
    source = (ROOT / "user/src/clock_render.c").read_text()
    expected = 'static const char *themes[THEME_COUNT]={"翻页","卡片","日历","极简","分栏","刻度"};\n'
    assert source.count(expected) == 1
    source = source.replace(expected, "")
    for first, after, replacement in (
        ("static void flip_card(", "static void temperature(", ""),
        ("static void home(", "static void focus_page(", "static void home(const ClockFace *f) { (void)f; }\n"),
    ):
        begin, end = source.index(first), source.index(after)
        source = source[:begin] + replacement + source[end:]
    label = "text(12,130,themes[f->settings.theme],0);"
    assert source.count(label) == 1
    source = source.replace(label, "")
    with tempfile.TemporaryDirectory(prefix="clock-theme-budget-") as temp:
        temp = Path(temp)
        replacement = temp / "clock_render.o"
        flags = ["-mcpu=cortex-m0plus", "-mthumb", "-std=c11", "-Os", "-ffreestanding",
                 "-ffunction-sections", "-fdata-sections", "-fno-common", "-Wall", "-Wextra", "-Werror"]
        command = ["arm-none-eabi-gcc", *flags, "-Iuser/inc", "-Ilib/inc", "-I.cmsis/include",
                   "-x", "c", "-c", "-", "-o", str(replacement)]
        subprocess.run(command, cwd=ROOT, input=source, text=True, check=True, capture_output=True)
        objects = [replacement if item.name == "clock_render.o" else item
                   for item in sorted((release / "user/src").glob("*.o"))]
        objects += [release / "lib/src" / name for name in
                    ("system_cw32l010.o", "cw32l010_gpio.o", "cw32l010_spi.o", "cw32l010_sysctrl.o")]
        objects.append(release / "startup.o")
        target = temp / "blank-home.elf"
        subprocess.run(["arm-none-eabi-gcc", "-mcpu=cortex-m0plus", "-mthumb", "-nostdlib",
                        "-Tlink/CW32L010_FLASH.ld", "-Wl,--gc-sections", *map(str, objects),
                        "-lgcc", "-o", str(target)], cwd=ROOT, check=True, capture_output=True)
        binary = temp / "blank-home.bin"
        subprocess.run(["arm-none-eabi-objcopy", "-O", "binary", str(target), str(binary)], check=True)
        return binary.stat().st_size


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "design/theme-study-20261020/memory/selected.json")
    args = parser.parse_args()
    release = ROOT / "releases/uc8251-six-themes-20261020"
    baseline = (release / "clock.bin").read_bytes()
    previous = json.loads((ROOT / "design/theme-study-20261020/memory/report.json").read_text())
    assert hashlib.sha256(baseline).hexdigest() == previous["baseline_sha256"]
    requests = {}
    resources = {}
    compact = {}
    for number in SELECTED:
        background, fonts = memory.capture_theme(number)
        name = f"background-{number:02d}"
        encoded = memory.verified_bitmap(background)
        resources[name] = memory.align4(len(encoded))
        compact[name] = repack_image(encoded)
        for key, chars in fonts.items():
            requests.setdefault(key, set()).update(chars)
    for key, chars in requests.items():
        name = f"font-{key[0]}-{key[1]}-{key[2]:g}"
        encoded = memory.atlas(*key, chars)
        glyph_count, = struct.unpack_from("<H", encoded)
        offset = 10 + glyph_count * 10
        resources[name] = memory.align4(len(encoded))
        compact[name] = repack_image(encoded[offset:], encoded[:offset])
    if 13 in SELECTED:
        resources["pixel-digit-patterns"] = compact["pixel-digit-patterns"] = 52
    blank = without_old_home_baseline(release)
    allowances = len(SELECTED) * memory.THEME_CODE_ALLOWANCE + memory.COMMON_CODE_ALLOWANCE
    result = {
        "kind": "design-budget-not-final-firmware", "selected": list(SELECTED),
        "firmware_modified": False, "flashed": False,
        "baseline_flash_bytes": len(baseline), "blank_home_comparison_flash_bytes": blank,
        "old_home_and_names_reclaimed_bytes": len(baseline) - blank,
        "application_limit_bytes": memory.APPLICATION_LIMIT,
        "free_after_blank_home_bytes": memory.APPLICATION_LIMIT - blank,
        "previous_codec_selected_asset_bytes": sum(resources.values()),
        "compact_codec_selected_asset_bytes": sum(compact.values()),
        "layout_and_common_code_allowance_bytes": allowances,
        "flash_headroom_bytes": memory.FLASH_HEADROOM,
        "compact_total_application_budget_bytes": blank + sum(compact.values()) + allowances,
        "compact_budget_with_headroom_bytes": blank + sum(compact.values()) + allowances + memory.FLASH_HEADROOM,
        "compact_shared_scratch_buffer_bytes": BLOCK_BYTES,
        "resources": {key: {"previous_bytes": size, "compact_bytes": compact[key]}
                      for key, size in resources.items()},
        "caveats": [
            "Blank-home comparison removes old home layouts and names, not menu/editor/focus fonts; it is not runnable firmware.",
            "Unneeded small Chinese glyphs are still retained, so this does not credit their possible removal.",
            f"PackBits with per-block XOR prediction is lossless and block-decoding is bounded to {BLOCK_BYTES} output bytes.",
            "Code allowances are estimates, not an integrated ARM build; final Flash/RAM/refresh speed still need verification.",
        ],
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n")
    print(json.dumps({key: value for key, value in result.items() if key != "resources"}, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
