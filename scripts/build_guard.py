#!/usr/bin/env python3
import os
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC_CORE = ROOT / "src" / "core"
SRC_DAEMON = ROOT / "src" / "daemon"

# Expected sources (must match CMake)
CORE_SOURCES = {
    "platform/rish_pipe.c",
    "platform/april_common.c",
    "platform/april_linux.c",
    "event/april_event.c",
    "detection/leo_detection.c",
    "detection/raph_memory.c",
    "detection/raph_network.c",
    "detection/don_integrity.c",
    "detection/don_behavior.c",
    "detection/casey_hook.c",
    "detection/casey_kernel.c",
    "detection/mikey_miui.c",
    "detection/don_memorypressure.c",
    "log/april_log.c",
    "fugitoid_log_impl.c",
    "resource_snapshot.c",
}

DAEMON_SOURCES = {
    "main.c",
    "daemon_core.c",
    "ipc.c",
    "service.c",
    "daemon_common.c",
}

ALLOWED_EXTS = {".c", ".h"}


def log(msg):
    print(f"🐢 [KRANG] {msg}")


def ensure_dir(path: Path):
    if not path.exists():
        log(f"Creating missing directory: {path}")
        path.mkdir(parents=True, exist_ok=True)


def scan_tree(base: Path):
    files = []
    for p in base.rglob("*"):
        if p.is_file():
            files.append(p)
    return files


def normalise_filenames(base: Path):
    """Silently normalise casing and extensions inside base."""
    for p in scan_tree(base):
        rel = p.relative_to(base)
        ext = p.suffix
        if ext.lower() not in ALLOWED_EXTS:
            # silently delete junk like .o, .obj, .tmp in source tree
            if ext in {".o", ".obj", ".tmp", ".log"}:
                log(f"Removing stale file: {rel}")
                try:
                    p.unlink()
                except OSError:
                    pass
            continue

        # normalise to lowercase path
        target_rel = Path(str(rel).lower())
        target = base / target_rel
        if target != p:
            ensure_dir(target.parent)
            log(f"Normalising filename: {rel} -> {target_rel}")
            try:
                if target.exists():
                    target.unlink()
                p.rename(target)
            except OSError:
                pass


def check_required(base: Path, required_rel_paths, label: str):
    missing = []
    for rel in required_rel_paths:
        if not (base / rel).exists():
            missing.append(rel)
    if missing:
        log(f"{label}: missing required sources:")
        for m in missing:
            print(f"  - {m}")
        return False
    return True


def detect_duplicate_filenames(dirs):
    seen = {}
    dupes = []
    for base in dirs:
        for p in scan_tree(base):
            name = p.name

            # Ignore CMake files
            if name.lower().startswith("cmakelists"):
                continue

            locs = seen.setdefault(name, [])
            locs.append(p)
    for name, locs in seen.items():
        if len(locs) > 1:
            dupes.append((name, locs))
    if dupes:
        log("Duplicate filenames detected across modules:")
        for name, locs in dupes:
            print(f"  {name}:")
            for p in locs:
                print(f"    - {p}")
        return False
    return True


def main():
    log("Initialising self-healing build guard…")

    ensure_dir(SRC_CORE)
    ensure_dir(SRC_DAEMON)

    # Normalise filenames and clean junk
    normalise_filenames(SRC_CORE)
    normalise_filenames(SRC_DAEMON)

    ok = True

    # Required source checks
    if not check_required(SRC_CORE, CORE_SOURCES, "CORE"):
        ok = False
    if not check_required(SRC_DAEMON, DAEMON_SOURCES, "DAEMON"):
        ok = False

    # Duplicate filename detection
    if not detect_duplicate_filenames([SRC_CORE, SRC_DAEMON]):
        ok = False

    if not ok:
        log("Fatal issues detected. Halting build.")
        return 1

    log("Build guard completed. All systems go.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
