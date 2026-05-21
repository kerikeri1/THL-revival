THL Revival — Roadmap 

 Current status

- Bootable initramfs
- BusyBox 
- GPG 2.4.9 
- Menu 
- Shell 
- gpggrid — anti-keylogger passphrase entry with full charset and multi-page grid 
- Paranoid mode — GPG background noise + file thrashing + morse LED 
- Wipe — secure file and disk erasure via menu 
- Backup to encrypted USB with LUKS 

Next steps

GPG image signing

Sign the final image with GPG — same as the original `signing.asc`.

boot from real USB

Test and document boot from physical USB hardware, not just QEMU.

gpggrid in menu

Add gpggrid as a dedicated menu option for passphrase entry directly from the main menu.
