#include "Console_Kyla.h"
#include "Instruction_Enrique.h"
#include "Report_Ced.h"

#include <filesystem>
#include <chrono>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>

using namespace csopesy;

namespace {
int failures = 0;

void contains(const std::string& text, const std::string& needle, const char* check) {
    if (text.find(needle) == std::string::npos) {
        ++failures;
        std::cerr << "FAIL: " << check << " (missing: " << needle << ")\n";
    }
}

Process sample(ProcessId pid, std::string name, ProcessState state,
               std::optional<std::uint64_t> assigned,
               std::optional<std::uint64_t> last) {
    Process process{};
    process.pid = pid;
    process.name = std::move(name);
    process.createdAt = std::chrono::system_clock::now();
    process.state = state;
    process.assignedCore = assigned;
    process.lastCore = last;
    process.program.totalInstructions = 10;
    process.progress.executedInstructions = state == ProcessState::FINISHED ? 10 : 4;
    if (state == ProcessState::FINISHED) process.finishedAt = process.createdAt;
    return process;
}
}

int main() {
    UtilizationSnapshot snapshot{2, {
        sample(1, "active", ProcessState::RUNNING, 0, 0),
        sample(2, "ready", ProcessState::READY, std::nullopt, std::nullopt),
        sample(3, "sleeping", ProcessState::SLEEPING, std::nullopt, 1),
        sample(4, "finished", ProcessState::FINISHED, std::nullopt, 1)
    }};

    const auto formatted = report::utilization(snapshot);
    contains(formatted, "Used Cores", "occupied core count");
    contains(formatted, "Available Cores", "available core count");
    contains(formatted, "50.00%", "utilization percentage");
    contains(formatted, "UNFINISHED PROCESSES", "unfinished section");
    contains(formatted, "FINISHED PROCESSES", "finished section");
    contains(formatted, "active", "running process name");
    contains(formatted, "RUNNING", "running process state");
    contains(formatted, "4/10", "running process progress");
    contains(formatted, "ready", "ready process included");
    contains(formatted, "READY", "ready process state");
    contains(formatted, "sleeping", "sleeping process included");
    contains(formatted, "SLEEPING", "sleeping process state");
    contains(formatted, "finished", "finished process included");
    contains(formatted, "FINISHED", "finished process state");
    contains(formatted, "10/10", "finished process progress");
    contains(formatted, "Created At", "creation timestamp");

    auto idleSnapshot = snapshot;
    idleSnapshot.processes.front().state = ProcessState::READY;
    idleSnapshot.processes.front().assignedCore.reset();
    contains(report::utilization(idleSnapshot), "0.00%", "zero utilization");
    auto fullSnapshot = snapshot;
    fullSnapshot.processes[1].state = ProcessState::RUNNING;
    fullSnapshot.processes[1].assignedCore = 1;
    contains(report::utilization(fullSnapshot), "100.00%", "full utilization");

    const auto uniqueSuffix = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto path = std::filesystem::temp_directory_path() / ("csopesy_report_ced_test_" + uniqueSuffix + ".txt");
    try {
        report::saveUtilizationReport(snapshot, path.string());
        std::ifstream input(path, std::ios::binary);
        const std::string saved{std::istreambuf_iterator<char>(input),
                                std::istreambuf_iterator<char>()};
        if (saved != formatted) {
            ++failures;
            std::cerr << "FAIL: saved report matches the rendered report\n";
        }
    } catch (const std::exception& error) {
        ++failures;
        std::cerr << "FAIL: report file writing: " << error.what() << '\n';
    }
    std::error_code ignored;
    std::filesystem::remove(path, ignored);

    const auto tempDir = std::filesystem::temp_directory_path() / ("csopesy_report_ced_integration_" + uniqueSuffix);
    std::filesystem::create_directories(tempDir);
    const auto configPath = tempDir / "config.txt";
    {
        std::ofstream config(configPath);
        config << "num-cpu 2\nscheduler \"rr\"\nquantum-cycles 5\n"
                  "batch-process-freq 1\nmin-ins 1\nmax-ins 2\ndelays-per-exec 0\n";
    }
    const auto previousPath = std::filesystem::current_path();
    try {
        std::filesystem::current_path(tempDir);
        Console console(configPath.string());
        std::ostringstream output;
        console.executeLine("initialize", output);
        const auto program = parseAndCompile("PRINT() PRINT()");
        auto& registry = console.processRegistry();
        const auto running = registry.createProcess("active", program->processProgram(program));
        registry.dispatch(running, 0);
        registry.setProgress(running, InstructionProgress{1, 1, {}});
        const auto ready = registry.createProcess("ready", program->processProgram(program));
        const auto finished = registry.createProcess("finished", program->processProgram(program));
        registry.dispatch(finished, 1);
        registry.setProgress(finished, InstructionProgress{2, 2, {}});
        registry.complete(finished);

        output.str(std::string{});
        output.clear();
        console.executeLine("screen -ls", output);
        const auto listing = output.str();
        contains(listing, "CPU UTILIZATION REPORT", "screen -ls integration report");
        contains(listing, "active", "screen -ls running process");
        contains(listing, "ready", "screen -ls ready process");
        contains(listing, "finished", "screen -ls finished process");
        contains(listing, "FINISHED PROCESSES", "screen -ls finished section");

        output.str(std::string{});
        output.clear();
        console.executeLine("report-util", output);
        std::ifstream savedFile("csopesy-log.txt", std::ios::binary);
        const std::string saved{std::istreambuf_iterator<char>(savedFile),
                                std::istreambuf_iterator<char>()};
        contains(saved, "active", "report-util includes running process");
        contains(saved, "RUNNING", "report-util includes running state");
        contains(saved, "1/2", "report-util includes running progress");
        contains(saved, "ready", "report-util includes ready process");
        contains(saved, "0/2", "report-util includes ready progress");
        contains(saved, "finished", "report-util includes finished process");
        contains(saved, "2/2", "report-util includes finished progress");
        contains(output.str(), "saved to csopesy-log.txt", "report-util success message");
        (void)ready;
    } catch (const std::exception& error) {
        ++failures;
        std::cerr << "FAIL: console reporting integration: " << error.what() << '\n';
    }
    std::filesystem::current_path(previousPath);
    std::filesystem::remove_all(tempDir, ignored);

    if (failures) return 1;
    std::cout << "Ced reporting checks passed.\n";
    return 0;
}
