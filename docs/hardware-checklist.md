# Hardware checklist (manual — agent does not execute console flash)

**Risk note:** Emergency recovery is not a guarantee of zero risk. Back up saves and CFW/config to a PC before experiments.

## A. Before any console change

- [ ] Record exact **OFW/CFW string** from the PSP (System Information / ARK settings / recovery). Do not assume `6.60 PRO-B`.
- [ ] Note model (expect PSP-3008), region, Memory Stick free space.
- [ ] Copy `ms0:/PSP/SAVEDATA`, plugin configs (`seplugins`, `PLUGINS.TXT` / ARK plugin lists), and any custom themes to PC.
- [ ] Confirm you will **not** install permanent CustomIPL / DC10 without separate written sign-off.

## B. M1 OwnShell EBOOT (after host/emulator smoke)

- [ ] Place `EBOOT.PBP` under `ms0:/PSP/GAME/OWNSHELL/` (or chosen title id folder).
- [ ] Launch from XMB or ARK Custom Launcher.
- [ ] Russian labels readable on 480×272; focus contrast OK on graphite + cyan.
- [ ] Library lists entries from `/ISO` and `/PSP/GAME` (and `ef0` if present).
- [ ] Theme presets + circular stick picker: X commit, O cancel restores previous.
- [ ] Files: list, breadcrumb, open/up, rename/copy/delete with confirm; refuse `flash*`.
- [ ] Remove card mid-op (if safe to test): no fake success; clear error.

## C. Wi‑Fi / WPA2 (optional; not required for M1 offline)

- [ ] Prefer isolated **guest/IoT SSID: 2.4 GHz, WPA2-AES only**, client isolation on.
- [ ] Avoid opening main home Wi‑Fi (no WEP, no password removal).
- [ ] If using iPhone hotspot: enable Maximize Compatibility (2.4 GHz).
- [ ] On ARK-5: enable WPA2 in CFW settings only if desired; reboot; test assoc + IP.
- [ ] Treat HTTPS/app downloads as separate success criteria from assoc/IP.
- [ ] If unstable: disable WPA2 toggle; document failure mode (DHCP, band, mixed WPA3).

## D. Plugin disable / rollback (before HOME spike)

- [ ] Know how to disable plugins: ARK plugin manager and/or edit `PLUGINS.TXT` / seplugins on PC.
- [ ] Document hold-combo / recovery boot path for this CFW build.
- [ ] Keep a known-good plugin config copy on PC labeled with date + CFW string.

## E. HOME PRX spike (feature-flag off by default)

- [ ] Install only after M1 acceptance; flag disabled at first boot.
- [ ] Enable overlay alone; verify HOME open/close + game resume.
- [ ] Test disable/recovery path once successfully.
- [ ] Brightness/volume; battery only if API returns real data.
- [ ] Music card with local files; measure hitching; no claim of mixing game audio.
- [ ] Screenshot path verified writable on ms0.

## F. Savestate spike (experimental, per-game)

- [ ] Enable flag for **one** test title only.
- [ ] Save + load at multiple points; note crashes/black screens.
- [ ] Do not claim universal support; leave flag off for untested titles.
- [ ] Confirm not applicable / unsafe expectations for PSP-1000 class devices.

## G. After session

- [ ] Copy any new logs/configs off the stick.
- [ ] Leave risky plugins disabled unless intentionally kept.
- [ ] Update this checklist with CFW string + pass/fail dates (no fabricated FPS).
