#!/bin/sh
set -eu
project=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
cd "$project"
if test -f MarrowOS.ext4; then exit 0; fi
if test -f artifacts/MarrowOS-runtime.tar.gz; then
    tar --strip-components=1 -xzf artifacts/MarrowOS-runtime.tar.gz MarrowOS/MarrowOS.ext4
else
    cat artifacts/MarrowOS-runtime.tar.gz.part-* | tar --strip-components=1 -xzf - MarrowOS/MarrowOS.ext4
fi
