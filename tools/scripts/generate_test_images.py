#!/usr/bin/env python3
"""生成 AOI 合成测试图像的脚本占位入口。"""

from pathlib import Path


def main() -> None:
    fixtures = Path("tests/data")
    fixtures.mkdir(parents=True, exist_ok=True)
    print(f"[bootstrap] test fixture directory ready: {fixtures}")


if __name__ == "__main__":
    main()
