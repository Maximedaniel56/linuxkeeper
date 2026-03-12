# Submitting LinuxKeeper to the AUR

## Prerequisites

1. Create an AUR account at https://aur.archlinux.org
2. Generate an SSH key pair if you do not already have one:
   ```
   ssh-keygen -t ed25519 -C "your-email@example.com"
   ```
3. Add your SSH public key (`~/.ssh/id_ed25519.pub`) to your AUR account under "My Account" > "SSH Public Key".

## Submitting the Package

1. Clone the (empty) AUR repository for linuxkeeper:
   ```
   git clone ssh://aur@aur.archlinux.org/linuxkeeper.git /tmp/aur-linuxkeeper
   ```

2. Copy the PKGBUILD and .SRCINFO into the cloned repository:
   ```
   cp packaging/PKGBUILD /tmp/aur-linuxkeeper/
   cp packaging/.SRCINFO /tmp/aur-linuxkeeper/
   ```

3. Commit and push:
   ```
   cd /tmp/aur-linuxkeeper
   git add PKGBUILD .SRCINFO
   git commit -m "Initial upload: linuxkeeper 1.0.0"
   git push origin master
   ```

## Updating the Package

When releasing a new version:

1. Update `pkgver` in `PKGBUILD`.
2. Reset `pkgrel` to `1` (or increment it if only the PKGBUILD changed, not the upstream version).
3. Update `sha256sums` if you replace `SKIP` with actual checksums (recommended for production). You can generate them with:
   ```
   makepkg -g
   ```
4. Regenerate `.SRCINFO`:
   ```
   makepkg --printsrcinfo > .SRCINFO
   ```
5. Commit and push to the AUR repository.

## Testing Locally

Before submitting, test the package builds correctly:
```
cd packaging
makepkg -si
```

This will download the source, build, and install the package on your Arch system.
