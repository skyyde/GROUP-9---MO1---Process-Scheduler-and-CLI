#include "Console_Kyla.h"
#include "Scheduler_Enrique.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using namespace csopesy;
namespace fs = std::filesystem;

namespace {
int failures = 0;

void check(bool condition, const std::string& label) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << label << '\n';
    }
}

struct Scenario {
    const char* scheduler;
    std::uint64_t cpus;
    std::uint64_t quantum;
    std::uint64_t frequency;
    std::uint64_t minInstructions;
    std::uint64_t maxInstructions;
    std::uint64_t delay;
};

void writeConfig(const fs::path& path, const Scenario& scenario) {
    std::ofstream out(path);
    out << "num-cpu " << scenario.cpus << '\n'
        << "scheduler \"" << scenario.scheduler << "\"\n"
        << "quantum-cycles " << scenario.quantum << '\n'
        << "batch-process-freq " << scenario.frequency << '\n'
        << "min-ins " << scenario.minInstructions << '\n'
        << "max-ins " << scenario.maxInstructions << '\n'
        << "delays-per-exec " << scenario.delay << '\n';
}

std::uint64_t totalProgress(const ProcessRegistry& registry) {
    std::uint64_t result = 0;
    for (const auto& process : registry.allSnapshots()) result += process.progress.executedInstructions;
    return result;
}

bool contains(const std::string& value, const std::string& fragment) {
    return value.find(fragment) != std::string::npos;
}

void runConfigurationMatrix(const fs::path& root) {
    const std::vector<Scenario> scenarios{
        {"fcfs", 1, 1, 1, 20, 30, 0},
        {"rr", 4, 5, 5, 100, 200, 2},
        {"rr", 128, 1, 1, 1, 2, 0}
    };
    std::size_t index = 0;
    for (const auto& scenario : scenarios) {
        const auto folder = root / ("matrix-" + std::to_string(++index));
        fs::create_directories(folder);
        const auto configPath = folder / "config.txt";
        writeConfig(configPath, scenario);

        ProgramSourceOptions programOptions;
        programOptions.programDirectory.clear();
        programOptions.seed = 100 + index;
        auto backend = std::make_shared<SchedulerBackend>(programOptions, std::chrono::milliseconds(2));
        Console console(configPath.string());
        console.setBackend(backend);
        std::ostringstream output;
        check(console.executeLine("initialize", output) && console.isInitialized(),
              "matrix config " + std::to_string(index) + " initializes without recompiling");
        const auto* config = console.configuration();
        check(config && config->numCpu == scenario.cpus &&
              config->quantumCycles == scenario.quantum &&
              config->batchProcessFreq == scenario.frequency &&
              config->minIns == scenario.minInstructions &&
              config->maxIns == scenario.maxInstructions &&
              config->delaysPerExec == scenario.delay,
              "matrix config " + std::to_string(index) + " values load from config.txt");
        check(backend->scheduler() && backend->scheduler()->cores().size() == scenario.cpus,
              "matrix config " + std::to_string(index) + " creates configured CPU cores");
        output.str(std::string{});
        output.clear();
        console.executeLine("screen -s matrix-process", output);
        check(contains(output.str(), "PROCESS MONITOR"),
              "matrix config " + std::to_string(index) + " accepts process creation");
        output.str(std::string{});
        output.clear();
        console.executeLine("screen -ls", output);
        check(contains(output.str(), "CPU UTILIZATION REPORT"),
              "matrix config " + std::to_string(index) + " reports utilization");
    }
}

void runConsoleAndGenerationIntegration(const fs::path& root) {
    const Scenario scenario{"rr", 4, 5, 1, 100, 200, 2};
    const auto folder = root / "integration";
    fs::create_directories(folder);
    const auto configPath = folder / "config.txt";
    writeConfig(configPath, scenario);

    ProgramSourceOptions programOptions;
    programOptions.programDirectory.clear();
    programOptions.seed = 42;
    auto backend = std::make_shared<SchedulerBackend>(programOptions, std::chrono::milliseconds(2));
    Console console(configPath.string());
    console.setBackend(backend);
    std::ostringstream output;
    check(console.executeLine("initialize", output) && console.isInitialized(),
          "integration initializes from config.txt");

    output.str(std::string{});
    output.clear();
    console.executeLine("screen -s manual", output);
    check(contains(output.str(), "Created process manual"), "screen -s creates and opens a process");
    check(console.attachedProcess().has_value(), "new process becomes attached");
    output.str(std::string{});
    output.clear();
    console.executeLine("process-smi", output);
    check(contains(output.str(), "PROCESS MONITOR"), "process-smi refreshes the process screen");
    output.str(std::string{});
    output.clear();
    console.executeLine("exit", output);
    check(!console.attachedProcess(), "process-screen exit returns to the main menu");
    output.str(std::string{});
    output.clear();
    console.executeLine("screen -r manual", output);
    check(console.attachedProcess().has_value(), "screen -r reattaches without creating a new process");
    output.str(std::string{});
    output.clear();
    console.executeLine("exit", output);

    const auto initialCount = console.processRegistry().allSnapshots().size();
    output.str(std::string{});
    output.clear();
    console.executeLine("scheduler-start", output);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (console.processRegistry().allSnapshots().size() < initialCount + 2 &&
           std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    check(console.processRegistry().allSnapshots().size() >= initialCount + 2,
          "scheduler-start creates generated processes at the configured frequency");
    output.str(std::string{});
    output.clear();
    console.executeLine("scheduler-stop", output);
    const auto stoppedCount = console.processRegistry().allSnapshots().size();
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    check(console.processRegistry().allSnapshots().size() == stoppedCount,
          "scheduler-stop stops generation without removing processes");

    const auto progressBeforeReport = totalProgress(console.processRegistry());
    output.str(std::string{});
    output.clear();
    console.executeLine("screen -ls", output);
    check(contains(output.str(), "UNFINISHED PROCESSES"),
          "screen -ls lists processes during scheduling");
    output.str(std::string{});
    output.clear();
    console.executeLine("report-util", output);
    std::ifstream report("csopesy-log.txt");
    const std::string contents{std::istreambuf_iterator<char>(report),
                               std::istreambuf_iterator<char>()};
    check(contains(contents, "CPU UTILIZATION REPORT") && contains(contents, "manual"),
          "report-util writes a live process report");
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    check(totalProgress(console.processRegistry()) > progressBeforeReport,
          "CPU scheduling continues while reports are generated");
    check(console.runtimeStatus().scheduler == ServiceState::Running,
          "reporting leaves the CPU scheduler running");
}
}

int main() {
    const auto root = fs::temp_directory_path() /
        ("csopesy_phase8_ced_" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto previous = fs::current_path();
    std::error_code ignored;
    try {
        fs::create_directories(root);
        fs::current_path(root);
        runConfigurationMatrix(root);
        runConsoleAndGenerationIntegration(root);
    } catch (const std::exception& error) {
        ++failures;
        std::cerr << "FAIL: Phase 8 integration setup: " << error.what() << '\n';
    }
    fs::current_path(previous);
    fs::remove_all(root, ignored);
    if (failures) return 1;
    std::cout << "Phase 8 integration checks passed.\n";
    return 0;
}
