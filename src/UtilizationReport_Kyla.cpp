// Existing report serialization, kept separate so terminal styling cannot change the file.
#include "TerminalView_Kyla.h"
#include <ctime>
#include <iomanip>
#include <set>
#include <sstream>

namespace csopesy::terminal {
namespace {
int colorSlot() {
    static const int slot = std::ios_base::xalloc();
    return slot;
}
const char* style(std::ostream& out, const char* code) {
    return out.iword(colorSlot()) ? code : "";
}
void section(std::ostream& out, const std::string& title) {
    out << '\n' << style(out, "\033[1;36m") << "  " << title << "  "
        << std::string(title.size() < 66 ? 66 - title.size() : 0, '-')
        << style(out, "\033[0m") << '\n';
}

// Never allow PRINT data to emit terminal control sequences. Wrap UTF-8 at
// code-point boundaries (the application does not depend on a terminal library).
void wrapped(std::ostream& out, const std::string& text, std::string prefix = "   ") {
    out << prefix;
    const std::string indent(prefix.size(), ' ');
    std::size_t column = prefix.size();
    for (unsigned char c : text) {
        if (c == '\n') { out << '\n' << indent; column = prefix.size(); continue; }
        if ((c & 0xc0) != 0x80) {
            if (column >= 72) { out << '\n' << indent; column = prefix.size(); }
            ++column;
        }
        out << ((c < 32 || c == 127) ? ' ' : static_cast<char>(c));
    }
    out << '\n';
}
void field(std::ostream& out, const std::string& label, const std::string& value) {
    std::ostringstream prefix;
    prefix << "   " << std::left << std::setw(22) << label << "  ";
    wrapped(out, value, prefix.str());
}
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
std::string percent(std::uint64_t numerator, std::uint64_t denominator) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(2)
        << (denominator ? 100.0L * numerator / denominator : 0.0L) << '%';
    return out.str();
}
} // namespace

std::string utilization(const UtilizationSnapshot& snapshot) {
    std::ostringstream out;
    std::set<std::uint64_t> occupied;
    for (const auto& record : snapshot.processes) {
        if (record.state == ProcessState::RUNNING && record.assignedCore &&
            *record.assignedCore < snapshot.totalCores) occupied.insert(*record.assignedCore);
    }
    section(out, "CPU UTILIZATION REPORT");
    field(out, "Total Cores", std::to_string(snapshot.totalCores));
    field(out, "Used Cores", std::to_string(occupied.size()));
    field(out, "Available Cores", std::to_string(snapshot.totalCores - occupied.size()));
    field(out, "CPU Utilization", percent(occupied.size(), snapshot.totalCores));
    for (bool finished : {false, true}) {
        section(out, finished ? "FINISHED PROCESSES" : "UNFINISHED PROCESSES");
        out << "   " << std::left << std::setw(17) << "Name" << std::setw(10) << "State"
            << std::setw(6) << (finished ? "Last" : "Core") << "Executed/Total\n";
        std::size_t count = 0;
        for (const auto& record : snapshot.processes) {
            if ((record.state == ProcessState::FINISHED) != finished) continue;
            ++count;
            const auto name = record.name.size() > 16 ? record.name.substr(0, 13) + "..." : record.name;
            out << "   " << std::left << std::setw(17) << name << std::setw(10) << stateText(record.state)
                << std::setw(6) << coreText(finished ? record.lastCore : record.assignedCore)
                << record.progress.executedInstructions << '/' << record.program.totalInstructions << '\n';
            if (name != record.name) field(out, "Full Name", record.name);
            field(out, "PID / Created At", std::to_string(record.pid) + " / " + timeText(record.createdAt));
            if (record.finishedAt) field(out, "Finished At", timeText(*record.finishedAt));
        }
        if (count) wrapped(out, "Total: " + std::to_string(count));
        if (!count) wrapped(out, finished ? "No finished processes." : "No unfinished processes.");
    }
    out << '\n';
    return out.str();
}
} // namespace csopesy::terminal
