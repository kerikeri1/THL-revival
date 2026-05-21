# THL-revival

Paranoia is not a disorder, it's a feature. Every single thing running on your machine needs to justify its existence. Why is it here? When does it run? How does it work? No network you didn't personally approve. No process you didn't explicitly invite. No data touching a disk you don't own. Everything in RAM. Everything ephemeral. Everything yours. THL Revival doesn't want to be "secure" like those other distros with their fancy frameworks and compliance checklists. That's not the point. The point is that you, the person reading this, should have total control over every bit running on your hardware.

keep being paranoid, keep having no trust ;P

-kerikeri1

---

## What is this?

This is not a new distro. This is not a "modern secure OS". This is Tinfoil Hat Linux — born in 2002 by The Shmoo Group — brought back to life on modern hardware.

The original ran on a single 1.44MB floppy. Everything in RAM. GPG as the only tool that mattered. Boot, encrypt, shut down, leave no trace. That idea never got old. The floppy did tho (rip!!!)

THL Revival is a faithful reconstruction of the original philosophy — same paranoia, same zero trust model, same RAM-only approach. We updated what had to change. We kept everything that made THL, THL.

---

What works

- Bootable live OS — runs entirely in RAM, nothing touches the disk
- BusyBox + shell
- GnuPG 2.4.9 — key generation, encryption, decryption
- gpggrid — anti-keylogger passphrase entry, full charset, multi-page grid
- Paranoid mode — GPG background noise + file thrashing + morse LED on keyboard LEDs
- Wipe — secure erasure of files and disks
- Encrypted USB backup with LUKS

---

How to run

```bash
qemu-system-x86_64 \
  -kernel bzImage \
  -initrd initramfs.gz \
  -append "console=ttyS0 quiet rdinit=/init" \
  -nographic -m 512M
```

---

Status

— see [docs/roadmap.md](docs/roadmap.md) for what's next.
