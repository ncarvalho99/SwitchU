<div align="center">
    <h1>SwitchU</h1>
    <p>A Wii U-style custom home menu replacement for Nintendo Switch</p>
</div>

<p align="center">
  <a rel="LICENSE" href="https://github.com/PoloNX/SwitchU/blob/master/LICENSE">
    <img src="https://img.shields.io/static/v1?label=license&message=GPLV3&labelColor=111111&color=0057da&style=for-the-badge" alt="License">
  </a>
  <a rel="VERSION" href="https://github.com/PoloNX/SwitchU/releases">
    <img src="https://img.shields.io/static/v1?label=version&message=1.1.0&labelColor=111111&color=06f&style=for-the-badge" alt="Version">
  </a>
  <a rel="BUILD" href="https://github.com/PoloNX/SwitchU/actions">
      <img src="https://img.shields.io/github/actions/workflow/status/PoloNX/SwitchU/switch.yml?branch=master &labelColor=111111&color=06f&style=for-the-badge" alt=Build>
  </a>
</p>

---

- [Features](#features)
- [Screenshots](#screenshots)
- [How to build](#how-to-build)
- [Help me](#help-me)
- [Credits](#credits)
- [License](#license)


## Screenshots

![](./screenshots/1.jpg)

<details>
  <summary><b>More screenshots</b></summary>

![](./screenshots/2.jpg)
![](./screenshots/3.jpg)
![](./screenshots/4.jpg)
![](./screenshots/5.jpg)

</details>

## How to build

### Requirements

- [devkitPro](https://devkitpro.org/wiki/Getting_Started)
- [Xmake](https://xmake.io/#/)

### Clone

```bash
git clone --recursive https://github.com/PoloNX/SwitchU
cd SwitchU
```

### Build (production daemon + external menu mode)

```bash
xmake f -p cross --toolchain=devkita64 --homebrew=n --backend=deko3d
xmake
```

### Build (homebrew .nro mode)

```bash
xmake f -p cross --toolchain=devkita64 --homebrew=y --backend=deko3d
xmake
```

### Clean

```bash
xmake clean
```

Build outputs are generated under `build/cross/aarch64/<mode>/`.

## Know issues
- Some settings are not implemented yet
- Current icons are very ugly, feel free to replace them with better ones
- You may experience somme crash when using a lot of sysmodules. I tried to reduce the ram usage but I still get some instability when launching a game.

## TODO
- Add a pannel when pressing + on a game to show more information about it
- Add video in background for the home menu
- Add a steamgriddb integration to get game banners and backgrounds

## Help me

If you want to help, open an issue when you find a bug and open a pull request if you have a fix.

## Credits

- Thanks to [Xortroll](https://github.com/Xortroll) for the help and for [uLaunch](https://github.com/Xortroll/uLaunch) which inspired this project a lot

## License

This project is licensed under the GNU General Public License v3.0. See the [LICENSE](https://github.com/PoloNX/SwitchU/blob/master/LICENSE) file for details.

---

# Homebrew launching — test build

This branch adds launching homebrew from the grid. It is **not** in a release
and is not the default; it is here to be tried and reported on. Read
[docs/homebrew-recovery.md](docs/homebrew-recovery.md) before turning it on —
it is short, and it is the way back out.

## What it does

`.nro` files on the card appear in the grid beside your games. Opening one runs
that homebrew, and leaving it returns to the page you started from.

Everything is under **SwitchU → Homebrew**, in the order you meet it: turn it
on, choose what it costs, restart the menu to see it, then manage the files.

## What it costs, and why

A home menu cannot run an `.nro` by itself. Homebrew runs inside `nx-hbloader`,
and `nx-hbloader` runs by taking the place of a system applet. So one applet
stops being itself while this is on, and which one is yours to pick:

| Setting | What stops working |
| --- | --- |
| **Parental controls** (default) | The PIN prompt is replaced, so parental controls stop being enforced |
| Album | Opening the Album gives homebrew instead of the screenshot gallery |

Parental controls is the default because most consoles never set it up. **If
yours has, switch to Album before enabling this.** The restrictions keep looking
configured in Settings while nothing applies them, and the console says nothing
about it. Holding **R** while the system raises a PIN prompt gives the real
applet, so it stays reachable for anyone who knows.

Atmosphère keeps **one** loader path for the whole system, so while this is on,
SwitchU's loader answers for every homebrew override on the console — including
one you set up on the Album yourself. The previous value is saved and put back
when you turn this off.

## Turning it off is the whole way back

Everything this writes lives on the microSD card under `atmosphere/`. Nothing
touches internal storage. Switching it off in the menu removes what it added; if
the console will not boot, deleting
`atmosphere/config/override_config.ini` from a computer does the same. The
override only takes effect when an applet launches, long after boot, so it
cannot stop the console starting. Details, including how to get parental
controls back, are in [docs/homebrew-recovery.md](docs/homebrew-recovery.md).

## What does not work, and will not

**Ports installed by another homebrew.** PortNX and the like install a shortcut
of their own that passes the port arguments saying which game to load. Launching
the `.nro` from the grid passes only its path — which is all any plain launcher
does — so a port that needs telling reports a missing file and stops. Those
arguments live inside the installed shortcut in a format nobody promises to
keep, so nothing here can recover them. **Open those from the shortcut their
installer made.**

Self-contained homebrew is unaffected. A port that finds its own data by a fixed
path, like Render96, launches fine.

## Hiding duplicates

An installed shortcut and the emulator behind it are two entries for what looks
like one thing — PortNX gives Ocarina of Time its real name and icon, and Ship
of Harkinian then shows up under its own. Which of the two you want is not
something to guess at, so **SwitchU → Homebrew** lets you take any file out of
the grid while leaving it on the card. Leave it on the card: the shortcut needs
it.

Deleting is there too, and removes the `.nro` only. The folder beside it stays,
because those hold saves and settings — JKSV keeps its backups in one.

## How it works

- `lib/switchu-hbloader/` — a vendored copy of
  [nx-hbloader](https://github.com/switchbrew/nx-hbloader) v2.4.5 with one
  change. Upstream always opens `hbmenu.nro` when started as an applet: the path
  it runs is only ever set by homebrew already running, through the
  `envSetNextLoad` ABI, so a menu starting it has no way to name a target. The
  fork reads one line from `sdmc:/config/SwitchU/next_nro.txt`, deletes it, and
  falls back to hbmenu exactly as upstream does when there is no request. Built
  from source by the `switchu-hbloader` xmake target, so the binary that ships
  and the source in this tree cannot drift apart. ISC, notice included.
- `projects/menu/src/launcher/HblOverride.*` — writes and removes the override.
  It adds one entry in the first free slot, leaves every other line as found,
  backs the file up first, and never overwrites anybody's own `hbl.nsp`.
- `SystemMessage::LaunchHomebrew` — the menu does not start applets, it asks the
  daemon. This is the first `Launch*` message carrying an argument, because
  every other one has a fixed applet and this one's is a setting.
- `tests/hbl_override/` — runs the override writer on a PC against a fake SD
  tree. It caught two defects a compiler never would.

## Reporting a problem

Turn the console off, put the card in a computer, and send whatever is in
`atmosphere/crash_reports`. Say which build. The `.log` files there are plain
text with nothing personal in them. Without them there is nothing to go on — the
console shows a code that says a crash happened and nothing about where.

