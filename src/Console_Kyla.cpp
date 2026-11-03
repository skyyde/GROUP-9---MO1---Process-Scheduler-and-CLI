// Kyla - Phase 1: command parsing, initialization and the input loop.
#include "Console_Kyla.h"

#include <array>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace csopesy {
namespace {
struct SimpleCommand {
    const char* name;
    CommandType type;
};
constexpr std::array<SimpleCommand, 5> SimpleCommands{{
    {"initialize", CommandType::Initialize}, {"exit", CommandType::Exit},
    {"scheduler-start", CommandType::SchedulerStart},
    {"scheduler-stop", CommandType::SchedulerStop}, {"report-util", CommandType::ReportUtil}
}};

bool isOperational(CommandType type) {
    switch (type) {
    case CommandType::ScreenCreate:
    case CommandType::ScreenResume:
    case CommandType::ScreenList:
    case CommandType::SchedulerStart:
    case CommandType::SchedulerStop:
    case CommandType::ReportUtil:
        return true;
    default:
        return false;
    }
}
} // namespace

Command parseCommand(const std::string& line) {
    std::istringstream input(line);
    std::vector<std::string> tokens;
    for (std::string token; input >> token;) {
        tokens.push_back(std::move(token));
    }
    if (tokens.empty()) {
        return {CommandType::Empty, {}, {}};
    }
    for (const auto& command : SimpleCommands) {
        if (tokens[0] == command.name) {
            if (tokens.size() != 1) {
                return {CommandType::Invalid, {}, "Usage: " + std::string(command.name) +
                                                   " (no arguments)."};
            }
            return {command.type, {}, {}};
        }
    }
    if (tokens[0] == "screen") {
        if (tokens.size() == 2 && tokens[1] == "-ls") {
            return {CommandType::ScreenList, {}, {}};
        }
        if (tokens.size() == 3 && (tokens[1] == "-s" || tokens[1] == "-r")) {
            return {tokens[1] == "-s" ? CommandType::ScreenCreate : CommandType::ScreenResume,
                    tokens[2], {}};
        }
        return {CommandType::Invalid, {},
                "Usage: screen -s <process_name> | screen -r <process_name> | screen -ls."};
    }
    return {CommandType::Invalid, {}, "Unknown command '" + tokens[0] + "'."};
}

Console::Console(std::string configPath) : configPath_(std::move(configPath)) {}

Console::~Console() { shutdown(); }

void Console::shutdown() noexcept {
    if (backendStarted_) {
        backend_->shutdown();
        backendStarted_ = false;
    }
}

void Console::setBackend(std::shared_ptr<ConsoleBackend> backend) {
    if (isInitialized()) throw std::logic_error("Install the backend before initialization.");
    backend_ = std::move(backend);
}

void Console::setClearScreen(std::function<void()> clearScreen) {
    clearScreen_ = std::move(clearScreen);
}

RuntimeStatus Console::runtimeStatus() const {
    return backend_ && backendStarted_ ? backend_->status() : RuntimeStatus{};
}

UtilizationSnapshot Console::utilizationSnapshot() const {
    return {processRegistry().configuration().numCpu, processRegistry().allSnapshots()};
}

void Console::renderStatus(std::ostream& output) const {
    const auto state = runtimeStatus();
    terminal::status(output, sessionActive_, inputReady_, isInitialized(),
                     processes_ != nullptr, state);
}

bool Console::attachProcess(ProcessId pid, std::ostream& output) {
    const auto record = processRegistry().findByPid(pid);
    if (!record || record->state == ProcessState::FINISHED) return false;
    attached_ = pid;
    if (clearScreen_) clearScreen_();
    terminal::process(output, *record);
    return true;
}

bool Console::isInitialized() const noexcept { return config_.has_value(); }

const Config* Console::configuration() const noexcept {
    return config_ ? &*config_ : nullptr;
}

