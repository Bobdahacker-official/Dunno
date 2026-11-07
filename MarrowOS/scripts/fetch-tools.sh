#!/bin/sh
set -eu
project=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
: "${MARROW_WORK:=$project/build}"
mkdir -p "$MARROW_WORK/apt/lists/partial" "$MARROW_WORK/apt/cache/archives/partial" "$MARROW_WORK/tools" "$MARROW_WORK/hostinclude" "$MARROW_WORK/kernel"
printf '%s\n' 'deb https://deb.debian.org/debian trixie main non-free-firmware' > "$MARROW_WORK/apt/sources.list"
touch "$MARROW_WORK/apt/status"
apt_run() {
 /usr/bin/apt-get -o "Dir::Etc::sourcelist=$MARROW_WORK/apt/sources.list" -o Dir::Etc::sourceparts=- \
  -o "Dir::State::lists=$MARROW_WORK/apt/lists" -o "Dir::State::status=$MARROW_WORK/apt/status" \
  -o "Dir::Cache=$MARROW_WORK/apt/cache" -o "APT::Sandbox::User=$(id -un)" "$@"
}
apt_run update
apt_run --download-only --no-install-recommends -y install linux-source-6.12 flex bison bc libelf-dev libssl-dev busybox-static qemu-system-x86 qemu-utils e2fsprogs cpio grub-pc-bin grub-common xorriso iw wpasupplicant wireless-regdb tcc libtcc-dev
for package in "$MARROW_WORK/apt/cache/archives/"*.deb; do dpkg-deb -x "$package" "$MARROW_WORK/tools"; done
cp "$MARROW_WORK/tools/usr/include/libelf.h" "$MARROW_WORK/tools/usr/include/gelf.h" "$MARROW_WORK/hostinclude/"
cp -r "$MARROW_WORK/tools/usr/include/elfutils" "$MARROW_WORK/hostinclude/"
cp "$MARROW_WORK/tools/usr/share/seabios/"*.bin "$MARROW_WORK/tools/usr/share/qemu/"
tar -xJf "$MARROW_WORK/tools/usr/src/linux-source-6.12.tar.xz" -C "$MARROW_WORK/kernel" --strip-components=1
cat > "$MARROW_WORK/environment.sh" <<EOF_ENV
export MARROW_WORK='$MARROW_WORK'
export TOOLS_ROOT='$MARROW_WORK/tools'
export KERNEL_SRC='$MARROW_WORK/kernel'
export PATH='$MARROW_WORK/tools/usr/bin:$MARROW_WORK/tools/usr/sbin:$MARROW_WORK/tools/sbin':\$PATH
export LD_LIBRARY_PATH='$MARROW_WORK/tools/usr/lib/x86_64-linux-gnu'
export BISON_PKGDATADIR='$MARROW_WORK/tools/usr/share/bison'
export M4='$MARROW_WORK/tools/usr/bin/m4'
export HOSTCFLAGS='-O2 -I$MARROW_WORK/hostinclude'
export HOSTLDFLAGS='-L$MARROW_WORK/tools/usr/lib/x86_64-linux-gnu'
export PKG_CONFIG_PATH='$MARROW_WORK/tools/usr/lib/x86_64-linux-gnu/pkgconfig'
EOF_ENV
printf 'Tools ready. Run: . "%s/environment.sh"\n' "$MARROW_WORK"
