#include <chrono>
#include <thread>
#include <csignal>
#include <atomic>
#include <cstdio>

#include "binder_thermal_reader.h"
#include "fugitoid_logger.h"

static std::atomic<bool> g_running{true};

static void handle_signal(int) {
    g_running = false;
}

int main() {
    // Basic setup
    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);

    fugitoid_log("THERMALD", "Leatherhead thermald starting up");

    while (g_running) {
        // Read thermal data via Binder / dumpsys
        auto temps = BinderThermalReader::readTemperatures();

        if (temps.empty()) {
            fugitoid_log("THERMAL", "No thermal readings available (dumpsys returned nothing)");
        } else {
            for (const auto& t : temps) {
                fugitoid_log("THERMAL",
                             "%s: %.1f°C (type=%d status=%d)",
                             t.name.c_str(), t.value, t.type, t.status);
            }
        }

        // TODO: later – send to Splinter / IPC, compute aggregates, etc.

        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    fugitoid_log("THERMALD", "Leatherhead thermald shutting down");
    return 0;
}
