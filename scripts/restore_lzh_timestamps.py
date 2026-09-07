"""Restore modification times from Human68k's level-2 LZH headers.

unar can swap the two 32-bit halves of extended Windows FILETIME values.
This helper reads each value as one little-endian 64-bit integer instead.
It changes timestamps only; extraction remains the responsibility of unar.
"""
import os
from pathlib import Path
import struct
import sys


def modification_times(data):
    offset = 0
    entries = []
    while offset < len(data):
        if data[offset:] == b"\x00":
            break
        if offset + 26 > len(data) or data[offset + 20] != 2:
            raise ValueError("Expected an LZH level-2 header")
        size = struct.unpack_from("<H", data, offset)[0]
        packed = struct.unpack_from("<I", data, offset + 7)[0]
        end = offset + size
        if size < 26 or end + packed > len(data):
            raise ValueError("Invalid LZH entry bounds")
        mtime_ns = struct.unpack_from("<I", data, offset + 15)[0] * 1_000_000_000
        name = None
        pos = offset + 24
        while True:
            if pos + 2 > end:
                raise ValueError("Missing extended-header terminator")
            ext_size = struct.unpack_from("<H", data, pos)[0]
            if ext_size == 0:
                break
            if ext_size < 3 or pos + ext_size > end:
                raise ValueError("Invalid extended-header bounds")
            kind = data[pos + 2]
            payload = data[pos + 3:pos + ext_size]
            if kind == 0x01:
                name = payload.decode("shift_jis")
            elif kind == 0x02:
                raise ValueError("Directory entries are not supported by this helper")
            elif kind == 0x41:
                if len(payload) != 24:
                    raise ValueError("Invalid Windows timestamp extension")
                # Creation, modification, access: three little-endian FILETIMEs.
                ticks = struct.unpack_from("<Q", payload, 8)[0]
                mtime_ns = (ticks - 116_444_736_000_000_000) * 100
            elif kind == 0x54:
                if len(payload) != 4:
                    raise ValueError("Invalid Unix timestamp extension")
                mtime_ns = struct.unpack("<I", payload)[0] * 1_000_000_000
            pos += ext_size
        if not name or name in (".", "..") or any(c in name for c in "/\\:\x00"):
            raise ValueError("Expected a plain archive filename")
        entries.append((name, mtime_ns))
        offset = end + packed
    if not entries:
        raise ValueError("No archive entries found")
    return entries


def main():
    if len(sys.argv) != 3:
        raise SystemExit("Usage: restore_lzh_timestamps.py ARCHIVE EXTRACTED_DIRECTORY")
    archive, directory = map(Path, sys.argv[1:])
    root = directory.resolve(strict=True)
    pending = []
    for name, mtime_ns in modification_times(archive.read_bytes()):
        path = root / name
        if path.is_symlink() or path.resolve(strict=True).parent != root or not path.is_file():
            raise ValueError("Expected a regular extracted file: " + name)
        pending.append((path, path.stat().st_atime_ns, mtime_ns))
    for path, atime_ns, mtime_ns in pending:
        os.utime(path, ns=(atime_ns, mtime_ns))
        print("Restored modification time: " + path.name)


if __name__ == "__main__":
    main()