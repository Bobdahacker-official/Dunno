# MarrowOS 0.1

A bootable x86-64 Linux-based OS for local AI experiments. Custom desktop, init,
training engine, compiler front end, and Wi-Fi helper are C; the dot-product
routine is x86-64 assembly. Linux 6.12.107 was compiled for this image.

Run `./run.sh` with QEMU installed. On first launch, the disk is restored from
`artifacts/MarrowOS-runtime.tar.gz`. The boot ISO and screenshots are included.
The 2 GiB ext4 disk holds the system and `/workspace`; edits survive shutdown.
F1-F6 select views. Ctrl+Alt+F2 opens a text console; Ctrl+Alt+F1 returns.
In Files: arrows select, Enter opens, E edits, Ctrl+S saves, Esc leaves editing.

```sh
cc /workspace/src/train.c /workspace/src/dot.S -lm -o /workspace/train
/workspace/train train /workspace/datasets/spiral-train.csv /workspace/models/custom.weights 600
/workspace/train eval /workspace/datasets/spiral-test.csv /workspace/models/custom.weights
/workspace/train infer /workspace/models/custom.weights 0.2 0.5
poweroff
```

The trainer uses a 2-24-2 tanh/softmax network, Adam, CSV datasets, checksummed
float32 checkpoints, and a separate held-out evaluation set. `train` accepts a
checkpoint as its final argument to warm-start from existing weights.
Ethernet: VirtIO, Intel e1000/e1000e, RTL8139, RTL8169. Wi-Fi: ath9k PCI/PCIe and
RTL8187 USB, with WPA2 configuration from Network. `iw` and `wpa_cli` are included.

Build custom components with `make`. Build the kernel with
`make kernel KERNEL_SRC=/path/to/linux-source-6.12`. Assemble a fresh image using
`scripts/image.sh` with `KERNEL_SRC`, `TOOLS_ROOT`, and `MARROW_WORK` set.
For an unprivileged Debian toolchain, run `scripts/fetch-tools.sh`, source
`build/environment.sh`, then build the kernel and run `scripts/image.sh`.
