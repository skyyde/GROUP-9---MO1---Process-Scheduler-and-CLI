// Kyla - Phase 1: configuration settings and file loading.
#pragma once

#include <cstdint>
#include <istream>
#include <stdexcept>
#include <string>

namespace csopesy {

enum class SchedulerPolicy { Fcfs, RoundRobin };

// The console stores these settings after all values have been validated.
struct Config {
    std::uint64_t numCpu;
    SchedulerPolicy scheduler;
    std::uint64_t quantumCycles;
    std::uint64_t batchProcessFreq;
    std::uint64_t minIns;
    std::uint64_t maxIns;
    std::uint64_t delaysPerExec;
};

// The inclusive 2^32 limit needs 64-bit storage.
inline constexpr std::uint64_t MaxConfigCycles = std::uint64_t{1} << 32;

class ConfigError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Invalid settings or unreadable input throw ConfigError.
Config parseConfig(std::istream& input);
Config loadConfig(const std::string& path);

} // namespace csopesy
