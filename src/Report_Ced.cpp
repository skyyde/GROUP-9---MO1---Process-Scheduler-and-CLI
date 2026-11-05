#include "Report_Ced.h"

#include <ctime>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>
#include <stdexcept>

namespace csopesy::report {
namespace {

std::string timeText(ProcessTime point) {
    const auto raw = std::chrono::system_clock::to_time_t(point);
    std::tm local{};
#ifdef _WIN32
    if (localtime_s(&local, &raw) != 0) return "unavailable";
#else
    if (!localtime_r(&raw, &local)) return "unavailable";
#endif
    std::ostringstream out;
    out << std::put_time(&local, "%Y-%m-%d %H:%M:%S");
    return out.str();
}

const char* stateText(ProcessState state) {
    switch (state) {
    case ProcessState::READY: return "READY";
    case ProcessState::RUNNING: return "RUNNING";
    case ProcessState::SLEEPING: return "SLEEPING";
    case ProcessState::FINISHED: return "FINISHED";
    }
    return "UNKNOWN";
}

std::string coreText(std::optional<std::uint64_t> core) {
    return core ? std::to_string(*core) : "-";
}

std::string percent(std::uint64_t used, std::uint64_t total) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(2)
        << (total ? 100.0L * used / total : 0.0L) << '%';
    return out.str();
}

void field(std::ostream& out, const std::string& label, const std::string& value) {
    out << "   " << std::left << std::setw(22) << label << "  " << value << '\n';
}

} // namespace

std::string utilization(const UtilizationSnapshot& snapshot) {
    std::ostringstream out;
    std::set<std::uint64_t> occupied;
    for (const auto& process : snapshot.processes) {
        if (process.state == ProcessState::RUNNING && process.assignedCore &&
            *process.assignedCore < snapshot.totalCores) {
            occupied.insert(*process.assignedCore);
        }
    }

    out << "CSOPESY CPU UTILIZATION REPORT\n";
    out << "==============================\n";
    field(out, "Total Cores", std::to_string(snapshot.totalCores));
    field(out, "Used Cores", std::to_string(occupied.size()));
    field(out, "Available Cores", std::to_string(snapshot.totalCores - occupied.size()));
    field(out, "CPU Utilization", percent(occupied.size(), snapshot.totalCores));

    for (const bool finished : {false, true}) {
        out << '\n' << (finished ? "FINISHED PROCESSES" : "UNFINISHED PROCESSES") << '\n';
        out << "-------------------\n";
        out << "Name | State | " << (finished ? "Last CPU" : "Assigned CPU")
            << " | Executed / Total Instructions\n";
        std::size_t count = 0;
        for (const auto& process : snapshot.processes) {
            if ((process.state == ProcessState::FINISHED) != finished) continue;
            ++count;
            out << process.name << " | " << stateText(process.state) << " | "
                << coreText(finished ? process.lastCore : process.assignedCore) << " | "
                << process.progress.executedInstructions << " / "
                << process.program.totalInstructions << '\n';
            field(out, "PID", std::to_string(process.pid));
            field(out, "Created At", timeText(process.createdAt));
            if (process.finishedAt) field(out, "Finished At", timeText(*process.finishedAt));
        }
        out << (count ? "Total: " + std::to_string(count) :
                        (finished ? "No finished processes." : "No unfinished processes.")) << '\n';
    }
    return out.str();
}

void saveUtilizationReport(const UtilizationSnapshot& snapshot, const std::string& path) {
    if (path.empty()) throw std::invalid_argument("Report path cannot be empty.");
    const auto contents = utilization(snapshot);
    std::ofstream file(path, std::ios::out | std::ios::trunc | std::ios::binary);
    if (!file) throw std::runtime_error("Cannot write " + path + ".");
    file.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    file.close();
    if (!file) throw std::runtime_error("Unable to finish writing " + path + ".");
}

} // namespace csopesy::report
