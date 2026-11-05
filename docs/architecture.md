# Architecture — PSP Shell + HOME Overlay (M1)

**Target:** PSP-3008 (3000 family), native UI 480×272, no touch.  
**CFW base:** ARK-5 (active successor). User may currently be on unconfirmed `6.60 PRO-B` — confirm on device before any console change.  
**Scope of this doc:** product architecture for M1 shell EBOOT and later HOME PRX spikes. Not a flash/install guide.

## Goals

1. Own optimized shell as independent `EBOOT.PBP` (games / music / apps / files + settings).
2. Separate in-game HOME module as `PRX` (different memory/resource context) — **spike only after M1**.
3. Small, independently testable milestones. Offline-first. No permanent IPL/flash writes in M1.

## Language / stack (decision)

| Choice | Decision | Why |
|--------|----------|-----|
| Language | **C99** | Matches pspdev/pspsdk samples; smaller binary; easy host unit tests of pure logic |
| Graphics | **GU** (immediate-mode quads + textured glyphs) | No heavy deps for M1; ya2d optional later if needed for themes |
| Build | **GNU Makefile** via `psp-config` / `build.mak` | Same pattern as Launcher-Menus / pspsdk samples |
| CFW SDK | `psp-cfw-sdk` (when linking ARK-specific APIs) | Required by ARK-5 README for CFW work; M1 userland EBOOT may not need it yet |

C++/SDL3 deferred until M1 proves input/scan/files/theme on hardware. Avoid bundling legacy PRX without audit.

## Process split

```
┌─────────────────────────────┐     ┌──────────────────────────────┐
│  shell/  (EBOOT.PBP)        │     │  home_prx/  (future spike)   │
│  User-mode launcher         │     │  Kernel/user plugin context  │
│  Library, Files, Theme, …   │     │  HOME intercept, music card  │
│  Host-testable core/*.c     │     │  Feature-flagged savestate   │
└─────────────────────────────┘     └──────────────────────────────┘
         ms0:/PSP/GAME/<TITLE>/              seplugins + PLUGINS.TXT
```

Shared pure logic lives under `shell/src/{theme,path,input,library,files}` with `#ifdef HOST_TEST` / dual compile so host GCC can run unit tests without pspdev.

## Shell modules (M1)

| Module | Responsibility |
|--------|----------------|
| `ui` | Screen layout 480×272, tabs, focus ring, confirm dialogs |
| `theme` | Presets + circular stick color picker; HSV→RGB; contrast guards; persist |
| `library` | Incremental scan of game paths; focus navigation; lazy cover stubs |
| `files` | Dual-pane-capable browser; sanitize paths; confirm destructive ops |
| `input` | Button/analog mapping; stick deadzone; edge-triggered nav |
| `path` | Flash-partition deny; traversal block; ms0/ef0 roots only |
| `font` | Embedded Noto Sans subset + FreeType (AA Latin/Cyrillic); soft-FB blend |

### Tabs

- **Игры / Музыка / Приложения / Файлы** — main strip  
- **Настройки** — separate entry (theme, view mode, about, experimental flags off by default)

### Theme persistence

Binary/text config on Memory Stick, e.g. `ms0:/PSP/SYSTEM/ownshell/theme.cfg` (created only on ms0/ef0). Stores accent RGB, wallpaper id, library view, HOME module toggles (placeholders until PRX exists).

### Color picker

- Stick angle → hue; radius → saturation; optional D-pad nudge; separate brightness control  
- Deadzone ~0.25 stick magnitude before hue changes  
- **X** commit, **O** cancel/restore previous  
- Reject accents that fail minimum contrast vs graphite background / focus text

## Game / app scan roots (initial)

Aligned with common ARK / Memory Stick layouts (refine after hardware check):

| Path | Kind |
|------|------|
| `ms0:/ISO/` | ISO/CSO/ZSO umd images |
| `ms0:/PSP/GAME/` | Homebrew / EBOOT folders |
| `ms0:/PSP/GAME150/` | Legacy 1.50 (if present) |
| `ef0:/…` | Same when internal storage present (Go); treat as second root |

Do not scan or write `flash0:`, `flash1:`, `flash2:`, `flash3:`.

## Files safety

- Allow roots: `ms0:/`, `ef0:/` only  
- Normalize `..`, reject absolute escapes and `flash*:`, `disc0:`, `host*:`, `umd0:` for write  
- Destructive ops require confirm; copy/move show progress + cancel; never report success on truncated write  
- L/R switches dual location for copy source/dest (M1: implement list + copy/delete/rename; move as copy+delete with confirm)

## HOME PRX (out of M1 — spike gates)

Feature flags default **off**:

1. HOME intercept + dim overlay + resume/exit + disable path  
2. Music card (local playlist only)  
3. Brightness/volume; battery/FPS only if APIs return real data  
4. Screenshot  
5. Savestate (pspstates-class) — per-game hardware validation  
6. Exit to own shell  

Candidates for audit only (see `docs/compatibility-matrix.md`): Custom-Home-Mod, PspStates-Experiment, MP3PlayerPlugin. Prefer reimplementation after license/source review over blind PRX bundling.

## Network / Telegram / store

Out of M1. USB transfer is the baseline. FTP/LAN only on trusted isolated LAN later. Telegram = separate feasibility spike (TLS/MTProto/RAM/session revoke). No pirate catalogs. No plaintext secrets. No auto-messaging.

## Testing layers

| Layer | What |
|-------|------|
| **host** | Theme math, deadzone, path sanitize, list filter, dummy tree file ops |
| **emulator** | PPSSPP smoke of EBOOT (input/UI) — not proof of ARK plugins |
| **real PSP** | Boot on target ARK; Files on real ms0; WPA2/network; HOME/savestate spikes |

Never claim real-hardware validation without running the checklist on device.

## Memory budget (guidance)

PSP-3000 userland is tight. M1 targets: avoid loading all covers at once; incremental scan; FreeType glyph cache (embedded TTF subset); keep EBOOT + heap under available user partition (measure with `.map` when toolchain available).

## Non-goals (explicit)

- Browser/React mock as product  
- Forking ARK for cosmetics  
- Auto-flash / permanent IPL without sign-off  
- Weakening home Wi-Fi  
- “Works on all games” for HOME/savestate/music-over-game  
