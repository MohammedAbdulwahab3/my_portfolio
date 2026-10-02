#!/usr/bin/env python3
"""Single-threaded pure-Python baseline, in the style of a Volatility plugin.

Does the same work as `memforge scan -p strings,netioc -j 1` (ASCII +
UTF-16LE strings, URLs, IPv4) using Python's C-implemented `re` module over a
memory-mapped image - i.e. a *favourable* Python baseline: real Volatility
plugins add layer translation and per-object Python overhead on top.
"""
import mmap
import re
import sys
import time

ASCII = re.compile(rb"[\t\x20-\x7e]{6,}")
WIDE = re.compile(rb"(?:[\t\x20-\x7e]\x00){6,}")
URL = re.compile(rb"(?<![A-Za-z0-9])(?:https?|ftp|hxxps?)://[A-Za-z0-9\-._~:/?#\[\]@!$&'()*+,;=%]+", re.I)
IPV4 = re.compile(rb"(?<![0-9.A-Za-z])(?:(?:25[0-5]|2[0-4]\d|1?\d?\d)\.){3}(?:25[0-5]|2[0-4]\d|1?\d?\d)(?![0-9A-Za-z])")

def main(path):
    with open(path, "rb") as f:
        mm = mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ)
        t0 = time.perf_counter()
        counts = {}
        for name, rx in (("ascii", ASCII), ("utf16le", WIDE), ("url", URL), ("ipv4", IPV4)):
            n = 0
            for m in rx.finditer(mm):
                _ = (m.start(), m.group())  # materialize like a plugin would
                n += 1
            counts[name] = n
        dt = time.perf_counter() - t0
        size = len(mm)
    print(f"python-baseline {path}: {size / 2**20:.0f} MiB in {dt:.2f} s "
          f"({size / dt / 1e9:.3f} GB/s) {counts}")

if __name__ == "__main__":
    main(sys.argv[1])
