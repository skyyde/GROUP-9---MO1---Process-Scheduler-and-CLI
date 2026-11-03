#include "Config_Kyla.h"

#include <array>
#include <charconv>
#include <fstream>
#include <map>
#include <sstream>
#include <system_error>

namespace csopesy {
namespace {
constexpr std::array<const char*, 7> RequiredKeys{
    "num-cpu", "scheduler", "quantum-cycles", "batch-process-freq",
    "min-ins", "max-ins", "delays-per-exec"
};

std::uint64_t readNumber(const std::map<std::string, std::string>& values,
                         const std::string& key, std::uint64_t minimum,
                         std::uint64_t maximum) {
    const auto& token = values.at(key);
    std::uint64_t number = 0;
    // Digits only: reject signs, fractions, exponent notation and partial parses.
    if (token.empty() || token.find_first_not_of("0123456789") != std::string::npos) {
        throw ConfigError("Setting '" + key + "' requires an unsigned decimal integer.");
    }
    const auto result = std::from_chars(token.data(), token.data() + token.size(), number);
    if (result.ec != std::errc{} || result.ptr != token.data() + token.size() ||
        number < minimum || number > maximum) {
        throw ConfigError("Setting '" + key + "' must be in [" +
                          std::to_string(minimum) + ", " + std::to_string(maximum) + "].");
    }
    return number;
}
} // namespace

Config parseConfig(std::istream& input) {
    std::map<std::string, std::string> values;
    std::string line;
    std::size_t lineNumber = 0;
    try {
        while (std::getline(input, line)) {
            ++lineNumber;
            std::istringstream tokens(line);
            std::string key, value, extra;
            if (!(tokens >> key)) {
                continue;
            }
            bool known = false;
            for (const auto* required : RequiredKeys) {
                known = known || key == required;
            }
            if (!known) {
                throw ConfigError("Unknown setting '" + key + "' on line " +
                                  std::to_string(lineNumber) + ".");
            }
            if (!(tokens >> value) || (tokens >> extra)) {
                throw ConfigError("Setting '" + key + "' on line " +
                                  std::to_string(lineNumber) + " requires exactly one value.");
            }
            if (!values.emplace(key, value).second) {
                throw ConfigError("Duplicate setting '" + key + "'.");
            }
        }
    } catch (const std::ios_base::failure&) {
        throw ConfigError("Unable to read configuration input.");
    }
    if (input.bad() || (input.fail() && !input.eof())) {
        throw ConfigError("Unable to read configuration input.");
    }
    for (const auto* key : RequiredKeys) {
        if (values.count(key) == 0) {
            throw ConfigError("Missing required setting '" + std::string(key) + "'.");
        }
    }

    auto scheduler = values.at("scheduler");
    if (scheduler.size() >= 2 && scheduler.front() == '"' && scheduler.back() == '"') {
        scheduler = scheduler.substr(1, scheduler.size() - 2);
    }
    if (scheduler != "fcfs" && scheduler != "rr") {
        throw ConfigError("Setting 'scheduler' must be \"fcfs\" or \"rr\".");
    }

    Config config{
        readNumber(values, "num-cpu", 1, 128),
        scheduler == "fcfs" ? SchedulerPolicy::Fcfs : SchedulerPolicy::RoundRobin,
        readNumber(values, "quantum-cycles", 1, MaxConfigCycles),
        readNumber(values, "batch-process-freq", 1, MaxConfigCycles),
        readNumber(values, "min-ins", 1, MaxConfigCycles),
        readNumber(values, "max-ins", 1, MaxConfigCycles),
        readNumber(values, "delays-per-exec", 0, MaxConfigCycles)
    };
    if (config.minIns > config.maxIns) {
        throw ConfigError("Setting 'min-ins' must be less than or equal to 'max-ins'.");
    }
    return config;
}

Config loadConfig(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw ConfigError("Cannot open configuration file '" + path + "'.");
    }
    return parseConfig(file);
}

} // namespace csopesy
