#!/usr/bin/env python3
"""Dataset collection bootstrap script for AOI image capture."""

from pathlib import Path


def main() -> None:
    output = Path("dataset/raw")
    output.mkdir(parents=True, exist_ok=True)
    print(f"[bootstrap] dataset output ready: {output}")


if __name__ == "__main__":
    main()

