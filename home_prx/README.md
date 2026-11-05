# HOME PRX — spike placeholder (not built in M1)

In-game HOME overlay will live here as a separate `PRX` with feature flags:

- `experimental_home` — intercept physical HOME, dim, resume/exit, disable path
- `experimental_music` — local playlist card only
- `experimental_savestate` — full RAM snapshot; per-game hardware validation

**Do not** auto-enable via installer scripts. See `docs/hardware-checklist.md` sections D–F.

Candidate audits before any copy: Custom-Home-Mod, PspStates-Experiment, MP3PlayerPlugin — licenses + ARK-5/3008 retest required. Prefer reimplementation after audit.
