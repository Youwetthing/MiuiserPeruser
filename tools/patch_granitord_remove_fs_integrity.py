#!/usr/bin/env python3
import sys

PATH = "src/daemon/granitord.c"

EDITS = []

EDITS.append(('''static void check_filesystem_integrity(char *fs_json, size_t fs_size) {
    fs_json[0] = 0;

    /* dm-verity status from dmesg */
    char buf[4096];
    char *verity = rish_read("dmesg 2>/dev/null | grep -i 'dm-verity.*corruption' | tail -3",
                              buf, sizeof(buf));
    if (verity && strlen(verity) > 0) {
        char *line = strtok(verity, "\\n");
        while (line) {
            char entry[512];
            snprintf(entry, sizeof(entry), "{\\"type\\":\\"verity_corruption\\",\\"msg\\":\\"%.200s\\"},", line);
            strncat(fs_json, entry, fs_size - 1);
            line = strtok(NULL, "\\n");
        }
    }

    /* f2fs errors */
    char *f2fs = rish_read("dmesg 2>/dev/null | grep -iE 'f2fs.*error|f2fs.*corruption' | tail -3",
                            buf, sizeof(buf));
    if (f2fs && strlen(f2fs) > 0) {
        char *line = strtok(f2fs, "\\n");
        while (line) {
            char entry[512];
            snprintf(entry, sizeof(entry), "{\\"type\\":\\"f2fs_error\\",\\"msg\\":\\"%.200s\\"},", line);
            strncat(fs_json, entry, fs_size - 1);
            line = strtok(NULL, "\\n");
        }
    }

    /* Check if verity is disabled on any partition */
    char *mounts = rish_read("cat /proc/mounts 2>/dev/null | grep -E 'system|vendor' | grep -v 'dm-verity' | tr '\\n' ' '",
                              buf, sizeof(buf));
    if (mounts && strlen(mounts) > 0) {
        char entry[256];
        snprintf(entry, sizeof(entry), "{\\"type\\":\\"verity_disabled\\",\\"mounts\\":\\"%.100s\\"},", mounts);
        strncat(fs_json, entry, fs_size - 1);
    }
}

''', ''))

EDITS.append(('''    const char *drift, const char *attest,
    const char *fs_events, const char *persist, const char *mimd,
    const char *threat_indicator)
{''', '''    const char *drift, const char *attest,
    const char *persist, const char *mimd,
    const char *threat_indicator)
{'''))

EDITS.append(('''    char d[4096], a[4096], f[4096], p[4096], m[4096], hw[512], vu[4096];
    strncpy(d, drift, sizeof(d) - 1); d[sizeof(d) - 1] = 0;
    strncpy(a, attest, sizeof(a) - 1); a[sizeof(a) - 1] = 0;
    strncpy(f, fs_events, sizeof(f) - 1); f[sizeof(f) - 1] = 0;
    strncpy(p, persist, sizeof(p) - 1); p[sizeof(p) - 1] = 0;
    strncpy(m, mimd, sizeof(m) - 1); m[sizeof(m) - 1] = 0;
    strncpy(hw, hard_json, sizeof(hw) - 1); hw[sizeof(hw) - 1] = 0;
    strncpy(vu, vuln_json, sizeof(vu) - 1); vu[sizeof(vu) - 1] = 0;
    strip_trailing_comma(d);
    strip_trailing_comma(a);
    strip_trailing_comma(f);
    strip_trailing_comma(p);
    strip_trailing_comma(m);
    strip_trailing_comma(vu);''', '''    char d[4096], a[4096], p[4096], m[4096], hw[512], vu[4096];
    strncpy(d, drift, sizeof(d) - 1); d[sizeof(d) - 1] = 0;
    strncpy(a, attest, sizeof(a) - 1); a[sizeof(a) - 1] = 0;
    strncpy(p, persist, sizeof(p) - 1); p[sizeof(p) - 1] = 0;
    strncpy(m, mimd, sizeof(m) - 1); m[sizeof(m) - 1] = 0;
    strncpy(hw, hard_json, sizeof(hw) - 1); hw[sizeof(hw) - 1] = 0;
    strncpy(vu, vuln_json, sizeof(vu) - 1); vu[sizeof(vu) - 1] = 0;
    strip_trailing_comma(d);
    strip_trailing_comma(a);
    strip_trailing_comma(p);
    strip_trailing_comma(m);
    strip_trailing_comma(vu);'''))

EDITS.append(('''        "  \\"hardware_attestation\\": [%s],\\n\\n"
        "  \\"filesystem_integrity\\": [%s],\\n\\n"
        "  \\"persistence_audit\\": [%s],\\n\\n"''', '''        "  \\"hardware_attestation\\": [%s],\\n\\n"
        "  \\"persistence_audit\\": [%s],\\n\\n"'''))

EDITS.append(('''        d, a, f, p, m, threat_indicator);''', '''        d, a, p, m, threat_indicator);'''))

EDITS.append(('''    /* ── Deep dive: filesystem integrity ──────────────────────────────── */
    char fs_json[4096] = "";
    check_filesystem_integrity(fs_json, sizeof(fs_json));

''', ''))

EDITS.append(('''    int confidence = correlate_threats(score, enforcing, rooted,
                                        drift_json, attest_json,
                                        fs_json, persist_json,
                                        threat_json, sizeof(threat_json));''', '''    int confidence = correlate_threats(score, enforcing, rooted,
                                        drift_json, attest_json,
                                        "", persist_json,
                                        threat_json, sizeof(threat_json));'''))

EDITS.append(('''    if (strlen(fs_json) > 0)
        printf("[GRANITOR]  FS: %s\\n", fs_json);
''', ''))

EDITS.append(('''               drift_json, attest_json, fs_json, persist_json, mimd_json, threat_json);''', '''               drift_json, attest_json, persist_json, mimd_json, threat_json);'''))

def main():
    with open(PATH, "r") as f:
        content = f.read()

    for i, (old, new) in enumerate(EDITS):
        count = content.count(old)
        if count != 1:
            print(f"ABORT at edit {i}: expected exactly 1 match, found {count}", file=sys.stderr)
            print(f"--- OLD snippet ---\n{old[:300]}", file=sys.stderr)
            sys.exit(1)
        content = content.replace(old, new)

    with open(PATH, "w") as f:
        f.write(content)

    print(f"OK: applied {len(EDITS)} edits to {PATH}")

if __name__ == "__main__":
    main()
