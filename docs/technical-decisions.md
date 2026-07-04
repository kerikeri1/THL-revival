# THL Revival — Technical Decisions

This document explains non-obvious technical choices made during development.
The target audience is anyone who wants to audit the project, understand why
something is the way it is, or contribute without breaking the philosophy.

---

## Kernel

### Starting point: allnoconfig

The original THL kernel was compiled for a specific purpose: boot, run GPG,
shut down. No networking, no sound, no graphics stack.

Our first kernel build started from a default configuration,a
desktop-oriented config with 1579 enabled options, 144 networking options,
full WiFi stack (CFG80211, MAC80211, drivers for Atheros, Broadcom, Intel,
Realtek), sound, DRM, and more. The kernel image was 14MB.

This directly contradicted the manifesto: "every single thing running on
this machine needs to justify its existence."

We rebuilt from `make allnoconfig` everything disabled by default and
enabled only what could be justified. The result is 617 enabled options and
a 2.5MB kernel image.

### Why CONFIG_NET=y despite "no network"

The manifesto says "no network you didn't personally approve." This refers
to network connectivity: Ethernet, WiFi, IP stack, routing. None of those
are enabled.

`CONFIG_NET=y` is required as a dependency for Unix domain sockets
(`CONFIG_UNIX=y`), which gpg-agent uses for IPC between itself and gpg.
Without Unix sockets, gpg-agent cannot start and GPG key operations fail.

There is no network interface driver, no IP stack, no Ethernet support.
`CONFIG_NET=y` here enables only the socket abstraction layer, not any
form of network connectivity.

Verified: `grep -E '^CONFIG_(WLAN|WIRELESS|INET|IPV6|ETHERNET)' .config | grep '=y'`
returns zero results except `CONFIG_WIRELESS=y` which is an empty container
with no drivers underneath it.

### Why LUKS1 instead of LUKS2

LUKS2 uses argon2id as the default key derivation function, configured with
1GB of memory by default on modern systems. The cryptsetup binary inside THL
does not have enough RAM available in the initramfs environment to complete
the argon2id computation.

LUKS1 uses PBKDF2 which works within the memory constraints of our minimal
environment. This is a compatibility constraint, not a security downgrade for
our threat model, the USB backup is a short-term storage medium used in
air-gapped conditions, not long-term archival storage.

### What is not in the kernel and why

| Category | Status | Reason |
|---|---|---|
| Networking (TCP/IP, Ethernet) | disabled | not needed, attack surface |
| WiFi / WLAN | disabled | original THL had no networking |
| Bluetooth | disabled | not needed, attack surface |
| Sound | disabled | not needed |
| DRM / graphics | disabled | THL runs in text console only |
| Kernel modules | disabled | all drivers built-in or absent, no runtime loading |

---

## gpggrid

### The original vulnerability

The first version of gpggrid stored the passphrase in a `static` local
variable inside `get_passphrase()`:

```c
static char passphrase[MAX_PASS];
```

`static` means the variable lives in the process's static segment for the
entire lifetime of the process not on the stack. This was necessary to
return a pointer to the caller without undefined behaviour, but created a
security problem: the passphrase remained in memory until `memset()` was
explicitly called at the end of `main()`.

The problem is timing. If the process was interrupted by a signal (SIGINT
from Ctrl+C, SIGTERM from kill, SIGHUP from terminal close) between the
moment the user entered the passphrase and the moment `memset()` was called,
the process terminated without wiping. The passphrase stayed in RAM
indefinitely.

On a normal system with an active OS, that memory would be overwritten
quickly by other processes. On THL — which runs entirely in RAM with minimal
process activity, the passphrase could persist for the entire session.

A cold boot attack (physically removing power and reading RAM contents before
they decay) could recover it.

This was a direct contradiction of the manifesto for the project's most
iconic feature.

### The fix

The fix has three parts:

**1. Signal handlers**
We install handlers for SIGINT, SIGTERM, SIGHUP, and SIGQUIT that call
`secure_bzero()` on the passphrase before calling `_exit()`. SIGSEGV and
SIGABRT are intentionally NOT intercepted: a segfault implies undefined
behaviour already, and calling memset on potentially corrupt memory could
make things worse.

**2. Global pointer, stack storage**
The passphrase buffer lives on the stack of `main()` — minimum lifetime,
automatically gone when the function returns. A global volatile pointer
`g_secret` points to it so the signal handler can reach it from any
execution context without the buffer being global itself.

**3. Immediate wipe after use**
The passphrase is wiped with `secure_bzero()` immediately after being
written to the pipe, not at program exit. There is no reason to keep it
in memory after GPG has received it.

### Why secure_bzero instead of memset

The C compiler is allowed to optimize away `memset()` calls if it determines
that the memory is not read afterwards. This is a known issue, several
real world security vulnerabilities have been caused by compilers removing
"unnecessary" memset calls on sensitive buffers.

`secure_bzero()` uses a `volatile unsigned char *` pointer. The `volatile`
qualifier tells the compiler that every write is observable and must not be
elided. The compiler cannot prove that nobody will read that memory, so it
must emit every store.

```c
static inline void secure_bzero(void *ptr, size_t len)
{
    volatile unsigned char *p = (volatile unsigned char *)ptr;
    while (len--)
        *p++ = 0;
}
```

### Why the signal handler calls secure_bzero

POSIX defines a limited set of functions that are safe to call from a signal
handler (async-signal-safe functions). `memset` and our `secure_bzero` are
not on that list — they are not async-signal-safe by specification.

However, on Linux, `secure_bzero` is simply a sequence of memory stores with
no system calls, no malloc, no locks. In practice this is safe. We accept
this trade-off deliberately: the alternative is leaving key material in RAM,
which is worse. This decision is documented in the code:

