# SwitchU transition-speed audit

- Audit date: 2026-08-24
- Latest hardware follow-up: 2026-08-27
- Audited source: `309735b` plus the latency changes described here; latest controlled production build `c952106`
- Production topology: qlaunch-replacement daemon plus external all-foreground library-applet menu

## Outcome

The largest launch delay was not Horizon title creation. SwitchU deliberately waited for a 1.45-second visual sequence before sending any launch or resume command. After the command, the daemon waited for the disposable menu process to die before starting the title. The recorded median from command receipt to application foreground request completion was 1.1 seconds. Thus, the pre-audit median lower bound from activation to the foreground request was about 2.55 seconds, plus the unmeasured synchronous configuration write and SD commit.

This audit lands a conservative optimization set. The visual gate is now 340 ms, the recency write overlaps it, startup catalog I/O overlaps other initialization, unused overlay trees are created on first use, redundant service sessions and IPC payloads are removed, render and background work are separated by core, and process teardown no longer repeats global GPU waits or waits on polling/network timeouts. The source-level launch gate is 1.11 seconds shorter. ARM-counter traces prove that the recency commit is hidden by the animation, and the controlled production matrix below establishes current-build distributions. A same-console pre-change distribution is still absent, so these results do not by themselves prove a distribution-level speedup.

The largest remaining return cost is architectural. SwitchU destroys the menu process before a title receives the foreground, then creates a new menu process after HOME or title exit. The daemon survives, but the menu's heap, widget tree, fonts, GPU context, textures, and service sessions do not. Existing logs show a median 1.8 seconds from HOME to `MenuReady` and 1.9 seconds from natural application exit to `MenuReady`.

Raw `.nro` launching is not implemented. Items described as homebrew in the production menu are installed forwarder applications and use the same title-ID path as games. The standalone SwitchU `.nro` target is a UI/test build, not a production NRO dispatcher.

## Measurement baseline

These figures were derived from three captured daemon sessions under `.logs/3` and the return session `.logs/9/daemon-1970-01-01_00-00-27-027.log`. Timestamps have only 100 ms resolution. Percentiles use the nearest-rank observation, so the results locate the major costs but do not replace the millisecond instrumentation specified later.

| Interval | Samples | Minimum | Median | p95 | Maximum |
| --- | ---: | ---: | ---: | ---: | ---: |
| Launch command received to menu holder finished | 31 | 700 ms | 900 ms | 1,100 ms | 1,200 ms |
| Menu holder finished to application foreground request complete | 31 | 0 ms | 200 ms | 800 ms | 900 ms |
| Launch command received to application foreground request complete | 31 | 900 ms | 1,100 ms | 1,600 ms | 1,700 ms |
| HOME request to `MenuReady` | 15 | 1,300 ms | 1,800 ms | 2,000 ms | 2,000 ms |
| Natural application finish to `MenuReady` | 15 | 1,700 ms | 1,900 ms | 2,000 ms | 2,100 ms |

Representative evidence:

- `.logs/3/daemon-1970-01-01_00-00-25-964.log`: command at `00:04:42.5`, holder finished at `00:04:43.6`, foreground request completed at `00:04:43.7`, and launch completed at `00:04:43.8`.
- The same session: command at `00:07:27.1`, holder finished at `00:07:27.8`, and a previous application join plus the new foreground request completed at `00:07:28.4`.
- `.logs/9/daemon-1970-01-01_00-00-27-027.log`: HOME at `00:01:17.1`, holder start completed at `00:01:17.2`, and `MenuReady` arrived at `00:01:18.8`.
- The same session: application finish at `00:01:45.9` and `MenuReady` at `00:01:47.7`.

The baseline excludes the old 1.45-second source-defined animation and does not isolate the configuration commit. Before this patch, the full user-visible launch was therefore longer than the command-to-foreground table indicates.

## Hardware follow-up: deployed audit build

The deployed daemon and menu matched the audited build by SHA-256: daemon `exefs.nsp` `53D86264DEAB87966ACB2F62F85B5E1A037DF0853352C72EAE6FC249CB5F89E3` and menu `main` `E7B9310F3149D620EB5DDD96479F305032F9DEF37F077D3E3C951FC9EBDE7708`. The 2026-08-24 smoke test exercised Mii Editor, one installed official title (`01006BB00C6F0000`), one installed homebrew forwarder (`05446530ACA7E000`), two HOME returns, and reboot. It did not exercise arbitrary raw NRO launch.

| Observed interval | Sample | Historical median | Interpretation |
| --- | ---: | ---: | --- |
| Official title: command to holder death | 800 ms | 900 ms | Successful and 100 ms below the old median, but one sample at 100 ms resolution cannot establish improvement. |
| Official title: command to foreground request complete | 1,000 ms | 1,100 ms | Successful and within the old distribution. |
| Forwarder replacing the official title: command to holder death | 700 ms | 900 ms | Successful and within the old distribution. |
| Forwarder replacing the official title: command to foreground request complete | 1,200 ms | 1,100 ms | Includes about 400 ms to close the previous application; still within the old distribution. |
| HOME from official title to `MenuReady` | 1,800 ms | 1,800 ms | No return improvement established. |
| HOME from forwarder to `MenuReady` | 2,000 ms | 1,800 ms | No return improvement established. |
| Menu `onCreate` to first frame | 1,100 ms cold; 900-1,000 ms on return | Not previously isolated | Confirms cold menu reconstruction remains the dominant return-side target. |

The functional results from the first build were mixed:

- Every menu holder exited with reason 0; both titles reached foreground; both HOME returns reached `MenuReady`; reboot remained responsive; and no new fatal report appeared.
- Recency persistence worked: `config.json` recorded sequences 90 and 91 for the two tested title IDs. The production battery widget consumed daemon status, confirming the redundant direct PSM path is gone.
- Lazy overlays worked. Settings was not built during startup, its first open cost 326 ms, and its cached second open cost 0 ms. Theme Shop was built only on first use.
- Exact-size/one-way SMI, asynchronous catalog loading, core placement, and one-drain GPU teardown were functionally stable in this smoke test. Their individual latency effects were not observable with the current markers. HTTP cancellation was not exercised because no transfer was active at handoff.
- The 340 ms launch gate and overlapped recency commit cannot be measured from these logs. Menu-side launch messages were buffered away at process exit, and there are no activation, animation-complete, commit-complete, or GPU-drain markers.
- The Bluetooth stop-event change failed on every fresh menu with `0x10801`, Horizon kernel `ResultLimitReached` (description 132). The follow-up source removes that event, clears `g_threadRunning`, cancels the existing worker wait with `svcCancelSynchronization(g_thread.handle)`, and retains the 500 ms timeout only as a race fallback. Misleading unconditional initialization messages were also corrected. The fix is built and deployed; Bluetooth connect/disconnect and handoff teardown still require hardware validation.

This session was a smoke test, not the required performance run. The deployed follow-up build transports raw ARM-system-tick landmarks across menu/daemon IPC and emits bounded `[trace-*]` records only after the measured endpoint. The controlled matrix was completed later as recorded below; the 100-cycle soak remains pending.

## Hardware follow-up: millisecond traces and empty Bluetooth discovery

The 2026-08-25 connected-card run exercised official title `0100F2C0115B6000`, installed homebrew forwarder `05446530ACA7E000`, and two HOME returns. `tools/analyze-transition-traces.ps1` parsed all bounded trace records successfully. No fatal, holder, or foreground error appeared.

| Instrumented interval | Official title | Forwarder | Finding |
| --- | ---: | ---: | --- |
| Activation to launch command | 1,158.132 ms | 339.119 ms | The official sample includes 824.623 ms in the user picker; the forwarder had no meaningful picker delay. |
| User decision through animation | 333.480 ms | 321.839 ms | Confirms the reduced visual gate on hardware. |
| Animation end to recency commit | 0.002 ms | 0.003 ms | The durable save was fully hidden by the animation in both samples. |
| Command to menu-holder death | 1,304.517 ms | 892.705 ms | Holder/process teardown remains a major launch cost. |
| Command to application foreground | 1,663.859 ms | 1,775.636 ms | The forwarder includes 659.075 ms closing the previous title. |
| Activation to application foreground | 2,821.992 ms | 2,114.755 ms | End-to-end trace endpoint is the foreground request, not a photographed first game frame. |

The two reason-2 HOME returns reached the first input-dispatchable menu frame in 2,142.189 ms and 2,116.309 ms. Cold `WiiUMenuApp::onCreate` consumed 971.205 ms and 891.428 ms; holder start to menu `main` consumed 649.215 ms and 677.563 ms. These measurements reinforce the next targets: cold menu construction and holder/process handoff. They are not yet a 30-sample baseline.

Bluetooth initialization is now hardware-closed: the corrected build logged `No connected audio device`, `Initialized`, and `Thread started` without `0x10801`, and one short lifecycle logged `Thread exiting` plus `Finalized`. However, three scans lasting about 29.4 s, 8.5 s, and 17.4 s produced no discovered-device change. The tested source wrote only the persistent `set:sys` Bluetooth flag rather than applying the live `btm:sys` radio, depended on a connection event that does not announce discovery-list updates, and hid discovered results after scanning. The same traces exposed 208.023 ms and 481.513 ms Bluetooth shutdown spans, consistent with a cancellation-before-wait-registration race.

