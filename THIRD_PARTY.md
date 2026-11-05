# Third-party notices

OwnShell M1 source in this repository is original unless noted below.  
**No third-party PRX binaries are bundled in M1.**

## Upstream references (not vendored)

| Project | URL | License | Use in M1 |
|---------|-----|---------|-----------|
| ARK-5 | https://github.com/PSP-Arkfive/ARK-5 | GPL-3.0 | CFW target docs only |
| FasterARK | https://github.com/PSP-Arkfive/FasterARK | see upstream | install docs only |
| Launcher-Menus | https://github.com/PSP-Arkfive/Launcher-Menus | GPL-3.0 | architecture/reference; **not copied** |
| pspdev / pspsdk | https://github.com/pspdev/pspdev | various (see tree) | toolchain to build EBOOT |
| psp-cfw-sdk | https://github.com/pspdev/psp-cfw-sdk | see upstream | needed for ARK/launcher builds later |

## Legacy candidates (audit only — not included)

| Project | URL | Notes |
|---------|-----|-------|
| Custom-Home-Mod | https://github.com/PSP-Archive/Custom-Home-Mod | HOME intercept candidate; license per source file |
| PspStates-Experiment | https://github.com/PSP-Archive/PspStates-Experiment | savestate spike only |
| MP3PlayerPlugin | GameBrew wiki | no maintained GitHub upstream; prefer own player |

## Fonts

8×8 ASCII table in `shell/src/font/font.c` is an original compact bitmap set for M1.  
Cyrillic glyphs are original hand-packed bitmaps for UI strings.

When any external code is vendored later, add: name, version/commit, license text path, and files touched.
