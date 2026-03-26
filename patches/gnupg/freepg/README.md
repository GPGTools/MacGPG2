Selected FreePG patches for the GnuPG build live directly in this folder.

`create_gpg.sh` applies `freepg/*.patch` before the local MacGPG2 GnuPG
patches. Files under `freepg/skipped/` are kept for reference only and are
not considered by the build script.

Patch source:

- [freepg/gnupg](https://gitlab.com/freepg/gnupg)
- branch snapshot used locally: `master-freepg`
