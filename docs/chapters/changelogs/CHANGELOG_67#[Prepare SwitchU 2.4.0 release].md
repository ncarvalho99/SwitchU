### 2026-09-05  Prepare SwitchU 2.4.0 release  `opencode`

1. **What:**
   - Prepared fork release 2.4.0 from the ten commits after v2.3.2, including game-port platform selection, metadata matching and availability fixes, liquid-glass platform dossier, bundled localization, and proxy model restrictions.
   - Updated the compiled version, bilingual release notes, and embedded console update notes.
   - Included interrupted local fixes: SD asset base for the platform picker, game-details overlay mode, and antialiased battery-ring drawing; regenerated shader binaries through the release build.
2. **Why:**
   - Provide the requested 2.4.0 release with updater-visible version and release notes that match the completed changes.
3. **Files:**
   - `xmake.lua`, `CHANGELOG.md`, `romfs/notes/release-notes.txt`, `.gitignore`.
   - `projects/menu/src/core/WiiUMenuApp.cpp`, `projects/menu/src/details/GameDetailsScreen.cpp`, `projects/menu/src/widgets/GlossyIcon.cpp`.
   - `romfs/shaders/blur_pass_vsh.dksh`, `romfs/shaders/pass_vsh.dksh`.
4. **Impact:**
   - Builds identify themselves as SwitchU 2.4.0 and the manager can compare and install the matching GitHub release archive.
   - No console SD-card, server, or upstream lineage mutation occurred in this release-preparation change.
5. **Docs:**
   - `CHANGELOG.md` contains user-facing bilingual notes; this immutable entry records release-preparation evidence.

**Validation:**
- 2026-09-05, working tree based on `7a66c81f46a4053d03e1f5b933118cffa3f3312c`: `powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build-local.ps1 -Mode release -Variant sysmodule -SkipConsoleDeploy`: **PASS**; generated `artifacts/SwitchU-sysmodule-release.zip` (48,659,635 bytes). Docker build emitted non-fatal compiler option notes and a detached-HEAD warning inside build tooling.
- 2026-09-05, same working tree: `python -c "import zipfile; p='artifacts/SwitchU-sysmodule-release.zip'; z=zipfile.ZipFile(p); n=z.namelist(); assert n and all(x.startswith(('atmosphere/','switch/')) for x in n), n"`: **PASS**; archive payload uses updater-required `atmosphere/` and `switch/` roots.
- 2026-09-05, same working tree: `git diff --check`: **PASS**.
