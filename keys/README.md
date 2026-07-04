# THL Revival — Release Signing

All releases are signed with the THL Revival GPG key.

## Verify a release

```bash
# Import the public key
gpg --import thl-revival-public.asc

# Verify bzImage
gpg --verify bzImage.asc bzImage

# Verify initramfs
gpg --verify initramfs_new.gz.asc initramfs_new.gz
```

## Key fingerprint

AD6C DEC0 22A6 7AD8 81AA  8014 61A2 6CA7 4005 B386
