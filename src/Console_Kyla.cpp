// Kyla - Phase 1: command parsing, initialization and the input loop.
#include "Console_Kyla.h"

#include <array>
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
    const auto command = parseCommand(line);
    switch (command.type) {
    case CommandType::Empty:
        return true;
    case CommandType::Invalid:
        output << "Error: " << command.error << '\n';
        return true;
    case CommandType::Exit:
        output << "Exiting CSOPESY Emulator...\n";
        return false;
    case CommandType::Initialize:
        if (isInitialized()) {
            output << "CSOPESY emulator is already initialized.\n";
            return true;
        }
        output << "Reading configuration...\n";
        try {
            // Validate the config and create the registry before marking initialization complete.
            const Config validated = loadConfig(configPath_);
            // Only the console can create a registry with validated settings.
            auto processes = std::unique_ptr<ProcessRegistry>(new ProcessRegistry(validated));
            config_ = validated;
            processes_ = std::move(processes);
        } catch (const ConfigError& error) {
            output << "Error: " << error.what() << '\n';
            return true;
        }
        output << "Configuration loaded successfully.\nCSOPESY emulator initialized.\n";
        return true;
    default:
        if (!isInitialized()) {
            output << "Error: Please initialize the emulator first.\n";
            return true;
        }
        const auto handler = handlers_.find(command.type);
        if (handler != handlers_.end()) {
            handler->second(command, *config_, output);
        } else {
            output << "Command recognized.\n"
                      "This feature will be implemented in a later phase.\n";
        }
        return true;
    }
}

void Console::run(std::istream& input, std::ostream& output) {
    output << "==========================================\n"
              "              C S O P E S Y\n"
              "==========================================\n"
              "       Welcome to CSOPESY Emulator!\n\n"
              "Developers:\nKyla | Enrique | Nicos | Ced\n\n"
              "==========================================\n";
    std::string line;
    while (true) {
        output << "root:\\> " << std::flush;
        if (!std::getline(input, line)) {
            if (input.bad() || !input.eof()) {
                output << "\nError: Unable to read console input.\n";
            } else {
                output << '\n';
            }
            break;
        }
        if (!executeLine(line, output)) {
            break;
        }
    }
}

} // namespace csopesy
