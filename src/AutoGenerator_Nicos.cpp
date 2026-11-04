// Nicos - Phase 6: automatic process generation.
#include "AutoGenerator_Nicos.h"

#include <iomanip>
#include <sstream>
#include <utility>

namespace csopesy {

AutoGenerator::AutoGenerator(std::uint64_t batchProcessFreq, const ProcessRegistry& registry,
                             CreateFn createProcess)
    : frequency_(batchProcessFreq ? batchProcessFreq : 1), registry_(registry),
      createProcess_(std::move(createProcess)) {}

// Enables generation and restarts the cycle count.
void AutoGenerator::start() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (enabled_) return;
    enabled_ = true;
    ticksSinceLast_ = 0;
}

// Disables generation.
void AutoGenerator::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    enabled_ = false;
}

bool AutoGenerator::enabled() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return enabled_;
}

// Returns the next unused name: p01, p02, ...
std::string AutoGenerator::nextFreeName() {
    for (;;) {
        std::ostringstream name;
        name << 'p' << std::setw(2) << std::setfill('0') << nextNumber_++;
        if (!registry_.findByName(name.str())) return name.str();
    }
}

// Creates one process every frequency_ ticks; never throws so the tick loop keeps running.
void AutoGenerator::onTick(std::uint64_t) {
    try {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!enabled_ || ++ticksSinceLast_ < frequency_) return;
        ticksSinceLast_ = 0;
        for (int attempt = 0; attempt < 3; ++attempt) {
            try {
                createProcess_(nextFreeName());
                return;
            } catch (const ProcessError&) { // Name was taken meanwhile; try the next one.
            }
        }
    } catch (...) {
    }
}

} // namespace csopesy
