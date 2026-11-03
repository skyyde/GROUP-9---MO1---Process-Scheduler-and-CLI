#pragma once

#include <cstdint>
#include <istream>
#include <stdexcept>
#include <string>

namespace csopesy {

enum class SchedulerPolicy { Fcfs, RoundRobin };

// Phase 1, Kyla: settings are published by Console only after full validation.
struct Config {
    std::uint64_t numCpu;
    SchedulerPolicy scheduler;
    std::uint64_t quantumCycles;
    std::uint64_t batchProcessFreq;
    std::uint64_t minIns;
    std::uint64_t maxIns;
    std::uint64_t delaysPerExec;
};

inline constexpr std::uint64_t MaxConfigCycles = std::uint64_t{1} << 32;

class ConfigError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Both functions throw ConfigError on invalid or unreadable input; no defaults.
Config parseConfig(std::istream& input);
Config loadConfig(const std::string& path);

} // namespace csopesy
