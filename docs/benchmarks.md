# Measurements from the test system

All numbers below are observations on a **single** ASUS X99 / P104-100 test system. Workload and instrument details should be expanded as original logs become available.

## PCIe transfer rate

| Configuration | Negotiated link | Reported Strata transfer rate |
| --- | --- | --- |
| Baseline | Gen1 x4 | ~0.8 GB/s |
| Modified driver + two-stage retrain | Gen2 x4 | ~1.66–1.68 GB/s |

The change is approximately a **2x** increase in observed transfer rate. The transfer-rate measurement should not be confused with LLM inference speedup.

## Independent GPU workload

With Strata not occupying the P104's VRAM, `llama-bench` successfully exercised the P104 at **5,005 MHz memory clock**:

- Model: Qwen3.5 4B Q4 variant, approximately 2.78 GiB
- Device: `CUDA1` (P104-100 in the tested enumeration)
- All eligible layers offloaded with `-ngl 99`
- `pp512`: **1,043.06 ± 1.39 tokens/s**
- `tg128`: **47.49 ± 0.03 tokens/s**

These numbers are for a **different model** from the Strata problem investigation and are not directly comparable with its tokens/s.

## Observed idle and active clocks

| GPU | Idle SM clock | Idle memory clock | Observed idle board power |
| --- | ---: | ---: | ---: |
| NVIDIA GTX 1080 | ~139 MHz | ~810 MHz | ~10.5–11 W |
| NVIDIA P104-100 | ~139 MHz | ~405 MHz | ~5.6–6.3 W |

Idle board-power differences cannot be attributed solely to memory frequency: PCB design, memory type, voltage regulation, fan operation, and firmware also matter.

## Strata memory-clock anomaly

| State | P104 SM clock | P104 memory clock | P104 load | Observed inference speed |
| --- | ---: | ---: | ---: | ---: |
| Fast | ~1,898–1,911 MHz | 5,005 MHz | ~100% | ~15–22 tokens/s |
| Slow | ~1,898–1,911 MHz | 405 MHz | ~100% | ~3–5 tokens/s |

**Important:** This is an observed correlation, not a proven root-cause analysis. See [memory-clocking-issue.md](memory-clocking-issue.md).

## Strata: Flash-Next workload comparisons (2026-11-26)

These are **user-measured, single-run observations** on the x99 system. Models were run in Strata, using two different kinds of work. The wattage appears to be **total observed system power**, not GPU-only telemetry; measurement scope and sampling method should be recorded before doing formal energy comparisons.

### Workload 1: Edward Snowden Wikipedia article

Approximately **20,000 input tokens**, summarized by the model. Generated-token count was not recorded.

| GPU configuration | Prefill (tokens/s) | Decode (tokens/s) | Observed power (W) |
| --- | ---: | ---: | ---: |
| GTX 1080 + P104 | 328 | 17.1 | ~310 |
| GTX 1080 | 225 | 27.0 | ~285 |
| P104 | 115 | 16.8 | ~220 |

Compared with the GTX 1080 alone, adding P104 improved prefill by **45.8%**, reduced decode throughput by **36.7%**, and increased observed power by **~25 W**.

**Illustrative break-even:** 20,000/328 versus 20,000/225 implies ~28 seconds of prefill time saved. At 17.1 versus 27.0 decode tokens/s, that advantage disappears at approximately **1,300 generated tokens**. This simplified calculation omits prompt-processing overhead, caching, startup and timing variability.

### Workload 2: Generate a MATLAB PV-panel model

Around **3,000 generated tokens**; prompt length was not recorded.

| GPU configuration | Prefill (tokens/s) | Decode (tokens/s) | Observed power (W) |
| --- | ---: | ---: | ---: |
| GTX 1080 + P104 | 37 | 19.6 | ~325 |
| GTX 1080 | 77 | 20.8 | ~290 |
| P104 | 34 | 14.9 | ~250 |

Compared with GTX 1080 alone, adding P104 reduced prefill by **51.9%**, reduced decode throughput by **5.8%**, and increased observed power by **~35 W**. For 3,000 generated tokens, decode alone takes approximately **153 s** for the dual configuration versus **144 s** for GTX 1080, before accounting for the dual configuration's worse prefill.

### Interpretation and limitations

