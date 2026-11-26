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