```c
/*
 * secure_bzero() is intentionally called from the signal handler.
 * Although POSIX does not specify arbitrary memory writes as an
 * async-signal-safe primitive, on Linux this is simply a sequence
 * of stores and is preferable to leaving key material in RAM.
 */
```

### Known remaining limitation

If the kernel swaps the passphrase page to disk before the wipe occurs, the
passphrase would exist on disk in plaintext. Protection against this requires
`mlock()` to pin the page in RAM and prevent swapping. THL runs without swap
enabled, which mitigates this in practice, but `mlock()` is a future
improvement.

---

## GPG Stack

### Why gpg-agent is required

GnuPG 2.x has a hard dependency on gpg-agent. Unlike GPG 1.x (which the
original THL used), GPG 2.x delegates all private key operations to
gpg-agent. There is no way to use GPG 2.x for key generation or decryption
without gpg-agent running.

The original THL used GPG 1.0.6 which had no agent, everything ran in a
single process. We accept this architectural change because GPG 2.x provides
significantly stronger cryptography (Ed25519, AES256, SHA512) and the agent
model does not weaken the security for our threat model.

### Why pinentry-curses

GPG 2.x requires a pinentry program to handle passphrase input. Several
variants exist: pinentry-gnome3, pinentry-x11, pinentry-curses, pinentry-tty.

THL runs in a text console with no graphical environment. pinentry-curses
is the only variant that works in this environment. It uses ncurses for
terminal handling, which is why libncursesw.so.6 is included in the
initramfs.

### Why --pinentry-mode loopback

By default, gpg-agent delegates passphrase input to the pinentry program
via a separate process and a Unix socket. In our QEMU serial console
environment (/dev/ttyS0 mapped to /dev/tty), this delegation fails with
"Inappropriate ioctl for device" because the pinentry process cannot
properly access the terminal.

`--pinentry-mode loopback` tells gpg-agent to read the passphrase directly
from the calling process's stdin instead of launching pinentry. This works
reliably on our serial console and is the correct mode for programmatic
passphrase input (which is exactly what gpggrid does).

### Why GNUPGHOME=/tmp/gnupg

The original THL stored GPG data on the floppy disk, loaded into a ramdisk
on boot. We use `/tmp/gnupg` which is on tmpfs — RAM-backed, destroyed on
shutdown, never touches a physical disk. This preserves the original
"everything in RAM, nothing on disk" guarantee.

The directory is created in `/init` with `chmod 700` to prevent other
processes from reading key material.

### Why dynamic linking for GPG

Ideally GPG would be compiled statically like BusyBox. The original THL
compiled GPG 1.0.6 statically with uClibc. GnuPG 2.x has a significantly
larger dependency tree (libgcrypt, libassuan, libnpth, libgpg-error) which
makes static compilation more complex.

We use the system GPG binary with its shared libraries copied into the
initramfs. This is a pragmatic choice, the libraries are verified against
the system package manager and the entire initramfs is GPG-signed. Static
compilation of GnuPG 2.x is a future improvement.

---

## Architecture

### Why a single C file for gpggrid

gpggrid could be split into modules: `grid.c`, `secure_mem.c`, `signals.c`,
`main.c`. This would be conventional for a project of this size.

We chose a single file deliberately. THL's philosophy is auditability —
anyone should be able to read the entire tool in one sitting and understand
exactly what it does. A single file means:

- one file to open, one file to read, one file to verify
- no build system beyond a single gcc invocation
- no header files to cross-reference
- the entire security-critical code is visible without navigation

The original THL shipped shell scripts for the same reason — maximum
transparency, minimum complexity.

### Why the passphrase lives on the stack

The passphrase buffer is declared in `main()` and passed by pointer to
`get_passphrase()`. It could have been a global variable (simpler) or
heap-allocated with malloc (more flexible).

Stack allocation was chosen because:

- **Minimum lifetime**: stack memory is automatically reclaimed when the
  function returns. A global would persist for the entire process lifetime
  even after being wiped.
- **No heap fragmentation**: malloc introduces allocator metadata adjacent
  to the allocation. On some implementations, sensitive data on the heap
  can leave traces in allocator structures even after free().
- **Explicit control**: the buffer's scope is visible in the source. A
  reader can see exactly when it exists and when it is wiped.

The only concession is `g_secret` a global volatile pointer to the stack
buffer, necessary so the signal handler can reach it from any execution
context. The pointer is set to NULL immediately after the wipe.

### Why VLA for gpg_argv

```c
char *gpg_argv[argc + 5];
```

This is a Variable Length Array, its size is determined at runtime. We use
it instead of `malloc()` for the same reason as stack allocation for the
passphrase: no heap allocation, no leak possible, automatically cleaned up
when the scope ends. The array is small (argc will never be large in
practice) and lives only until `execvp()` replaces the process image.

### Why O_CLOEXEC on /dev/urandom

```c
urandom_fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
```

`O_CLOEXEC` marks the file descriptor to be automatically closed when
`execvp()` is called. Without it, the fd would be inherited by the GPG
child process, an unnecessary open file descriptor in a process that has
no use for it. We close it explicitly in the child anyway, but `O_CLOEXEC`
is the belt-and-suspenders approach and costs nothing.

### Why write_all instead of a single write

A single `write()` call on a pipe is not guaranteed to write all bytes
requested, even for small buffers. The kernel may return a short write if
the pipe buffer is temporarily full or if the call is interrupted. For
most data this is acceptable — for a passphrase, a silent truncation would
cause GPG to receive an incorrect passphrase with no error message.

`write_all()` loops until all bytes are written or an unrecoverable error
occurs, handling `EINTR` (interrupted by signal) correctly.
