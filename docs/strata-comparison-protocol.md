# Reproducible Flash-Next GPU comparison protocol

The immediate goal is to compare **GTX 1080**, **GTX 1080 + P104-100**, and **GTX 1080 + GTX 1070** on identical Strata / Flash-Next tasks, while detecting the known P104 memory-clock issue.

## Before the run

Record:

- Date, kernel, NVIDIA driver, Strata CLI version and actual engine version
- Exact Flash-Next model file, hash, quantization, context window and drafting settings
- GPU IDs, PCIe `LnkSta` rate/width, and idle power
- The GPU allocation, offload, expert cache and layer split selected by Strata
- Host power measurement method and whether reported power is wall draw or summed sensors
- Prompt and output token counts for **each** run

After installing/removing a GPU, confirm the current CUDA order. Previously `CUDA0` was GTX 1080 and `CUDA1` was P104; this is **not** guaranteed after reconfiguration.

## Workloads

**A. Long-context summary.** Reuse the *exact* Wikipedia Edward Snowden input (~20k tokens), and use the same summarization instruction and output limit for every configuration. Record time to first token, prefill tokens/s, generated count and decode tokens/s.

**B. Code generation.** Reuse the *exact* prompt requesting a MATLAB PV-panel model (~3k generated tokens in the original run). Match decoding and output limit across runs; record both prefill and decode metrics.

Use stable inference settings and the same random seed / temperature when supported. Deterministic output length makes speed comparisons easier.

## Repeat each configuration

- Run **at least 3 repetitions** per workload after a warm-up.
- Record **all runs**, not just the fastest.
- Sample each GPU's memory clocks, power and utilization throughout.
- On the P104, classify every run as **normal (5,005 MHz)** or **slow (405 MHz)**. Do not combine the two states into one reported median.
- Record both **elapsed time** and **tokens/s**. A fast prefill may not offset a slower decode.
- Compare energy as **power × measured elapsed time**, not only instantaneous watts.

## Example: per-GPU telemetry on Proxmox host

```bash
nvidia-smi -L
nvidia-smi --query-gpu=timestamp,index,name,pstate,clocks.sm,clocks.mem,power.draw,utilization.gpu,memory.used \
  --format=csv -l 1 | tee clocks-and-power.csv
```

Use the same logging interval across tests. `nvidia-smi` board power is not necessarily equivalent to total wall-system power.

Before physical GPU changes, shut down the host and remove power. Then verify PCIe link status and device ordering after boot. The Gen2 patch is tied to the tested environment; do not invoke retraining or edit NVIDIA MMIO while an active CUDA workload is running.

## Results template

| Workload | Configuration | Runs | Median TTFT | Median PP t/s | Median TG t/s | Generated tokens | Wall elapsed | Wall Wh | P104 VRAM clock |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| Snowden summary | GTX 1080 | | | | | | | | N/A |
| Snowden summary | GTX 1080 + P104 | | | | | | | | |
| Snowden summary | GTX 1080 + GTX 1070 | | | | | | | | N/A |
| MATLAB PV model | GTX 1080 | | | | | | | | N/A |
| MATLAB PV model | GTX 1080 + P104 | | | | | | | | |
| MATLAB PV model | GTX 1080 + GTX 1070 | | | | | | | | N/A |

An improvement in link bandwidth does not imply better throughput, energy efficiency, or user-perceived latency.
