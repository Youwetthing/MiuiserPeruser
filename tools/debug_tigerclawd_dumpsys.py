#!/usr/bin/env python3
PATH = "src/daemon/tigerclawd.c"
OLD = '''    char *raw = bexec("dumpsys device_policy 2>/dev/null");
    if (!raw) return -1;'''
NEW = '''    char *raw = bexec("dumpsys device_policy 2>/dev/null");
    if (!raw) return -1;
    { char dbgmsg[128]; snprintf(dbgmsg, sizeof(dbgmsg),
      "DEBUG raw dumpsys length=%zu, contains_admin_list=%d",
      strlen(raw), strstr(raw, "Enabled Device Admins") != NULL);
      tlog("INFO", dbgmsg); }'''
with open(PATH) as f: c = f.read()
assert c.count(OLD) == 1, f"found {c.count(OLD)}x"
c = c.replace(OLD, NEW)
with open(PATH, "w") as f: f.write(c)
print("OK")
