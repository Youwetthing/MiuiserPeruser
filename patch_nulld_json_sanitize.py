#!/usr/bin/env python3
"""
patch_nulld_json_sanitize.py

pkg names in get_package_sessions() come from sscanf-parsing raw
`dumpsys netstats` output, then get written unescaped into the
"suspicious_transmitters" JSON string field in write_json(). A package
name (or malformed dumpsys line) containing a '"' or '\\' would corrupt
the hand-rolled JSON. Same fix class as rocksteadyd's proc name --
sanitize at the parse/extraction point so every downstream consumer
(tlog messages, gaveld_emit ctx, the JSON field) is covered in one place.

Usage: run from ~/MiuiserPeruser
    python3 patch_nulld_json_sanitize.py
"""
import shutil
import sys

TARGET = "src/daemon/nulld.c"
BACKUP = TARGET + ".bak2"

EDITS = []

# --- Edit 1: add sanitize_field() helper ---------------------------------
EDITS.append((
    "Add sanitize_field() helper",
    'typedef struct { char pkg[128]; int uid; int sessions; } pkg_sessions_t;',
    '''typedef struct { char pkg[128]; int uid; int sessions; } pkg_sessions_t;

/* Package names come from parsing raw dumpsys netstats output -- a
 * '"' or '\\\\' in a malformed/adversarial line would corrupt the
 * hand-rolled JSON in write_json()'s suspicious_transmitters field.
 * Sanitize at extraction time so every downstream consumer (tlog,
 * gaveld_emit ctx, JSON) is covered. */
static void sanitize_field(char *buf, size_t bufsz)
{
    if (!buf) return;
    for (size_t i = 0; i < bufsz && buf[i] != '\\0'; i++) {
        unsigned char c = (unsigned char)buf[i];
        if (c == '"' || c == '\\\\' || c == '\\n' || c == '\\r' || c < 0x20) {
            buf[i] = '_';
        }
    }
}'''
))

# --- Edit 2: apply sanitize_field() right after pkg is parsed -----------
EDITS.append((
    "Sanitize pkg name after sscanf extraction",
    'out[count].sessions = sessions;',
    'out[count].sessions = sessions;\n            sanitize_field(out[count].pkg, sizeof(out[count].pkg));'
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

    if "sanitize_field(out[count].pkg" not in content:
        print("WARNING: sanitize_field() call site not found post-patch -- check manually.")
    else:
        print("Residual grep clean: sanitize_field() wired to pkg extraction.")


if __name__ == "__main__":
    main()
