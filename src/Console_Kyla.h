#pragma once

#include "Config_Kyla.h"

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

// Phase 1, Kyla. Future modules register handlers; none are installed by Phase 1.
class Console {
public:
    using CommandHandler = std::function<void(const Command&, const Config&, std::ostream&)>;

    explicit Console(std::string configPath = "config.txt");
    void run(std::istream& input, std::ostream& output);
    // Returns false only for a syntactically valid exit command.
    bool executeLine(const std::string& line, std::ostream& output);
    bool isInitialized() const noexcept;
    // Null before initialization; read-only settings remain stable afterwards.
    const Config* configuration() const noexcept;
    // Only operational commands can be registered. Initialization gates all handlers.
    void registerHandler(CommandType type, CommandHandler handler);

private:
    std::string configPath_;
    std::optional<Config> config_;
    std::map<CommandType, CommandHandler> handlers_;
};

} // namespace csopesy
