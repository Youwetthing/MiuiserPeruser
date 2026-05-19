#!/bin/bash
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../env.sh"
echo "🛡️ [SYNDICATE HAUS] Applying Stealth Shizuku Mask..."

# Define the stealth detection function in a temp file
cat << 'INNER_EOF' > /tmp/stealth_shizuku.c
static int check_shizuku_stealth(void) {
    // 1. Check for the starter artifact (Silent)
    if (access("/data/local/tmp/shizuku_starter", F_OK) == 0) return 1;

    // 2. Check the Environment (Silent)
    if (getenv("SHIZUKU_TOKEN") != NULL) return 1;

    // 3. Scan the Unix Sockets in /proc (Silent - No Shell Spawned)
    FILE *fp = fopen("/proc/net/unix", "r");
    if (fp) {
        char line[512];
        while (fgets(line, sizeof(line), fp)) {
            if (strstr(line, "shizuku")) {
                fclose(fp);
                return 1;
            }
        }
        fclose(fp);
    }
    return 0;
}
INNER_EOF

# Inject it into capabilities_extra.c without destroying the file
# We'll just replace the old 'check_shizuku' with the stealth version
sed -i '/static int check_shizuku/,/}/d' ~/MiuiserPeruser/src/daemon/capabilities_extra.c
cat /tmp/stealth_shizuku.c >> ~/MiuiserPeruser/src/daemon/capabilities_extra.c
sed -i 's/check_shizuku_stealth/check_shizuku/g' ~/MiuiserPeruser/src/daemon/capabilities_extra.c

echo "🥋 Training the Ninja..."
gcc -c ~/MiuiserPeruser/src/daemon/capabilities_extra.c -I. -I./include -I./src/core/include -I./src/daemon -o ~/MiuiserPeruser/bin/cap_extra.o

echo "⚡ Re-linking the Haus..."
~/MiuiserPeruser/Build_All.sh

echo "✅ [COMPLETE] Stealth Mask is Active. No Repo-Boom today."
