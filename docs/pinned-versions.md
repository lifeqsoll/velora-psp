# Pinned versions (verified 2026-11-05)

Do not flash or install these automatically. Pins are for documentation and reproducible **host** builds.

## CFW / installer

| Component | Upstream | Pin at check | License |
|-----------|----------|--------------|---------|
| ARK-5 | https://github.com/PSP-Arkfive/ARK-5 | Releases tag `latest` (pre-release); artifact `ARK-5.zip`; commit seen `ccf40a9` | GPL-3.0 |
| FasterARK | https://github.com/PSP-Arkfive/FasterARK | Releases tag `latest` (2026-11-02): `ARK_UPDATE.zip`, `FasterARK_psp_full.zip`, `FasterARK_psp_lite.zip`, `themes.zip`, `FasterARK_psvita.vpk` | see repo |
| Launcher-Menus | https://github.com/PSP-Arkfive/Launcher-Menus | `main` at check (GPL-3.0); build needs ya2d + psp-cfw-sdk | GPL-3.0 |
| ARK-4 (legacy bridge) | https://github.com/PSP-Archive/ARK-4 | tag `v42069r206` / `v4.20.69 r206` | archived |

## Toolchain

| Component | Upstream | Pin |
|-----------|----------|-----|
| pspdev | https://github.com/pspdev/pspdev/releases | **`v20261101`** (2026-11-01) preferred for reproducibility; ARK-5 README also accepts rolling `latest` |
| pspsdk | bundled in pspdev | follows pspdev release |
| psp-cfw-sdk | https://github.com/pspdev/psp-cfw-sdk | latest via `psp-pacman` when building CFW/launcher |

## This project

| Artifact | Version |
|----------|---------|
| OwnShell M1 | `0.1.0-m1` (see `shell/Makefile` `VERSION`) |

## Re-verify command ideas (host only)

```bash
# Inspect remote release metadata — do not download onto the PSP automatically
curl -sL https://api.github.com/repos/PSP-Arkfive/ARK-5/releases/latest | head
curl -sL https://api.github.com/repos/pspdev/pspdev/releases/latest | head
```