- PCIe Gen2 doubled observed **link transfer rate**, but **did not imply faster end-to-end inference** on these Strata workloads.
- Multiple GPUs can help when prompt processing dominates but hurt when generation dominates, particularly if communication and cache/offload behavior add overhead.
- **User-confirmed:** The P104 ran at its normal **5,005 MHz memory clock during all benchmark runs**. The separate intermittent 405 MHz Strata issue therefore does **not** explain these throughput results. Individual clock traces were not supplied with this comparison; the clock confirmation is from the tester. Multi-GPU split, cache/offload scheduling, communication overhead and workload characteristics remain possible explanations.
- Planned comparison: repeat both workloads using GTX 1080 + GTX 1070, with identical Strata/Flash-Next versions, quantization, context and GPU/cache settings, and monitor clocks, power and run-to-run variance.

## Independent CUDA PCIe transfer benchmark (2026-11-26)

After a confirmed cold boot and successful automatic PCIe unlock, `lspci` reported **`LnkSta: Speed 5GT/s, Width x4`** for the P104. Strata CT101 was shut down; the benchmark ran in CT100 with CUDA 12.9 and no active Strata inference.

A small custom CUDA program used a **64 MiB pinned host buffer** and **64 MiB device buffer**, copied **50 times per direction**, warmed up each transfer direction, and measured elapsed time with CUDA events. Bandwidth uses decimal GB/s. The loop measures repeated one-direction transfers, *not simultaneous bidirectional PCIe throughput*.

### Measurements

| Device and direction | Run 1 | Run 2 | Run 3 | Mean | Range |
| --- | ---: | ---: | ---: | ---: | --- |
| P104-100 Gen2 x4: host → GPU | 1.626 | 1.627 | 1.626 | **1.626 GB/s** | 1.626–1.627 |
| P104-100 Gen2 x4: GPU → host | 1.708 | 1.708 | 1.707 | **1.708 GB/s** | 1.707–1.708 |
| GTX 1080 (previously observed Gen3 x16): host → GPU | 12.046 | 12.187 | 12.192 | **12.142 GB/s** | 12.046–12.192 |
| GTX 1080 (previously observed Gen3 x16): GPU → host | 12.298 | 12.034 | 11.836 | **12.056 GB/s** | 11.836–12.298 |

The P104 reaches approximately **81.3%** (host → GPU) and **85.4%** (GPU → host) of Gen2 x4's nominal **2.0 GB/s per-direction line payload budget before protocol overhead**. In these measurements GTX 1080 has approximately **7.47×** faster host → GPU transfers and **7.06×** faster GPU → host transfers.

### Test reproducibility and caveats

The exact short CUDA source used for this test is archived at [benchmarks/pcie-bandwidth.cu](../benchmarks/pcie-bandwidth.cu). Compile with a compatible CUDA toolkit (for example, `nvcc -O3 -arch=sm_61 -o pcie-bandwidth benchmarks/pcie-bandwidth.cu`) and pass the current CUDA device index as its argument. Do not assume GPU indices remain stable after hardware changes.

- `CUDA0`: GTX 1080, `CUDA1`: P104-100 at the time of testing.
- Three repeated measurements show very little variability on P104.
- The GTX 1080 link was **previously** observed as Gen3 x16; its `LnkSta` was not rechecked in this specific test.
- A dedicated **Gen1 x4 CUDA measurement using the same code** has now been completed (see results below). Earlier ~0.8 GB/s figures came from Strata and are separate observations.
- These tests measure **transfer throughput**, not inference speedup, latency or energy efficiency.

### Matched Gen1 x4 baseline with the same CUDA program

With the automatic Gen2 service disabled and after a full power-down/start, the P104 showed both `LnkCap: Speed 2.5GT/s, Width x4` and `LnkSta: Speed 2.5GT/s, Width x4`. Strata CT101 remained stopped. The same `pcie-bandwidth` binary and CUDA1 P104 were tested three times:

| Direction | Gen1 run 1 | Gen1 run 2 | Gen1 run 3 | Gen1 mean | Gen2 mean | Gen2 / Gen1 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Host → GPU | 0.811 | 0.812 | 0.812 | **0.8117 GB/s** | **1.6263 GB/s** | **2.004×** |
| GPU → Host | 0.855 | 0.855 | 0.855 | **0.8550 GB/s** | **1.7077 GB/s** | **1.997×** |

This matched comparison demonstrates approximately **2× actual one-way CUDA transfer throughput** on the tested P104 when the Gen2 unlock procedure is enabled. The NVIDIA driver binary remained patched in the Gen1 control; the changed variable was boot-time PCIe declaration/retraining, verified by `LnkSta`. This establishes the benefit of the procedure under test, not independent compatibility with other hardware or with the unmodified NVIDIA driver.

**Restore note:** after the Gen1 test, re-enable `p104-gen2.service`, perform a controlled power-down/start with no active CUDA work, and confirm `LnkSta: Speed 5GT/s, Width x4` and correct CT101 startup before returning to normal use. This restoration has not yet been confirmed by a new log.
