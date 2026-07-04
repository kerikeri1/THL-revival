# THL Revival — Technical Decisions

This document explains non-obvious technical choices made during development.
The target audience is anyone who wants to audit the project, understand why
something is the way it is, or contribute without breaking the philosophy.

---

## Threat Model

THL Revival is designed to protect against a specific set of threats. Being
explicit about what it does and does not protect against is more useful than
vague security claims.

### What THL protects against

Untrusted computers: THL boots from external media and runs entirely in RAM.
The host operating system is never executed. No data is written to the host
disk at any point during a session.

Forensic acquisition after shutdown: all sensitive data lives in RAM. On
shutdown, RAM loses its contents within seconds at room temperature. There
is no swap partition and no disk writes during normal operation.

Offline disk analysis: because nothing is written to disk, there is nothing
to analyze after the fact.

Hardware keyloggers: gpggrid allows passphrase entry via grid coordinates
rather than direct key presses. A keylogger records coordinates that are
meaningless without the grid, which changes for every character.

Cold boot attacks on session data: sensitive buffers (passphrases, grid
contents) are wiped immediately after use with secure_bzero and never stored
in global or heap memory.

### What THL does not protect against

Malicious firmware: THL cannot verify or replace UEFI, BIOS, or device
firmware. A compromised firmware can observe memory before the kernel loads.

Compromised CPU microcode: microcode updates are applied by firmware before
the kernel runs. THL has no visibility into this layer.

DMA attacks: devices with direct memory access (Thunderbolt, FireWire, some
PCIe devices) can read RAM directly. THL does not enable IOMMU protection.

Physical observation: a camera or shoulder surfing can capture what is
displayed on screen. ctheme-style contrast reduction is a future consideration.

Malicious kernel: THL trusts its own kernel. A compromised bzImage would
compromise everything. This is mitigated by GPG image signing.

Long-term memory remanence: RAM contents can persist for minutes if cooled
deliberately (liquid nitrogen attacks). This is outside the scope of THL's
threat model.

---

## Kernel

### Why allnoconfig

The first kernel build started from a default desktop configuration:
1579 enabled options, 144 networking options, full WiFi stack, sound, DRM,
and a 14MB image.

This failed the manifesto criterion: every single thing running on this
machine needs to justify its existence.

We rebuilt from make allnoconfig (everything disabled) and enabled only what
could be justified. The result is 617 enabled options and a 2.5MB image.

### Why CONFIG_NET=y despite no networking

Problem: gpg-agent requires Unix domain sockets for IPC with gpg. Unix
sockets depend on CONFIG_UNIX, which depends on CONFIG_NET.

Impact: without CONFIG_NET, gpg-agent cannot start and all GPG key
operations fail.

Resolution: CONFIG_NET=y enables only the socket abstraction layer. There
is no Ethernet driver, no IP stack, no WiFi, no routing. The presence of
CONFIG_NET does not enable any form of network connectivity.

Verified with: grep -E '^CONFIG_(WLAN|WIRELESS|INET|IPV6|ETHERNET)' .config | grep '=y'
returns no connectivity-related results.

### Why LUKS1 instead of LUKS2

Problem: LUKS2 uses argon2id as its default key derivation function,
configured with 1GB of memory by default. The cryptsetup binary inside THL
does not have enough available RAM to complete the argon2id computation in
our minimal initramfs environment.

Impact: cryptsetup fails silently when attempting to open a LUKS2 volume.

Resolution: LUKS1 uses PBKDF2 which works within our memory constraints.
This is a compatibility constraint, not a security downgrade for our threat
model. The USB backup is short-term storage used in air-gapped conditions.

### Rejected alternatives

LUKS2 with reduced memory cost: tuning argon2id to use less memory would
allow LUKS2 to work in our environment. Rejected because it weakens the key
derivation in ways that are non-obvious to users who format their own USB
drives with default settings.

