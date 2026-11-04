// Nicos - Phase 6: automatic process generation.
#pragma once

#include "Process_Kyla.h"

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>

namespace csopesy {

// Creates a process every batch-process-freq CPU cycles while enabled.
class AutoGenerator {
public:
    // Creates and admits one process with the given name.
    using CreateFn = std::function<void(const std::string& name)>;

    AutoGenerator(std::uint64_t batchProcessFreq, const ProcessRegistry& registry, CreateFn createProcess);

    void start();                    // Enables generation; no effect if already enabled.
    void stop();                     // Disables generation; existing processes keep running.
    bool enabled() const;
    void onTick(std::uint64_t tick); // Called once per CPU cycle by the scheduler.

private:
    const std::uint64_t frequency_;
    const ProcessRegistry& registry_;
    CreateFn createProcess_;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::uint64_t ticksSinceLast_ = 0;
    std::uint64_t nextNumber_ = 1;

    std::string nextFreeName();
};

} // namespace csopesy
