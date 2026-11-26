# Unresolved: P104-100 memory clock stuck at 405 MHz

**Status: unresolved; reproduction observed repeatedly.**

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

## Still to determine

1. Does the intermittent issue reproduce with the **unmodified driver and Gen1 x4**, using the **same** Strata release and model?
2. Does it occur with other CUDA applications under comparable VRAM pressure and scheduling?
3. Does Strata's CUDA initialization, cache/offloading behavior, or GPU execution scheduling correlate with the clock-state transition?
4. Are GPU firmware differences relevant?

## Reporting

Useful diagnostic reports should include GPU board/subsystem IDs, VBIOS revision, NVIDIA driver, kernel, negotiated link state, running workload/engine build, `nvidia-smi` clock and power traces, and a reproducible test sequence. Do not share API keys, private filesystem paths, or proprietary driver binaries.
