# Unresolved: P104-100 memory clock stuck at 405 MHz

**Status: unresolved; strongly associated with the Gen2 unlock configuration in initial matched observations.**

## Symptoms

During some Strata inference requests, the P104 remains in P0 with an SM clock of approximately 1,898–1,911 MHz while its VRAM clock remains at 405 MHz. The GPU reports approximately 99–100% utilization; `nvidia-smi pmon` reported approximately 91–95% memory activity for the Strata process during a slow run. Board power was commonly around 40–50 W, versus substantially higher when memory ran at 5,005 MHz.

Observed decoding rates for the investigated Strata workload were ~3–5 tokens/s in the slow state versus ~15–22 tokens/s in the fast state.

## Tests and observations

- A prefill with approximately 20,853 prompt tokens **did not** raise the 405 MHz memory clock.
- `nvidia-smi -lmc 5005,5005` returned **Not Supported** on this P104.
- `nvidia-smi -ac 5005,1911` also returned **Not Supported**.
- A separate `llama-bench` process on the P104 reached 5,005 MHz during a standalone CUDA workload.
- A small custom CUDA memory-workload process did **not** reliably cause the clock to rise while Strata was actively generating.
- Ending a slow Strata generation sometimes preceded a rise to 5,005 MHz at 0% utilization a few seconds later. On another trial, it remained at 405 MHz until the card entered P8 roughly 26 seconds later.
- Subsequent Strata requests could start normally at 5,005 MHz **or** re-enter the slow 405 MHz state.
- Concurrent heavy work on the **other GPU** (GTX 1080) did not raise the P104's memory clock while Strata continued to generate.
- `nvidia-smi` reported no active GPU clock-event throttling reasons during the slow state.
- Only the Strata process was present in the P104 compute-process listing during a sampled slow state.

## Sample transition

```text
02:00:09  P0  1898 MHz SM  405 MHz memory   100% load
02:00:10  P0  1898 MHz SM  405 MHz memory     0% load
02:00:12  P0  1607 MHz SM 5005 MHz memory     0% load
02:00:14  P0  1771 MHz SM 5005 MHz memory    83% load
02:00:15  P0  1898 MHz SM 5005 MHz memory   100% load
```

This transition was **not universal**, and should not be described as a reliable workaround.

## Gen1-versus-Gen2 control (2026-11-26)

After completing the Gen2 transfer benchmark, the operator disabled the automatic `p104-gen2.service`, performed a power-down/cold-start, and independently verified `LnkCap` and `LnkSta` at **2.5 GT/s x4**. The NVIDIA driver binary **remained patched**; the tested difference was disabling the boot-time declaration and retrain procedure, resulting in a Gen1 link.

The operator then ran repeated Strata/Flash-Next inference requests on P104, deliberately waiting for the GPU memory clock to return to its idle **405 MHz** before each subsequent request, including both shorter and longer wait periods. **No recurrence of the slow P0/405 MHz memory-clock state was observed in this Gen1 configuration.** In contrast, the earlier Gen2 configuration had repeatedly reproduced the issue after both short and long waits.

This is strong **within-system evidence associating the anomaly with the Gen2-unlock configuration**. It does *not* yet establish that negotiated Gen2 speed itself is the root cause: `p104declare` performs GPU MMIO/PCIe changes, and the separate root-port retrain is another variable. The exact number of successful Gen1 requests was not recorded, and the sample does not establish a statistical failure-rate bound. A matched return-to-Gen2 run has now been completed: the fault returned on the first reported attempt.

The standalone CUDA copy benchmark verified **0.812 / 0.855 GB/s (Gen1 x4)** versus **1.626 / 1.708 GB/s (Gen2 x4)** in host-to-device/device-to-host directions, so the Gen2 bandwidth improvement is real even though the accompanying memory-clock issue warrants investigation.

## Still to determine

1. Can the effect be separated between declaration writes, PCIe link speed and retrain?
2. Does it occur with an **unmodified** NVIDIA driver in Gen1 x4 (separate control; not yet tested)?
3. Does it occur with other CUDA applications under comparable VRAM pressure and scheduling?
4. Does Strata's CUDA initialization, cache/offloading behavior, or GPU execution scheduling correlate with the clock-state transition?
5. Are GPU firmware differences relevant?

## Reporting

Useful diagnostic reports should include GPU board/subsystem IDs, VBIOS revision, NVIDIA driver, kernel, negotiated link state, running workload/engine build, `nvidia-smi` clock and power traces, and a reproducible test sequence. Do not share API keys, private filesystem paths, or proprietary driver binaries.

## A–B–A confirmation: return to Gen2 (2026-11-26, 20:42–20:55)

After the successful Gen1 control, the operator re-enabled `p104-gen2.service` and cold-started the machine. The boot log reported:

```text
20:42:59 NVIDIA patch and kernel modules verified
20:42:59 P104 found at 0000:02:00.0, upstream 0000:00:02.0
20:43:02 After v6: speed=1 width=4
20:43:02 Requesting additional root-port retrain (v7)
20:43:04 SUCCESS: PCIe Gen2 x4 established
20:43:07 CT101 started successfully
LnkSta: Speed 5GT/s, Width x4
```