No encryption on USB backup: rejected outright. An unencrypted backup of
GPG private keys contradicts the entire purpose of THL.

### What is not in the kernel and why

Networking (TCP/IP, Ethernet): disabled. Not needed, significant attack
surface, contradicts the original THL design.

WiFi and WLAN: disabled. The original THL explicitly had no networking.

Bluetooth: disabled. Not needed, attack surface.

Sound: disabled. Not needed for a GPG tool.

DRM and graphics stack: disabled. THL runs in text console only.

Loadable kernel modules: disabled. All required drivers are built-in.
No runtime loading means no ability to insert unauthorized drivers.

---

## GPG Stack

### Why gpg-agent is required

GnuPG 2.x has a hard dependency on gpg-agent. Unlike GPG 1.0.6 (which the
original THL used), GPG 2.x delegates all private key operations to the
agent. There is no way to use GPG 2.x without gpg-agent running.

The original THL ran everything in a single process with no agent. We accept
this architectural change because GPG 2.x provides significantly stronger
cryptography (Ed25519, AES256, SHA512) and the agent model does not weaken
the security for our threat model.

### Why pinentry-curses

GPG 2.x requires a pinentry program for passphrase input. THL runs in a
text console with no graphical environment. pinentry-curses is the only
variant that works in this environment. libncursesw.so.6 is included in the
initramfs as a direct result of this dependency.

### Why --pinentry-mode loopback

Problem: by default, gpg-agent delegates passphrase input to pinentry via
a separate process and a Unix socket. In our QEMU serial console environment
(/dev/ttyS0 mapped to /dev/tty), this fails with "Inappropriate ioctl for
device".

Resolution: --pinentry-mode loopback tells gpg-agent to read the passphrase
directly from the calling process stdin. This works reliably on a serial
console and is the correct mode for programmatic passphrase input, which is
exactly what gpggrid does.

### Why GNUPGHOME=/tmp/gnupg

The original THL stored GPG data on the floppy disk, loaded into a ramdisk
on boot. We use /tmp/gnupg which is on tmpfs: RAM-backed, destroyed on
shutdown, never touches a physical disk. This preserves the original
guarantee that nothing persists after the session ends.

The directory is created in /init with chmod 700 to prevent other processes
from reading key material.

### Why dynamic linking for GPG

Ideally GPG would be compiled statically like BusyBox. The original THL
compiled GPG 1.0.6 statically with uClibc. GnuPG 2.x has a significantly
larger dependency tree (libgcrypt, libassuan, libnpth, libgpg-error) which
makes static compilation more complex.

We use the system GPG binary with its shared libraries copied into the
initramfs. The libraries are verified against the system package manager
and the entire initramfs is GPG-signed. Static compilation of GnuPG 2.x
is listed in future work.

---

## Memory Model

### Security evolution of gpggrid

Version 1: passphrase stored in static local variable inside
get_passphrase(). Wiped with memset() at end of main().

Problem identified: if the process was interrupted by a signal between
passphrase entry and the memset() call, the process terminated without
wiping. The passphrase remained in RAM indefinitely.

Version 2: moved passphrase to stack of main(). Added global volatile
pointer g_secret so the signal handler can reach it. Added signal handlers
for SIGINT, SIGTERM, SIGHUP, SIGQUIT. Added secure_bzero().

Version 3: added write_all() to prevent silent passphrase truncation on
partial pipe writes. Added O_CLOEXEC on /dev/urandom. Added explicit wipe
of passphrase in child process if execvp() fails. Added n==0 check in
write_all() to prevent infinite loop on unexpected pipe closure.

Current implementation: passphrase lives on stack of main(), wiped
immediately after handoff to gpg, signal handlers wipe on any clean
termination signal.

### Why secure_bzero instead of memset

Theory: the C standard (C11 6.5.2.2) permits the compiler to remove
memset() calls if it can prove the memory is not read afterwards. This
optimization is legal and has been observed in practice with GCC and Clang
when optimizing security-critical code.

