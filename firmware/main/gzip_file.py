"""Compresses a file for embedding. The output is byte-for-byte repeatable between builds."""
import gzip
import sys
from pathlib import Path


def main() -> None:
    source, target = Path(sys.argv[1]), Path(sys.argv[2])
    target.write_bytes(gzip.compress(source.read_bytes(), compresslevel=9, mtime=0))


if __name__ == "__main__":
    main()
