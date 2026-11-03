// Kyla - Phase 1: console commands and initialization.
#pragma once

#include "Config_Kyla.h"
#include "Process_Kyla.h"
#include "TerminalView_Kyla.h"

#include <functional>
#include <istream>
#include <map>
#include <optional>
#include <ostream>
#include <string>

namespace csopesy {

enum class CommandType {
    Empty, Invalid, Initialize, Exit, ScreenCreate, ScreenResume,
    ScreenList, SchedulerStart, SchedulerStop, ReportUtil
};

struct Command {
    CommandType type;
    std::string processName;
    std::string error;
};

Command parseCommand(const std::string& line);

// Adapter contract for the future modules, not an execution engine.
// initialize starts available CPU workers; it must not enable generation.
// shutdown stops/joins all workers, is idempotent and also handles failed starts.
// status must return synchronized real service state, never inferred from config.
// createProcess supplies a real immutable program and admits the new registry
// record to the scheduler exactly once; validate prerequisites before mutation.
// All worker state/core changes must use ProcessRegistry (the reporting authority).
class ConsoleBackend {
public:
    virtual ~ConsoleBackend() = default;
    virtual void initialize(const Config&, ProcessRegistry&) = 0;
    virtual void shutdown() noexcept = 0;
    virtual RuntimeStatus status() const = 0;
    virtual ProcessId createProcess(const std::string& name) = 0;
    virtual void startGeneration() = 0;
    virtual void stopGeneration() = 0;
};

// Later modules can register handlers for their commands.
class Console {
public:
    using CommandHandler = std::function<void(const Command&, const Config&, std::ostream&)>;

    explicit Console(std::string configPath = "config.txt");
    ~Console();
    void run(std::istream& input, std::ostream& output);
    // Returns false when the user enters a valid exit command.
    bool executeLine(const std::string& line, std::ostream& output);
    bool isInitialized() const noexcept;
    // Returns nullptr until initialization succeeds.
    const Config* configuration() const noexcept;
    // Access the Phase 2 registry after initialization; otherwise throws logic_error.
    // Get this reference on the console thread before sharing it with workers.
    // Future workers must stop before the console is destroyed.
    ProcessRegistry& processRegistry();
    const ProcessRegistry& processRegistry() const;
    // Handlers run only after initialization. Built-in commands cannot be replaced.
    void registerHandler(CommandType type, CommandHandler handler);
    // Install on the console thread, before initialize. Existing handlers remain
    // supported; the adapter additionally supplies lifecycle and service status.
    void setBackend(std::shared_ptr<ConsoleBackend> backend);
    // Main installs native clearing only for interactive stdin AND stdout.
    void setClearScreen(std::function<void()> clearScreen);
    bool attachProcess(ProcessId pid, std::ostream& output);
    std::optional<ProcessId> attachedProcess() const noexcept { return attached_; }
    RuntimeStatus runtimeStatus() const;
    UtilizationSnapshot utilizationSnapshot() const;

private:
    std::string configPath_;
    std::optional<Config> config_;
    std::unique_ptr<ProcessRegistry> processes_;
    std::map<CommandType, CommandHandler> handlers_;
    std::shared_ptr<ConsoleBackend> backend_;
    bool backendStarted_ = false;
    std::optional<ProcessId> attached_;
    std::function<void()> clearScreen_;
    bool sessionActive_ = false;
    bool inputReady_ = false;

    bool executeCommand(const Command& command, std::ostream& output);
    bool executeAttached(const std::string& line, std::ostream& output);
    void renderStatus(std::ostream& output) const;
    void shutdown() noexcept;
};

} // namespace csopesy
