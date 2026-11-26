#!/usr/bin/env python3
"""Strict, version-pinned P104 no-Gen1 patch for ONE verified NVIDIA module.

Default operation is read-only. Applying/reverting replaces the on-disk module
atomically and requires an original-module backup. Reboot before testing.
"""
import argparse
import hashlib
import os
from pathlib import Path
import shutil
import sys
import tempfile

KERNEL = "7.0.14-30-pve"
VERSION = "580.196.07"
DEFAULT_MODULE = Path(f"/lib/modules/{KERNEL}/updates/dkms/nvidia.ko")
ORIGINAL_SHA = "c39da54254828b703b53d6318de08204956f4158842d3f19627fb210a9204f57"
PATCHED_SHA = "13a8cc4c401ce247c06320cb516cb079a11343cc8a5b5aa588f59199778f52dd"
SIGNATURE = bytes.fromhex(
    "8d42ff415641554989fd41544531e453"
    "4883ed404889751089550c83f8020f87"
)
ALTERED = bytearray(SIGNATURE)
ALTERED[2] = 0xFE
ALTERED[0x1D] = 0x01
ALTERED = bytes(ALTERED)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def abort(msg):
    raise SystemExit("ABORT: " + msg)


def atomic_replace(path, data):
    fd, tmp = tempfile.mkstemp(prefix=".p104-nvidia-", dir=path.parent)
    try:
        with os.fdopen(fd, "wb") as output:
            output.write(data)
            output.flush()
            os.fsync(output.fileno())
        shutil.copymode(path, tmp)
        os.replace(tmp, path)
        directory_fd = os.open(path.parent, os.O_RDONLY)
        try:
            os.fsync(directory_fd)
        finally:
            os.close(directory_fd)
    finally:
        if os.path.exists(tmp):
            os.unlink(tmp)


def main():
    cli = argparse.ArgumentParser(description=__doc__)
    cli.add_argument("action", choices=("check", "apply", "revert"), nargs="?", default="check")
    cli.add_argument("--module", type=Path, default=DEFAULT_MODULE)
    cli.add_argument("--backup", type=Path, default=None)
    args = cli.parse_args()
    module = args.module
    backup = args.backup or module.with_name(module.name + ".p104-original")

    if not module.is_file() or module.is_symlink():
        abort("module is missing or is a symlink")
    data = module.read_bytes()
    sha = digest(data)
    print(f"Module: {module}\nSHA256: {sha}\nExpected driver: {VERSION}, kernel: {KERNEL}")

    if args.action == "check":
        if sha == ORIGINAL_SHA:
            if data.count(SIGNATURE) != 1 or data.count(ALTERED):
                abort("original SHA matches but signature count is wrong")
            print(f"Verified ORIGINAL; signature at {data.index(SIGNATURE):#x}")
        elif sha == PATCHED_SHA:
            if data.count(ALTERED) != 1 or data.count(SIGNATURE):
                abort("patched SHA matches but signature count is wrong")
            print(f"Verified PATCHED; signature at {data.index(ALTERED):#x}")
        else:
            abort("unknown binary hash: do not patch this module")
        return

    if os.geteuid() != 0:
        abort("root privileges required for modifications")
    if args.module == DEFAULT_MODULE and os.uname().release != KERNEL:
        abort("running kernel does not match pinned target")

    if args.action == "apply":
        if sha == PATCHED_SHA:
            print("Already patched; no changes")
            return
        if sha != ORIGINAL_SHA:
            abort("unrecognized original module hash")
        if data.count(SIGNATURE) != 1 or data.count(ALTERED):
            abort("signature must occur exactly once")
        pos = data.index(SIGNATURE)
        result = bytearray(data)
        result[pos + 2] = 0xFE
        result[pos + 0x1D] = 0x01
        result = bytes(result)
        if digest(result) != PATCHED_SHA:
            abort("generated patched binary does not match independently observed SHA")
        if backup.exists():
            if backup.is_symlink() or digest(backup.read_bytes()) != ORIGINAL_SHA:
                abort("existing backup is invalid")
        else:
            # Use exclusive creation: never silently overwrite an existing backup.
            with backup.open("xb") as out:
                out.write(data)
                out.flush()
                os.fsync(out.fileno())
            shutil.copymode(module, backup)
        if digest(backup.read_bytes()) != ORIGINAL_SHA:
            abort("backup verification failed")
        atomic_replace(module, result)
        print(f"PATCHED and verified: {digest(module.read_bytes())}")
    else:
        if sha == ORIGINAL_SHA:
            print("Already original; no changes")
            return
        if sha != PATCHED_SHA:
            abort("refusing to revert an unrecognized module")
        if not backup.is_file() or backup.is_symlink():
            abort("no valid backup file")
        original = backup.read_bytes()
        if digest(original) != ORIGINAL_SHA or original.count(SIGNATURE) != 1:
            abort("backup integrity check failed")
        atomic_replace(module, original)
        print(f"RESTORED and verified: {digest(module.read_bytes())}")

    print("Disk image updated. Reboot required before testing the changed module.")


if __name__ == "__main__":
    main()
