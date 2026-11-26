# Checklist before importing working code

The original code and scripts must be obtained from the test host and reviewed. Do **not** substitute reconstructed code.

- [x] Archive `/root/p104-gen2-v10-declare/p104declare.c`
- [x] Archive `/root/p104-gen2-v10-train/p104train.c`
- [x] Archive `/usr/local/sbin/p104-gen2-boot`
- [x] Archive the text of `p104-gen2.service`
- [x] Review all four historical scripts and document the exact two-byte patch
- [x] Confirm original expected SHA and observed patched SHA; provide strict version-pinned helper
- [ ] Run the new helper's read-only check on x99 and verify end-to-end apply/revert on a safely recoverable test setup
- [ ] Confirm every PCIe/MMIO register access and the intended GPU/root port
- [ ] Replace environment-specific paths, hostnames, and BDFs where necessary
- [ ] Remove accidental credentials, tokens, personal paths, and debug artifacts
- [ ] Document module build prerequisites for the matching Proxmox kernel
- [ ] Document rollback for mismatched driver hashes and failed link retraining
- [ ] Confirm systemd dependencies and that no active CUDA workload is interrupted
- [ ] Decide on an open-source license for original authored code
- [ ] Validate the published steps against a clean boot before announcing a release

Do not upload NVIDIA's patched binary module; publish only audited instructions or scripts operating on a user's own installation.
