# Version-locked NVIDIA module patch

**Experimental. Tested only with a locally installed, uncompressed NVIDIA 580.196.07 `nvidia.ko` from Linux `7.0.14-30-pve`. Do not run `apply` on another build.**

The original investigation used the `_nv017105rm` function's mode gate. On the examined driver, it accepted mode values 1, 2 and 3; mode 1 was observed writing a register value that caused the PCIe link to return to Gen1 during CUDA initialization. The two-byte modification changes the unsigned bounds check to accept modes 2 and 3, but reject mode 1.

| File-relative offset | Before | After | x86-64 effect |
| --- | --- | --- | --- |
| Signature start + `0x02` | `FF` | `FE` | `lea -1(%rdx),%eax` → `lea -2(%rdx),%eax` |
| Signature start + `0x1D` | `02` | `01` | `cmp $2,%eax` → `cmp $1,%eax` |

This is **not** an arbitrary search-and-replace of NVIDIA drivers. The published helper `scripts/p104-driver-patch.py` checks a pinned original SHA-256, requires exactly one byte signature, constructs the patched bytes in memory, validates the *entire output* against the SHA-256 measured on the working host, and only then writes to disk. Revert requires a verified original backup.

## Verified binary hashes

| Binary | SHA-256 |
| --- | --- |
| Original | `c39da54254828b703b53d6318de08204956f4158842d3f19627fb210a9204f57` |
| Patched | `13a8cc4c401ce247c06320cb516cb079a11343cc8a5b5aa588f59199778f52dd` |

The **patched** SHA was independently checked against the running host's on-disk module on 2026-11-26. The **original** SHA is provided by the host's archived `apply_patch.py`; the original binary has not been supplied here for independent recomputation. Accordingly the strict patch script will abort unless the expected original and generated patched hashes actually match on the user's machine.

## Usage (read first)

Commands below are shown **for reference on the exact test setup only**:

```bash
python3 scripts/p104-driver-patch.py check
sudo python3 scripts/p104-driver-patch.py apply
# After a controlled reboot, verify both the driver and negotiated link.
sudo python3 scripts/p104-driver-patch.py revert
# Reboot again to load the restored driver.
```

**Before applying:** ensure out-of-band recovery, a verified backup, no active CUDA workload, and a matching driver module. The script changes the on-disk module, not an already loaded copy. Never retrain the PCIe link with running GPU jobs.

## Historical scripts reviewed

- `p104-driver-patch/apply_patch.py`: strict original SHA and signature verification; patches the two bytes; atomic replacement; retains a backup.
- `p104-driver-patch/check_patch.py`: read-only detection of either signature.
- `p104-gen2/patch/patch_no_gen1.py`: earlier research prototype, containing disassembly and mode-bound reasoning; **do not use as a release installer**, because its apply/revert validation and write handling are less strict.
- `p104-gen2-v10-patch.py`: **unrelated to the NVIDIA binary patch**; a one-time development migration that converted the two kernel modules from hardcoded PCI addresses to dynamic PCI discovery. It is not meant to run against the published code (which is already converted).

## Limitations

- Original proof was based on inspection of one driver build and observed runtime behavior, not official NVIDIA documentation.
- Patch may cease to work after DKMS rebuilds, driver updates or kernel upgrades; never automatically repatch a different binary.
- Secure Boot/module signing and distro-specific compressed module handling are outside this procedure.
- Kernel driver patch alone does not accomplish Gen2: the separate declaration and retraining steps were necessary on the tested host.
- The intermittent P104 memory-clock anomaly under Strata remains unresolved and may or may not relate to this modification.
