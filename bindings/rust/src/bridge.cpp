#include "xlnpwmon/xlnpwmon++.hpp"
#include "xlnpwmon/src/lib.rs.h"

std::unique_ptr<xlnpwmon::PowerMonitor> new_power_monitor() {
    return std::make_unique<xlnpwmon::PowerMonitor>();
} 