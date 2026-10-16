#!/usr/bin/env python3
"""Build a Project Oberon file-system image for the kernel's RAM disk.

Usage: mkpodisk.py OUTPUT NSECTORS NAME=PATH [NAME=PATH ...]

Layout follows FileDir.Mod / Files.Mod (Project Oberon 2013), little-endian:
  * Sector numbers are logical 1 KiB sectors; on-disk "disk addresses" are
    sector * 29 (Kernel.GetSector divides by 29).
  * Sectors 0..63 are reserved by Kernel.InitSecMap; the root directory page
    is at disk address DirRootAdr = 29 (sector 1).
  * A DirPage is {mark, m, p0: INTEGER; fill: 52 bytes; e: 24 x DirEntry},
    DirEntry = {name: 32 chars; adr, p: INTEGER}.
  * A file's first sector is its header {mark, name[32], aleng, bleng, date,
    ext[12], sec[64]} (352 bytes, HeaderSize) followed by the first
    1024 - 352 bytes of data. aleng * 1024 + bleng = length + 352, and sec[i]
    holds the disk address of the file's i-th sector (sec[0] = the header).
"""
import struct
import sys

SECTOR = 1024
HEADER_SIZE = 352
FN_LENGTH = 32
SEC_TAB_SIZE = 64
DIR_PG_SIZE = 24
DIR_MARK = 0x9B1EA38D
HEADER_MARK = 0x9BA71D86
ROOT_SECTOR = 1
FIRST_FILE_SECTOR = 2


def dir_page(entries):
    page = struct.pack("<IiI", DIR_MARK, len(entries), 0) + bytes(52)
    for name, adr in entries:
        page += name.encode("ascii").ljust(FN_LENGTH, b"\0") + struct.pack("<ii", adr, 0)
    return page.ljust(SECTOR, b"\0")


def main():
    if len(sys.argv) < 4:
        sys.exit(__doc__)
    out, nsectors = sys.argv[1], int(sys.argv[2])
    files = sorted(arg.split("=", 1) for arg in sys.argv[3:])   # B-tree page order
    if len(files) > DIR_PG_SIZE:
        sys.exit("mkpodisk: at most %d files fit in the root page" % DIR_PG_SIZE)

    disk = bytearray(nsectors * SECTOR)
    next_sector = FIRST_FILE_SECTOR
    entries = []
    for name, path in files:
        if len(name) >= FN_LENGTH:
            sys.exit("mkpodisk: file name too long: " + name)
        data = open(path, "rb").read()
        total = HEADER_SIZE + len(data)
        aleng, bleng = divmod(total, SECTOR)
        if bleng == 0 and aleng > 0:
            aleng, bleng = aleng - 1, SECTOR
        nsec = aleng + 1
        if nsec > SEC_TAB_SIZE:
            sys.exit("mkpodisk: %s needs extension sectors (unsupported)" % name)
        secs = list(range(next_sector, next_sector + nsec))
        next_sector += nsec
        if next_sector > nsectors:
            sys.exit("mkpodisk: image too small")

        header = struct.pack("<I", HEADER_MARK) + name.encode("ascii").ljust(FN_LENGTH, b"\0")
        header += struct.pack("<iii", aleng, bleng, 0)
        header += bytes(12 * 4)                                   # extension table
        header += struct.pack("<%di" % SEC_TAB_SIZE,
                              *([s * 29 for s in secs] + [0] * (SEC_TAB_SIZE - nsec)))
        assert len(header) == HEADER_SIZE
        content = header + data
        for i, s in enumerate(secs):
            chunk = content[i * SECTOR:(i + 1) * SECTOR]
            disk[s * SECTOR:s * SECTOR + len(chunk)] = chunk
        entries.append((name, secs[0] * 29))

    disk[ROOT_SECTOR * SECTOR:(ROOT_SECTOR + 1) * SECTOR] = dir_page(entries)
    with open(out, "wb") as f:
        f.write(disk)


if __name__ == "__main__":
    main()
