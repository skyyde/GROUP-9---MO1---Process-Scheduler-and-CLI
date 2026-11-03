// Kyla - Phase 1: console commands and initialization.
#pragma once

#include "Config_Kyla.h"
#include "Process_Kyla.h"

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

// Later modules can register handlers for their commands.
class Console {
public:
    using CommandHandler = std::function<void(const Command&, const Config&, std::ostream&)>;

    explicit Console(std::string configPath = "config.txt");
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

private:
    std::string configPath_;
    std::optional<Config> config_;
    std::unique_ptr<ProcessRegistry> processes_;
    std::map<CommandType, CommandHandler> handlers_;
};

} // namespace csopesy
