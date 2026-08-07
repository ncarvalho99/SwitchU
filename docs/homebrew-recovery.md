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
| **Parental controls** (default) | `0100000000001001` | The parental-control PIN prompt is replaced, so parental controls no longer block anything |
| Album | `010000000000100D` | Opening the Album gives homebrew instead of the screenshot gallery |

Parental controls is the default because on most consoles it is never set up,
and taking it costs those consoles nothing while leaving the Album working.

**If this console has parental controls set up, change this setting before
enabling homebrew launching.** The PIN prompt is what enforces them, and while
it is replaced they stop being enforced at all — the restrictions still appear
configured in Settings, and are no longer applied. Nothing warns about this on
the console itself, which is why it is stated here. Switching the setting to
Album restores enforcement immediately.

Holding **R** while the system raises a PIN prompt gives the real applet instead
of the loader, so the prompt still works for anyone who knows to do it. Do not
hold R while launching homebrew from the grid, though: the menu starts the
applet with no arguments at all, which the loader ignores and the real applet
aborts on. That crash is the applet, not the console, and nothing is left broken
afterwards.

Whichever is chosen, the other applet is untouched.

## Files this feature adds

```
sdmc:/switch/SwitchU/homebrew/hbl.nsp          the loader SwitchU ships
sdmc:/atmosphere/config/override_config.ini    which title ID it takes over,
                                               and which loader answers
```

`override_config.ini` is a file Atmosphère reads and other tools also write to.
If it already exists it is edited, not replaced, and the previous contents are
copied to `override_config.ini.switchu.bak` beside it first.

### This changes the loader for the whole console

Stock nx-hbloader always opens `sdmc:/hbmenu.nro`; there is no way to hand it a
particular NRO when it starts as an applet. Launching an individual homebrew
from the grid therefore needs a modified loader, and SwitchU ships one.

Atmosphère holds **one** loader path for the entire system — `path=` under
`[hbl_config]` is a single global, not a per-title setting. Pointing it at
SwitchU's loader means every homebrew override on the console uses SwitchU's
loader, **including one you set up yourself on the Album**. The previous value
is saved in the `.bak` file and put back when the feature is turned off.

If you rely on a specific loader build of your own, that is the setting that
takes it away, and turning this feature off is what returns it.

## Undoing it

### Normal case — the console boots

1. Turn the setting off in SwitchU, or
2. Power off, put the SD card in a computer, and delete
   `sdmc:/atmosphere/config/override_config.ini`.
   If `override_config.ini.switchu.bak` is there, rename it back over the
   original instead of deleting — that restores whatever the file held before.

Restoring the `.bak` matters more than it used to: it carries the loader path
your console used before, so putting it back returns your own hbl setup along
with everything else.

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
gone — either by switching the host to Album or by deleting
`override_config.ini` — the real applet answers again and the existing PIN
works as before. The PIN itself lives in the console's own settings, and this
feature never reads, writes or clears it. The restrictions were configured the
whole time; only the prompt that enforces them was standing aside.

## What this never touches

- The console's internal storage — no title is installed, moved or deleted
- The parental-control PIN, or any other setting held in NAND
- `sdmc:/atmosphere/contents/`, apart from SwitchU's own `0100000000001000`
  that the normal installation already places there
- `sdmc:/atmosphere/hbl.nsp` — SwitchU's loader lives under `switch/SwitchU/`
  and a loader you installed yourself is never overwritten or deleted
- Kernel patches, KIPs, or anything loaded before Atmosphère hands off

## If something goes wrong anyway

`sdmc:/atmosphere/crash_reports/` and `sdmc:/config/SwitchU/` are what make a
report actionable, together with the `SwitchU-symbols-*` artifact from the
**same build** — symbols from a different build resolve to the wrong lines and
send the search in the wrong direction.
