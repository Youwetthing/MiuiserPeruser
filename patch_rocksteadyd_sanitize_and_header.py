#!/usr/bin/env python3
"""
patch_rocksteadyd_sanitize_and_header.py

Two edits, closing out the JSON/wire-protocol axis for rocksteadyd
before commit:

1. sanitize_field() (local static, same fleet convention as
   fugitoidd's fg_app/ANR/CRASH fields and bebopd's wl->tag) applied
   to procs[i].name immediately after it's parsed out of the (comm)
   field in /proc/[pid]/stat. A process name containing '|' would
   corrupt the '|'-delimited APRIL protocol in splinterd_emit(); one
   containing a raw quote or control char would corrupt
   write_results()'s hand-rolled JSON if name ever gets threaded
   through there. Sanitizing at extraction time (rather than at each
   emit call site) covers printf, gaveld_emit, and splinterd_emit in
   one place.

2. Stale header comment ("cpu0-3 efficiency, cpu4-7 performance")
   updated to reflect the N-cluster-generic detection already shipped.

Usage: run from ~/MiuiserPeruser
    python3 patch_rocksteadyd_sanitize_and_header.py
"""
import shutil
import sys

TARGET = "src/daemon/rocksteadyd.c"
BACKUP = TARGET + ".bak7"

EDITS = []

# --- Edit 1: header comment -------------------------------------------
EDITS.append((
    "Update stale N-cluster header comment",
    '''8-core (cpu0-3 efficiency,
 * cpu4-7 performance) ?''',
    '''8-core, N-cluster (clusters
 * detected dynamically via cpuinfo_max_freq) ?'''
))

# --- Edit 2: add sanitize_field() helper --------------------------------
EDITS.append((
    "Add sanitize_field() helper",
    '''/* ?? /proc/[pid]/stat reader ???????????????????????????????????????????? */''',
    '''/* ?? Field sanitization (wire-protocol safety) ??????????????????????????? */

/* Process comm names can contain '|', which would corrupt the
 * '|'-delimited APRIL protocol used by splinterd_emit(), or raw
 * quote/control characters that would corrupt write_results()'s
 * hand-rolled JSON if name is ever threaded through there. Same fix
 * class as fugitoidd's fg_app/ANR/CRASH fields and bebopd's wl->tag.
 * Copied as a local static per the fleet convention. */
static void sanitize_field(char *buf, size_t bufsz)
{
    if (!buf) return;
    for (size_t i = 0; i < bufsz && buf[i] != '\\0'; i++) {
        unsigned char c = (unsigned char)buf[i];
        if (c == '|' || c == '"' || c == '\\n' || c == '\\r' || c < 0x20) {
            buf[i] = '_';
        }
    }
}

/* ?? /proc/[pid]/stat reader ???????????????????????????????????????????? */'''
))

# --- Edit 3: apply sanitize_field() at extraction point -----------------
EDITS.append((
    "Sanitize p->name after extraction from (comm) field",
    '''            strncpy(p->name, nb + 1, len);
            p->name[len] = '\\0';
        }''',
    '''            strncpy(p->name, nb + 1, len);
            p->name[len] = '\\0';
            sanitize_field(p->name, sizeof(p->name));
        }'''
))


def main():
    try:
        with open(TARGET, "r") as f:
            content = f.read()
    except FileNotFoundError:
        print(f"ABORT: {TARGET} not found -- run from ~/MiuiserPeruser")
        sys.exit(1)

    for label, old, new in EDITS:
        count = content.count(old)
        if count == 0:
            print(f"ABORT: anchor for '{label}' not found verbatim. No changes written.")
            sys.exit(1)
        if count > 1:
            print(f"ABORT: anchor for '{label}' matched {count} times, expected exactly 1. No changes written.")
            sys.exit(1)

    shutil.copy2(TARGET, BACKUP)
    print(f"Backed up {TARGET} -> {BACKUP}")

    for label, old, new in EDITS:
        content = content.replace(old, new)
        print(f"Applied: {label}")

    with open(TARGET, "w") as f:
        f.write(content)
    print(f"Patched {TARGET} ({len(EDITS)} edits)")

    if "cpu0-3 efficiency" in content:
        print("WARNING: stale header text still present -- check manually.")
    else:
        print("Residual grep clean: stale header comment gone.")

    if "sanitize_field(p->name" not in content:
        print("WARNING: sanitize_field() call site not found post-patch -- check manually.")
    else:
        print("Residual grep clean: sanitize_field() wired to p->name.")


if __name__ == "__main__":
    main()
