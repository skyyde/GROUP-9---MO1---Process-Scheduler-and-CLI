// Kyla - Phase 2: process data and registry operations.
#pragma once

#include "Config_Kyla.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace csopesy {

class Console;
class InstructionProgram; // Enrique will define this in Phase 3.
using ProcessId = std::uint64_t;
using ProcessTime = std::chrono::system_clock::time_point;
enum class ProcessState { READY, RUNNING, SLEEPING, FINISHED };

class ProcessError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// The handle stays null until an instruction program is attached.
// Static count tracks program positions; total count includes loop repetitions.
// Phase 3 calculates the total using the same counting rule as executedInstructions.
struct ProcessProgram {
    std::shared_ptr<const InstructionProgram> handle;
    std::uint64_t staticInstructionCount = 0;
    std::uint64_t totalInstructions = 0;
};

// Saved loop context, ordered from the outermost loop to the innermost.
// Positions use [begin, end); next may equal end when the body is done.
struct LoopFrame {
    std::uint64_t begin;
    std::uint64_t end;
    std::uint64_t next;
    std::uint64_t remainingIterations;
};

struct InstructionProgress {
    std::uint64_t currentInstruction = 0; // Can move back when a loop repeats.
    std::uint64_t executedInstructions = 0; // Includes repetitions and never decreases.
    std::vector<LoopFrame> loops;
};

struct PrintLogEntry {
    ProcessTime timestamp;
    std::optional<std::uint64_t> coreId;
    std::string message;
};

// Lookups return a copy of this record, so callers cannot change registry data.
// Copies remain valid after the registry is destroyed.
struct Process {
    ProcessId pid;
    std::string name;
    ProcessTime createdAt;
    ProcessState state = ProcessState::READY;
    std::optional<std::uint64_t> assignedCore;
    std::optional<std::uint64_t> lastCore;
    std::optional<ProcessTime> finishedAt;
    ProcessProgram program;
    InstructionProgress progress;
    std::map<std::string, std::uint16_t> variables;
    std::vector<PrintLogEntry> printLogs;
    std::uint64_t remainingSleepTicks = 0;
    std::uint64_t remainingQuantum = 0;
    std::uint64_t remainingExecutionDelay = 0;
};

namespace detail {
// Zero marks an exhausted PID sequence. The registry keeps its own sequence.
ProcessId allocatePid(std::atomic<ProcessId>& next);
}

class ProcessRegistry {
public:
    ProcessRegistry(const ProcessRegistry&) = delete;
    ProcessRegistry& operator=(const ProcessRegistry&) = delete;

    // Names use 1-64 ASCII letters, digits, underscores or hyphens.
    // They are case-sensitive and stay reserved after a process finishes.
    static bool validName(const std::string& name) noexcept;
    Config configuration() const noexcept { return config_; }
    ProcessId createProcess(std::string name, ProcessProgram program = {});
    void attachProgram(ProcessId pid, std::shared_ptr<const InstructionProgram> program);

    std::optional<Process> findByPid(ProcessId pid) const; // Includes finished processes.
    std::optional<Process> findByName(const std::string& name) const;
    std::optional<Process> findActiveByName(const std::string& name) const;
    std::vector<Process> activeSnapshots() const;
    std::vector<Process> finishedSnapshots() const;
    // Returns READY records without removing them. Phase 4 manages the queue
    // and avoids duplicate entries; dispatch checks the current state again.
    std::vector<Process> readySnapshots() const;

    // Invalid IDs, states or values throw ProcessError.
    // State changes and core checks happen together under the registry lock.
    void dispatch(ProcessId pid, std::uint64_t coreId);
    void releaseToReady(ProcessId pid);
    void sleep(ProcessId pid, std::uint64_t ticks);
    void setSleepTicks(ProcessId pid, std::uint64_t remaining); // Stores the scheduler's remaining ticks.
    void wake(ProcessId pid); // Requires SLEEPING with zero ticks left.
    bool complete(ProcessId pid); // Requires final progress; returns false if already finished.

    void setProgress(ProcessId pid, InstructionProgress progress);
    void setExecutionCounters(ProcessId pid, std::uint64_t quantum, std::uint64_t delay);
    void setVariable(ProcessId pid, std::string name, std::uint64_t value);
    std::optional<std::uint16_t> variable(ProcessId pid, const std::string& name) const;
    void appendLog(ProcessId pid, PrintLogEntry entry);

private:
    friend class Console;
    // The console creates this after validating the config.
    explicit ProcessRegistry(const Config& validatedConfig);
    const Config config_;
    mutable std::mutex mutex_; // Protects both maps and all process data.
    std::map<ProcessId, Process> records_;
    std::map<std::string, ProcessId> names_;

    Process& require(ProcessId pid); // Internal helpers below require the lock.
    const Process& require(ProcessId pid) const;
    Process& requireActive(ProcessId pid);
    std::vector<Process> snapshots(std::optional<ProcessState> state) const;
};

} // namespace csopesy
