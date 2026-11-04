/*
 * CSOPESY Major Output 1
 * Assigned Member: Enrique
 * Phase: 4 - CPU Scheduler
 * File responsibility: Future public interfaces for CPU Scheduler.
 *
 * Features to implement:
 * - Background CPU tick loop that continues while console input waits
 * - Configurable logical CPU cores and shared READY queue
 * - FCFS and Round Robin scheduling; quantum expiration and preemption
 * - Dispatch processes; handle sleeping and resumption
 * - Execution delays, CPU core occupancy tracking, thread-safe scheduling
 *
 * TODO: NOT IMPLEMENTED YET
 * Reserved for Phase 4; excluded from the Phase 1 build.
 */
//Phase 4: CPU scheduler
#pragma once

#include "AutoGenerator_Nicos.h"
#include "Console_Kyla.h"
#include "Instruction_Enrique.h"
#include "Process_Kyla.h"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace csopesy {

inline constexpr std::chrono::milliseconds DefaultTickInterval{10};

struct CpuCore {
    std::uint64_t id = 0;
    std::optional<ProcessId> process;
    bool occupied() const noexcept { return process.has_value(); }
};

class CpuScheduler {
public:
    CpuScheduler(const Config& config, ProcessRegistry& registry);
    ~CpuScheduler();
    CpuScheduler(const CpuScheduler&) = delete;
    CpuScheduler& operator=(const CpuScheduler&) = delete;

    void start(std::chrono::milliseconds tickInterval = DefaultTickInterval);
    void stop() noexcept;
    bool running() const noexcept;

    void admit(ProcessId pid);

    void step();
    std::uint64_t runUntilIdle(std::uint64_t maxTicks);

    std::uint64_t tickCount() const;
    std::vector<CpuCore> cores() const;
    std::vector<ProcessId> readyQueue() const;
    std::vector<ProcessId> sleeping() const;
    bool idle() const;
    std::string lastError() const;

    void setTickObserver(std::function<void(std::uint64_t)> observer);

private:
    struct Task {
        std::string name;
        std::shared_ptr<const InstructionProgram> program;
        ExecutionState state;
        std::uint64_t quantumLeft = 0;
        std::uint64_t delayLeft = 0;
        std::uint64_t sleepLeft = 0;
    };

    const Config config_;
    ProcessRegistry& registry_;

    mutable std::mutex mutex_;
    std::vector<CpuCore> cores_;
    std::deque<ProcessId> ready_;
    std::set<ProcessId> queued_;
    std::vector<ProcessId> sleeping_;
    std::map<ProcessId, Task> tasks_;
    std::uint64_t tick_ = 0;
    std::function<void(std::uint64_t)> observer_;
    std::string lastError_;

    std::thread thread_;
    mutable std::mutex threadMutex_;
    std::condition_variable wakeThread_;
    bool stopRequested_ = false;
    bool threadRunning_ = false;

    void tickLocked();
    void enqueueLocked(ProcessId pid);
    void runCycleLocked(CpuCore& core);
    void finishLocked(ProcessId pid);
    void storeCountersLocked(ProcessId pid, const Task& task);
    void threadMain(std::chrono::milliseconds interval);
};

struct ProgramSourceOptions {
    GeneratorMode mode = GeneratorMode::Random;
    std::string programDirectory = "programs";
    std::optional<std::uint64_t> seed;
};

class SchedulerBackend : public ConsoleBackend {
public:
    explicit SchedulerBackend(ProgramSourceOptions options = {},
                              std::chrono::milliseconds tickInterval = DefaultTickInterval);
    ~SchedulerBackend() override;

    void initialize(const Config& config, ProcessRegistry& registry) override;
    void shutdown() noexcept override;
    RuntimeStatus status() const override;
    ProcessId createProcess(const std::string& name) override;
    void startGeneration() override;
    void stopGeneration() override;

    CpuScheduler* scheduler() noexcept { return scheduler_.get(); }
    std::shared_ptr<const InstructionProgram> programFor(const std::string& name);

private:
    ProgramSourceOptions options_;
    std::chrono::milliseconds tickInterval_;
    std::optional<Config> config_;
    ProcessRegistry* registry_ = nullptr;
    std::unique_ptr<AutoGenerator> generator_;
    std::unique_ptr<CpuScheduler> scheduler_;
    std::mutex createMutex_;
    std::mt19937_64 rng_;
};

}
