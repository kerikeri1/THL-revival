## Current status
- Bootable initramfs 
- BusyBox 
- GPG 2.4.9 
- Menu 
- Shell 
- gpggrid — anti-keylogger passphrase entry with full charset and multi-page grid 
- Paranoid mode — GPG background noise + file thrashing + morse LED 
- Wipe — secure erasure of files and disks 
- Backup to encrypted USB with LUKS 
- GPG image signing 
- Kernel rebuilt from allnoconfig — minimal, no networking/wifi/sound/drm 
- gpggrid secure memory handling — signal handlers, wipe on exit 

## Next steps

### boot from real USB
Test and document boot from physical USB hardware, not just QEMU.
