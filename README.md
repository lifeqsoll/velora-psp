# OwnShell — native PSP-3008 shell (ARK) + future HOME overlay

Нативный шелл 480×272 для PSP-3008 под **ARK-5**. M1 = bootable `EBOOT.PBP` с библиотекой, темой (пресеты + круговая палитра стиком) и безопасным каркасом **Файлы**. HOME PRX / savestate / музыка поверх игры — отдельные spikes с флагами.

## Docs

- [docs/architecture.md](docs/architecture.md)
- [docs/compatibility-matrix.md](docs/compatibility-matrix.md)
- [docs/pinned-versions.md](docs/pinned-versions.md)
- [docs/hardware-checklist.md](docs/hardware-checklist.md)
- [THIRD_PARTY.md](THIRD_PARTY.md)

## Host tests (no PSP required)

```bash
make -C host_tests test
```

Layer tag: **host**.

## Build EBOOT (pspdev)

1. Install [pspdev](https://github.com/pspdev/pspdev/releases) — pin **`v20261101`** (see pinned-versions).
2. `export PSPDEV=/path/to/pspdev` and put `$PSPDEV/bin` on `PATH`.
3. `make -C shell`
4. Copy `shell/EBOOT.PBP` to `ms0:/PSP/GAME/OWNSHELL/EBOOT.PBP`.

Layer tag after copy+launch on device: **real PSP**. PPSSPP can smoke UI only: **emulator**.

## Safety

- No flash0/flash1 writes in build/CI.
- Confirm CFW string on device before migrating from presumed PRO-B to ARK-5.
- Do not weaken home Wi‑Fi; prefer guest 2.4 GHz WPA2-AES.

## Status

See agent report in chat after first scaffold. M1 is intentionally small and expandable.
