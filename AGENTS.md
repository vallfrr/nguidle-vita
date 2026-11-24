# Agent instructions

This is the PS Vita port of NGU Idle on the nguidle-rust engine. Read `README.md`, then the engine's `docs/PORTING.md` (how a
port is structured) and `docs/ENGINE-API.md` (what is public, including the C ABI). `ARCHITECTURE.md` says what this
repository owns and how it differs from the engine's port template.

- Run `just check` (it builds the VPK) before and after each change. Work on a topic branch.
- The host follows the engine's platform contract, through the C ABI declared in `engine_api.h`: input first, then game time
  (`ngu_frame`), then the draw list (`ngu_emit`), then textures, text fields and sound. Call the engine from one thread only
  and copy the draw list and texture pixels before another thread uses them; this host presents on a second thread.
- Use one game on the console at a time and never re-enable VitaCompanion. Hardware measurements follow `docs/` of the engine and
  the tools in `tools/`; a build check does not measure performance.
- Never commit the game's files, extracted assets, saves, VPKs or machine-specific paths.
- Update the engine pin together: the `engine` submodule revision and `engine.tag` (the tag), then `just check`.
- Keep code, comments and documentation in English. Report what was tested and what was not.
