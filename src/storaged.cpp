#include <atomic>
#include <csignal>
#include <chrono>
#include <thread>
#include <cstdio>
#include <cstring>
#include <string>
#include <fstream>
#include <sstream>

#include "fugitoid_logger.h"

static std::atomic<bool> g_running{true};

static void handle_signal(int) {
    g_running = false;
}

struct IoSelfStats {
    unsigned long long rchar = 0;
    unsigned long long wchar = 0;
    unsigned long long syscr = 0;
    unsigned long long syscw = 0;
    unsigned long long read_bytes = 0;
    unsigned long long write_bytes = 0;
    unsigned long long cancelled_write_bytes = 0;
    bool valid = false;
};

struct VmStats {
    long nr_dirty = 0;
    long nr_writeback = 0;
    long nr_dirty_threshold = 0;
    long nr_dirty_background_threshold = 0;
    bool valid = false;
};

struct MemStats {
    long mem_total_kb = 0;
    long mem_free_kb = 0;
    long mem_available_kb = 0;
    long swap_total_kb = 0;
    long swap_free_kb = 0;
    long dirty_kb = 0;
    long writeback_kb = 0;
    bool valid = false;
};

static IoSelfStats read_self_io() {
    IoSelfStats s;
    std::ifstream in("/proc/self/io");
    if (!in.is_open()) return s;

    std::string key;
    unsigned long long value;
    while (in >> key >> value) {
        if (key == "rchar:") s.rchar = value;
        else if (key == "wchar:") s.wchar = value;
        else if (key == "syscr:") s.syscr = value;
        else if (key == "syscw:") s.syscw = value;
        else if (key == "read_bytes:") s.read_bytes = value;
        else if (key == "write_bytes:") s.write_bytes = value;
        else if (key == "cancelled_write_bytes:") s.cancelled_write_bytes = value;
    }

    s.valid = true;
    return s;
}

static VmStats read_vmstat() {
    VmStats v;
    std::ifstream in("/proc/vmstat");
    if (!in.is_open()) return v;

    std::string key;
    long value;
    while (in >> key >> value) {
        if (key == "nr_dirty") v.nr_dirty = value;
        else if (key == "nr_writeback") v.nr_writeback = value;
        else if (key == "nr_dirty_threshold") v.nr_dirty_threshold = value;
        else if (key == "nr_dirty_background_threshold") v.nr_dirty_background_threshold = value;
    }

    v.valid = true;
    return v;
}

static MemStats read_meminfo() {
    MemStats m;
    std::ifstream in("/proc/meminfo");
    if (!in.is_open()) return m;

    std::string key;
    long value;
    std::string unit;
    while (in >> key >> value >> unit) {
        if (key == "MemTotal:") m.mem_total_kb = value;
        else if (key == "MemFree:") m.mem_free_kb = value;
        else if (key == "MemAvailable:") m.mem_available_kb = value;
        else if (key == "SwapTotal:") m.swap_total_kb = value;
        else if (key == "SwapFree:") m.swap_free_kb = value;
        else if (key == "Dirty:") m.dirty_kb = value;
        else if (key == "Writeback:") m.writeback_kb = value;
    }

    m.valid = true;
    return m;
}

int main() {
    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);

    fugitoid_log("TigerClaw", "storaged (Tiger Claw v2.1, MIUI-safe, UID-adaptive) starting up");

    IoSelfStats prev_io = read_self_io();
    MemStats prev_mem = read_meminfo();
    VmStats prev_vm = read_vmstat(); // may be invalid — that's fine

    while (g_running) {
        std::this_thread::sleep_for(std::chrono::seconds(5));

        IoSelfStats cur_io = read_self_io();
        MemStats cur_mem = read_meminfo();
        VmStats cur_vm = read_vmstat();

        unsigned long long d_read_bytes  = cur_io.valid ? (cur_io.read_bytes  - prev_io.read_bytes) : 0;
        unsigned long long d_write_bytes = cur_io.valid ? (cur_io.write_bytes - prev_io.write_bytes) : 0;
        unsigned long long d_cw_bytes    = cur_io.valid ? (cur_io.cancelled_write_bytes - prev_io.cancelled_write_bytes) : 0;

        long avail_pct = 0;
        if (cur_mem.valid && cur_mem.mem_total_kb > 0) {
            avail_pct = (cur_mem.mem_available_kb * 100L) / cur_mem.mem_total_kb;
        }

        long dirty_pct = -1;
        if (cur_vm.valid && cur_vm.nr_dirty_threshold > 0) {
            dirty_pct = (cur_vm.nr_dirty * 100L) / cur_vm.nr_dirty_threshold;
        }

        if (cur_vm.valid) {
            fugitoid_log(
                "TigerClaw",
                "self_io: read=%llu write=%llu cancelled=%llu | "
                "vm: dirty=%ld writeback=%ld dirty%%=%ld | "
                "mem: avail=%ldkB(%ld%%) swap_free=%ldkB",
                d_read_bytes, d_write_bytes, d_cw_bytes,
                cur_vm.nr_dirty, cur_vm.nr_writeback, dirty_pct,
                cur_mem.mem_available_kb, avail_pct, cur_mem.swap_free_kb
            );
        } else {
            fugitoid_log(
                "TigerClaw",
                "self_io: read=%llu write=%llu cancelled=%llu | "
                "vm: unavailable | "
                "mem: avail=%ldkB(%ld%%) swap_free=%ldkB",
                d_read_bytes, d_write_bytes, d_cw_bytes,
                cur_mem.mem_available_kb, avail_pct, cur_mem.swap_free_kb
            );
        }

        prev_io = cur_io;
        prev_mem = cur_mem;
        prev_vm = cur_vm;
    }

    fugitoid_log("TigerClaw", "storaged (Tiger Claw v2.1) shutting down");
    return 0;
}
