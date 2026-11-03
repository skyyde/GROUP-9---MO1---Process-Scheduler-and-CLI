// Kyla - terminal presentation only; no instruction execution or scheduling.
#pragma once

#include "Process_Kyla.h"
#include <ostream>
#include <string>

namespace csopesy {

enum class ServiceState { Unavailable, Stopped, Running };
struct RuntimeStatus {
    ServiceState scheduler = ServiceState::Unavailable;
    ServiceState generation = ServiceState::Unavailable;
};

// Registry core assignments are the occupancy source of truth. Capture all
// records under one lock, then format the detached value outside that lock.
struct UtilizationSnapshot {
    std::uint64_t totalCores;
    std::vector<Process> processes;
};

namespace terminal {
// Styling is opt-in per stream; reports and redirected output stay plain text.
void enableColor(std::ostream& output, bool enabled);
// Zero uses the current terminal width (88 columns for redirected streams).
void setWidth(std::ostream& output, unsigned columns);
void prompt(std::ostream& output, const std::string& name);
void banner(std::ostream& output);
void status(std::ostream& output, bool sessionActive, bool inputReady,
            bool configured, bool registryReady, RuntimeStatus runtime);
void configuration(std::ostream& output, const Config& config);
void commands(std::ostream& output);
void notice(std::ostream& output, const std::string& message);
void notice(std::ostream& output, const std::string& message, const std::string& kind);
void process(std::ostream& output, const Process& record);
std::string utilization(const UtilizationSnapshot& snapshot);
// Screen presentation is independent of the unchanged on-disk report format.
void utilizationDashboard(std::ostream& output, const UtilizationSnapshot& snapshot);
void runtime(std::ostream& output, RuntimeStatus status);
} // namespace terminal
} // namespace csopesy
