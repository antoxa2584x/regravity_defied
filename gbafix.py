#!/usr/bin/env python3
"""Patch a GBA ROM binary with the Nintendo logo and valid header checksum.

The GBA BIOS verifies the 156-byte Nintendo logo at offset 0x04 and the
complement checksum at 0xBD before booting. Without them the ROM runs in
emulators but silently locks up on real hardware.
"""
import os
import sys

# 156-byte compressed Nintendo logo bitmap (verified from devkitPro gbafix.c)
# Source: https://raw.githubusercontent.com/devkitPro/gba-tools/master/src/gbafix.c
NINTENDO_LOGO = bytes([
    0x24,0xFF,0xAE,0x51,0x69,0x9A,0xA2,0x21,0x3D,0x84,0x82,0x0A,0x84,0xE4,0x09,0xAD,
    0x11,0x24,0x8B,0x98,0xC0,0x81,0x7F,0x21,0xA3,0x52,0xBE,0x19,0x93,0x09,0xCE,0x20,
    0x10,0x46,0x4A,0x4A,0xF8,0x27,0x31,0xEC,0x58,0xC7,0xE8,0x33,0x82,0xE3,0xCE,0xBF,
    0x85,0xF4,0xDF,0x94,0xCE,0x4B,0x09,0xC1,0x94,0x56,0x8A,0xC0,0x13,0x72,0xA7,0xFC,
    0x9F,0x84,0x4D,0x73,0xA3,0xCA,0x9A,0x61,0x58,0x97,0xA3,0x27,0xFC,0x03,0x98,0x76,
    0x23,0x1D,0xC7,0x61,0x03,0x04,0xAE,0x56,0xBF,0x38,0x84,0x00,0x40,0xA7,0x0E,0xFD,
    0xFF,0x52,0xFE,0x03,0x6F,0x95,0x30,0xF1,0x97,0xFB,0xC0,0x85,0x60,0xD6,0x80,0x25,
    0xA9,0x63,0xBE,0x03,0x01,0x4E,0x38,0xE2,0xF9,0xA2,0x34,0xFF,0xBB,0x3E,0x03,0x44,
    0x78,0x00,0x90,0xCB,0x88,0x11,0x3A,0x94,0x65,0xC0,0x7C,0x63,0x87,0xF0,0x3C,0xAF,
    0xD6,0x25,0xE4,0x8B,0x38,0x0A,0xAC,0x72,0x21,0xD4,0xF8,0x07,
])

assert len(NINTENDO_LOGO) == 156, f"logo must be 156 bytes, got {len(NINTENDO_LOGO)}"


def fix_gba(path, title=None, code=None, version=None, maker=None):
    with open(path, 'rb') as f:
        rom = bytearray(f.read())

    if len(rom) < 0xC0:
        rom.extend(b'\x00' * (0xC0 - len(rom)))

    # Write Nintendo logo at header offset 0x04
    rom[0x04:0x04 + 156] = NINTENDO_LOGO

    # Write Game Title at header offset 0xA0 (max 12 chars)
    if title:
        title_bytes = title.upper().encode('ascii')[:12]
        rom[0xA0:0xA0 + len(title_bytes)] = title_bytes
        # Pad with zeros if shorter than 12
        if len(title_bytes) < 12:
            rom[0xA0 + len(title_bytes):0xA0 + 12] = b'\x00' * (12 - len(title_bytes))

    # Write Game Code at header offset 0xAC (4 chars)
    if code:
        code_bytes = code.upper().encode('ascii')[:4]
        rom[0xAC:0xAC + len(code_bytes)] = code_bytes
        # Pad with zeros if shorter than 4
        if len(code_bytes) < 4:
            rom[0xAC + len(code_bytes):0xAC + 4] = b'\x00' * (4 - len(code_bytes))

    # Write Maker Code at header offset 0xB0 (2 chars)
    if maker:
        maker_bytes = maker.upper().encode('ascii')[:2]
        rom[0xB0:0xB0 + len(maker_bytes)] = maker_bytes
        if len(maker_bytes) < 2:
            rom[0xB0 + len(maker_bytes):0xB0 + 2] = b'\x00' * (2 - len(maker_bytes))
    else:
        # Default to "01" (Nintendo) or "ZZ" if not specified? 
        # Many homebrews use "00" or "01". Let's use "00" as default if empty.
        if rom[0xB0:0xB2] == b'\x00\x00':
             rom[0xB0:0xB2] = b'00'

    # Fixed value at 0xB2 (must be 0x96)
    rom[0xB2] = 0x96

    # Write Software version at header offset 0xBC (1 byte)
    if version is not None:
        try:
            # Handle "0.9" -> 9, "1.2" -> 12, etc.
            if isinstance(version, str) and '.' in version:
                v = int(version.replace('.', '')) & 0xFF
            else:
                v = int(float(version)) & 0xFF
            rom[0xBC] = v
        except ValueError:
            pass

    # Complement checksum: -(sum(bytes[0xA0..0xBC]) + 0x19) & 0xFF
    checksum = sum(rom[0xA0:0xBD]) & 0xFF
    rom[0xBD] = (-(checksum + 0x19)) & 0xFF

    tmp = path + '.tmp'
    with open(tmp, 'wb') as f:
        f.write(rom)
    os.replace(tmp, path)

    msg = f"gbafix: {path}  logo=OK  checksum=0x{rom[0xBD]:02X}"
    if title: msg += f" title='{title[:12].upper()}'"
    if code: msg += f" code='{code[:4].upper()}'"
    if maker: msg += f" maker='{maker[:2].upper()}'"
    if version is not None: msg += f" version={rom[0xBC]}"
    print(msg)


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description='Fix GBA ROM header')
    parser.add_argument('rom', help='Path to GBA ROM file')
    parser.add_argument('-t', '--title', help='Game title (max 12 characters)')
    parser.add_argument('-c', '--code', help='Game code (4 characters)')
    parser.add_argument('-m', '--maker', help='Maker code (2 characters)')
    parser.add_argument('-v', '--version', help='Software version (0-255)')
    
    args = parser.parse_args()
    fix_gba(args.rom, args.title, args.code, args.version, args.maker)
