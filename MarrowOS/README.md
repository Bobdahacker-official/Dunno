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

## Workspace update

Dark mode is the default; the top-bar switch saves the theme preference.
Files supports creating files and folders. Edit text with arrows, Home/End,
Delete, and Ctrl+S. Terminal is a persistent PTY shell with Python 3.13, pip,
curl, Git, and the native compiler; Ctrl+C interrupts and Up recalls a command.

Model Editor stores its editable configuration in `/workspace/project.json`.
Template cycles through local text chat, the C classifier, an OpenAI-compatible
chat endpoint, and a custom Python training module. Python module and Dataset
open the respective folders; use New file to add modules or datasets.
Run module executes the configured Python file. Import copies a local file or
fetches an HTTP(S) URL into `import_target`. Train saves model weights or a remote
model connection. Upload sends the selected model to `upload_url` with HTTP PUT.
Remote chat accepts a key through `api_key_env` or the configured `api_key_file`.

Chat selects saved `.chat.json` models, keeps conversation history on disk,
and runs inference locally or against the configured API. The bundled local
model uses learned token transitions and conversation-example retrieval.
Power User provides live system reports and an editable shell script.
