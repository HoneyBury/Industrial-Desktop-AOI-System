#!/usr/bin/env python3
"""Generate synthetic AOI test images for unit and integration fixtures."""

from pathlib import Path


def main() -> None:
    fixtures = Path("tests/data")
    fixtures.mkdir(parents=True, exist_ok=True)
    print(f"[bootstrap] test fixture directory ready: {fixtures}")


if __name__ == "__main__":
    main()

