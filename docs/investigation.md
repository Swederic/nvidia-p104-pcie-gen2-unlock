# Investigation and working method

## Background

An NVIDIA P104-100 mining GPU was observed negotiating **PCIe Gen1 x4** on an ASUS X99 platform. The objective was to determine whether a stable **Gen2 x4** link could be established without replacing the GPU.

This document describes the **observed working sequence**, not a generalized guide. Exact kernel source and driver patch bytes have not yet been imported or audited.

## Sequence discovered on the test host

1. Identify the P104-100 using PCI vendor/device ID `10de:1b87`.
2. Apply a narrowly targeted, locally generated modification to the installed NVIDIA driver (two-byte patch on driver `580.196.07`). The working boot flow verifies the expected module hash/version before proceeding.
3. Use a first custom kernel module, `p104declare`, for the required PCIe declaration/configuration step.
4. Use a second custom kernel module, `p104train`, to request an additional PCIe link retrain.
5. Inspect the negotiated link, expecting **5 GT/s x4**.
6. Run this through a systemd-controlled boot procedure before Strata starts.

The two custom modules' working sources were stored on the test host at:

```text
/root/p104-gen2-v10-declare/p104declare.c
/root/p104-gen2-v10-train/p104train.c
```

The boot script was stored at `/usr/local/sbin/p104-gen2-boot`, with a systemd service named `p104-gen2.service`. These paths are **provenance information**, not packaged installation assets.

## Dynamic device discovery

The implementation was revised to avoid hardcoded PCI BDF assumptions. On the verified system, removing the GTX 1070 changed the P104 address from `0000:03:00.0` to `0000:02:00.0`. The P104 remained identifiable by vendor/device ID, and the upstream root port could be discovered from the PCI device's topology.

The working implementation requires exactly one matching P104, avoiding silent selection of the wrong card.

## Reboot behavior

A cold-boot check confirmed that the startup procedure restored **Gen2 x4**. The systemd ordering matters: avoid running declaration/retrain steps while Strata is already using CUDA.

## Important distinctions

- A link capability or advertised maximum does not prove the **negotiated** link speed. Check `LnkSta`.
- A negotiated Gen2 link alone does not prove higher real-world LLM token throughput. Model architecture, expert caching, and CPU/GPU split strongly influence throughput.
- The intermittent **405 MHz** memory-clock behavior is unresolved. It must not be silently attributed to or excluded from the PCIe modification.

## Before writing installation instructions

The exact patch script, SHA-256 checks, original module hash, kernel code, and service ordering must be extracted from the running host and independently audited. In particular, register values and patch offsets must not be reconstructed from narrative descriptions.

## Cold-boot verification (2026-11-26)

The operator supplied **two complete boot-service logs**, one from the running configuration before a deliberate cold-start and one after. These are observations on the same host, not independent-machine validation.

| Event | Before cold start (16:19) | After cold start (18:34) |
| --- | --- | --- |
| NVIDIA module and kernel-module checks | Passed | Passed |
| P104 and upstream root port | `0000:02:00.0` / `0000:00:02.0` | Same |
| Link after v6 declaration | Gen1 x4 (`speed=1 width=4`) | Gen1 x4 (`speed=1 width=4`) |
| Additional v7 retrain requested | Yes | Yes |
| Service reported Gen2 x4 | 16:19:32 | 18:34:51 |
| CT101 (Strata) started successfully | 16:19:35 | 18:34:54 |
| Independent `lspci` negotiated link | `LnkSta: Speed 5GT/s, Width x4` | `LnkSta: Speed 5GT/s, Width x4` |

The post-cold-start service began at **18:34:45** and finished at **18:34:54**, with a successful Gen2 x4 check **before** CT101 was started. This confirms the intended ordering for this tested boot, while not establishing reliability across other hardware or driver revisions.

The v6 step deliberately did **not** finish at Gen2 on either recorded boot; the subsequent v7 root-port retrain was necessary in both recorded sequences.

### Key post-cold-start log excerpt

```text
Nov 26 18:34:45 P104-GEN2: NVIDIA patch and kernel modules verified
Nov 26 18:34:45 P104-GEN2: P104 found at 0000:02:00.0, upstream 0000:00:02.0
Nov 26 18:34:49 P104-GEN2: After v6: speed=1 width=4
Nov 26 18:34:49 P104-GEN2: Requesting additional root-port retrain (v7)
Nov 26 18:34:51 P104-GEN2: SUCCESS: PCIe Gen2 x4 established
Nov 26 18:34:54 P104-GEN2: CT101 started successfully
```

```text
LnkCap: Port #0, Speed 5GT/s, Width x4
LnkSta: Speed 5GT/s, Width x4
```