ProcessRegistry& Console::processRegistry() {
    if (!processes_) throw std::logic_error("Initialize the emulator before accessing processes.");
    return *processes_;
}

const ProcessRegistry& Console::processRegistry() const {
    if (!processes_) throw std::logic_error("Initialize the emulator before accessing processes.");
    return *processes_;
}

void Console::registerHandler(CommandType type, CommandHandler handler) {
    if (!isOperational(type) || !handler) {
        throw std::invalid_argument("A handler requires an operational command and a callable.");
    }
    handlers_[type] = std::move(handler);
}

bool Console::executeLine(const std::string& line, std::ostream& output) {
    try {
        return attached_ ? executeAttached(line, output) : executeCommand(parseCommand(line), output);
    } catch (const std::exception& error) {
        terminal::notice(output, "Error: " + std::string(error.what()), "ERROR");
        return true;
    }
}

bool Console::executeAttached(const std::string& line, std::ostream& output) {
    std::istringstream input(line);
    std::string command, extra;
    if (!(input >> command)) return true;
    if (command != "exit" && command != "process-smi") {
        terminal::notice(output, "Error: Process screen accepts only process-smi or exit.", "ERROR");
    } else if (input >> extra) {
        terminal::notice(output, "Error: Usage: " + command + " (no arguments).", "ERROR");
    } else if (command == "exit") {
        attached_.reset();
        if (clearScreen_) clearScreen_();
        terminal::notice(output, "Returned to main console.");
        renderStatus(output);
        terminal::commands(output);
    } else {
        // Lookup by PID includes completed records until the user detaches.
        const auto record = processRegistry().findByPid(*attached_);
        if (record) terminal::process(output, *record);
        else {
            attached_.reset();
            terminal::notice(output, "Error: Attached process is no longer available.", "ERROR");
        }
    }
    return true;
}

