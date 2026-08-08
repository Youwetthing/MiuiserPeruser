#!/data/data/com.termux/files/usr/bin/bash
ADB=/data/data/com.termux/files/home/.cargo/bin/adb_cli
STATE=/data/data/com.termux/files/home/MiuiserPeruser/pipes/state
mkdir -p "$STATE"

# No hardcoded device address/port -- `local` delegates device routing to
# the adb server (default 127.0.0.1:5037, adb_cli's own default), which
# already knows the connected device. Confirmed live: `adb_cli local shell
# "echo test"` works with zero device-specific arguments.

# ── rahzerd_all: key=value lines for rz_prebaked_get() ──────────────────
read -r -d '' REMOTE_ALL_CMD << 'REMOTE'
for k in net.dns1 net.dns2 persist.private_dns_mode persist.dns.mode.hostname \
         gsm.sim.state ro.fm.type persist.sys.powerkeeper \
         persist.sys.miui.turbosched persist.sys.perfshielder \
         miui.whetstone.power persist.sys.smartpower; do
    v=$(getprop "$k" 2>/dev/null)
    [ -n "$v" ] && echo "$k=$v"
done
for f in /proc/net/wireless \
         /sys/class/net/wlan0/statistics/tx_bytes \
         /sys/class/net/wlan0/statistics/rx_bytes \
         /sys/class/net/eth0/operstate \
         /sys/class/net/eth0/address \
         /sys/class/net/eth0/statistics/tx_bytes \
         /sys/class/net/eth0/statistics/rx_bytes \
         /sys/class/net/eth0/speed \
         /sys/class/android_usb/android0/state \
         /sys/class/bluetooth/hci0/type \
         /proc/net/route; do
    v=$(cat "$f" 2>/dev/null | tr '\n' ' ')
    [ -n "$v" ] && echo "$f=$v"
done
# Primary wifi connection state. Prefer the IsPrimary: 1 line when one
# exists (device connected); fall back to the first mWifiInfo line when
# it doesn't (wifi off/disconnected -- both radios report IsPrimary: 0
# in that state, confirmed live 2026-08-08). The C side already checks
# for "Supplicant state: COMPLETED" to determine connected vs not, so
# capturing a disconnected line here is correct and expected, not a bug.
w=$(dumpsys wifi 2>/dev/null | grep "mWifiInfo" | grep "IsPrimary: 1" | tr '\n' ' ')
if [ -z "$w" ]; then
    w=$(dumpsys wifi 2>/dev/null | grep "mWifiInfo" | head -1 | tr '\n' ' ')
fi
[ -n "$w" ] && echo "mwifiinfo=$w"

# AOSP-level default network state, ground truth from ConnectivityManager
# (same value apps get from getActiveNetwork()). Replaces the old
# net.connectivity.status prop check -- that prop does not exist on this
# device/HyperOS version (confirmed empty via getprop, 2026-08-08), so the
# old aosp_reports_connected leg in rahzerd.c was permanently stuck at 0,
# firing a constant maximum-confidence XIAOMI_DIVERGENCE false positive.
# "Active default network: -1" means no default network (disconnected);
# any other integer is a real network ID (connected). ~130ms round trip,
# well within rahzerd's poll budget.
adn=$(dumpsys connectivity 2>/dev/null | grep "Active default network" | tr '\n' ' ')
[ -n "$adn" ] && echo "activedefaultnetwork=$adn"

# HyperOS connectivity services. `service check` queries the service
# manager directly (found/not found) -- much cheaper and more reliable
# than the old ds("activity") -> dumpsys activity -- that dumpsys target
# lists running activity/process records, not registered services, so
# it never contained these names regardless of whether the services were
# actually running (confirmed live 2026-08-08: both report "found" via
# `service check`, both absent from `dumpsys activity services`).
amlconn=$(service check AmlConnectivityService 2>/dev/null)
[ -n "$amlconn" ] && echo "amlconnsvc=$amlconn"
amlwifi=$(service check AmlMiuiWifiService 2>/dev/null)
[ -n "$amlwifi" ] && echo "amlwifisvc=$amlwifi"
REMOTE

timeout 10 "$ADB" local shell "$REMOTE_ALL_CMD" > "$STATE/rahzerd_all" 2>/dev/null || {
    echo "[dump_rahzerd] rahzerd_all: adb_cli timed out or failed" >&2
}

# ── rahzerd_mobile: raw dumpsys telephony.registry, verbatim ────────────
# rz_probe_mobile() does substring matching directly against this raw
# text (getRilDataRadioTechnology=N, mOperatorAlphaLongRaw=, domain=PS
# transportType=WWAN registrationState=HOME) -- do not reformat.
timeout 10 "$ADB" local shell "dumpsys telephony.registry 2>/dev/null" \
    > "$STATE/rahzerd_mobile" 2>/dev/null || {
    echo "[dump_rahzerd] rahzerd_mobile: adb_cli timed out or failed" >&2
}
