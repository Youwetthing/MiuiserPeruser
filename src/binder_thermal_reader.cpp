#include "binder_thermal_reader.h"
#include <cstdio>
#include <regex>
#include <iostream>
#include <unistd.h>

static bool command_exists(const char* cmd) {
    return access(cmd, X_OK) == 0;
}

static FILE* open_dumpsys_pipe() {
    // 1. Termux-ADB path
    if (command_exists("/data/data/com.termux/files/usr/bin/adb")) {
        return popen("/data/data/com.termux/files/usr/bin/adb shell dumpsys thermalservice", "r");
    }

    // 2. Standard adb in PATH
    if (system("which adb > /dev/null 2>&1") == 0) {
        return popen("adb shell dumpsys thermalservice", "r");
    }

    // 3. Direct dumpsys (LADB / ADB shell)
    if (command_exists("/system/bin/dumpsys")) {
        return popen("/system/bin/dumpsys thermalservice", "r");
    }

    // 4. Fallback: try plain dumpsys
    return popen("dumpsys thermalservice", "r");
}

std::vector<ThermalReading> BinderThermalReader::readTemperatures() {
    std::vector<ThermalReading> readings;

    FILE* pipe = open_dumpsys_pipe();
    if (!pipe) return readings;

    char line[512];
    std::regex tempRegex(
        R"(Temperature\{mValue=([0-9.]+), mType=([0-9]+), mName=([A-Za-z_]+), mStatus=([0-9]+)\})"
    );

    while (fgets(line, sizeof(line), pipe)) {
        std::cmatch match;
        if (std::regex_search(line, match, tempRegex)) {
            ThermalReading r;
            r.value = std::stof(match[1]);
            r.type = std::stoi(match[2]);
            r.name = match[3];
            r.status = std::stoi(match[4]);
            readings.push_back(r);
        }
    }

    pclose(pipe);
    return readings;
}
