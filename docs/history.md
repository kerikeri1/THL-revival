THL Revival — History
The original
Tinfoil Hat Linux was created by The Shmoo Group in 2002. It was a single floppy disk live OS designed for one purpose: give you a safe place to use GPG on a machine you don't trust.
The original featured:

Linux kernel 2.4.x
GnuPG 1.0.6 compiled with uClibc
BusyBox patched with custom tools
gpggrid — grid-based passphrase entry to defeat hardware keyloggers
paranoid mode — GPG background noise + file thrashing + morse LED
loop-AES encrypted ramdisk
SYSLINUX bootloader on a 1.44MB floppy

The last known release was version 1.000, dated February 2002.
The starting point
This project started from a single file: tinfoil.gz — a FAT12 floppy image with SYSLINUX, mislabeled with a .gz extension.
From that file i extracted:

bzImage — the original kernel
initrd.gz — the original filesystem (ext2)
readme.txt — original documentation
signing.asc — GPG signature
All original scripts: menu, paranoid, backup, bggpg.sh

Reading those scripts was the foundation of this revival — every decision in THL Revival traces back to what The Shmoo Group originally built.
The revival
THL Revival is a faithful reconstruction of the original, updated for modern hardware:

Kernel 6.12.28
GnuPG 2.4.9
BusyBox static
gpggrid rewritten in C with full charset and multi-page grid
Paranoid mode fully implemented
USB instead of floppy
Encrypted USB backup with LUKS instead of loop-AES
Wipe integrated in menu