bool Console::executeCommand(const Command& command, std::ostream& output) {
    switch (command.type) {
    case CommandType::Empty:
        return true;
    case CommandType::Invalid:
        terminal::notice(output, "Error: " + command.error, "ERROR");
        return true;
    case CommandType::Exit:
        shutdown();
        terminal::notice(output, "Exiting CSOPESY Emulator...");
        return false;
    case CommandType::Initialize:
        if (isInitialized()) {
            terminal::notice(output, "CSOPESY emulator is already initialized.");
            return true;
        }
        terminal::notice(output, "Reading configuration...");
        try {
            // Validate the config and create the registry before marking initialization complete.
            const Config validated = loadConfig(configPath_);
            // Only the console can create a registry with validated settings.
            auto processes = std::unique_ptr<ProcessRegistry>(new ProcessRegistry(validated));
            config_ = validated;
            processes_ = std::move(processes);
            if (backend_) {
                // Mark before starting so partial starts also get joined.
                backendStarted_ = true;
                backend_->initialize(*config_, *processes_);
            }
        } catch (const std::exception& error) {
            shutdown();
            processes_.reset();
            config_.reset();
            terminal::notice(output, "Error: " + std::string(error.what()), "ERROR");
            return true;
        }
        terminal::notice(output, "CSOPESY emulator initialized.", "SUCCESS");
        renderStatus(output);
        terminal::configuration(output, *config_);
        return true;
    default:
        if (!isInitialized()) {
            terminal::notice(output, "Error: Please initialize the emulator first.", "ERROR");
            return true;
        }
        if (command.type == CommandType::ScreenCreate || command.type == CommandType::ScreenResume) {
            if (!ProcessRegistry::validName(command.processName)) {
                terminal::notice(output, "Error: Process name must use 1-64 ASCII letters, digits, '_' or '-'.", "ERROR");
                return true;
            }
        }
        const auto handler = handlers_.find(command.type);
        if (handler != handlers_.end()) {
            handler->second(command, *config_, output);
            return true;
        }
        switch (command.type) {
        case CommandType::ScreenCreate: {
            if (processes_->findByName(command.processName)) {
                terminal::notice(output, "Error: Process name is already registered.", "ERROR");
            } else if (!backend_ || !backendStarted_ ||
                       runtimeStatus().scheduler == ServiceState::Unavailable) {
                terminal::notice(output, "Unavailable: process creation requires the instruction and CPU scheduler backends.", "UNAVAILABLE");
            } else {
                const auto pid = backend_->createProcess(command.processName);
                // A fast process may finish before attachment. Show its final
                // information for this initial create, but never allow resume.
                const auto record = processes_->findByPid(pid);
                if (!record || record->name != command.processName || !record->program.handle)
                    throw std::logic_error("Backend did not register the requested process with a program.");
                attached_ = pid;
                if (clearScreen_) clearScreen_();
                terminal::notice(output, "Created process " + record->name + ".", "SUCCESS");
                terminal::process(output, *record);
            }
            break;
        }
        case CommandType::ScreenResume: {
            const auto record = processes_->findActiveByName(command.processName);
            if (!record || !attachProcess(record->pid, output))
                terminal::notice(output, "Process " + command.processName + " not found.", "ERROR");
            break;
        }
        case CommandType::ScreenList:
            terminal::utilizationDashboard(output, utilizationSnapshot());
            terminal::runtime(output, runtimeStatus());
            break;
        case CommandType::ReportUtil: {
            const auto report = terminal::utilization(utilizationSnapshot());
            std::ofstream file("csopesy-log.txt", std::ios::out | std::ios::trunc);
            if (!file) throw std::runtime_error("Cannot write csopesy-log.txt.");
            file << report;
            file.close();
            if (!file) throw std::runtime_error("Unable to finish writing csopesy-log.txt.");
            terminal::notice(output, "Utilization report saved to csopesy-log.txt.", "SUCCESS");
            break;
        }
        case CommandType::SchedulerStart:
        case CommandType::SchedulerStop: {
            const auto state = runtimeStatus();
            const bool start = command.type == CommandType::SchedulerStart;
            if (state.generation == ServiceState::Unavailable) {
                terminal::notice(output, "Unavailable: automatic process generation backend is not implemented.", "UNAVAILABLE");
            } else if ((state.generation == ServiceState::Running) == start) {
                terminal::notice(output, start ? "Automatic generation is already enabled."
                                               : "Automatic generation is already stopped.");
            } else {
                if (start) backend_->startGeneration();
                else backend_->stopGeneration();
                terminal::notice(output, start ? "Automatic generation enabled."
                                               : "Automatic generation stopped.", "SUCCESS");
            }
            terminal::runtime(output, runtimeStatus());
            break;
        }
        default: break;
        }
        return true;
    }
}

void Console::run(std::istream& input, std::ostream& output) {
    sessionActive_ = true;
    inputReady_ = input.good();
    // Includes exception paths (e.g. an ostream configured to throw).
    struct SessionGuard {
        Console& console;
        ~SessionGuard() {
            console.shutdown();
            console.sessionActive_ = false;
            console.inputReady_ = false;
        }
    } guard{*this};
    terminal::banner(output);
    renderStatus(output);
    if (config_) terminal::configuration(output, *config_);
    terminal::commands(output);
    if (!isInitialized()) terminal::notice(output, "Type initialize to begin.");
    std::string line;
    while (true) {
        const auto record = attached_ ? processes_->findByPid(*attached_) : std::nullopt;
        terminal::prompt(output, record ? record->name : "root");
        bool read = false;
        try { read = static_cast<bool>(std::getline(input, line)); }
        catch (const std::ios_base::failure&) { /* Inspect EOF/bad below. */ }
        inputReady_ = read;
        // Keep the next box on its own line even when redirected input is not echoed.
        output << '\n';
        if (!read) {
            if (input.bad() || !input.eof()) {
                terminal::notice(output, "Error: Unable to read console input.", "ERROR");
            }
            break;
        }
        if (!executeLine(line, output)) {
            break;
        }
    }
}

} // namespace csopesy
