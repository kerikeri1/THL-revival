# THL Revival — Threat Model

This document defines what THL Revival protects against, what it does not
protect against, and the assumptions it makes. Any security claim not listed
here should be considered out of scope.

---

## Attacker Model

THL assumes an attacker who:

has physical or software access to the host machine before or after the session.

can read persistent storage (disk, flash, NVRAM) after shutdown.

can log keystrokes via hardware or software means installed on the host.

can observe network traffic if networking were present.

does not have physical access to RAM during an active session.

does not control the boot image or the kernel shipped with THL.

does not have access to the user's external media (USB drive) during the session.

---

## Goals

THL is designed to protect against:

Untrusted host operating system: THL boots from external media and runs
entirely in RAM. The host OS is never executed during a THL session.

Software keyloggers: gpggrid allows passphrase entry via grid coordinates
rather than direct key presses. A keylogger records coordinates that are
meaningless without the grid, which changes for every character entered.

Persistent storage leakage: nothing is written to the host disk during
normal operation. All session data lives in RAM and is destroyed on shutdown.

Accidental secret persistence: sensitive buffers are wiped immediately after
use. No passphrase or key material lingers in memory beyond its required
lifetime.

Forensic acquisition after shutdown: RAM loses its contents within seconds
at room temperature. With no swap and no disk writes, there is nothing to
recover from persistent storage after a session ends.

---

## Non Goals

THL does NOT protect against:

Malicious firmware: THL cannot verify or replace UEFI, BIOS, or device
firmware. A compromised firmware can observe memory before the kernel loads.

DMA attacks: devices with direct memory access (Thunderbolt, FireWire, some
PCIe devices) can read RAM directly. THL does not enable IOMMU protection.

Compromised CPU microcode: microcode updates are applied by firmware before
the kernel runs. THL has no visibility into this layer.

Hardware implants: physical modifications to the hardware are outside the
scope of software-based protections.

Side channels requiring physical instrumentation: power analysis, EM
emission analysis, and timing attacks that require physical access to the
hardware during a session are not addressed.

Physical observation: shoulder surfing and camera-based screen capture are
not fully mitigated. Screen contrast reduction is listed as future work.

Long-term memory remanence: RAM contents can persist for minutes if cooled
deliberately. This is outside the scope of THL's threat model.

Malicious kernel or boot image: THL trusts its own kernel and binaries.
GPG image signing allows users to verify the boot image has not been
tampered with, but THL cannot protect against a compromised image that
the user chooses to trust.

---

## Assumptions

The user trusts:

The boot image and kernel shipped with THL. Verify the GPG signature before
use. The public key and signatures are in the keys/ directory of this repository.

The GPG binary and its dependencies included in the initramfs.

The external media (USB drive) used to boot THL has not been tampered with.

The physical environment enough to type passphrases without camera observation.

---

## Assets

Sensitive assets protected by THL:

GPG private keys: loaded into RAM from external media, never written to the
host disk, wiped on shutdown.

Passphrases: entered via gpggrid, stored only in RAM for the minimum
required time, wiped immediately after use with secure_bzero.

Plaintext before encryption: edited in RAM on tmpfs, never written to the
host disk.

Encrypted output: written to external media only, never to the host disk.

---

## Memory Policy

Secrets in THL follow these rules:

Live only in RAM, never on persistent storage.

Occupy the minimum possible scope: stack allocation preferred over heap,
heap preferred over global.

Be wiped immediately after use with secure_bzero, not at program exit.

Be unreachable by other processes: GNUPGHOME is chmod 700, sensitive
directories are created with restricted permissions.

Signal handlers wipe sensitive buffers before exit on SIGINT, SIGTERM,
SIGHUP, and SIGQUIT.

---

## Why No Networking

Networking is intentionally omitted from THL.

Every network stack introduces additional attack surface, background daemons,
kernel code paths, and device drivers. The original THL (2002) made this
choice explicitly: the OS does not support networking, all binaries are
compiled statically, and all non-root partitions are mounted with
no-execute permissions.

THL Revival preserves this decision. The only exception is CONFIG_NET=y
in the kernel, which is required as a dependency for Unix domain sockets
used by gpg-agent. No network interface driver, IP stack, or connectivity
of any kind is enabled. This is documented in detail in
docs/technical-decisions.md.

Users who require networking are expected to build their own kernel and
accept the corresponding trade-offs.
