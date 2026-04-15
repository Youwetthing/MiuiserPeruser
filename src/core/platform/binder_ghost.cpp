#include <vector>
#include <string>
struct ThermalReading { std::string name; float value; int type; int status; };
namespace BinderThermalReader { std::vector<ThermalReading> readTemperatures() { return {}; } }
