THL Revival — Roadmap

Current status (v0.1-alpha)
- Bootable initramfs 
- BusyBox 
- GPG 2.4.9 
- Menu 
- Shell 
- gpggrid — anti-keylogger passphrase entry with full charset and multi-page grid 
- Paranoid mode — GPG background noise + file thrashing + morse LED 

Next steps

wipe integration
Add wipe/shred as a dedicated menu option — secure file and disk erasure, faithful to the original.

backup to USB with LUKS
Replace the original floppy backup with USB — same logic, same philosophy, encrypted with LUKS.

GPG image signing
Sign the final image with GPG — same as the original `signing.asc`. Zero trust starts with the image itself.

boot from real USB
Test and document boot from physical USB hardware, not just QEMU.
