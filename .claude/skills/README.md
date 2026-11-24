# Skills for assistants (Claude Code, Codex, others)

| skill | source | license | role |
|---|---|---|---|
| `vita-port` | [Yannis100/vita-port-skill](https://github.com/Yannis100/vita-port-skill) | MIT | Vita porting: VitaSDK, vitaGL, memory, input, VPK, build/deploy/capture loop, measurements |
| `vita-coredump`, `vita-err`, `vita-gpucrash`, `vita-gpuprof`, `vita-perf` | tools by [shoui520](https://github.com/shoui520) (psp2_core_parse, psp2_err, psp2_gpucrash_parse, psp2-cex-gpues4-prof, libperf) | GPL-3.0 | CPU/GPU crash analysis, error codes, GPU and CPU profiling on console |

The tools are cloned into `agent/tools/` (git-ignored) on first use, as each skill explains.
Engine and conformance rules: see the skills of [`nguidle-rust`](https://github.com/vallfrr/nguidle-rust).

The `vita-port` reference was compared with `Yannis100/vita-port-skill` commit
`8a0a8519317d2ad6ca095eb13975704a67b9a78e`. Its `SKILL.md` and the debugging,
runtime and toolchain references have local edits; `check_vpk.py` was moved to
the repository's `tools/` directory. Treat all third-party guidance as unverified
until checked against the SDK and the project. The five diagnostic guides describe
separate third-party tools; their exact source revisions are not established here.
