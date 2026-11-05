# Compatibility matrix

Checked against upstream on **2026-11-05**. Re-verify Releases before pinning binaries for a console install.

## Hardware / OFW / CFW

| Item | Status | Notes |
|------|--------|-------|
| PSP-3008 (3000) | **Target** | 480×272, HOME button, ~64MB RAM |
| OFW 6.60 / 6.61 | Supported by FasterARK | Also 6.60TT / 6.60DT |
| User claim: `6.60 PRO-B` | **Unconfirmed** | Confirm System Information / CFW string on device before changes |
| ARK-4 (`PSP-Archive/ARK-4`) | Archived legacy | Last official bridge release `v4.20.69 r206` / tag `v42069r206` |
| ARK-5 (`PSP-Arkfive/ARK-5`) | **Active target CFW** | GPL-3.0; Releases has pre-release tag `latest` (`ARK-5.zip`, commit noted `ccf40a9` at check time) |
| FasterARK | Official installer/updater | Tag `latest` (published 2026-11-02): `ARK_UPDATE.zip`, `FasterARK_psp_full.zip`, `FasterARK_psp_lite.zip`, themes, Vita VPK |
| Permanent CustomIPL / DC10 | Supported by FasterARK full | **Out of M1**; only after explicit user sign-off |
| Adrenaline / Vita ePSP | Supported by ARK lineage | Not M1 validation target |

## Toolchain

| Item | Pin / note |
|------|------------|
| pspdev | Release **`v20261101`** (2026-11-01) — also `latest` rolling tag; ARK-5 README asks for latest pspdev |
| psp-cfw-sdk | Required to build ARK / Launcher-Menus style packages |
| ya2d | Used by Launcher-Menus; optional for our M1 (GU first) |

## Features vs CFW

| Feature | PRO-B (legacy) | ARK-4 | ARK-5 | M1 plan |
|---------|----------------|-------|-------|---------|
| Run unsigned EBOOT | Yes (typical CFW) | Yes | Yes | Required |
| Custom launcher (Launcher-Menus) | N/A / separate | Yes | Yes (bundled in FasterARK full) | Reference only |
| WPA2-AES 2.4 GHz | No (stock/PRO) | Experimental (wpa2psp embedded) | Present; still experimental; maintainer noted instability | Document UX limits; guest SSID |
| Plugin manager / PLUGINS.TXT | seplugins | Yes | Improved | Disable path for HOME spike |
| True OC | No | Limited | Yes (not Vita) | Not M1 |

## WPA2 reality check

Upstream (Wololo 2025-02-14 + ARK maintainers + issue #710):

- Works only **2.4 GHz**, **WPA2-AES** (not TKIP)
- Mixed WPA2/WPA3 often breaks DHCP
- Assoc/IP ≠ HTTPS success
- Experimental / intermittent per maintainer
- Safe user options: isolated guest/IoT 2.4 GHz WPA2-AES; temporary hotspot with Maximize Compatibility; **do not** open main home network

User symptoms (home AP nearly invisible; other APs “security not supported”) fit band/security mismatch more than “PRO-B always broken”.

## Third-party candidates (audit only — not bundled in M1)

| Candidate | Role | License / caution | ARK-5 / 3008 |
|-----------|------|-------------------|--------------|
| Launcher-Menus | Custom launcher reference | **GPL-3.0** | Supported reference |
| Custom-Home-Mod | HOME intercept | Per-file audit needed (Archive mirror) | Legacy; retest |
| PspStates-Experiment | Full RAM savestate Slim/3000 | Audit | Experimental; not 1K; feature-flag |
| MP3PlayerPlugin | Music over game | Mixed / GameBrew; no maintained GH upstream | Prefer own player after spike |
| CXMB / Categories Lite | XMB themes | Often irrelevant to own launcher | Do not conflate |

## Shell M1 vs later spikes

| Capability | Host | Emulator | Real 3008 + ARK-5 |
|------------|------|----------|-------------------|
| Boot EBOOT | n/a | smoke | **acceptance** |
| Theme / picker math | unit | UI | visual |
| Library scan | dummy tree | yes | real ms0 paths |
| Safe Files | unit + tmpfs | yes | real card / eject edge |
| HOME PRX | — | limited | spike + rollback |
| Music in-game | — | limited | spike + CPU/RAM measure |
| Savestate | — | unreliable | per-game flag |
| Telegram / store | — | — | feasibility later |
