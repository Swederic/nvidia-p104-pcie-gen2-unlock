# NVIDIA P104-100 PCIe Gen2 x4 unlock (experimental)

An experimental, documented method for running an NVIDIA P104-100 mining GPU at **PCIe Gen2 x4** instead of its observed **Gen1 x4** operating mode on a Linux/Proxmox host.

> **Status:** Gen2 x4 operation has been demonstrated on one specific system, including reboot-time automation and a measured transfer-rate improvement. **This is not a universal or validated driver patch.** An intermittent GPU memory-clock issue remains unresolved. The host-supplied prototype sources are archived under [experimental/](experimental/README.md) **for review only**; the proprietary NVIDIA driver patch and any patched binaries are **not** included.

## Demonstrated result

| Measurement | Before | After |
| --- | --- | --- |
| Negotiated PCIe link | Gen1 x4 (2.5 GT/s x4) | Gen2 x4 (5 GT/s x4) |
| Observed Strata transfer rate | Approximately 0.8 GB/s | Approximately 1.66–1.68 GB/s |
| Reboot persistence | N/A | Gen2 established again by a systemd boot-time procedure |

The transfer-rate figures are observations from this system, **not** a standardized cross-platform PCIe benchmark. Actual LLM throughput depends on model layout, offloading, cache behavior, and other factors.

## Tested environment

- Motherboard: ASUS Rampage V Extreme (X99)
- CPU: Intel Xeon E5-2698 v4
- OS / virtualization: Proxmox VE, Linux kernel `7.0.14-30-pve` in the latest verified configuration
- NVIDIA driver: `580.196.07` (locally modified; version-specific)
- GPU: NVIDIA P104-100, PCI vendor/device ID `10de:1b87`
- CUDA/LLM workloads: Debian LXC containers, llama.cpp and Strata
- Other GPU during testing: NVIDIA GTX 1080
- A GTX 1070 was subsequently removed; the P104 PCI address changed, and automatic detection continued to work

These observations do not establish compatibility with other P104 revisions, boards, firmware, NVIDIA releases, or kernels.

## How the tested approach works

The prototype has **three distinct parts**, all required by the tested setup:

1. A **two-byte, version-specific patch** to the locally installed NVIDIA kernel driver binary to prevent the observed Gen1 restriction.
2. A kernel module (`p104declare.c`) that modifies the relevant PCIe configuration/MMIO state.
3. A second kernel module (`p104train.c`) that requests another link retraining operation after the declaration step.

A boot-time script (`p104-gen2-boot`) validates the expected driver/kernel environment, locates a **unique** P104 by PCI ID rather than a hardcoded bus address, identifies the upstream bridge, runs the two modules in order, and verifies that the negotiated link is Gen2 x4. A systemd service runs this sequence before starting the Strata container.

**Important:** The original host-supplied kernel and boot sources are now in [experimental/](experimental/README.md). They are **not** vetted installation instructions. The NVIDIA binary patch script, exact offsets, and full pre/post validation evidence are still missing; this README deliberately does not invent them.

## Verification

For a P104 in a known slot, inspect link capability and current negotiated status with `lspci -vv -s <PCI_BDF>`. For example:

```bash
lspci -Dnn | grep -i '10de:1b87'
lspci -vv -s 0000:02:00.0 | grep -E 'LnkCap:|LnkSta:'
```

The example BDF is **not guaranteed**. It changed on the test host after removing another GPU. Confirm the actual BDF before querying it. A successful test should report `LnkSta: Speed 5GT/s, Width x4` and should be checked after a cold boot.

## Known unresolved issue: GPU memory clock

During some Strata inference requests, the P104 enters P0 with an SM clock around **1,898–1,911 MHz** but retains **405 MHz memory clock**, even while reporting approximately 99–100% GPU load and 91–95% memory-controller activity. Other requests correctly use **5,005 MHz memory clock**. Observed decode performance differed substantially (approximately 3–5 versus 15–22 tokens/s for the workload being investigated).

A separate llama.cpp benchmark successfully reached 5,005 MHz on the P104; an auxiliary CUDA-memory workload did **not** reliably resolve slow Strata requests. In some trials the memory clock rose after inference was stopped, but not consistently. **Causation is unknown:** this behavior has not been isolated from the driver modification, PCIe link mode, firmware, or Strata itself. See [the investigation notes](docs/memory-clocking-issue.md).

## Safety and limitations

- **Experimental kernel-level code:** incorrect driver patches or PCIe/MMIO writes may hang the system, corrupt data, or require a hard power cycle.
- **Not a plug-and-play installer.** Do not apply the approach to an unknown driver binary or GPU revision.
- Only patch a locally installed driver after checking exact version and SHA-256. Never distribute proprietary NVIDIA binaries.
- Do not retrain a PCIe link while the GPU has active CUDA workloads.
- Keep a known-good recovery path, including the ability to boot without the custom service and revert the driver modification.
- Do not treat the unresolved 405 MHz memory-clock issue as solved.

## Repository roadmap

- [x] Publish the initial method and measured observations
- [x] Archive the host-supplied kernel-module sources for review (not a supported release)\n- [ ] Audit and harden error handling in the archived sources
- [ ] Audit and import an exact-version, hash-checked patch script (no patched NVIDIA binaries)
- [ ] Audit and import the boot script and systemd unit
- [ ] Document rollback and boot recovery
- [ ] Test Gen1 versus Gen2 with the same Strata version and model
- [ ] Seek independent confirmation on another P104-100

See [investigation](docs/investigation.md), [benchmarks](docs/benchmarks.md), and [known issue](docs/memory-clocking-issue.md).

## License

A license will be selected when the authored kernel-module and script sources are added. No NVIDIA driver binaries are distributed in this repository.
