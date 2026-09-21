#!/usr/bin/env python3
"""Write a 4 KiB Linux swap v1 header for a raw swap partition (what mkswap would write).

The partition is flashed with this image once (fastboot flash swap swap.img); the kernel then
accepts it with swapon, so no mkswap has to run on the device. Page size 4096 (this kernel is
the 4k-page GKI build).

usage: gen-swap-image.py <partition-size-MiB> <out>     e.g. 8192 swap.img
"""
import struct
import sys
import uuid

PAGE = 4096
MARGIN_PAGES = 256  # 1 MiB slack: never claim the very last pages of the partition

size_mib = int(sys.argv[1])
pages = size_mib * 1024 * 1024 // PAGE
last_page = pages - 1 - MARGIN_PAGES

hdr = bytearray(PAGE)
# union swap_header { struct { char reserved[PAGE - 10]; char magic[10]; } magic;
#                     struct { char bootbits[1024]; u32 version, last_page, nr_badpages;
#                              u8 uuid[16]; char volume_name[16]; u32 padding[117];
#                              u32 badpages[1]; } info; }
struct.pack_into("<III", hdr, 1024, 1, last_page, 0)
hdr[1036:1052] = uuid.UUID("5a7d2d10-0b3e-4c6a-9f5e-6d0f3a1e2b44").bytes
hdr[1052:1068] = b"vim3-swap".ljust(16, b"\0")
hdr[PAGE - 10:PAGE] = b"SWAPSPACE2"

with open(sys.argv[2], "wb") as f:
    f.write(hdr)
print(f"{sys.argv[2]}: version 1, {pages} pages, last_page {last_page}")
