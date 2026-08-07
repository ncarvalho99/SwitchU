# The loader that ships beside the menu

`hbl.nsp` is **not committed**. It is produced by the build from
`lib/switchu-hbloader/`, a vendored and modified copy of [nx-hbloader], and
lands here on its way into the install layout.

It carries a SwitchU change, so shipping a binary nobody in this repository
produced would leave no way to tie the artifact to the source it came from. The
xmake target `switchu-hbloader` runs its Makefile and copies the result here;
`.gitignore` treats it as the build output it is.

| | |
| --- | --- |
| Source | `lib/switchu-hbloader/` |
| Upstream | https://github.com/switchbrew/nx-hbloader |
| Base | `82b95122` (v2.4.5), recorded in `lib/switchu-hbloader/UPSTREAM_COMMIT` |
| Licence | ISC — `LICENSE-nx-hbloader.md` here, and `LICENSE.md` with the source |

## What the change does

Stock nx-hbloader always opens `sdmc:/hbmenu.nro` when started as an applet:
the path it runs is only ever set by homebrew already running, through the
`envSetNextLoad` ABI, so a menu launching it has no way to name a target.

The fork reads one line from `sdmc:/config/SwitchU/next_nro.txt`, deletes it,
and falls back to hbmenu exactly as upstream does when there is no request. The
diff is confined to `loadNro()` and one helper above it.

## Why the licence is here as well as with the source

ISC asks that the copyright and permission notice travel with the software.
Whoever receives the release zip does not receive this repository, so the notice
is installed to the SD card next to the binary.

## Updating against upstream

Diff `lib/switchu-hbloader/source/main.c` against the recorded base commit,
rebase the change onto the new release, and update `UPSTREAM_COMMIT` and the
table above. The modification is marked in the source with `SwitchU
modification` comments at both sites.

[nx-hbloader]: https://github.com/switchbrew/nx-hbloader
