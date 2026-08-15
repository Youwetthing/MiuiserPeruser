#!/usr/bin/env python3
import sys

PATH = "src/daemon/shredderd.c"

OLD = '''static void check_filesystem_integrity(char *fs_json, size_t fs_size) {
    fs_json[0] = 0;
    char buf[4096];

    char *system_dm = rish_read("cat /proc/mounts 2>/dev/null | grep ' /system ' | grep -c '^/dev/block/dm-'", buf, sizeof(buf));
    char *vendor_dm = rish_read("cat /proc/mounts 2>/dev/null | grep ' /vendor ' | grep -c '^/dev/block/dm-'", buf, sizeof(buf));
    char *system_rw = rish_read("cat /proc/mounts 2>/dev/null | grep ' /system ' | grep -v ' ro ' | wc -l", buf, sizeof(buf));

    int system_on_dm = (system_dm && strcmp(system_dm, "1") == 0);
    int vendor_on_dm = (vendor_dm && strcmp(vendor_dm, "1") == 0);
    int system_not_ro = (system_rw && strcmp(system_rw, "0") != 0);

    if (!system_on_dm || !vendor_on_dm || system_not_ro) {
        char entry[512];
        snprintf(entry, sizeof(entry),
            "{\\"type\\":\\"verity_anomaly\\",\\"system_on_dm\\":%s,\\"vendor_on_dm\\":%s,\\"system_not_ro\\":%s},",
            system_on_dm ? "true" : "false",
            vendor_on_dm ? "true" : "false",
            system_not_ro ? "true" : "false");
        strncat(fs_json, entry, fs_size - strlen(fs_json) - 1);
    }
}'''

NEW = '''static void check_filesystem_integrity(char *fs_json, size_t fs_size) {
    fs_json[0] = 0;

    char verity_buf[4096];
    char *verity = rish_read("dmesg 2>/dev/null | grep -i 'dm-verity.*corruption' | tail -3",
                              verity_buf, sizeof(verity_buf));
    if (verity && strlen(verity) > 0) {
        char *line = strtok(verity, "\\n");
        while (line) {
            char entry[512];
            snprintf(entry, sizeof(entry), "{\\"type\\":\\"verity_corruption\\",\\"msg\\":\\"%.200s\\"},", line);
            strncat(fs_json, entry, fs_size - strlen(fs_json) - 1);
            line = strtok(NULL, "\\n");
        }
    }

    char f2fs_buf[4096];
    char *f2fs = rish_read("dmesg 2>/dev/null | grep -iE 'f2fs.*error|f2fs.*corruption' | tail -3",
                            f2fs_buf, sizeof(f2fs_buf));
    if (f2fs && strlen(f2fs) > 0) {
        char *line = strtok(f2fs, "\\n");
        while (line) {
            char entry[512];
            snprintf(entry, sizeof(entry), "{\\"type\\":\\"f2fs_error\\",\\"msg\\":\\"%.200s\\"},", line);
            strncat(fs_json, entry, fs_size - strlen(fs_json) - 1);
            line = strtok(NULL, "\\n");
        }
    }

    char system_dm_buf[64], vendor_dm_buf[64], system_rw_buf[64];
    char *system_dm = rish_read("cat /proc/mounts 2>/dev/null | grep ' /system ' | grep -c '^/dev/block/dm-'", system_dm_buf, sizeof(system_dm_buf));
    char *vendor_dm = rish_read("cat /proc/mounts 2>/dev/null | grep ' /vendor ' | grep -c '^/dev/block/dm-'", vendor_dm_buf, sizeof(vendor_dm_buf));
    char *system_rw = rish_read("cat /proc/mounts 2>/dev/null | grep ' /system ' | grep -v ' ro ' | wc -l", system_rw_buf, sizeof(system_rw_buf));

    int system_on_dm = (system_dm && strcmp(system_dm, "1") == 0);
    int vendor_on_dm = (vendor_dm && strcmp(vendor_dm, "1") == 0);
    int system_not_ro = (system_rw && strcmp(system_rw, "0") != 0);

    if (!system_on_dm || !vendor_on_dm || system_not_ro) {
        char entry[512];
        snprintf(entry, sizeof(entry),
            "{\\"type\\":\\"verity_anomaly\\",\\"system_on_dm\\":%s,\\"vendor_on_dm\\":%s,\\"system_not_ro\\":%s},",
            system_on_dm ? "true" : "false",
            vendor_on_dm ? "true" : "false",
            system_not_ro ? "true" : "false");
        strncat(fs_json, entry, fs_size - strlen(fs_json) - 1);
    }

    char mounts_buf[4096];
    char *mounts = rish_read("cat /proc/mounts 2>/dev/null | grep -E 'system|vendor' | grep -v 'dm-verity' | tr '\\n' ' '",
                              mounts_buf, sizeof(mounts_buf));
    if (mounts && strlen(mounts) > 0) {
        char entry[256];
        snprintf(entry, sizeof(entry), "{\\"type\\":\\"verity_disabled\\",\\"mounts\\":\\"%.100s\\"},", mounts);
        strncat(fs_json, entry, fs_size - strlen(fs_json) - 1);
    }
}'''

def main():
    with open(PATH, "r") as f:
        content = f.read()
    count = content.count(OLD)
    if count != 1:
        print(f"ABORT: expected exactly 1 match, found {count}", file=sys.stderr)
        sys.exit(1)
    content = content.replace(OLD, NEW)
    with open(PATH, "w") as f:
        f.write(content)
    print(f"OK: patched {PATH}")

if __name__ == "__main__":
    main()
