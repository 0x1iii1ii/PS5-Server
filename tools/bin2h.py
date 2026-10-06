#!/usr/bin/python3

import os
import sys
import gzip


SUPPORTED_EXTENSIONS = ('.bin', '.elf', '.js', '.css', '.html', '.appcache')


if len(sys.argv) < 2:
    sys.exit(
        "file required\n\n"
        "Example: bin2h payload.bin payload.elf script.js"
    )


def get_filename(filename):
    sfilename = os.path.basename(filename)

    for ext in SUPPORTED_EXTENSIONS:
        if sfilename.lower().endswith(ext):
            sfilename = sfilename[:-len(ext)]
            break

    sfilename = sfilename.replace('.', '_')
    sfilename = sfilename.replace('-', '_')
    sfilename = sfilename.replace(' ', '_')

    return sfilename


all_dat = ""


# Process every dropped file
for filename in sys.argv[1:]:

    if not os.path.isfile(filename):
        print("Skipping: " + filename)
        continue

    ext = os.path.splitext(filename)[1].lower()

    if ext not in SUPPORTED_EXTENSIONS:
        print("Skipping unsupported file: " + filename)
        continue

    print("Processing: " + filename)

    # Read input file
    with open(filename, 'rb') as f:
        bindat = f.read()

    # Create temporary gzip file
    gzfilename = filename + ".gz"

    with gzip.open(gzfilename, 'wb') as f:
        f.write(bindat)

    sfilename = get_filename(filename)

    # Start C array
    tmpdat = (
        "static const uint8_t "
        + sfilename
        + "_gz[] PROGMEM = {\n"
    )

    cnt = 0

    # Read gzip file
    with open(gzfilename, 'rb') as f:
        chnk = f.read(1)

        while chnk:
            if cnt == 31:
                cnt = 0
                tmpdat += "%s,\n" % ord(chnk)
            else:
                tmpdat += "%s, " % ord(chnk)

            cnt += 1
            chnk = f.read(1)

    # Remove trailing comma/space/newline
    if tmpdat.endswith(","):
        tmpdat = tmpdat[:-1]
    elif tmpdat.endswith(", "):
        tmpdat = tmpdat[:-2]
    elif tmpdat.endswith(",\n"):
        tmpdat = tmpdat[:-2]

    tmpdat += "\n};\n\n"

    # Add to combined header
    all_dat += tmpdat

    # Delete temporary gzip file
    if os.path.exists(gzfilename):
        os.remove(gzfilename)


# Output file beside the first input file
output_dir = os.path.dirname(os.path.abspath(sys.argv[1]))
output_file = os.path.join(output_dir, "firmware_gz.h")


# Remove old output
if os.path.exists(output_file):
    os.remove(output_file)


# Write combined header
with open(output_file, 'w+', encoding="utf-8") as f:
    f.write(all_dat)


print("\nGenerated:")
print(output_file)
