#!/usr/bin/python3

import os
import sys
from pathlib import Path


# Files that should be included
SUPPORTED_EXTENSIONS = {
    ".html",
    ".js",
    ".css",
    ".elf",
    ".appcache",
}


def make_gz_name(relative_path):
    """
    Convert a relative web path to the corresponding gzip array name.

    Examples:
        index.html                  -> index_gz
        1.00.js                     -> o1_00_gz
        psfree/alert.js             -> alert_gz
        psfree/psfree.js            -> psfreeo_gz
        psfree/module/int64.js      -> int64m_gz
        psfree/module/offset.js     -> offset_gz
        main.css                    -> main_gz
        main.js                     -> mainj_gz
        elfldr.elf                  -> elfldr_gz
    """

    path = Path(relative_path)
    name = path.name

    # Remove extension
    stem = path.stem

    # Replace dots / dashes / spaces with underscore
    stem = stem.replace(".", "_")
    stem = stem.replace("-", "_")
    stem = stem.replace(" ", "_")

    # Version files:
    # 1.00.js -> o1_00_gz
    # 2.25.js -> o2_25_gz
    if path.suffix.lower() == ".js" and stem and stem[0].isdigit():
        stem = "o" + stem

    # Special names matching your existing generated gzip arrays
    normalized = relative_path.replace("\\", "/").lstrip("/")

    special_names = {
        "psfree/psfree.js": "psfreeo_gz",
        "psfree/module/int64.js": "int64m_gz",
        "main.js": "mainj_gz",
    }

    if normalized in special_names:
        return special_names[normalized]

    return stem + "_gz"


def collect_files(root):
    """
    Recursively find supported files.
    """

    files = []

    for path in root.rglob("*"):
        if not path.is_file():
            continue

        # Don't include generated gzip/header files
        if path.suffix.lower() not in SUPPORTED_EXTENSIONS:
            continue

        files.append(path)

    return files


def sort_files(root, files):
    """
    Sort in roughly the same order as your current asset table:

        index.html
        1.xx.js
        2.xx.js
        3.xx.js
        4.xx.js
        5.xx.js
        psfree/...
        everything else
    """

    def sort_key(path):
        rel = path.relative_to(root).as_posix()
        name = path.name

        # index.html first
        if rel == "index.html":
            return (0, 0, "")

        # Version JS files: 1.xx, 2.xx, etc.
        if path.suffix.lower() == ".js":
            stem = path.stem

            if stem and stem[0].isdigit():
                try:
                    parts = stem.split(".")
                    major = int(parts[0])
                    minor = int(parts[1]) if len(parts) > 1 else 0
                    return (1, major, minor, rel)
                except ValueError:
                    pass

        # psfree directory next
        if rel.startswith("psfree/"):
            return (2, 0, 0, rel)

        # Everything else
        return (3, 0, 0, rel)

    return sorted(files, key=sort_key)


def generate_header(root, files, output_file):
    """
    Generate S00...Sxx and gzAssets[].
    """

    output = []

    for index, path in enumerate(files):

        rel = path.relative_to(root).as_posix()
        web_path = "/" + rel

        output.append(
            f'static const char S{index:02d}[] PROGMEM = "{web_path}";'
        )
        output.append("")

    output.append(
        "static const GzAsset gzAssets[] PROGMEM = {"
    )
    output.append("")

    for index, path in enumerate(files):

        rel = path.relative_to(root).as_posix()
        gz_name = make_gz_name(rel)

        output.append(
            f"  {{ S{index:02d}, {gz_name:<22}, sizeof({gz_name}) }},"
        )

    output.append("")
    output.append("};")
    output.append("")
    output.append(
        "static const uint8_t GZ_ASSET_COUNT = "
        "sizeof(gzAssets) / sizeof(gzAssets[0]);"
    )
    output.append("")

    with open(output_file, "w", encoding="utf-8") as f:
        f.write("\n".join(output))


def main():

    # Optional root directory:
    #
    #   python3 gen_assets.py
    #   python3 gen_assets.py ./www
    #
    if len(sys.argv) > 1:
        root = Path(sys.argv[1]).resolve()
    else:
        root = Path.cwd().resolve()

    if not root.is_dir():
        sys.exit(f"Directory not found: {root}")

    print(f"Scanning: {root}")

    files = collect_files(root)
    files = sort_files(root, files)

    if not files:
        sys.exit("No supported files found.")

    output_file = root / "gz_assets.h"

    generate_header(root, files, output_file)

    print()
    print(f"Found {len(files)} files")
    print(f"Generated: {output_file}")
    print()

    for i, path in enumerate(files):
        rel = path.relative_to(root).as_posix()
        gz_name = make_gz_name(rel)

        print(
            f"S{i:02d}: /{rel:<35} -> {gz_name}"
        )


if __name__ == "__main__":
    main()