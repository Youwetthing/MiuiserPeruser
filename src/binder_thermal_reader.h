#pragma once
#include <string>
#include <vector>

struct ThermalReading {
    std::string name;
    float value;
    int type;
    int status;
};

class BinderThermalReader {
public:
    static std::vector<ThermalReading> readTemperatures();
};
