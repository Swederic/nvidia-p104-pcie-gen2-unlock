# Archived prototype source: read before use

These four files were supplied from the working X99 host and placed here **as experimental reference material**, not a supported installer:

- `p104declare.c`: vendor-specific GPU BAR0 MMIO writes, PCIe capability and link-control changes, an initial retrain request.
- `p104train.c`: second root-port retraining request.
- `p104-gen2-boot`: kernel/driver hash checks, unique P104 detection, module load/unload, link verification, and start of CT101.
- `p104-gen2.service`: Proxmox-specific boot integration.

The original NVIDIA driver **patch script and its offsets are not included**. The boot script assumes a locally patched NVIDIA kernel module and a particular SHA-256 hash. There is no independent evidence that these files work on hardware other than the documented system.

## Review findings (not yet fixed in the archived files)

1. **Error propagation:** `p104declare.c` returns `0` from its init routine even after several MMIO/PCIe errors. Likewise `p104train.c` can log an unexpected initial state or a failed status read and return success. The boot script includes a final negotiated-link check, but silent errors and partially applied modifications remain risks.
2. **Partial hardware configuration:** `p104declare.c` can stop after one or more writes without restoring previous register values. Its unload routine explicitly does not restore hardware state.
3. **Hardware-specific register constants:** BAR0 offsets, magic values and factory-state checks were observed on one board/card. They must not be assumed universal, even across P104-100 variants.
4. **Driver patch provenance:** the boot script checks the SHA-256 of an already modified `nvidia.ko` but does not show how the original module was identified or how the patch was constructed. Do not skip validation of original and patched binaries.
5. **Runtime-safety checks:** the boot script checks CT101 and NVIDIA device users, but other GPU workloads/containers and races still need auditing before this becomes an installer.
6. **Hardcoded host configuration:** the kernel release, CTID `101`, absolute module paths, service dependencies and the driver location are specific to the verified Proxmox installation.
7. **Build and regression validation:** the uploaded archive contains C sources, a shell script and systemd unit only; it does not contain a Makefile, compiled objects, pre/post-patch binary, complete test logs or rollback procedure.

## Safe path to a release

Audit the sources, fix error handling, add clean rollback documentation, verify module build steps against the exact matching kernel headers and test **Gen1 versus Gen2** while keeping the driver and Strata workload otherwise constant. Do not execute these files on production hardware without an out-of-band recovery option.

The memory-clock anomaly described in [the investigation](../docs/memory-clocking-issue.md) is unresolved, and may be independent of the PCIe upgrade.