Linux reality: secure_bzero uses a volatile unsigned char pointer. The
volatile qualifier tells the compiler that every write is observable and
must not be elided. No conforming compiler can remove these stores.

Tradeoff: secure_bzero is marginally slower than memset on large buffers.
For our MAX_PASS of 256 bytes this is immeasurable. The correctness
guarantee is worth it.

### Why the signal handler calls secure_bzero

Theory: POSIX defines a limited set of async-signal-safe functions. memset
and secure_bzero are not on that list. Calling them from a signal handler
is technically undefined behaviour per POSIX.

Linux reality: secure_bzero is a sequence of memory stores with no system
calls, no malloc, no locks. On Linux this is safe in practice.

Tradeoff: we accept the theoretical POSIX non-compliance because the
alternative is leaving key material in RAM on interruption. This decision
is documented explicitly in the source code so any auditor understands it
is deliberate.

### Rejected alternatives

Global buffer: rejected because the buffer would persist for the entire
process lifetime even after being wiped. Stack allocation gives the minimum
possible lifetime.

Heap allocation with malloc: rejected because malloc introduces allocator
metadata adjacent to the allocation. On some implementations, sensitive
data on the heap can leave traces in allocator structures even after free().

Single write() for passphrase: rejected because a single write() on a pipe
is not guaranteed to write all bytes. A silent truncation would cause GPG to
receive an incorrect passphrase with no error message.

---

## Architecture

### Why a single C file for gpggrid

gpggrid could be split into modules: grid.c, secure_mem.c, signals.c,
main.c. We chose a single file deliberately.

THL's philosophy is auditability. Anyone should be able to read the entire
tool in one sitting and understand exactly what it does. A single file means
one file to open, one file to read, one file to verify. No build system
beyond a single gcc invocation. No header files to cross-reference. The
entire security-critical code is visible without navigation.

The original THL shipped shell scripts for the same reason: maximum
transparency, minimum complexity.

### Why VLA for gpg_argv

```c
char *gpg_argv[argc + 5];
```

Variable Length Array instead of malloc(): no heap allocation, no leak
possible, automatically cleaned up when the scope ends. The array is small
(argc will never be large in practice) and lives only until execvp()
replaces the process image.

### Why O_CLOEXEC on /dev/urandom

O_CLOEXEC marks the file descriptor to be automatically closed when execvp()
is called. Without it, the fd would be inherited by the GPG child process,
an unnecessary open file descriptor in a process that has no use for it.
We close it explicitly in the child anyway, but O_CLOEXEC is the correct
approach and costs nothing.

---

## Known Limitations

mlock(): the passphrase page could theoretically be swapped to disk by the
kernel before the wipe occurs if swap is enabled. mlock() would pin the page
in RAM and prevent swapping. THL runs without swap, which mitigates this in
practice. mlock() is listed in future work.

MADV_DONTDUMP: marking the passphrase page with MADV_DONTDUMP would exclude
it from core dumps. Relevant if core dumps are enabled on the host system.

Static GPG compilation: the current GPG binary is dynamically linked. Static
compilation would eliminate the shared library dependency and reduce the
initramfs attack surface.

SIGSEGV and SIGABRT: these signals are not intercepted. A segfault implies
undefined behaviour already, and running secure_bzero on potentially corrupt
memory could make things worse. Key material may not be wiped if the process
crashes. This is an accepted limitation.

---

## Future Work

mlock() on the passphrase buffer to prevent swap exposure.

MADV_DONTDUMP on sensitive memory pages.

explicit_bzero() support via preprocessor detection where available.

Static compilation of GnuPG 2.x with musl libc.

USB boot verification on physical hardware.

Signed release process with reproducible builds.

ctheme-style screen contrast reduction for anti-shoulder-surf and
anti-photography countermeasures.

Constant-time comparison functions for any future operations that compare
sensitive values.
