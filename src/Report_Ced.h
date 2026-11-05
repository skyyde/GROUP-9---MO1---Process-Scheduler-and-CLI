#pragma once

#include "TerminalView_Kyla.h"

#include <string>

namespace csopesy::report {

// Build the utilization report from one detached registry snapshot.
std::string utilization(const UtilizationSnapshot& snapshot);

// Replace the requested report file with a complete, current snapshot. std::runtime_error if the file cannot be written or finalized.
void saveUtilizationReport(const UtilizationSnapshot& snapshot,
                           const std::string& path = "csopesy-log.txt");

} // namespace csopesy::report
