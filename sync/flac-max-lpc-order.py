#!/usr/bin/env python3
"""flac-max-lpc-order.py <file.flac> [max_frames]

Prints the highest predictor order (LPC or fixed) used by the first
max_frames (default 2000, ~3 min at 44.1 kHz) frames of a FLAC file.

The FLAC "streamable subset" caps the LPC order at 12 for <= 48 kHz audio,
and the VS1053b plugin can't decode files beyond it in real time: encoders
run past the subset (e.g. ffmpeg's -compression_level 9..12, or flac
--lax) use orders up to 32, roughly 2.5x the decode work per sample. Those
files play choppy on the device even though data reaches the chip in time.

Only frame and subframe headers are parsed (no decoding): each frame header
is located by its sync code and validated with its CRC-8. Prints 0 when the
file can't be parsed.
"""
import sys

READ_LIMIT = 32 * 1024 * 1024


def crc8(data):
    c = 0
    for x in data:
        c ^= x
        for _ in range(8):
            c = ((c << 1) ^ 0x07) & 0xFF if c & 0x80 else (c << 1) & 0xFF
    return c


def audio_start(d):
    pos = 0
    if d[:3] == b"ID3":
        pos = 10 + ((d[6] << 21) | (d[7] << 14) | (d[8] << 7) | d[9])
    if d[pos:pos + 4] != b"fLaC":
        return None
    pos += 4
    while pos + 4 <= len(d):
        header = d[pos]
        pos += 4 + int.from_bytes(d[pos + 1:pos + 4], "big")
        if header & 0x80:
            return pos
    return None


def max_order(d, max_frames):
    k = audio_start(d)
    if k is None:
        return 0
    best = 0
    frames = 0
    while frames < max_frames:
        k = d.find(b"\xff\xf8", k)
        if k < 0 or k + 16 > len(d):
            break
        block_code = d[k + 2] >> 4
        rate_code = d[k + 2] & 0x0F
        q = k + 4
        # UTF-8 coded frame number: count leading ones of the first byte.
        first, extra, mask = d[q], 0, 0x80
        while first & mask and extra < 7:
            extra += 1
            mask >>= 1
        q += 1 + (extra - 1 if extra > 1 else 0)
        q += {6: 1, 7: 2}.get(block_code, 0)
        q += {12: 1, 13: 2, 14: 2}.get(rate_code, 0)
        if q >= len(d) or crc8(d[k:q]) != d[q]:
            k += 1  # false sync inside audio data
            continue
        subframe_type = (d[q + 1] >> 1) & 0x3F
        if subframe_type >= 32:
            best = max(best, (subframe_type & 31) + 1)  # LPC
        elif 8 <= subframe_type < 16:
            best = max(best, subframe_type & 7)  # fixed
        frames += 1
        k = q + 1
    return best


def main():
    if len(sys.argv) < 2:
        print(__doc__, file=sys.stderr)
        return 2
    max_frames = int(sys.argv[2]) if len(sys.argv) > 2 else 2000
    try:
        with open(sys.argv[1], "rb") as f:
            data = f.read(READ_LIMIT)
    except OSError:
        print(0)
        return 0
    print(max_order(data, max_frames))
    return 0


if __name__ == "__main__":
    sys.exit(main())
