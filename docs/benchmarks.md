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
- The P104 has a known intermittent **405 MHz memory clock** failure under Strata. **It has not been established whether these six benchmark runs all had the normal 5,005 MHz P104 memory clock.** Do not attribute the differences solely to multi-GPU architecture until memory-clock traces accompany each run.
- Planned comparison: repeat both workloads using GTX 1080 + GTX 1070, with identical Strata/Flash-Next versions, quantization, context and GPU/cache settings, and monitor clocks, power and run-to-run variance.
