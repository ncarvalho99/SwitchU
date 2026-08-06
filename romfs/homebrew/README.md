# Vendored nx-hbloader

`hbl.nsp` is a prebuilt release binary of [nx-hbloader], not something this
repository builds. It is committed rather than downloaded so that a clean
checkout builds offline and the exact bytes that ship are the exact bytes in
git history.

| | |
| --- | --- |
| Source | https://github.com/switchbrew/nx-hbloader |
| Release | v2.4.5, published 2025-11-15 |
| File | `hbl.nsp`, 40946 bytes, PFS0 |
| Licence | ISC — see `LICENSE-nx-hbloader.md` beside it |

The ISC licence permits redistribution provided the copyright and permission
notice travel with the software, which is why `LICENSE-nx-hbloader.md` sits in
this directory and is installed to the SD card next to the binary rather than
being left behind in the repository.

## How it is used

It is copied to `sdmc:/atmosphere/hbl.nsp` **only when nothing is there
already**. A loader the user installed themselves is very likely newer than this
one or patched for their setup, and replacing it would silently downgrade a
working console. See `projects/menu/src/launcher/HblOverride.cpp` and
`docs/homebrew-recovery.md`.

## Updating it

Replace both files from a newer release and update the table above. Nothing
else refers to the version, so a stale table is the only way this goes wrong.

[nx-hbloader]: https://github.com/switchbrew/nx-hbloader
