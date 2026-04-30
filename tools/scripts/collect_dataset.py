#!/usr/bin/env python3
"""AOI 图像采集脚本占位入口。"""

from pathlib import Path


def main() -> None:
    output = Path("dataset/raw")
    output.mkdir(parents=True, exist_ok=True)
    print(f"[bootstrap] dataset output ready: {output}")


if __name__ == "__main__":
    main()
