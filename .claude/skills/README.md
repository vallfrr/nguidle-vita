# Skills for assistants (Claude Code, Codex, others)

| skill | source | license | role |
|---|---|---|---|
| `vita-port` | [Yannis100/vita-port-skill](https://github.com/Yannis100/vita-port-skill) | MIT | Vita porting: VitaSDK, vitaGL, memory, input, VPK, build/deploy/capture loop, measurements |
| `vita-coredump`, `vita-err`, `vita-gpucrash`, `vita-gpuprof`, `vita-perf` | tools by [shoui520](https://github.com/shoui520) (psp2_core_parse, psp2_err, psp2_gpucrash_parse, psp2-cex-gpues4-prof, libperf) | GPL-3.0 | CPU/GPU crash analysis, error codes, GPU and CPU profiling on console |

The tools are cloned into `agent/tools/` (git-ignored) on first use, as each skill explains.
Engine and conformance rules: see the skills of [`nguidle-rust`](https://github.com/vallfrr/nguidle-rust).
