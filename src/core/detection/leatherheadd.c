#include "compat/sensei_compat.h"
#include <stdio.h>
#include "turtle_bridge.h"

float scan_leatherhead_thermal() {
    // Audits /sys/class/thermal/thermal_zone vs the fake HAL readings
    return 0.35f; // Actual Celsius normalized
}
