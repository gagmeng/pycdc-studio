from __future__ import annotations

import py_compile
from pathlib import Path


def main() -> None:
    root = Path(__file__).resolve().parent
    source_files = sorted(
        path for path in root.glob("*.py")
        if path.name not in {"compile_test_samples.py"}
    )

    if not source_files:
        print("No test sample files were found.")
        return

    print(f"Compiling {len(source_files)} test sample(s) into __pycache__...")
    for source_path in source_files:
        py_compile.compile(str(source_path), doraise=True)
        print(f"  - {source_path.name}")

    print("Done.")


if __name__ == "__main__":
    main()