The **first reported Strata inference attempt** after returning to Gen2 reproduced the pathological P0 state with a **1,911 MHz SM clock, 405 MHz VRAM clock, 100% GPU utilization and ~41–47 W power**, for at least 20 seconds during the captured samples:

```text
20:55:02 P8  139 MHz  405 MHz   6.05 W    0%
20:55:12 P8  139 MHz  405 MHz   5.71 W    0%
20:55:22 P8  139 MHz  405 MHz   5.71 W    0%
20:55:32 P0 1911 MHz  405 MHz  40.85 W  100%
20:55:42 P0 1911 MHz  405 MHz  42.46 W  100%
20:55:52 P0 1911 MHz  405 MHz  46.57 W  100%
```

**A–B–A summary:** A (Gen2): anomalous clock state reproduced previously; B (Gen1 with the same patched driver): no anomalous state in repeated trials while allowing VRAM to idle before each test; A (Gen2 restored): abnormal state **on the first attempt**. This substantially strengthens the association with the Gen2 unlock **procedure/configuration**, but does **not** isolate whether the causal factor is the link speed itself, declaration MMIO edits, retrain sequence, or a driver/firmware interaction. Refrain from describing the link speed alone as proven cause.

The Gen2 procedure should be considered **unreliable for Strata inference on this particular system** until the cause is identified.

## Extended trace: recovery at workload phase boundary (2026-11-26, 20:54–20:58)

The operator subsequently supplied a **10-second sampling trace** from the Gen2 A-return run. The low-clock behavior was *not persistent for the whole inference session*. The operator reports that VRAM clock recovered after completion of prompt processing. This is consistent with the first observed idle-to-high-clock transition, but prompt/decode boundary times were not separately logged.

| Time | P-state | SM MHz | VRAM MHz | GPU util. | Board W | Note |
| --- | --- | ---: | ---: | ---: | ---: | --- |
| 20:55:22 | P8 | 139 | 405 | 0% | 5.71 | Idle before work |
| 20:55:32–20:56:22 | P0 | 1911 | **405** | 100% | 40.85–47.90 | Wrong memory clock throughout sampled active interval |
| 20:56:32 | P0 | 1607 | **5005** | 0% | 39.14 | Frequency recovered as utilization dropped |
| 20:56:52–20:57:02 | P0 | 1898–1911 | 5005 | 100% | 84.44–94.00 | Active work at normal memory clock |
| 20:57:42 | P8 | 139 | 405 | 0% | 5.95 | Returned to idle |
| 20:57:52–20:58:02 | P0 | 1898 | **405** | 100% | 42.44–42.95 | Fault recurs on next busy phase |
| 20:58:12–20:58:22 | P0 | 1898 | 405 | 0% | 25.07 | Utilization ceased, still low memory clock |
| 20:58:32 | P8 | 139 | 405 | 0% | 5.95 | Idle |
| 20:58:42 | P0 | 1898 | **5005** | 100% | 140.12 | Later active sample at normal clock |

**Interpretation:** Under Gen2 unlock, the card can enter P0 at a high SM clock yet remain at its idle 405 MHz VRAM clock during sustained activity. At least one recovery occurred with utilization at 0%, and a subsequent request reproduced the problem. The last high-frequency sample does *not* establish precisely when or why the transition happened, given the 10-second interval.

**Hypothesis, not demonstrated mechanism:** GPU driver/firmware P-state/VRAM-clock transitions may interact with the PCIe Gen2 declaration and retrain procedure. The repeated successful Gen1 control and Gen2 A-return reproduction support association, not identification of the precise register or component responsible.

## CUDA keepalive experiment (2026-11-26, ~21:09–21:39)

The operator tested a lightweight CUDA keepalive in CT100 targeting the P104 (CUDA1) while keeping Gen2 x4 enabled. The simple helper repeatedly copies 8 MiB within GPU device memory at nominal 200 ms intervals.

- Under keepalive, P104 remained in **P0, 1,607 MHz SM / 5,005 MHz VRAM**, even with **0% GPU utilization**, drawing approximately **38.8–39.2 W** at the board; wall-system idle consumption reportedly increased **58 W → 91 W** (**+33 W**).
- A prolonged supplied telemetry log covering approximately **21:09–21:19** shows persistently high VRAM clock, including during sustained inference beginning ~21:13. The operator reported **no 405 MHz failures** while keepalive was in effect.
- Following a longer wait with the card observed back in **P8 (139 MHz SM / 405 MHz VRAM, ~5.6–5.8 W)** at 21:38:42–21:38:56, a subsequent inference request immediately entered **P0 at 1,911 MHz SM but only 405 MHz VRAM**, 100% GPU utilization. Samples at 21:38:58 through 21:39:12 remained wrong-clock at **~35–66 W**. The exact keepalive stop time was not supplied.

These observations support (but do not prove) a **low-power-to-active memory-clock transition** issue in the Gen2-unlocked configuration. A continuously active CUDA context or high memory clock can apparently avoid the symptom, at the cost of approximately **33 W more wall-system idle power**. The experiment does **not** distinguish the effects of the helper's work, CUDA context residency, and persistent P0 state. It is not yet a practical energy-efficient workaround.

Further tests should avoid changing kernel registers during active workloads. A short pre-inference warm-up might help but must be evaluated separately: a failed P8→P0 transition may not be corrected once a request is in progress.
