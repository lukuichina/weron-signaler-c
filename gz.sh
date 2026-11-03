#!/bin/bash
set -e
cd /root/github/weron-signaler-c
mkdir -p gz

# Each build-* dir contains exactly one binary.
# Names vary: weron-signaler-c, weron-signaler-c-armhf, weron-signaler-c-mipsel, etc.

for dir in build-linux-aarch64-musl-static \
           build-linux-armv7-musleabihf-static \
           build-linux-i686-musl-static \
           build-linux-mips-be-musl-static \
           build-linux-x86_64-musl-static \
           build-linux \
           build-mipsel-musl-sf \
           build-mipsel \
           build-mipsisa64r6 \
           build-mipsisa64r6el \
           build-armhf \
           build-win-i686-musl \
           build-win-x64-musl \
           build-win; do
    bin=$(ls "$dir"/weron-signaler-c* 2>/dev/null | head -1)
    if [ -n "$bin" ]; then
        tar cvfz "gz/${dir}.tar.gz" -C "$dir" "$(basename "$bin")"
    fi
done

echo "--- done ---"
ls -lh gz/