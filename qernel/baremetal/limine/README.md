# Limine bootloader files (not vendored in git)

This directory holds the prebuilt Limine BIOS binaries needed by `make iso`.
They're third-party, architecture-specific binaries, so they're
`.gitignore`d here rather than committed — only `limine.cfg` (ours) is
tracked.

## What's here vs. what's missing

| File                    | Status                  | Purpose |
|-------------------------|--------------------------|---------|
| `limine.cfg`             | tracked in git            | Boot menu entry for `kernel.elf`, Multiboot1 protocol |
| `limine-bios.sys`        | present (drop it back in if this dir gets cleaned) | Stage 2/3 BIOS loader: reads `limine.cfg` and loads the kernel |
| `limine-bios-cd.bin`     | **missing**                | El Torito "no emulation" boot image: what the BIOS actually jumps to when booting the ISO off a CD/USB |

`make iso` checks for both binaries and fails with a clear message if
either is missing.

## Where to get the missing file

Download the official Limine **binary release** (not the source tarball)
from the project's GitHub releases, matching the branch/version this
`limine-bios.sys` came from, and copy `limine-bios-cd.bin` in here next to
it. The binary release archive ships both files together, plus the UEFI
equivalents (`limine-uefi-cd.bin`, `BOOTX64.EFI`) if you also want a UEFI
boot path later — not required for the BIOS/QEMU `-cdrom` flow `make iso`
builds today.

This sandbox's network policy blocks direct `github.com` downloads, so
grab it on a machine that has access and upload it back into this
directory.

## Building and testing

```bash
make iso       # -> build/qernel.iso
make run-iso   # boots it in qemu-system-i386 -cdrom
```
