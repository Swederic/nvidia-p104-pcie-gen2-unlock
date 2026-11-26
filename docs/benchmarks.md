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
