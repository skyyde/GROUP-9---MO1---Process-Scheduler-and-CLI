#include "TerminalView_Kyla.h"

#include <algorithm>
#include <array>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <set>
#include <sstream>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <unistd.h>
#endif

namespace csopesy::terminal {
namespace {
int colorSlot() {
    static const int slot = std::ios_base::xalloc();
    return slot;
}
int widthSlot() {
    static const int slot = std::ios_base::xalloc();
    return slot;
}
const char* style(std::ostream& out, const char* code) {
    return out.iword(colorSlot()) ? code : "";
}
std::size_t columns(std::ostream& out) {
    if (out.iword(widthSlot()) > 0) return static_cast<std::size_t>(out.iword(widthSlot()));
    if (&out == &std::cout || &out == &std::cerr) {
#ifdef _WIN32
        CONSOLE_SCREEN_BUFFER_INFO info{};
        if (GetConsoleScreenBufferInfo(GetStdHandle(&out == &std::cout ?
                STD_OUTPUT_HANDLE : STD_ERROR_HANDLE), &info)) {
            return static_cast<std::size_t>(std::max(8, std::min(88,
                static_cast<int>(info.srWindow.Right - info.srWindow.Left))));
        }
#else
        winsize size{};
        if (ioctl(&out == &std::cout ? STDOUT_FILENO : STDERR_FILENO, TIOCGWINSZ, &size) == 0 && size.ws_col)
            return std::max<std::size_t>(8, std::min<std::size_t>(88, size.ws_col - 1));
#endif
    }
    return 88;
}

struct Glyph { std::string text; std::size_t width; };
// Decode before measuring or wrapping: the logo's three-byte glyphs each occupy
// one cell. Common combining marks and wide CJK/emoji logs are also supported.
// Invalid UTF-8 and terminal controls are replaced, never executed by the terminal.
std::vector<Glyph> glyphs(const std::string& text) {
    std::vector<Glyph> result;
    for (std::size_t i = 0; i < text.size();) {
        const auto first = static_cast<unsigned char>(text[i]);
        std::size_t length = first < 0x80 ? 1 : first >= 0xc2 && first <= 0xdf ? 2 :
                             first >= 0xe0 && first <= 0xef ? 3 : first >= 0xf0 && first <= 0xf4 ? 4 : 0;
        std::uint32_t cp = length == 1 ? first : first & (0x7f >> length);
        bool valid = length && i + length <= text.size();
        for (std::size_t j = 1; valid && j < length; ++j) {
            const auto next = static_cast<unsigned char>(text[i + j]);
            valid = (next & 0xc0) == 0x80;
            cp = (cp << 6) | (next & 0x3f);
        }
        valid = valid && !(length == 2 && cp < 0x80) && !(length == 3 && cp < 0x800) &&
                !(length == 4 && cp < 0x10000) && cp <= 0x10ffff && !(cp >= 0xd800 && cp <= 0xdfff);
        if (!valid) { result.push_back({"?", 1}); ++i; continue; }
        const bool control = cp < 32 || (cp >= 0x7f && cp <= 0x9f) ||
                             (cp >= 0x202a && cp <= 0x202e) || (cp >= 0x2066 && cp <= 0x2069);
        const bool combining = (cp >= 0x300 && cp <= 0x36f) || (cp >= 0x1ab0 && cp <= 0x1aff) ||
                               (cp >= 0x1dc0 && cp <= 0x1dff) || (cp >= 0xfe00 && cp <= 0xfe0f) ||
                               (cp >= 0xfe20 && cp <= 0xfe2f) || cp == 0x200d;
        const bool wide = (cp >= 0x1100 && cp <= 0x115f) || (cp >= 0x2e80 && cp <= 0xa4cf) ||
                          (cp >= 0xac00 && cp <= 0xd7a3) || (cp >= 0xf900 && cp <= 0xfaff) ||
                          (cp >= 0xfe10 && cp <= 0xfe19) || (cp >= 0xfe30 && cp <= 0xfe6f) ||
                          (cp >= 0xff01 && cp <= 0xff60) || (cp >= 0xffe0 && cp <= 0xffe6) ||
                          (cp >= 0x1f300 && cp <= 0x1faff) || (cp >= 0x20000 && cp <= 0x3fffd);
        result.push_back({control ? " " : text.substr(i, length), control ? 1u : combining ? 0u : wide ? 2u : 1u});
        i += length;
    }
    return result;
}
std::size_t visibleWidth(const std::string& text) {
    std::size_t width = 0;
    for (const auto& glyph : glyphs(text)) width += glyph.width;
    return width;
}
std::vector<std::string> wrap(const std::string& text, std::size_t width) {
    std::vector<std::string> lines;
    std::istringstream input(text);
    for (std::string paragraph; std::getline(input, paragraph);) {
        std::string line;
        std::size_t used = 0;
        for (const auto& glyph : glyphs(paragraph)) {
            if (used + glyph.width > width) { lines.push_back(line); line.clear(); used = 0; }
            line += glyph.text;
            used += glyph.width;
        }
        lines.push_back(line);
    }
    if (lines.empty()) lines.emplace_back();
    return lines;
}

class Box {
public:
    explicit Box(std::ostream& out, std::size_t width = 0) : out_(out), width_(width ? width : columns(out)) {}
    std::size_t contentWidth() const { return width_ - 4; }
    void border(char fill = '-') {
        out_ << style(out_, "\033[36m") << '+' << std::string(width_ - 2, fill) << '+'
             << style(out_, "\033[0m") << '\n';
    }
    void row(const std::string& text, bool center = false) {
        for (const auto& line : wrap(text, contentWidth())) {
            const auto spare = contentWidth() - visibleWidth(line);
            const auto left = center ? spare / 2 : 0;
            out_ << "| " << std::string(left, ' ') << line << std::string(spare - left, ' ') << " |\n";
        }
    }
    void section(const std::string& title) { border('='); row(title); border(); }
    void field(const std::string& label, const std::string& value) {
        if (contentWidth() < 44) { row(label + ':'); row("  " + value); return; }
        const auto prefix = label + std::string(label.size() < 22 ? 22 - label.size() : 0, ' ') + " : ";
        const auto lines = wrap(value, contentWidth() - prefix.size());
        for (std::size_t i = 0; i < lines.size(); ++i)
            row((i == 0 ? prefix : std::string(prefix.size(), ' ')) + lines[i]);
    }
    void command(const std::string& name, const std::string& description) {
        if (contentWidth() < 66) { row(name); row("  " + description); }
        else row(name + std::string(name.size() < 23 ? 23 - name.size() : 1, ' ') + description);
    }
    // Cell continuation rows preserve long names and 64-bit values in full.
    void tableRow(const std::vector<std::string>& cells, const std::vector<std::size_t>& widths) {
        std::vector<std::vector<std::string>> lines;
        std::size_t height = 1;
        for (std::size_t i = 0; i < cells.size(); ++i) {
            lines.push_back(wrap(cells[i], widths[i]));
            height = std::max(height, lines.back().size());
        }
        for (std::size_t line = 0; line < height; ++line) {
            std::string text;
            for (std::size_t i = 0; i < cells.size(); ++i) {
                if (i) text += " | ";
                const auto cell = line < lines[i].size() ? lines[i][line] : "";
                text += cell + std::string(widths[i] - visibleWidth(cell), ' ');
            }
            row(text);
        }
    }
private:
    std::ostream& out_;
    std::size_t width_;
};
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
std::string coreText(std::optional<std::uint64_t> core) { return core ? std::to_string(*core) : "-"; }
std::string progress(std::uint64_t numerator, std::uint64_t denominator) {
    const auto ratio = denominator ? static_cast<long double>(numerator) / denominator : 0.0L;
    const auto filled = static_cast<std::size_t>(std::clamp(ratio, 0.0L, 1.0L) * 20);
    std::ostringstream out;
    out << '[' << std::string(filled, '#') << std::string(20 - filled, '-') << "] "
        << std::fixed << std::setprecision(2) << ratio * 100 << '%';
    return out.str();
}
const char* serviceText(ServiceState state) {
    switch (state) {
    case ServiceState::Running: return "Running";
    case ServiceState::Stopped: return "Stopped";
    case ServiceState::Unavailable: return "Unavailable";
    }
    return "Unknown";
}
void runtimeFields(Box& box, RuntimeStatus value) {
    box.field("CPU scheduler", serviceText(value.scheduler));
    box.field("Process generation", serviceText(value.generation));
}
} // namespace

void enableColor(std::ostream& out, bool enabled) { out.iword(colorSlot()) = enabled; }
void setWidth(std::ostream& out, unsigned width) {
    out.iword(widthSlot()) = width ? static_cast<long>(std::clamp(width, 8u, 88u)) : 0;
}
void prompt(std::ostream& out, const std::string& name) {
    // Process names are already restricted to ASCII by the registry.
    out << '\n' << style(out, "\033[1;32m") << name << ":\\> " << style(out, "\033[0m") << std::flush;
}
void banner(std::ostream& out) {
    constexpr std::array<const char*, 6> logo{{
        u8"    ██████╗███████╗ ██████╗ ██████╗ ███████╗███████╗██╗   ██╗",
        u8"   ██╔════╝██╔════╝██╔═══██╗██╔══██╗██╔════╝██╔════╝╚██╗ ██╔╝",
        u8"   ██║     ███████╗██║   ██║██████╔╝█████╗  ███████╗ ╚████╔╝",
        u8"   ██║     ╚════██║██║   ██║██╔═══╝ ██╔══╝  ╚════██║  ╚██╔╝",
        u8"   ╚██████╗███████║╚██████╔╝██║     ███████╗███████║   ██║",
        u8"    ╚═════╝╚══════╝ ╚═════╝ ╚═╝     ╚══════╝╚══════╝   ╚═╝"
    }};
    std::size_t logoWidth = 0;
    for (const auto* line : logo) logoWidth = std::max(logoWidth, visibleWidth(line));
    // The exact logo cannot be reflowed; its box is the only minimum-width panel.
    Box box(out, std::max(columns(out), logoWidth + 4));
    out << '\n';
    box.border('=');
    box.row("");
    const auto padding = (box.contentWidth() - logoWidth) / 2;
    for (const auto* line : logo) box.row(std::string(padding, ' ') + line);
    box.row("");
    box.border();
    box.row("CSOPESY / Process Scheduler and CLI", true);
    box.border('=');
}
void status(std::ostream& out, bool /*session*/, bool /*input*/, bool configured,
            bool registry, RuntimeStatus value) {
    Box box(out);
    box.section("SYSTEM STATUS");
    box.field("Emulator", configured && registry ? "Ready" : "Awaiting initialization");
    if (configured) runtimeFields(box, value);
    box.border();
}
void runtime(std::ostream& out, RuntimeStatus value) {
    Box box(out);
    box.section("SCHEDULER STATUS");
    runtimeFields(box, value);
    box.border();
}
void configuration(std::ostream& out, const Config& config) {
    Box box(out);
    box.section("CONFIGURATION");
    box.field("CPU Cores", std::to_string(config.numCpu));
    box.field("Scheduling", config.scheduler == SchedulerPolicy::Fcfs ? "FCFS" : "Round Robin");
    box.field("Quantum Cycles", std::to_string(config.quantumCycles));
    box.field("Batch Frequency", std::to_string(config.batchProcessFreq));
    box.field("Minimum Ins.", std::to_string(config.minIns));
    box.field("Maximum Ins.", std::to_string(config.maxIns));
    box.field("Execution Delay", std::to_string(config.delaysPerExec));
    box.border();
}
void commands(std::ostream& out) {
    Box box(out);
    box.section("COMMAND CENTER");
    box.row("SESSION");
    box.command("initialize", "Load configuration and start emulator");
    box.command("exit", "Exit emulator");
    box.border();
    box.row("PROCESS SCREENS");
    box.command("screen -s <name>", "Create and open a process");
    box.command("screen -r <name>", "Open an unfinished process");
    box.command("screen -ls", "View CPU usage and processes");
    box.border();
    box.row("GENERATION & REPORTS");
    box.command("scheduler-start", "Start automatic process generation");
    box.command("scheduler-stop", "Stop automatic process generation");
    box.command("report-util", "Save report to csopesy-log.txt");
    box.border();
}
void notice(std::ostream& out, const std::string& message, const std::string& kind) {
    Box box(out);
    box.section(kind);
    box.row(message);
    box.border();
}
void notice(std::ostream& out, const std::string& message) { notice(out, message, "INFO"); }
void process(std::ostream& out, const Process& record) {
    Box box(out);
    box.section("PROCESS MONITOR");
    box.field("Process Name", record.name);
    box.field("Process ID", std::to_string(record.pid));
    box.field("State", stateText(record.state));
    box.field("Assigned CPU", coreText(record.assignedCore));
    box.field("Last CPU", coreText(record.lastCore));
    box.field("Created At", timeText(record.createdAt));
    if (record.finishedAt) box.field("Finished At", timeText(*record.finishedAt));
    box.section("EXECUTION STATUS");
    box.field("Current Line", std::to_string(record.progress.currentInstruction));
    box.field("Executed Instructions", std::to_string(record.progress.executedInstructions));
    box.field("Total Instructions", std::to_string(record.program.totalInstructions));
    box.field("Progress", record.state == ProcessState::FINISHED && record.program.totalInstructions == 0
              ? progress(1, 1) : progress(record.progress.executedInstructions, record.program.totalInstructions));
    if (!record.program.handle) box.field("Instruction Program", "Not attached");
    if (record.state == ProcessState::SLEEPING)
        box.field("Sleep Ticks Remaining", std::to_string(record.remainingSleepTicks));
    if (record.state == ProcessState::FINISHED) box.row("Finished!");
    box.section("PRINT LOGS");
    constexpr std::size_t VisibleLogs = 20;
    const auto begin = record.printLogs.size() > VisibleLogs ? record.printLogs.size() - VisibleLogs : 0;
    if (record.printLogs.empty()) box.row("No PRINT logs recorded.");
    if (begin) box.row("Showing latest 20 of " + std::to_string(record.printLogs.size()) +
                      " logs; full history retained in process record.");
    for (auto i = begin; i < record.printLogs.size(); ++i) {
        const auto& log = record.printLogs[i];
        box.row("[" + timeText(log.timestamp) + "] Core " + coreText(log.coreId) + ": " + log.message);
    }
    box.section("PROCESS COMMANDS");
    box.command("process-smi", "Refresh process information");
    box.command("exit", "Return to main console");
    box.border();
}
void utilizationDashboard(std::ostream& out, const UtilizationSnapshot& snapshot) {
    Box box(out);
    std::set<std::uint64_t> occupied;
    for (const auto& record : snapshot.processes) {
        if (record.state == ProcessState::RUNNING && record.assignedCore &&
            *record.assignedCore < snapshot.totalCores) occupied.insert(*record.assignedCore);
    }
    box.section("CPU UTILIZATION REPORT");
    box.field("Total Cores", std::to_string(snapshot.totalCores));
    box.field("Used Cores", std::to_string(occupied.size()));
    box.field("Available Cores", std::to_string(snapshot.totalCores - occupied.size()));
    box.field("CPU Utilization", progress(occupied.size(), snapshot.totalCores));
    for (bool finished : {false, true}) {
        box.section(finished ? "FINISHED PROCESSES" : "UNFINISHED PROCESSES");
        const bool table = box.contentWidth() >= 66;
        const std::vector<std::size_t> widths{table ? box.contentWidth() - 9 - 8 - 7 - 23 : 1, 8, 7, 23};
        if (table) {
            box.tableRow({"Name", "State", finished ? "Last" : "Core", "Executed / Total"}, widths);
            box.border();
        }
        std::size_t count = 0;
        for (const auto& record : snapshot.processes) {
            if ((record.state == ProcessState::FINISHED) != finished) continue;
            if (count++) box.border();
            const auto core = coreText(finished ? record.lastCore : record.assignedCore);
            const auto instructions = std::to_string(record.progress.executedInstructions) + " / " +
                                      std::to_string(record.program.totalInstructions);
            if (table) box.tableRow({record.name, stateText(record.state), core, instructions}, widths);
            else {
                box.field("Process Name", record.name);
                box.field("State", stateText(record.state));
                box.field(finished ? "Last CPU" : "Assigned CPU", core);
                box.field("Executed / Total", instructions);
            }
            box.field("PID / Created At", std::to_string(record.pid) + " / " + timeText(record.createdAt));
            if (!finished) box.field("Last CPU", coreText(record.lastCore));
            if (record.finishedAt) box.field("Finished At", timeText(*record.finishedAt));
        }
        if (count) { box.border(); box.row("Total: " + std::to_string(count)); }
        else box.row(finished ? "No finished processes." : "No unfinished processes.");
    }
    box.border();
}
} // namespace csopesy::terminal
