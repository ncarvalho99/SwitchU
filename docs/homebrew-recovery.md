# Launching homebrew: what it changes, and how to undo it

Read this before installing a build with homebrew launching enabled. Written
before the feature was, so that the way out exists whether or not the way in
works.

## The short version

Everything this feature adds lives on the microSD card, under `atmosphere/`.
Nothing is written to the console's internal storage. **Deleting the files
listed below restores stock behaviour completely** — there is no state left
behind in NAND to clean up, and no step that cannot be undone with the SD card
in a computer.

## Why an applet has to be taken over at all

SwitchU replaces qlaunch (`0100000000001000`), the home menu. A home menu cannot
run an NRO itself: homebrew runs inside `nx-hbloader`, and `nx-hbloader` runs by
taking the place of a system applet. Atmosphère does this with a file-based
override — the applet's title ID is pointed at `hbl.nsp` on the SD card.

So launching homebrew from the grid means one system applet stops being itself
while the override is in place. Which one is a setting, because the two
reasonable choices break different things.

## What gets taken over, and what that costs

| Setting | Title ID | What stops working while enabled |
| --- | --- | --- |
| **Album** (default) | `010000000000100D` | Opening the Album gives homebrew instead of the screenshot gallery |
| Parental controls | `0100000000001001` | The parental-control PIN prompt is replaced, so parental controls no longer block anything |

Album is the default because losing the screenshot gallery is an inconvenience,
while losing the parental-control prompt is a protection silently switching off.
If nobody on the console uses parental controls, the second option costs
nothing and keeps the Album intact — that is the only reason it is offered.

Whichever is chosen, the other applet is untouched.

## Files this feature adds

```
sdmc:/atmosphere/hbl.nsp                       nx-hbloader itself
sdmc:/atmosphere/config/override_config.ini    which title ID it takes over
```

`override_config.ini` is a file Atmosphère reads and other tools also write to.
If it already exists it is edited, not replaced, and the previous contents are
copied to `override_config.ini.switchu.bak` beside it first.

## Undoing it

### Normal case — the console boots

1. Turn the setting off in SwitchU, or
2. Power off, put the SD card in a computer, and delete
   `sdmc:/atmosphere/config/override_config.ini`.
   If `override_config.ini.switchu.bak` is there, rename it back over the
   original instead of deleting — that restores whatever the file held before.

Deleting `hbl.nsp` is optional. On its own it does nothing; it only matters
while an override points at it.

### The console does not boot, or hangs on the menu

The override is plain text on the SD card, so a computer is all that is needed.
No RCM payload, no NAND restore.

1. Hold **Power** for 12 seconds to force it off.
2. Put the microSD card in a computer.
3. Delete `atmosphere/config/override_config.ini` (or restore the `.bak`).
4. Put the card back and boot.

If it still does not boot, the cause is not this feature — the override only
takes effect when the applet is launched, which is long after boot. Rename
`atmosphere/contents/0100000000001000` to `…1000.off` to fall back to the stock
Nintendo home menu, and SwitchU is out of the picture entirely.

### Getting parental controls back

Nothing is stored, so there is nothing to restore: the moment the override is
gone, the real applet answers again and the existing PIN works as before. The
PIN itself lives in the console's own settings and this feature never reads or
writes it.

## What this never touches

- The console's internal storage — no title is installed, moved or deleted
- The parental-control PIN, or any other setting held in NAND
- `sdmc:/atmosphere/contents/`, apart from SwitchU's own `0100000000001000`
  that the normal installation already places there
- Kernel patches, KIPs, or anything loaded before Atmosphère hands off

## If something goes wrong anyway

`sdmc:/atmosphere/crash_reports/` and `sdmc:/config/SwitchU/` are what make a
report actionable, together with the `SwitchU-symbols-*` artifact from the
**same build** — symbols from a different build resolve to the wrong lines and
send the search in the wrong direction.
