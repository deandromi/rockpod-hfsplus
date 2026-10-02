#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
set -eu
src=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
for program in arm-elf-eabi-gcc arm-elf-eabi-cpp arm-elf-eabi-ld arm-elf-eabi-ar arm-elf-eabi-objcopy make perl python3 zip gcc; do
    if ! command -v "$program" >/dev/null 2>&1; then
        echo "Ontbreekt: $program. Installeer de Rockbox ARM-toolchain en voeg die toe aan PATH." >&2
        exit 1
    fi
done
version=$(arm-elf-eabi-gcc -dumpfullversion)
if [ "$version" != 9.5.0 ]; then
    echo "Deze Rockpod-bronboom verwacht GCC 9.5.0; gevonden: $version" >&2
    exit 1
fi
case ${JOBS:-2} in *[!0-9]*|' '|''|0) echo 'JOBS moet een positief geheel getal zijn.' >&2; exit 1;; esac
build=$(mktemp -d "$src/build-hfsplus-ipod6g.XXXXXX")
export VERSION=rockpod-hfs-ro-test
mkdir "$build/firmware" "$build/bootloader" "$build/output"
echo "Bouwmap: $build"
(
    cd "$build/firmware"
    sh "$src/tools/configure" --target=ipod6g --type=n
    make -j"${JOBS:-2}"
    make zip
    test -s rockbox.zip
    test -s rockbox.ipod
)
(
    cd "$build/bootloader"
    sh "$src/tools/configure" --target=ipod6g --type=b
    make -j"${JOBS:-2}"
    test -s bootloader-ipod6g.ipod
)
cp "$build/firmware/rockbox.zip" "$build/output/"
cp "$build/bootloader/bootloader-ipod6g.ipod" "$build/output/"
echo "Gebouwd (niet op hardware getest): $build/output"
echo 'Dit script installeert of flasht niets.'
