#!/bin/sh
set -eu
project=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
: "${MARROW_WORK:=$project/build}"
: "${TOOLS_ROOT:=/}"
: "${KERNEL_SRC:=$MARROW_WORK/kernel}"
export MARROW_WORK TOOLS_ROOT
make -C "$project" BUILD="$MARROW_WORK" all
"$MARROW_WORK/marrow-train" generate "$MARROW_WORK/data"
"$MARROW_WORK/marrow-train" train "$MARROW_WORK/data/spiral-train.csv" "$MARROW_WORK/spiral.weights" 600 > "$MARROW_WORK/training.log"
python3 "$project/scripts/rootfs.py"
# mke2fs populates the disk directly, without loop devices or privileged mounts.
image="$project/MarrowOS.ext4"
if test -e "$image"; then
    echo "Refusing to replace existing MarrowOS.ext4. Move it aside to create a fresh workspace." >&2
    exit 1
fi
truncate -s 2G "$image"
mke2fs -q -t ext4 -F -L MarrowRoot -m 0 -O ^metadata_csum_seed -d "$MARROW_WORK/rootfs" "$image"
mkdir -p "$MARROW_WORK/initramfs/bin" "$MARROW_WORK/initramfs/dev" "$MARROW_WORK/iso/boot/grub"
mkdir -p "$MARROW_WORK/initramfs/lib/firmware"
cp "$MARROW_WORK/rootfs/lib/firmware/"regulatory* "$MARROW_WORK/initramfs/lib/firmware/"
cp "$MARROW_WORK/marrow-init" "$MARROW_WORK/initramfs/init"
cp "$MARROW_WORK/rootfs/bin/busybox" "$MARROW_WORK/initramfs/bin/busybox"
ln -sf busybox "$MARROW_WORK/initramfs/bin/sh"
(cd "$MARROW_WORK/initramfs" && find . -print0 | cpio --null -o --format=newc 2>/dev/null | gzip -9) > "$MARROW_WORK/iso/boot/initrd.gz"
cp "$KERNEL_SRC/arch/x86/boot/bzImage" "$MARROW_WORK/iso/boot/bzImage"
cp "$project/boot/grub.cfg" "$MARROW_WORK/iso/boot/grub/grub.cfg"
cp "$KERNEL_SRC/.config" "$project/config/kernel.config"
cp "$MARROW_WORK/iso/boot/bzImage" "$project/boot/bzImage"
cp "$MARROW_WORK/iso/boot/initrd.gz" "$project/boot/initrd.gz"
if test "$TOOLS_ROOT" = /; then
    grub-mkrescue -o "$project/MarrowOS.iso" "$MARROW_WORK/iso"
else
    grub-mkrescue -d "$TOOLS_ROOT/usr/lib/grub/i386-pc" --locales= --fonts= --themes= -o "$project/MarrowOS.iso" "$MARROW_WORK/iso"
fi
printf 'MarrowOS built: %s\n' "$project"
