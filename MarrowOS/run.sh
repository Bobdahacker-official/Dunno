#!/bin/sh
set -eu
cd "$(dirname "$0")"
if ! test -f MarrowOS.ext4; then ./scripts/restore-disk.sh; fi
exec qemu-system-x86_64 -machine pc -m 2048 -smp 2 -vga std \
  -cdrom MarrowOS.iso -boot d \
  -drive file=MarrowOS.ext4,format=raw,if=virtio \
  -netdev user,id=net0,hostfwd=tcp:127.0.0.1:${MARROW_SSH_PORT:-2222}-:22 -device virtio-net-pci,netdev=net0 \
  -device qemu-xhci -device usb-tablet \
  -serial mon:stdio "$@"