The deployed 2026-08-25 correction now queries and controls the live `btm:sys` radio, polls discovery state and results every 500 ms, keeps completed results visible, and uses libnx `waitMulti` with a user-mode `UEvent` for race-free shutdown without another persistent kernel handle. Bluetooth discovery here means Bluetooth audio endpoints, not general phones, controllers, or keyboards; see Nintendo's [Bluetooth audio pairing guidance](https://www.nintendo.com/en-gb/Support/Troubleshooting/How-to-Add-and-Manage-Bluetooth-Audio-Devices-on-Nintendo-Switch-2040147.html) and the [Horizon BTM interface](https://switchbrew.org/wiki/BTM_services). At deployment time, fresh hardware discovery, connect/disconnect, and teardown timing remained pending.

The next connected run closed the discovery/connection portion of that gate. The manager reported live/configured radio state `1/1`, began with zero results, found one device after 11.8 seconds, completed with one result after 16.3 seconds, and then logged `Paired devices changed (1)` plus `Connected: Headphone GSOB`. All BTM results were `0x0`, the settings screen stayed at 60 FPS, and neither log contained a fatal, holder, rendering, or daemon error. The session rebooted without disconnecting or launching a title, so `UEvent` handoff timing remains unmeasured.

The following connected run closed the primary preflight and Bluetooth-handoff gates. Official title `01006BB00C6F0000` performed 84.312 ms of touch/save work and finished it 220.790 ms before the authoritative launch command; installed forwarder `05446530ACA7E000` performed 51.332 ms and finished 238.816 ms early. Both reported `attempted=1 complete=1 cache_hit=1` and zero tail. The forwarder replaced the still-running official title, whose close consumed 314.288 ms. Command-to-foreground measured 852.073 ms for the official title and 1,141.121 ms for the replacement; these are diagnostics, not a speed distribution or a same-title before/after proof. Three launch/resume menu exits measured Bluetooth shutdown at 0.179 ms, 7.583 ms, and 0.173 ms, closing the former 208.023-481.513 ms lost-wake regression. Three HOME returns reached the first input-dispatchable frame in 1,983.695-2,145.164 ms, and one resume reached application foreground in 1,115.655 ms from activation. No Bluetooth, holder, foreground, rendering, fatal, or daemon error appeared.

The first nonblocking explicit-close hardware test also passed. Suspended official title `01006BB00C6F0000` completed in 318.745 ms: 2.626 ms terminating its library applets, 34.209 ms sending graceful exit, 281.909 ms waiting through ten nonblocking daemon polls, zero forced time, normal reason 0, and cores `3/3`. The session contains exactly one `[trace-terminate]`, one `source=terminate` completion, one outgoing notification, and one incoming `ApplicationExited`. The menu continued rendering and opened/closed Settings repeatedly after completion. No timeout, force, fatal, holder, foreground, rendering, or daemon error appeared. This closes only the cooperative graceful path; forced timeout, active-library-applet, duplicate request, sleep/wake, and immediate follow-up launch remain gates.

A second connected session repeated the cooperative close in 287.909 ms: library-applet IPC 5.160 ms, exit-request IPC 36.760 ms, graceful polling 245.989 ms across eight polls, no force, all AM results `0x0`, and normal reason 0. Exactly one trace, completion route, outgoing notification, and incoming notification appeared again. The same menu launched installed forwarder `05446530ACA7E000` about 6.7 seconds later; preflight finished 228.381 ms before the authoritative command and command-to-foreground took 802.051 ms. Sleep/wake while the forwarder ran, its HOME return, and a later menu sleep/wake were clean. This validates sequential close-then-launch and ordinary sleep/wake stability, but not a launch queued during the 287.909 ms close window, close during sleep/wake, duplicate close, active library applet, or forced timeout.

A third attempt closed official title `01007EF00011E000` cooperatively in 350.393 ms: 6.612 ms library-applet IPC, 34.568 ms exit-request IPC, 309.213 ms graceful polling across seven polls, no force, all results `0x0`, normal reason 0, and cores `3/3`. Exactly one trace, completion route, and notification appeared with no fatal/error. Forwarder `05446530ACA7E000` still sent its preflight about 3.6 seconds after close completion and its authoritative command about 3.9 seconds after completion. The 87.137 ms preflight was reused and launch reached the foreground 867.061 ms after the command, but the ordering remained sequential; a human cannot reliably beat a roughly 300 ms cooperative close.

The follow-up therefore added an opt-in `-TerminationQueueTest` sysmodule build mode. It preserves the 15-second force deadline but holds termination event ownership for a minimum of eight seconds before observing cooperative completion, making the queue ordering human-testable without changing normal builds. It also corrected the real slow-path found by review: when the old menu holder is gone and a launch/applet action is already queued, completion now skips the redundant cold menu relaunch and consumes the queued action directly. The diagnostic build is not a performance build and must be replaced by a normal release immediately after this gate passes.

Hardware then exercised that exact branch. The diagnostic close retained ownership for 8.001 seconds, with 2.579 ms library-applet IPC, 37.681 ms exit-request IPC, 7,961.197 ms in the synthetic grace span, 728 polls, no force, all results `0x0`, normal reason 0, and cores `3/3`. Forwarder `05446530ACA7E000` queued about 4.223 seconds before completion; after the old menu holder died, the action remained deferred another 3.495 seconds. Ordering was exact: queued launch, holder death, one termination trace, one `source=terminate` route, one `menu relaunch skipped`, then one action handling and successful foreground. No `ApplicationExited` was sent because its menu receiver was already dead. Preflight remained inside the five-second reuse bound at 4.477 seconds and reported `cache_hit=1`. HOME returned from the forwarder to the first input-dispatchable frame in 1,779.973 ms, reboot completed, and no fatal, error, forced close, duplicate route, or throwaway menu appeared. The 4.238-second command-to-foreground value includes the deliberate hold and is not a performance sample.

The same opt-in switch now builds an explicit edge-test suite instead of applying the old eight-second hold to every close. The diagnostic close dialog exposes ordinary close, duplicate close, sleep/wake, and force-at-15-seconds actions. Duplicate sends two ordinary command-3 storages back to back. Sleep sends the sleep request automatically and keeps AM event ownership for 12 seconds, leaving the original deadline untouched. Force deliberately ignores an already-signalled cooperative completion until 15 seconds, sends the exact production `appletApplicationTerminate` IPC once, and permits the next loop to observe/join. That last action validates state-machine ordering and single-route behavior; it is not evidence from a live title that truly refuses graceful exit.

Hardware passed three suite actions on official title `01006BB00C6F0000`. Duplicate close completed once in 345.147 ms after one begin and one already-pending guard: 5.699 ms library IPC, 38.143 ms request IPC, 301.304 ms graceful polling, eight polls, no force, reason 0, and cores `3/3`. Sleep/wake requested sleep 0.2 seconds after close began, woke about 5.2 seconds later, restored the menu and 60 FPS rendering, then completed once at 12,039.924 ms after the deliberate 12-second hold; all AM results were `0x0`, `forced=0`, reason 0, and cores `3/3`. Controlled force sent `appletApplicationTerminate` once at 15,004.329 ms after request IPC and completed 12.732 ms later: 15,053.634 ms total, 1,475 polls, `forced=1`, all results `0x0`, reason 2, and cores `3/3`. Session totals were exactly three begins, three traces, three `source=terminate` routes, and three outgoing notifications. Duplicate and force notifications were durably received by the menu; the sleep menu's buffered tail ended before its receive record, but daemon delivery had one push and the next title launch succeeded. No duplicate route or log-visible lifecycle, holder, foreground, Bluetooth, rendering, or fatal fault appeared.

## Controlled transition matrix: normal production build

The final normal-build matrix used Stardew Valley `0100E65002BB8000` as the official title and installed sphaira forwarder `05446530ACA7E000`. It contains ten accepted cold official-title cycles, ten strict cold-forwarder cycles, and fifteen Stardew-to-sphaira replacements. The owner performed five replacement cycles beyond the planned minimum; all are retained because conditions stayed controlled and no sample was selected by result or duration. Three additional forwarder activations that overlapped a preceding close remain separate stress evidence rather than cold timing samples.

| Interval (ms, p50 / p95) | Cold official, n=10 | Cold forwarder, n=10 | Official-to-forwarder replacement, n=15 |
| --- | ---: | ---: | ---: |
| Activation to command | 1,292.530 / 1,376.532 | 334.578 / 343.461 | 335.861 / 345.452 |
| Command to foreground | 822.114 / 877.504 | 757.767 / 866.783 | 1,035.525 / 1,119.299 |
| Previous-title close | 0 / 0 | 0 / 0 | 197.638 / 230.212 |
| Preflight lead | 254.583 / 261.667 | 235.012 / 254.856 | 243.991 / 259.877 |
| Preflight tail | 0 / 0 | 0 / 0 | 0 / 0 |
| Menu GPU drain | 29.450 / 30.229 | 20.900 / 30.433 | 21.169 / 30.317 |
| HOME to menu ready | 1,650.329 / 1,805.206 | 1,626.706 / 1,904.257 | 1,628.264 / 2,143.496 |
| HOME to first input frame | 1,907.834 / 2,135.597 | 1,882.814 / 2,267.658 | 1,900.519 / 2,547.046 |
| Return `onCreate` | 933.912 / 981.375 | 869.159 / 915.337 | 843.244 / 960.558 |

All 35 accepted cycles reused complete preflight work with no post-command tail, reached foreground, returned through HOME, and completed the requested close without a lifecycle error or forced termination. Across the baseline-to-final interval, fatal and top-level crash inventories stayed at 8/18. All 71 new ERPT files contained routine code `2123-0011`; none contained forced-shutdown code `2165-1002`.

