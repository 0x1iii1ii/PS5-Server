import sys
import gzip
import shutil
from pathlib import Path

SUPPORTED_EXTENSIONS = {
    ".elf", ".bin",
    ".html", ".htm",
    ".js", ".css",
    ".json", ".xml",
    ".svg", ".ico",
    ".txt", ".manifest",".appcache",
}

def compress_file(file_path):
    file_path = Path(file_path)

    if not file_path.exists():
        print(f"File not found: {file_path}")
        return

    if file_path.suffix.lower() not in SUPPORTED_EXTENSIONS:
        print(f"Skipping unsupported file: {file_path.name}")
        return

    gz_path = file_path.with_suffix(file_path.suffix + ".gz")

    print(f"Compressing:")
    print(f"  {file_path}")
    print(f"  -> {gz_path}")

    try:
        with file_path.open("rb") as source:
            with gzip.open(gz_path, "wb", compresslevel=9) as target:
                shutil.copyfileobj(source, target)

        orig = file_path.stat().st_size
        comp = gz_path.stat().st_size
        ratio = (1 - comp / orig) * 100 if orig else 0
        print(f"Done: {orig} B -> {comp} B ({ratio:.1f}% saved)")

    except Exception as e:
        print(f"ERROR: {e}")

    print()


def main():
    if len(sys.argv) < 2:
        print("Drag and drop file(s) onto this script.")
        print(f"Supported: {', '.join(sorted(SUPPORTED_EXTENSIONS))}")
        input("Press Enter to exit...")
        return

    for filename in sys.argv[1:]:
        compress_file(filename)

    input("Press Enter to exit...")


if __name__ == "__main__":
    main()