No replacement-confirmation dialog is expected in this architecture. SwitchU is the qlaunch replacement and [launchApplication](../projects/daemon/src/app_manager.hpp#L594) directly requests graceful exit and joins the suspended application before creating the destination. Every one of the fifteen destination requests recorded Stardew as suspended, entered `closing previous app before launch`, and measured a nonzero previous close of 185.025-230.212 ms. The missing stock dialog therefore did not skip replacement; it confirms that SwitchU used its own direct handoff. Full derived evidence and distributions are in the [replacement analysis](../.logs/transition-matrix-replacement-20260827-152624/analysis.md).

The matrix establishes the current build's distribution, not a causal before/after comparison. HOME return remains dominated by rebuilding the disposable menu, especially `onCreate`. On replacement, the old-title close adds about 200 ms while application Start and foreground IPC remain only tens of microseconds. These results make GPU upload batching and cold asset-construction deduplication the next measurable optimization stage.

## Execution model

### Production mode

The daemon is program `0100000000001000`, declares `AppletType_SystemApplet`, starts on core 3, and has an 8 MiB heap. It replaces qlaunch and owns application lifecycle, HOME routing, system events, the application catalog, and the external-code filesystem server.

The visual menu is program `010000000000FFFF`, substituted for PhotoViewer program `010000000000100D`. The daemon registers Atmosphere external code, creates PhotoViewer as `LibAppletMode_AllForeground`, and pushes the menu state into its holder. The menu starts on core 0, may use cores 0-2, and negotiates a 224-416 MiB heap. See [menu_launcher.hpp](../projects/daemon/src/menu_launcher.hpp#L28), [ecs.cpp](../projects/daemon/src/ecs.cpp#L95), [menu.json](../projects/menu/menu.json), and [daemon.json](../projects/daemon/daemon.json).

Atmosphere commands 65000 and 65001 are extensions to `ldr:shel`, not stock Horizon calls; the repository constants and dispatcher match the [Atmosphere loader documentation](https://github.com/Atmosphere-NX/Atmosphere/blob/master/docs/components/modules/loader.md).

### Installed title path

An installed NSP, an installed cartridge/XCI-derived title, a game-card record, and an installed homebrew forwarder are not launched as files. Once installed and visible to `ns`, each is an application title ID. SwitchU sends that ID and an optional account UID to the daemon, and the daemon uses `AppletApplication`:

1. The menu acknowledges activation, immediately sends one-way `PrepareApplication`, persists recency, and later sends the authoritative `LaunchApplication` storage. The daemon can touch the title and probe save data while the menu still renders the 340 ms acknowledgement.
2. The menu requests its own exit.
3. The daemon queues the action but refuses to execute it while the menu holder exists.
4. After holder completion, the daemon reuses a fresh, exact, successful preflight or repeats the original synchronous touch/save path, then creates and starts the application and requests its foreground.

The exact gate is [handleAction](../projects/daemon/src/main.cpp#L1425). The exact AM path is [app::launch](../projects/daemon/src/app_manager.hpp#L358). libnx implements application join as an infinite wait on the state event and holder/application start as synchronous IPC plus event coordination; see the primary [libnx applet implementation](https://github.com/switchbrew/libnx/blob/master/nx/source/services/applet.c) and [applet API](https://github.com/switchbrew/libnx/blob/master/nx/include/switch/services/applet.h).

### Resume path

Resume uses the same menu animation and process exit, but skips title creation and save-data preflight. After holder death, [app::resume](../projects/daemon/src/app_manager.hpp#L503) unlocks the foreground and calls `appletApplicationRequestForApplicationToGetForeground`. The previous 1.45-second animation was therefore especially wasteful on resume; the Horizon work is normally only one request.

### Return path

HOME causes the daemon to mark the application suspended, unlock/take the foreground as required, register external menu content, and start a new library-applet holder in [openMenuFromHome](../projects/daemon/src/main.cpp#L767). Natural title completion is detected by [app::checkFinished](../projects/daemon/src/app_manager.hpp#L684); the daemon joins/closes the application and launches the same fresh menu in [mainLoop](../projects/daemon/src/main.cpp#L1576).

`MenuReady` currently means `WiiUMenuApp::onCreate` completed, not that the first frame reached the display. True glass-to-glass return time is therefore slightly longer than the log interval.

### Raw NRO path

No `.nro` enumeration, hbloader environment detection, `envSetNextLoad`, NRO mapping, or argv handoff exists in the repository. The production catalog comes from installed `ns` application records. `SWITCHU_HOMEBREW` only changes SwitchU itself into a standalone NRO and stubs production launch actions.

The correct implementation is a dedicated installed NRO-host application using the hbloader loader engine. The daemon would launch the host as a normal `AppletApplication`; the requested, validated `sdmc:/switch/.../*.nro` path would be delivered through a private launch parameter or an atomic request file. The host would map and run the NRO using the model in [nx-hbloader](https://github.com/switchbrew/nx-hbloader/blob/master/source/main.c). Within an existing hbloader host, `envSetNextLoad` can chain to another NRO, as documented by the [libnx environment API](https://github.com/switchbrew/libnx/blob/master/nx/include/switch/runtime/env.h), but it cannot turn the current system-applet daemon into an arbitrary NRO process launcher.

## IPC and Horizon service audit

| Service area | Current use | Latency finding | Action |
| --- | --- | --- | --- |
| AM / libapplet | Menu holder, installed applications, foreground, HOME, system library applets | Holder death remains serialized before application close/create/start, but title touch/save preflight can now overlap the menu animation. Previous-app and shutdown cleanup joins can still wait forever. Explicit close now polls without blocking and retains its 15-second grace period. | Keep AM ownership in daemon. Hardware-validate nonblocking close and the remaining preflight safety matrix. Never force earlier than the existing grace period without save-integrity testing. |
| `ns` | Daemon catalog/view/control cache; per-launch `nsTouchApplication`; menu settings/details/delete | `nsTouchApplication` and up to five save-data probes were wholly on the critical path; the deployed fail-closed preflight can overlap them. Menu eagerly opens a second `ns` session although first paint uses the daemon catalog. Catalog listing is chunked at 30 records. | Validate the preflight safety matrix, lazy-open menu `ns`, increase record chunks after stack/heap measurement, and pause cache work through first menu frame. |
| `pm`, `pdm` | No calls in SwitchU source | There is no PM/PDM overhead to remove. Application lifecycle is AM-based. Adding PM termination would duplicate AM state and risk fighting qlaunch semantics. | Do not add PM/PDM merely for speed. Use AM state events already owned by the daemon. |
| `sm` | Initialized once per daemon and once per fresh menu | No redundant per-command `smInitialize`. The repeated cost is a consequence of recreating the menu process. | Keep daemon session warm. Lazy-open menu-only settings services. |
| `set` | Locale selection | Needed before i18n. It is small and correctly process-scoped. | Keep on first-paint path unless locale is sent by daemon. |
| `set:sys` | Firmware version, sleep plan, settings tabs | Needed for HOS version setup and the current sleep backstop, but many settings IPC calls are only needed when Settings opens. | Cache firmware/sleep snapshot in daemon or retain current session; construct Settings lazily. |
| FS / SD | Config, layout, title cache, icons, ECS filesystem, logging | Synchronous full JSON save plus `fsdevCommitDevice` followed the old animation. ECS opens an SD filesystem on every menu start. Immediate logs compete for SD during startup. | Recency save now overlaps animation and still commits before exit. Consider a daemon-owned append-only recency journal and persistent ECS registration. |
| Atmosphere `ldr:shel` | External menu content registration | Register/unregister plus SD/subdirectory server setup repeats on every menu lifetime. Logs show holder start is usually only 0-100 ms, so this is secondary to cold menu initialization. | Hardware-test keeping registration alive for daemon lifetime. |
| Account | Profile enumeration, avatar reads, preselected-user launch data | Every fresh menu opens every profile and uploads every avatar synchronously. | Cache selected UID/nickname in daemon; decode only visible avatars initially. Preserve interactive selection rules from NACP. |
| PSM | Daemon event/initial state; formerly menu polling every second | Production opened two PSM clients and polled despite daemon notifications. | The redundant production menu client/poll is removed. Standalone NRO mode retains direct PSM. |
| PL | Formerly initialized in menu, never called | Pure redundant service initialization. | Removed. |
| LBL, SPL | Display and Atmosphere details tabs | They were opened on every fresh menu although only Settings uses them. | Next step: RAII lazy service holders created with Settings. |

## Threading and core-affinity audit

| Thread | Priority / core before audit | Work | Audit result |
| --- | --- | --- | --- |
| Daemon main | priority 40, core 3 from `daemon.json` | AM state machine, menu commands, title launch/return | Correct owner and core. Its 10 ms polling loop remains, but no rendering shares this process. Do not move it to application cores. |
| Daemon applet-event worker | priority 44, inherited/ideal core before audit | AM event reception and notification publication | Pinned to core 3. |
| Daemon control-cache worker | priority 45, inherited/ideal core before audit | `ns` control-data IPC plus SD metadata/icon writes | Pinned to core 3. It must still be paused around the first menu frame to remove SD contention. |
| Menu main/render/input | priority 44, core 0 from `menu.json` | deko3d submission, widgets, input, final texture uploads | Correctly remains core 0. No title launch IPC runs here after the handoff command, but synchronous GPU upload waits still block it during startup. |
| Two nxui pool workers | inherited process mask 0-2 | catalog parse, HTTP, decode, config/theme/artwork writes | Pinned round-robin to cores 1 and 2. The pool still drains queued work at destruction; cancellation remains mandatory for long jobs. |
| Bluetooth event worker | priority 44, inherited/ideal core before audit | `btm:sys` event wait and device-list IPC | Pinned to core 2. Connection changes wake through the Horizon event; active discovery is polled every 500 ms. A user-mode `UEvent` participates in the same `waitMulti` call, so shutdown cannot lose its wake and consumes no extra persistent kernel handle. Discovery/connect and three launch/resume handoffs passed on hardware. |
| Animated-background reader | C++ thread with process mask 0-2 before audit | sequential DDS reads and CPU-side queue fill | Pinned to core 2 so animated-theme I/O cannot migrate onto the render core. |

No process fork/exec primitive exists in this pipeline. Horizon application and library-applet creation is IPC managed by AM/libapplet; the expensive CPU-side work is around that IPC. The synchronous eSpeak path also does not create a SwitchU worker: `AccessibilityManager` requests `AUDIO_OUTPUT_SYNCHRONOUS` and runs synthesis on its caller. Core affinity calls are best effort because libnx returns a `Result`; profiling must record `svcGetCurrentProcessorNumber()` so a firmware or capability failure is visible instead of assumed away.

## Exact bottlenecks

### P0: title entry

1. [LaunchAnimation.hpp](../projects/menu/src/widgets/LaunchAnimation.hpp#L34) and [LaunchAnimation::onUpdate](../projects/menu/src/widgets/LaunchAnimation.cpp#L29) formerly delayed both launch and resume callbacks for exactly 1.45 seconds. The new total is 340 ms.
2. [WiiUMenuApp::makeIcon](../projects/menu/src/core/WiiUMenuApp.cpp#L1062) performed a complete, pretty-printed config save and SD commit after the visual wait. It now snapshots and submits that work before the animation, then waits only for any residual work before handoff.
3. [AppletLauncher::launchApplication](../projects/menu/src/launcher/AppletLauncher.cpp#L129) sends the authoritative command and requests menu exit. The earlier preflight lets touch/save work run while the holder lives, but [handleAction](../projects/daemon/src/main.cpp#L1425) still gates application close/create/start on holder death. The old trace showed a median 900 ms from command receipt to holder death.
4. [app::launch](../projects/daemon/src/app_manager.hpp#L358) requests exit and performs an infinite `appletApplicationJoin` when switching from an already-running application. It also closes `g_app` redundantly after that block.
5. The fallback [ensureApplicationSaveData](../projects/daemon/src/app_manager.hpp#L271) path reads cached NACP metadata and synchronously opens/probes account, device, temporary, cache, and BCAT save-data filesystems. Missing saves are created. The new prepared result bypasses this only after exact title/user, freshness, success, and metadata checks.
6. Preflight reuse, fallback touch/save, application creation/start, preselected-user storage, and foreground request remain ordered in [app_manager.hpp](../projects/daemon/src/app_manager.hpp#L358).

### P0: return and exit

1. [openMenuFromHome](../projects/daemon/src/main.cpp#L767) and natural-exit handling always create a new menu process. The recorded median return is 1.8-1.9 seconds.
2. [WiiUMenuApp::onCreate](../projects/menu/src/core/WiiUMenuApp.cpp#L274) initializes config, layout, i18n, accessibility, audio, Bluetooth, memory policy, fonts, game-card art, catalog, the full home tree, and profiles before `MenuReady`. Catalog I/O is now overlapped and the two large settings/theme overlay trees are lazy, but font and visible-shell work remain cold.
3. [loadResources](../projects/menu/src/core/WiiUMenuApp.cpp#L519) opens the same TTF at 24, 18, and 72 point sizes, opens the icon font, decodes game-card art, and finalizes the catalog.
4. [buildUserAvatarBar](../projects/menu/src/core/WiiUMenuApp.cpp#L539) lists every account, opens every profile, decompresses every avatar, and uploads each to the GPU synchronously.
5. Explicit close formerly blocked the daemon's only main loop for up to 15 seconds and dispatched AM command 30 directly. [beginTerminate](../projects/daemon/src/app_manager.hpp#L551) and [pollTerminate](../projects/daemon/src/app_manager.hpp#L604) now preserve that grace period through zero-time event polls; libnx `appletApplicationJoin` consumes command 30 only after the event is signalled. The cooperative path and queued handoff passed on hardware; the hostile edge suite remains pending.
6. `ThreadPool::~ThreadPool` in [ThreadPool.hpp](../lib/nxui/include/nxui/core/ThreadPool.hpp#L34) drains every queued task. A transfer queued before launch can therefore extend process death. HTTP handoff cancellation now covers both buffered and file transfers; task-level cancellation is still needed for all non-HTTP disk/decode jobs.

### P1: rendering and assets

1. [GpuDevice::uploadTexture](../lib/nxui/src/core/GpuDevice.cpp#L301) resets the upload command buffer, submits, and calls `m_queue.waitIdle()` for every texture or glyph. Startup font glyphs, icons, avatars, sidebars, and backgrounds therefore serialize the entire GPU queue.
2. [Texture::retireGpuResources](../lib/nxui/src/core/Texture.cpp#L37) formerly waited for the entire queue for every destroyed texture; [GpuDevice::shutdown](../lib/nxui/src/core/GpuDevice.cpp#L365) then waited again. Release shutdown now drains once through `beginBulkTeardown`.
3. [Renderer::loadShaders](../lib/nxui/src/core/Renderer.cpp#L73) reads shader binaries from SD. It reads `basic_vsh` three times, `blur_pass_vsh` twice, and `pass_vsh` twice into separate shader objects. Read each unique file once and share its code allocation where deko3d object lifetime permits.
4. Icon CPU decode is threaded, but final uploads remain main-thread/global-wait operations. Visible-page prioritization exists; a staging ring and fence-based upload batch is the next material GPU change.

### P1: daemon background work

1. [listApplicationRecords](../projects/daemon/src/main.cpp#L279) fetches only 30 records per synchronous `ns` call, up to 1,024.
2. The control-cache worker calls `nsGetApplicationControlData`, then writes `.meta` and icon files title by title. Captured individual calls ranged from a few milliseconds to over one second. It stops during application foreground but can still contend with a newly created menu.
3. Catalog rebuild clears and `shrink_to_fit`s vectors, discarding capacity useful for the next record event.
4. The daemon main loop sleeps 10 ms. This bounds many state transitions to a 10 ms polling quantum; it is not the source of the measured second-scale costs. Replace it with one multi-event wait only after every wake source is represented.

### P2: diagnostics and rare stalls

1. Startup file logging is forced immediate for 300 frames. That is useful for early crash capture but introduces SD flushes while the menu and catalog are reading the same card.
2. `getSystemStatus` polls up to 200 times at 10 ms. It is a startup-only request and normally completes quickly; its old 32 KiB stack buffer is now the exact 48-byte response size.
3. The old generic SMI reply path allocated and copied 32 KiB for commands whose senders never read replies. Those replies could also precede a later status response. Only `GetSystemStatus` now receives a response.
4. The first interruptible-Bluetooth correction used `svcCancelSynchronization` directly. Hardware traces measured 208.023-481.513 ms from HTTP cleanup to Bluetooth completion because cancellation could precede wait registration. `waitMulti` plus a user-mode `UEvent` now closes that race; the next hardware run must confirm a near-zero span.

## Changes implemented by this audit

| Change | Files | Expected effect |
| --- | --- | --- |
| Reduce launch/resume visual gate from 1,450 to 340 ms | `LaunchAnimation.hpp` | Removes a deterministic 1,110 ms before any IPC. |
| Overlap recency save/commit with the visual gate | `WiiUMenuApp.cpp` | Preserves the commit-before-exit guarantee while hiding most write latency. |
| Cancel all HTTP transfers at application/applet handoff | `WiiUMenuApp.cpp`, `ThemeHttp.cpp/.hpp` | Prevents process teardown waiting on a 30-second low-speed window; partial downloads are removed. |
| Start app-catalog load asynchronously and finalize during resource load | `WiiUMenuApp.cpp`, existing `AppListLoader` async API | Overlaps catalog SD/parsing work with i18n/audio/font setup. |
| Load config once and pass it into the activity | `main.cpp`, `WiiUMenuApp.cpp/.hpp` | Removes a duplicate JSON read/parse on every cold menu. |
| Create Settings and Theme Shop on first open | `WiiUMenuApp.cpp`, `WiiUMenuAppSettings.cpp` | Removes two large hidden overlay/widget trees and settings IPC from first paint; first open now pays that cost. |
| Remove unused PL and redundant production PSM clients | `main.cpp`, `BatteryWidget.cpp` | Fewer SM handshakes and no duplicate 1 Hz battery IPC. Daemon notifications supply production battery state. |
| Exact-size SMI storage and one-way commands | `smi_helpers.hpp`, `smi_commands.hpp`, daemon `main.cpp` | Replaces 32 KiB allocation/copy with exact 8-96 byte messages and eliminates unread reply objects. |
| One GPU drain for application teardown | nxui `Application`, `GpuDevice`, `Texture` | Removes one full-queue wait per texture plus the duplicate final wait. |
| Runtime-correct, interruptible Bluetooth worker | `BluetoothManager.cpp/.hpp`, `BluetoothTab.cpp`, `InternetTab.cpp` | Applies live radio state, polls audio discovery, keeps completed results visible, publishes list changes atomically, and wakes shutdown through a user-mode `UEvent` without `ResultLimitReached` or the direct-cancellation race. |
| Explicit core placement | `ThreadPool.hpp`, daemon `main.cpp`, `BluetoothManager.cpp`, `WaraWaraBackground.cpp` | Keeps menu rendering on core 0, menu workers/readers on cores 1-2, and daemon service/cache work on core 3. Priorities are unchanged. |
| Cross-process transition tracing | `smi_protocol.hpp`, `smi_commands.hpp`, menu/daemon launch code, nxui `Application` | Carries common ARM-counter landmarks without critical-path SD logging and reports launch, save-data, menu teardown, return-ready, and first-input-frame intervals in microseconds. |
| Overlap NS/save preflight with launch animation | `smi_protocol.hpp`, `smi_commands.hpp`, `AppletLauncher.cpp/.hpp`, `WiiUMenuApp.cpp`, daemon `app_manager.hpp` and `main.cpp` | Sends a one-way prepare after user selection. The daemon reuses it only for an exact title/user, successful work no older than five seconds, and byte-identical control metadata; otherwise the final launch repeats the original path. `[trace-preflight]` measures IPC, work, overlap, fallback, and cache hit. |
| Nonblocking explicit application close | daemon `app_manager.hpp` and `main.cpp` | Replaces the daemon-main-thread 15-second `eventWait` with a graceful/forced state machine polled every loop. It preserves the deadline, serializes AM event ownership, defers queued actions, skips a redundant cold menu when a destination is already queued, and emits one `[trace-terminate]` record at completion. |
| Lazy menu NS session | `core/NsService.cpp/.hpp`, `main.cpp`, `AppListLoader.cpp`, `WiiUMenuAppInteraction.cpp`, `StorageTab.cpp` | Removes `nsInitialize` from every cold menu process startup. Direct title-management features open one serialized session on demand and log its reason/result/duration; shutdown closes only a successfully opened session. |

libnx's `std::thread` setup applies the process CPU mask to new C++ threads, which is why explicit worker placement is needed; see [newlib.c](https://github.com/switchbrew/libnx/blob/master/nx/source/runtime/newlib.c) and [thread.c](https://github.com/switchbrew/libnx/blob/master/nx/source/kernel/thread.c).

Validation completed for both execution modes with:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build-local.ps1 `
  -Mode release -Variant sysmodule -SkipConsoleDeploy
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build-local.ps1 `
  -Mode release -Variant homebrew -SkipConsoleDeploy
```

After the preflight implementation, both execution modes were rebuilt from the exact final source. `release/homebrew` produced a 39,780,461-byte NRO with SHA-256 `B561E10EFDE4B220116EA6B0A12375D6AC858E1CF88246470CCA23D2A225AA86`; build and dist copies match. `release/sysmodule` produced `artifacts/SwitchU-sysmodule-release.zip`, 42,986,851 bytes with SHA-256 `8DC788FA839A2F68A5983FE3B91FA300D42E8C56BB2EC985A993271743A246D3`. Daemon `exefs.nsp` is `5EF1954C120DA1BF4D861526FC36ECFF36E35C5C24CDF2FAA37156658107BE8E`; menu `main` is `E544165474DF199BB4D16332E4B900D54E581B820F3FD64C4871C19F8EA35B75`; `main.npdm` remains `5D9EFE4E476B23F963F5CDE05DEE7FC591A216BDD8653FB50CAA2F17D42FB9CF`. Only the known Atmosphere nodiscard and `g_switchuHeapSize` toolchain warnings appeared. The sysmodule output was deployed to removable FAT32 card `G:`, and all three critical local/card hashes match. No commit, push, tag, release, server, or catalog mutation was performed.

After nonblocking termination, the affected production sysmodule was rebuilt twice: once without deployment and once through the standard verified deployment path. The final build completed in 2m45s with no warning/error lines and produced a 42,987,110-byte ZIP with SHA-256 `E0706F110334A577713F5C5B62AE87CA7939C17A7824100EAE6F9C12CCF71E5C`. Daemon `exefs.nsp` is 639,068 bytes with SHA-256 `70F56842894CF5B32264E307B0FFFBF387E4FC24A1CB8E4CD024E11D08CB3877`; the unchanged menu and NPDM retain the hashes above. All three local/card hashes match. The linked daemon contains `[trace-terminate]`; regenerated shader outputs were restored from the known-clean pre-build source state. The standalone homebrew build was not repeated because this patch changes only the production daemon, which that target does not contain.

After lazy NS initialization, both release variants were rebuilt from the final source. `release/homebrew` completed in 4m04s and produced a 39,780,461-byte NRO with SHA-256 `2C3750C8E62DA2CDA7451CD3165229BA770FCA60A70F470D58149FD5B222B186`; build and dist copies match. `release/sysmodule` completed and deployed in 5m42s, producing a 42,987,299-byte ZIP with SHA-256 `236310756CC518E0378A5BE2F357AB94BB8A408634F8E80FCC61AC2E26C25F8A`. The unchanged daemon remains `70F56842894CF5B32264E307B0FFFBF387E4FC24A1CB8E4CD024E11D08CB3877`; menu `main` is 7,793,076 bytes with SHA-256 `BCF5913951575332031A7E5CCD1271C1D948A47D92A186250688F4E4B6E0F9DD`; NPDM remains `5D9EFE4E476B23F963F5CDE05DEE7FC591A216BDD8653FB50CAA2F17D42FB9CF`. All three local/card hashes match. The uncompressed linked menu contains both lazy-NS telemetry strings. Build output contained the existing `g_switchuHeapSize` warning and two non-fatal detached-HEAD version-stamping diagnostics; both builds exited successfully. Regenerated shader outputs were restored from their verified clean pre-build source state. The hardware result is recorded below.

The deterministic queue-test sysmodule then built in 115.332 seconds and deployed through the verified non-mirroring path in 2m22s total. The 42,987,400-byte ZIP has SHA-256 `6707F2BCEE065F49982DC06CD787BC031AAD8FC5FBDD69FE5FE55FDB4C5F05D8`; daemon `exefs.nsp` is 639,794 bytes with SHA-256 `0B07C80B14C7855938CB55FE7C98732D773F426B4868D351502EE1080C51B214`. The unchanged menu and NPDM retain hashes `BCF5913951575332031A7E5CCD1271C1D948A47D92A186250688F4E4B6E0F9DD` and `5D9EFE4E476B23F963F5CDE05DEE7FC591A216BDD8653FB50CAA2F17D42FB9CF`; all local/card hashes match. The linked daemon contains both diagnostic-hold and queued-handoff markers. The only compiler warning was the known Atmosphere `nodiscard` warning, plus two non-fatal detached-HEAD version diagnostics. No tracked shader changed.

After the hardware gate passed, the normal release was rebuilt with the option explicitly off in 135.048 seconds and deployed in 3m47s total. The final 42,987,304-byte ZIP has SHA-256 `4D7762A88F1B6FD2F6EAEC5A12AFC45A5D6BB862E53C5FC9061C35056B251DCB`; daemon `exefs.nsp` is 639,508 bytes with SHA-256 `63A13E0DA867661AE1E4A1D13C06D30F657B0065CE996F31065DDB59708F00FC`. Local/card hashes match. The linked production daemon contains the queued-handoff marker and does not contain the diagnostic-hold marker. Menu and NPDM remain byte-identical to the hashes above. Build diagnostics were unchanged and no tracked shader changed.

For the first three close-edge gates, the expanded diagnostic release built in 161.706 seconds and deployed through the same verified `G:` path. Its 42,988,981-byte ZIP has SHA-256 `608F3C9B949F5ED67C2FB93B0B2966811AD92034E31C0E56756F6C8C499632D3`; daemon `exefs.nsp` is 640,119 bytes with SHA-256 `14763B1C19A0CE9EE3555D2ECE1DDDD03E2DBAF5B344CF1C63803C0176C10619`; menu `main` is 7,794,578 bytes with SHA-256 `6F49BB92EB018A656B7C989BF9E203C17576AAE120AECC128FD47C7F2995E999`; NPDM remains `5D9EFE4E476B23F963F5CDE05DEE7FC591A216BDD8653FB50CAA2F17D42FB9CF`. Local/card hashes matched and the linked binaries contained all marked controls. This is the exact build that produced the duplicate, sleep/wake, and controlled-force evidence above.

The active-library-applet follow-up added one diagnostic-only `AreAnyLibraryAppletsLeft` IPC before ordinary close and labeled that action `Applet close`. Its build completed in 168.151 seconds and deployed in 3m09s before card verification. The 42,988,972-byte ZIP had SHA-256 `B089093C0F79811CF7576D0EF8B40382DCF955AA1891F7C0558DE02113EC6DB8`; daemon `exefs.nsp` was 640,443 bytes with SHA-256 `BBC50B6849C30DE4414F9166E9295ACF4AA9146E22A6A1C77E5EDCE1A04E0B3E`; menu `main` was 7,794,691 bytes with SHA-256 `B7B0DB1178288B99870A07CDF8D9B43366348A2DE0BDADE50E86AA57360C9A08`; NPDM remained `5D9EFE4E476B23F963F5CDE05DEE7FC591A216BDD8653FB50CAA2F17D42FB9CF`. That package is retained only as the exact crash-symbol reference; it is no longer deployed.

### Implemented: safe HOME from a title-owned LibraryApplet

The full-screen-keyboard attempt found a real HOME-path defect before the `Applet close` action could be pressed. Atmosphere wrote crash report `01787668455_010000000000100d.log`: program `010000000000100D`, data abort at null address `0x40`, PC `SwitchU + 0x8115B4`, LR `+0x813324`, module ID `CE530CB40A279C83879DF7032E1C260EF134D6A0`. The exact deployed ELF had the same build ID. Symbolication resolved the PC to `dk::detail::MemBlock::getGpuAddrForImage`, called by `dkImageInitialize` from `Renderer::initialize`; the mandatory 1x1 white-texture allocation had returned an empty block. The menu log ended at `app.initialize`, while the daemon log proved it had just started the HOME menu beside the title and its still-open keyboard.

An intermediate build added `drainApplicationLibraryAppletForHome`, but hardware proved `AppletApplicationAreAnyLibraryAppletsLeft` returned false while the title keyboard was visibly active. That experiment neither observed nor protected the failing state and added synchronous HOME latency, so it was removed from the final path. SwitchU now preserves the keyboard and relies on checked real GPU allocations plus recoverable renderer initialization failure, as detailed in the incident-correction section below.

[Renderer::initialize](../lib/nxui/src/core/Renderer.cpp) now checks the mandatory white-texture block and upload before calling deko3d. A second failure therefore becomes a logged initialization failure instead of an Atmosphere reboot. The menu also records `[mem-startup]` before GPU setup so the selected 224-416 MiB heap rung and process headroom survive future reports.

That intermediate probe build compiled and deployed successfully, but the next hardware run disproved its premise. It was superseded by the final no-probe normal release documented in the incident-correction section and must not be restored.

## Architectural patches

### Implemented: prepare NS/save data while the exit animation runs

`PrepareApplication` is now a separate 40-byte one-way command, sent immediately after the user decision and before recency submission or the 340 ms visual acknowledgement. The persistent daemon runs `nsTouchApplication` and all five save-data probes while the menu continues rendering. The later `LaunchApplication` remains authoritative.

The implementation is deliberately fail-closed. A prepared result is reusable only when the title ID and all 16 UID bytes match, every touch/save operation completed successfully, the result was at most five seconds old when launch handling began, and a fresh control-cache read is byte-identical to the metadata used by preflight. Any missing command, IPC failure, missing cache, failed save operation, stale result, different user/title, or changed metadata invokes the former synchronous touch/save path. Production logs now name the exact decision as `reject=missing`, `title-mismatch`, `user-mismatch`, `incomplete`, `stale`, `metadata-missing`, or `metadata-changed`; `reject=none` is the only reusable state. `[trace-preflight]` reports the same result numerically (`0` through `7`) beside send/IPC/work/touch/save time, work lead or tail, age, completion, cache hit, and core.

The primary hardware path now passes for one required-user official title, one no-user forwarder, and A-to-B replacement. An opt-in `-PreflightMatrixTest` sysmodule build exercises the reusable baseline plus all seven rejection branches over eight successive real launches. Each case mutates only the daemon's prepared copy in RAM; it never edits a control-cache file, account, title, or save. The daemon logs `pass=1` only when the observed reason equals the injected reason, then every rejected case runs the unmodified synchronous touch/save fallback with the authoritative title and UID. This closes logic coverage without destructive setup, but it does not replace real optional-user, game-card-removal, same-boot-update, sleep/wake, power-loss, or invalid-title hardware tests.

Save precreation now retains the result of every existing open/create call in `LaunchTiming` and emits `[trace-save-state]` only after the foreground endpoint. Each account, device, temporary, cache, and BCAT category is reported as `not-requested`, `existing`, `created`, or `failed`, with the original open and create results. This adds no Horizon IPC and no synchronous critical-path logging. It makes a first-ever account save distinguishable from an already-existing save while retaining `[trace-save]` durations for latency analysis.

The [connected-console second-UID/first-save gate](../.logs/second-uid-first-save-20260826-155255/analysis.md) passed on the normal production build. A new local account's first Stardew Valley launch used `cache_hit=1`, reported `account=created` after the initial open failed and creation succeeded, reached foreground, and returned through HOME. One 192.620 ms graceful close later, the same private UID relaunched with `cache_hit=1`, `account=existing`, successful open, no creation attempt, and another normal foreground/HOME cycle. The private UID was consistent across both launches and distinct from the historical original account; no raw identifier is reproduced here. Fatal/crash inventories were unchanged, and all nine new ERPT files were routine `2123-0011` rather than `2165-1002`. This closes real second-account and first-ever account-save coverage for one required-user title, not optional-user or non-account save categories.

The diagnostic sequence is `baseline-hit`, `missing-preparation`, `title-mismatch`, `user-mismatch`, `incomplete-preparation`, `stale-preparation`, `metadata-missing`, and `metadata-changed`. The [connected-console matrix](../.logs/preflight-matrix-20260826-084124/analysis.md) passed all eight cases with the required ordered `pass=1` results. Only sequence 1 used `cache_hit=1`; sequences 2-8 used `cache_hit=0`, performed the real synchronous fallback, and launched Stardew Valley successfully. Fifteen successful NS touches prove eight preparations plus seven fallbacks. Eight normal closes and eight HOME returns completed with no forced close, launch/foreground error, crash report, or forced-shutdown ERPT. The diagnostic build is forbidden for performance measurements and was replaced immediately by a normal release after collection.

Reviewing the remaining failed-launch gate found that recovery depended on the next generic idle loop. Worse, `g_menuFastExitCount` counted every intentional title handoff forever, so after three ordinary launches the unrelated crash-loop guard could leave a failed handoff black for five seconds. A healthy `MenuReady` now resets that guard. Failed application launch or resume explicitly cold-launches the menu with transition reason `LaunchFailure`, and a failed resume no longer marks the application as foreground. This recovery is compiled in production; controlled daemon-injected failure and sleep/wake-preflight hardware tests remain required before closing the gate.

The first opt-in `-PreflightEdgeTest` attempt exposed two invalid assumptions on hardware. The production lock policy deliberately calls `closeActiveOverlays()`, so physical sleep removed the modal and the eventual Stardew launch came from a new, fresh preparation (`cache_hit=1`) rather than testing stale state. Horizon also accepted the guessed ID `01FFFFFFFFFFF000`: create, start, and foreground all succeeded before the placeholder exited naturally with reason 0, so the menu returned with ordinary reason 3 and the failure-recovery branch was never exercised. The catalog remained 22/22 and no control-cache entry was created, but arbitrary IDs are no longer used for fault injection.

The corrected diagnostic keeps only its own modal alive across the lock screen; production builds and all ordinary dialogs retain the close-on-lock behavior. It records whether a lock was observed, then `Launch selected` runs the unchanged recency, animation, and authoritative-command path. A sleep longer than the five-second preparation lifetime must produce `reject=stale cache_hit=0`, execute the synchronous fallback, and launch the selected installed title. `Failure recovery` carries that same real title and UID through diagnostic command 46, but the daemon returns a synthetic libnx result immediately before `appletCreateApplication`. No uninstalled title reaches Horizon. The existing production failure handler must then create exactly one reason-7 menu and reach its first frame. If any application is still running, the diagnostic refuses before closing it. All state, command handling, and markers are compiled out of normal builds, and the diagnostic build must be replaced immediately after evidence collection.

Corrected hardware evidence at `.logs/preflight-edge-corrected-20260826-094507` passes both gates. Physical message-22 sleep preserved the armed modal, message-26 wake arrived 10.9 seconds later, and no second preparation occurred. The final Stardew command rejected the 21.176-second-old work as `stale`, ran the real touch/save fallback, and successfully reached foreground. After a normal HOME return and 180.449 ms graceful close, command 46 reused a fresh preparation and injected `0x1F959` before application creation. Create, Start, and foreground phase times were all zero. One action failure produced one successful reason-7 recovery menu, ready in 1,881.889 ms and with a first-frame record at 1,745.563 ms; state remained `running=0 suspended=0`. No arbitrary ID, new crash/fatal, or `2165-1002` appeared. This hardware-closes stale-preflight fallback and new-application failed-handoff recovery.

The separate resume gate uses an opt-in `-ResumeFailureTest` harness. After a real suspended title returns to SwitchU, its diagnostic modal can either run the unchanged resume path or send command 47. The daemon still performs the real `appletUnlockForeground`, then substitutes synthetic libnx result `0x1F759` immediately before `appletApplicationRequestForApplicationToGetForeground`. It deliberately retains the real `AppletApplication`, `g_running`, and suspended title ID, invokes the production failure-recovery helper once, and never creates, starts, terminates, or replaces a title. Normal builds do not contain the diagnostic command, modal, or fault branch. Production also refuses to close the menu if resume-command enqueue fails, captures the suspended title ID with the queued action, and records `appletUnlockForeground` result/time separately from the foreground-request IPC.

The [connected-console resume-failure test](../.logs/resume-failure-20260826-143640/analysis.md) passed. Command 47 unlocked foreground successfully in 81 microseconds, injected `0x1F759`, and retained Stardew Valley as `running=1` with the same suspended ID. Exactly one failure produced one successful reason-7 recovery menu, ready in 1,662.650 milliseconds with a first input-dispatchable frame at 1,976.895 milliseconds. No application create, start, replacement, termination, or force operation occurred. The next ordinary command-2 resume used the same title ID, unlocked in 79 microseconds, and completed the real foreground request in 59 microseconds; the owner confirmed the same live session resumed. A subsequent HOME return also passed. Fatal/crash inventories were unchanged, and all 15 new ERPT records were routine `2123-0011`, with no `2165-1002`. This hardware-closes the deterministic resume-handoff recovery gate for one controlled sample; lifecycle soak coverage remains mandatory.

### Implemented: make application termination nonblocking

[beginTerminate](../projects/daemon/src/app_manager.hpp#L551) now requests library-applet termination and graceful application exit, then records a deadline exactly 15 seconds after the request IPC returns. [pollTerminate](../projects/daemon/src/app_manager.hpp#L604) checks the existing non-autoclear AM state event once per 10 ms daemon loop. If signalled, it calls libnx `appletApplicationJoin`; because the event is already signalled, that call cannot wait and only obtains the AM result/exit reason. If the deadline expires first, it sends `appletApplicationTerminate` once and continues polling until Horizon signals completion.

The ordinary natural-exit path refuses to touch the event while termination owns it, and [handleAction](../projects/daemon/src/main.cpp#L1425) defers all queued launch/library-applet actions until close completes. [routeFinishedApplication](../projects/daemon/src/main.cpp#L1558) sends exactly one `ApplicationExited` notification when the menu remains alive. If no menu is alive and no action is queued, it cold-launches the menu through the existing natural-exit route; if a destination is already queued, it skips that unnecessary holder and lets the same daemon loop consume the action. `[trace-terminate]` reports total, library IPC, request IPC, grace, force-to-completion, poll count, force state, Horizon results, exit reason, and cores. This removes the main-loop stall without shortening the save-friendly window. Three ordinary cooperative closes, deterministic queued handoff, duplicate command, close during sleep/wake, and the controlled force branch are verified. The first active-library-applet attempt instead exposed and fixed the separate HOME startup crash above; its corrective HOME path still needs one log-backed hardware retest.

### Implemented: lazy-open the menu NS session

Production `__appInit` no longer opens `ns` before window initialization and `main`. The daemon catalog is mandatory on the production startup path, so first paint does not require a second NS client. [NsService.cpp](../projects/menu/src/core/NsService.cpp) serializes lazy initialization with a libnx user-mode mutex, retries after a failed initialization, keeps one successful session for the remaining menu lifetime, and closes only that session at exit.

Every direct menu NS path now passes through the guard: game details, both software-deletion paths, Storage settings, and the defensive menu-side catalog scan. A failed lazy open produces unavailable details/storage or a failed deletion instead of dispatching through an invalid service. `[ns] lazy initialize` records the triggering feature, Horizon result, and handshake time in microseconds.

Hardware confirmed the functional gate across three menu lifetimes. Every lifetime reached frame 1 without NS. Opening game details produced the only lazy-open record, `reason=game-details rc=0x0 time_us=155`, and returned installed version `1.6.0`; Settings/Storage then built nine items without a second initialization. The next two lifetimes never opened NS. The first lifetime's archived file ended before all buffered shutdown records, so no close marker was durable. Horizon process teardown closes the session regardless; adding synchronous IPC or an SD flush to the launch handoff solely for telemetry would oppose the speed objective. This handshake is only a 0.155 ms saving on the tested console, not a material explanation for return-time variation.

### 3. Keep external menu content registered

The daemon and its SD filesystem server survive every menu. Test making registration daemon-wide:

```diff
 inline void cleanupHolder() {
     if (g_holderCreated) {
         appletHolderClose(&g_holder);
         g_holderCreated = false;
     }
     g_active = false;
-    if (g_externalRegistered) {
-        unregisterExternalContent(kMenuTakeoverProgramId);
-        g_externalRegistered = false;
-    }
 }

 inline Result prepare() {
-    Result ecsRc = registerExternalContent(kMenuTakeoverProgramId, menuPath);
-    if (R_FAILED(ecsRc)) return ecsRc;
-    g_externalRegistered = true;
+    if (!g_externalRegistered) {
+        Result ecsRc = registerExternalContent(kMenuTakeoverProgramId, menuPath);
+        if (R_FAILED(ecsRc)) return ecsRc;
+        g_externalRegistered = true;
+    }
     return create();
 }
```

Add an explicit daemon-shutdown function that terminates the holder and unregisters once. Validate at least 100 HOME/launch cycles, sleep/wake, staged updater reboot, SD removal, and a menu crash. If Atmosphere consumes or invalidates the external-code server after one load on any supported version, retain the current per-holder registration.

### Implemented, hardware validation in progress: fence-batched GPU uploads

[GpuDevice](../lib/nxui/src/core/GpuDevice.cpp) now owns four upload slots. Each has a 1 MiB fixed staging arena, a dedicated 64 KiB command arena, and a fence. Up to 32 ordinary texture copies share one command list. A source larger than 1 MiB gets a slot-owned temporary block and is submitted alone; that block remains alive until the same slot's fence signals. Slot allocation is checked before any command-buffer or CPU-address use.

`uploadTexture` copies caller bytes into owned staging memory and returns without a queue-wide wait. The batch ends with a full image-cache barrier and fence. [beginFrame/endFrame](../lib/nxui/src/core/GpuDevice.cpp) submit update-time and render-time uploads before the drawing list that can sample them. `beginFrame` explicitly flushes an update-time submission so it can overlap image acquisition. Ring reuse explicitly flushes deko3d's buffered queue commands before waiting for the oldest slot; this is required when a same-frame burst wraps the ring before `presentImage` or the automatic queue-flush threshold. Explicit resource retirement and shutdown still call `waitIdle`, which first submits any pending batch and then releases slot-owned temporary memory. This preserves source, destination, descriptor, and teardown lifetimes; it does not pretend that GPU synchronization can be deleted.

The first hardware pass exposed that missing queue kick: Theme Shop built successfully, then its first-render text uploads filled all four slots before `endFrame`. The render thread waited on slot zero while the corresponding fence-signal lists were still buffered, so no later present could flush them. The menu log stopped immediately after `show()`, HOME requests reached the daemon but the menu could not consume them, and fatal/crash inventories did not change. The targeted correction flushes only when update work must start before acquire or the CPU is about to wait on a wrapped slot; it does not restore per-texture queue-wide idles.

The corrected `db40931` build passed the focused Theme Shop hardware gate on 2026-08-28. The first open stayed active for 59.7 seconds while both catalogues produced 92 entries, six preview-cache evictions ran, and performance logging continued. Three close/reopen cycles then completed, for four `show()` and four `hide()` records in total. There are 64 live Theme Shop performance samples, the maximum recorded ring-wrap wait is `0.000 ms`, and the final sample after the last close is a healthy menu frame. Fatal and top-level crash inventories remained 8/18; all 39 new ERPT files are routine `2123-0011`, and none contains forced-power-loss code `2165-1002`. Full evidence is at `.logs/gpu-upload-ring-theme-shop-pass-20260828-074416`.

The pass also exposed one smaller CPU/allocation inefficiency rather than a correctness failure: after the operator left Theme Shop on its non-custom Update tab, one reopen called `buildTabs()` twice in the same 100 ms log bucket. `publishUpdateState()` and `refreshThemeShopState()` each rebuild the current non-custom tab before `show()` resets selection. Consolidating that reopen refresh into one state rebuild is a follow-up optimization; it is not evidence of another fence stall.

The normal release/sysmodule compiles with all diagnostic modes disabled. Runtime telemetry now reports upload count, batch count, and ring-wrap CPU wait. Theme Shop browsing, preview-cache eviction, and repeated reopen are now validated. Hardware must still validate title startup/return, animated and static theme changes, language glyph-cache clears, gallery previews, and rapid launch while uploads are pending. No return-time improvement is claimed before those checks and a comparison against the completed matrix.

## Memory and warm-state decision

Keeping the current menu GPU context alive beside a full application is not a safe incremental optimization. The disposable menu may receive 224-416 MiB, and its dynamic image budget can use a large portion of that heap. The daemon has only 8 MiB. Horizon's applet/application resource policy and SwitchU's holder gate currently ensure that menu memory is reclaimed before the next application is foregrounded.

Safe warm state in the current architecture is limited to the daemon:

- title IDs, names, view flags, NACP startup-user fields, and a small recency table;
- an open external-content registration if the hardware gate passes;
- a compact preflight cache keyed by title ID and account UID;
- no decoded 256x256 icons, framebuffers, font atlases, or deko3d context.

A genuinely sub-second return requires moving a minimal home renderer into the persistent system applet, or keeping a separate resident UI process under a verified resource contract. That is a major architecture: increase and measure daemon memory, move graphics/window ownership, isolate all blocking NS/FS/network work, and validate coexistence with memory-heavy games and common sysmodules. Suspending the present 224-416 MiB menu while a title runs is not recommended.

## 2026-08-25 incident corrections

### HOME while a title keyboard is visible

The first hardware attempt exposed a real null deko3d block in `Renderer::initialize`; the mandatory white-texture block/upload checks now stop that crash. The next attempt produced a black screen without a crash. The retained evidence identifies a different fault in [GpuDevice::allocImageMemory](../lib/nxui/src/core/GpuDevice.cpp): a pre-allocation guard compared `InfoType_TotalMemorySize - InfoType_UsedMemorySize` against the requested image plus 24 MiB. After `svcSetHeapSize`, the reserved heap is included in used memory. Failed 400 MiB-heap processes reported 413.5 MiB used; the successful 416 MiB-heap process reported 429.5 MiB used. The identical 13.5 MiB non-heap overhead proves the subtraction was not free space inside malloc. The guard rejected the 4 KiB white block before `MemBlockMaker::create()` whenever the outside-heap difference was 8.4-9.8 MiB, then happened to allow it at 28.4 MiB.

The false guard is removed. SwitchU's explicit image budget remains, and the checked real `MemBlockMaker` result is authoritative. The 40 MiB heap-ladder cap introduced from the same false inference was reverted before the final build. This avoids shrinking the menu heap based on an invalid metric.

[openMenuFromHome](../projects/daemon/src/main.cpp) also no longer calls `AppletApplicationAreAnyLibraryAppletsLeft` or requests child termination. Every probe returned false while the keyboard was still visible, adding 2.0-6.9 ms of synchronous HOME work without observing the state it was meant to protect. The corrected architecture preserves the title keyboard and relies on real allocation results plus the safe renderer failure path. Hardware passed under the exact former 400/421.9/413.5/8.4 MiB heap/process/used/outside-heap cohort: HOME reached the first input-dispatchable frame in 2,316.862 ms, the title resumed with the keyboard workflow usable, and no fatal or white-texture failure appeared.

### Remote v2.1.0 unclean shutdown

The supplied evidence does not attribute shutdown to SwitchU. ERPT `2165-1002` is Atmosphere's forced-shutdown-detected record: it proves the prior power transition was unclean but not its cause. The fatal context is a standard abort in system program `0100000000000034` (`fatal`), not SwitchU daemon `0100000000001000` or menu `010000000000100D`. SwitchU's latest menu log was healthy at about 60 FPS with 89% battery and no service/rendering failure. The daemon tail contains Horizon applet message 22, documented as a detected short power-button press, then its sleep path; there is no SwitchU shutdown/reboot command.

The next-release daemon hardens [startSleepSequence](../projects/daemon/src/main.cpp) and [handleAppletMessages](../projects/daemon/src/main.cpp): all applet-driven sleeps use one path, record source/battery/charger/application/menu state, flush the file log before committing the SD device, and only then request sleep. Messages 27 and 28 now honor Horizon's high-temperature and low-battery sleep requirements; existing short-button, auto-power-down, and CEC-standby behavior remains. This improves safety and makes the next report attributable, but cannot prevent hardware/PMIC loss, a power-button fault, or a separate Atmosphere abort.

The final normal archive is 42,990,307 bytes with SHA-256 `19048CA6C7C7473D0ECE36A8AA7C0223BBC58DAC4DE700835BF97D6C203C8BCE`. Local and connected-card hashes match for daemon `39FF74CFECF3E72699EBE8D6EE101F1CC2458847EE9313AAF552067F598721D1`, menu `0459AC2C9B20D8DAD54E7B5505F2155A8E97AFFD476CEF09D66479A35B203D8B`, and NPDM `5D9EFE4E476B23F963F5CDE05DEE7FC591A216BDD8653FB50CAA2F17D42FB9CF`. Normal message-22 sleep/wake also passed: the detailed breadcrumb survived reboot, wake arrived 7.9 seconds later, the title resumed, a second HOME reached its first input-dispatchable frame in 2,395.577 ms under the same former failure cohort, and no `2165-1002` ERPT appeared. This validates the ordinary SwitchU handler, not the source of the separate remote console's forced power loss.

## Millisecond profiling procedure

### 1. Capture low-overhead transition markers

The follow-up build implements this layer. It uses the common ARM system counter instead of wall-clock text. The menu carries raw ticks through `LaunchAppArgs`, `ResumeAppArgs`, `MenuReadyArgs`, `MenuFirstFrameArgs`, and `MenuClosingArgs`; the daemon converts them only after the measured endpoint and writes bounded records:

```cpp
struct TransitionStamp {
    const char* name;
    std::uint64_t tick;
};

inline std::uint64_t transitionNow() { return armGetSystemTick(); }
inline std::uint64_t transitionMs(std::uint64_t start, std::uint64_t end) {
    return armTicksToNs(end - start) / 1'000'000ULL;
}
```

The implemented records are:

1. `[trace-launch]` and `[trace-launch-phases]`: activation, user decision, animation, recency overlap, menu-holder exit, command delivery, previous-title close, NS touch, save preflight, application creation, launch-parameter setup, start, and foreground request.
2. `[trace-save]` and `[trace-save-state]`: account, device, temporary, cache, and BCAT durations plus exact not-requested/existing/created/failed outcomes and their existing IPC results.
3. `[trace-menu-exit]`: shutdown, GPU drain, HTTP cancellation, state persistence, service shutdown, Bluetooth shutdown, close-command delivery, and holder death.
4. `[trace-return-ready]`, `[trace-return-init]`, and `[trace-return-frame]`: HOME/natural-exit origin, ECS registration, holder create/start, menu main/init/GPU/renderer/blank frame/onCreate, `MenuReady`, image memory, and the first frame after menu input dispatch.
5. `[trace-resume]`: activation through animation/IPC/holder death and foreground reacquisition.
6. `[trace-terminate]`: explicit close request through graceful completion or the 15-second force threshold, including AM IPC results, poll count, exit reason, and daemon core placement.

The ARM counter is common across these processes during one boot, yielding cross-process intervals without relying on the old 100 ms log timestamps. Core and catalog/image context are included where relevant. Storage type, control-cache activity, and operation mode remain useful additions if the matrix exposes unexplained cohorts.

### 2. Avoid measuring the logger

No menu-side trace line is synchronously written during the launch critical path. Raw landmarks cross IPC, and daemon trace lines are written only after foreground, ready, first-frame, or holder-death endpoints. For the final camera-grade benchmark, disable unrelated immediate diagnostic logging or place all trace records in a fixed daemon ring, then flush after the run; retain immediate logging for crash investigation.

Convert captured logs to milliseconds and distribution summaries with:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\analyze-transition-traces.ps1 `
  -Path .\.logs\hardware\daemon-*.log -CsvPath .\.logs\transition-samples.csv
```

The analyzer reports count, minimum, p50, p90, p95, p99, maximum, mean, and median absolute deviation for every `_us` field, grouped by trace type and title/reason.

### 3. Run a controlled matrix

Perform at least 30 successful iterations per case, in handheld and docked modes where relevant:

1. Cold boot to first usable menu; HOME return to a suspended title; natural title exit to menu.
2. First launch after boot, repeated launch of the same title, A-to-B title replacement, and resume of the suspended title.
3. NAND-installed title, SD-installed title, game card, and installed homebrew forwarder.
4. With 1, 40, 120, and the maximum available title count; with default and animated themes; with one and eight user profiles.
5. During catalog refresh, control-cache fill, update check, theme download, and with no background work. A cancelled download must leave no partial destination file.

For a future raw NRO host, separately measure host cold launch, host-to-NRO map/start, NRO exit to host, and host exit to SwitchU. Do not combine these into the installed-title dataset.

### 4. Report distributions and real display time

For every interval report sample count, minimum, median, p90, p95, p99, maximum, mean, and median absolute deviation. Keep failures and timeouts as failures rather than dropping them. For official titles, SwitchU cannot instrument the title's first rendered frame; use 120/240 fps video or a photodiode on a black-to-white test title to measure activation-to-visible-content and HOME-to-visible-menu. Correlate camera frames with controller LED/input or an on-screen flash.

Suggested acceptance goals after the staged work, not claims about the current build:

| Flow | p50 goal | p95 goal |
| --- | ---: | ---: |
| Activation to launch IPC sent | 350 ms | 450 ms |
| Launch IPC received to foreground request complete, no previous app | 400 ms | 800 ms |
| Resume IPC received to foreground request complete | 100 ms | 250 ms |
| HOME to first usable menu frame | 900 ms | 1,200 ms |
| Natural title exit to first usable menu frame | 1,000 ms | 1,300 ms |
| Menu teardown after launch request | 150 ms | 300 ms |

### 5. Safety gates

Before accepting a speed change, run 100-cycle launch/HOME/resume and A-to-B replacement tests, then verify:

- no fatal report, applet crash, leaked holder, stale foreground flag, or black-screen recovery;
- save data exists for first launch under every NACP save category and user mode;
- title exit still gets the full graceful window and save-heavy games retain progress;
- sleep, wake, reboot, shutdown, game-card removal, and SD removal remain responsive;
- no partial config, catalog, theme, update, or artwork files after forced power loss testing;
- heap/image high-water marks remain below measured limits with memory-heavy games/sysmodules;
- no deko3d debug error, descriptor reuse, upload corruption, or use-after-free.

## Priority order

1. Complete the remaining real-state gates: optional-user modes and physical game-card removal/update. The connected catalog currently has no optional-user candidate. Required-user launch, no-user forwarder launch, A-to-B replacement, a distinct second UID, first-ever account-save creation followed by existing-save reuse, all seven rejection fallbacks, physical sleep/wake stale fallback, and deterministic launch/resume failed-handoff recovery now pass on hardware.
2. Preserve the completed controlled matrix as the current-build baseline: 10 cold official launches, 10 strict cold-forwarder launches, and 15 A-to-B replacements. Do not claim a before/after speedup without a comparable pre-change build, and retain the 100-cycle lifecycle soak as the release gate.
3. Hardware-validate the implemented fence-based upload batching: deko3d debug output, pending-upload launch, cache eviction, static/animated theme changes, preview loading, and glyph-cache clear. Then compare HOME-to-frame and `onCreate` against this matrix.
4. After that gate, deduplicate shader-file loads and cache animated-background manifests, measuring each independently so their return-time effects are not conflated.
5. If arbitrary NRO launch is a product requirement, build the dedicated hbloader-compatible NRO host and private request transport described above; installed forwarders already use the normal title-ID path.
6. Decide whether sub-second return justifies a persistent-renderer redesign; do not retain the current large menu heap beside applications without a verified Horizon resource model.
