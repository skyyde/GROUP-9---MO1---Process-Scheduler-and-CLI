// Kyla - Phase 2: process creation, state changes and data storage.
#include "Process_Kyla.h"

#include <limits>
#include <utility>

namespace csopesy {
namespace {
bool atEnd(const Process& process) {
    return process.progress.executedInstructions == process.program.totalInstructions &&
           process.progress.currentInstruction == process.program.staticInstructionCount &&
           process.progress.loops.empty();
}

void requireState(const Process& process, ProcessState state) {
    if (process.state != state) throw ProcessError("Invalid process state for this operation.");
}
}

ProcessId detail::allocatePid(std::atomic<ProcessId>& next) {
    auto candidate = next.load(std::memory_order_relaxed);
    for (;;) {
        if (candidate == 0) throw ProcessError("Process ID space exhausted.");
        const auto following = candidate == std::numeric_limits<ProcessId>::max()
            ? ProcessId{0} : candidate + 1;
        if (next.compare_exchange_weak(candidate, following, std::memory_order_relaxed)) {
            return candidate;
        }
    }
}

ProcessRegistry::ProcessRegistry(const Config& validatedConfig) : config_(validatedConfig) {}

bool ProcessRegistry::validName(const std::string& name) noexcept {
    if (name.empty() || name.size() > 64) return false;
    for (const char c : name) {
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-')) return false;
    }
    return true;
}

ProcessId ProcessRegistry::createProcess(std::string name, ProcessProgram program) {
    if (!validName(name)) throw ProcessError("Process name must use 1-64 ASCII letters, digits, '_' or '-'.");
    if ((program.staticInstructionCount == 0) != (program.totalInstructions == 0)) {
        throw ProcessError("Empty programs require both instruction counts to be zero.");
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (names_.count(name)) throw ProcessError("Process name is already registered.");
    // Keep PIDs unique across registries for the whole application run.
    // A failed creation may leave a gap, but its PID is never reused.
    static std::atomic<ProcessId> nextPid{1};
    const auto pid = detail::allocatePid(nextPid);
    Process process{};
    process.pid = pid;
    process.name = std::move(name);
    process.createdAt = std::chrono::system_clock::now();
    process.program = std::move(program);
    auto inserted = records_.emplace(pid, std::move(process));
    try {
        names_.emplace(inserted.first->second.name, pid);
    } catch (...) {
        records_.erase(inserted.first); // Roll back if the name index could not be updated.
        throw;
    }
    return pid;
}

Process& ProcessRegistry::require(ProcessId pid) {
    const auto found = records_.find(pid);
    if (found == records_.end()) throw ProcessError("Unknown process ID.");
    return found->second;
}

const Process& ProcessRegistry::require(ProcessId pid) const {
    const auto found = records_.find(pid);
    if (found == records_.end()) throw ProcessError("Unknown process ID.");
    return found->second;
}

Process& ProcessRegistry::requireActive(ProcessId pid) {
    auto& process = require(pid);
    if (process.state == ProcessState::FINISHED) throw ProcessError("Process is already finished.");
    return process;
}

void ProcessRegistry::attachProgram(ProcessId pid, std::shared_ptr<const InstructionProgram> program) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& process = require(pid);
    requireState(process, ProcessState::READY);
    if (!program || process.program.handle || process.progress.executedInstructions != 0 ||
        process.progress.currentInstruction != 0 || !process.progress.loops.empty()) {
        throw ProcessError("Attach a non-null program once, before execution progress begins.");
    }
    process.program.handle = std::move(program);
}

std::optional<Process> ProcessRegistry::findByPid(ProcessId pid) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto found = records_.find(pid);
    return found == records_.end() ? std::nullopt : std::optional<Process>{found->second};
}

std::optional<Process> ProcessRegistry::findByName(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto found = names_.find(name);
    return found == names_.end() ? std::nullopt : std::optional<Process>{require(found->second)};
}

std::optional<Process> ProcessRegistry::findActiveByName(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto found = names_.find(name);
    if (found == names_.end()) return std::nullopt;
    const auto& process = require(found->second);
    return process.state == ProcessState::FINISHED ? std::nullopt : std::optional<Process>{process};
}

std::vector<Process> ProcessRegistry::snapshots(std::optional<ProcessState> state) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Process> result;
    for (const auto& entry : records_) {
        const auto& process = entry.second;
        if (state ? process.state == *state : process.state != ProcessState::FINISHED) {
            result.push_back(process);
        }
    }
    return result;
}

std::vector<Process> ProcessRegistry::activeSnapshots() const { return snapshots(std::nullopt); }
std::vector<Process> ProcessRegistry::finishedSnapshots() const { return snapshots(ProcessState::FINISHED); }
std::vector<Process> ProcessRegistry::readySnapshots() const { return snapshots(ProcessState::READY); }

void ProcessRegistry::dispatch(ProcessId pid, std::uint64_t coreId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& process = require(pid);
    requireState(process, ProcessState::READY);
    if (coreId >= config_.numCpu) throw ProcessError("Core ID is outside configured CPU range.");
    if (atEnd(process)) throw ProcessError("Completed progress must be finalized before dispatch.");
    for (const auto& entry : records_) {
        if (entry.second.assignedCore == coreId) throw ProcessError("Core is already assigned.");
    }
    process.assignedCore = coreId;
    process.lastCore = coreId;
    process.state = ProcessState::RUNNING;
}

void ProcessRegistry::releaseToReady(ProcessId pid) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& process = require(pid);
    requireState(process, ProcessState::RUNNING);
    process.assignedCore.reset();
    process.state = ProcessState::READY;
}

void ProcessRegistry::sleep(ProcessId pid, std::uint64_t ticks) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& process = require(pid);
    requireState(process, ProcessState::RUNNING);
    process.remainingSleepTicks = ticks;
    process.assignedCore.reset();
    process.state = ProcessState::SLEEPING;
}

void ProcessRegistry::setSleepTicks(ProcessId pid, std::uint64_t remaining) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& process = require(pid);
    requireState(process, ProcessState::SLEEPING);
    if (remaining > process.remainingSleepTicks) throw ProcessError("Remaining sleep ticks cannot increase.");
    process.remainingSleepTicks = remaining;
}

void ProcessRegistry::wake(ProcessId pid) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& process = require(pid);
    requireState(process, ProcessState::SLEEPING);
    if (process.remainingSleepTicks != 0) throw ProcessError("Sleep duration has not elapsed.");
    process.state = ProcessState::READY;
}

void ProcessRegistry::setProgress(ProcessId pid, InstructionProgress progress) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& process = requireActive(pid);
    if (progress.currentInstruction > process.program.staticInstructionCount ||
        progress.executedInstructions > process.program.totalInstructions ||
        progress.executedInstructions < process.progress.executedInstructions) {
        throw ProcessError("Instruction progress is outside its program bounds or moves count backwards.");
    }
    std::uint64_t outerBegin = 0, outerEnd = process.program.staticInstructionCount;
    for (const auto& frame : progress.loops) {
        if (frame.begin < outerBegin || frame.end > outerEnd || frame.begin >= frame.end ||
            frame.next < frame.begin || frame.next > frame.end || frame.remainingIterations == 0) {
            throw ProcessError("Invalid nested loop context.");
        }
        outerBegin = frame.begin;
        outerEnd = frame.end;
    }
    process.progress = std::move(progress);
}

void ProcessRegistry::setExecutionCounters(ProcessId pid, std::uint64_t quantum, std::uint64_t delay) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& process = requireActive(pid);
    if (quantum > config_.quantumCycles || delay > config_.delaysPerExec) {
        throw ProcessError("Execution counters exceed configured limits.");
    }
    process.remainingQuantum = quantum;
    process.remainingExecutionDelay = delay;
}

void ProcessRegistry::setVariable(ProcessId pid, std::string name, std::uint64_t value) {
    if (name.empty() || value > std::numeric_limits<std::uint16_t>::max()) {
        throw ProcessError("Variable storage requires a name and a uint16 value.");
    }
    std::lock_guard<std::mutex> lock(mutex_);
    requireActive(pid).variables.insert_or_assign(std::move(name), static_cast<std::uint16_t>(value));
}

std::optional<std::uint16_t> ProcessRegistry::variable(ProcessId pid, const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto& variables = require(pid).variables;
    const auto found = variables.find(name);
    return found == variables.end() ? std::nullopt : std::optional<std::uint16_t>{found->second};
}

void ProcessRegistry::appendLog(ProcessId pid, PrintLogEntry entry) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& process = requireActive(pid);
    if (entry.coreId && *entry.coreId >= config_.numCpu) throw ProcessError("Log core ID is out of range.");
    process.printLogs.push_back(std::move(entry));
}

bool ProcessRegistry::complete(ProcessId pid) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& process = require(pid);
    if (process.state == ProcessState::FINISHED) return false;
    if ((process.state != ProcessState::READY && process.state != ProcessState::RUNNING) || !atEnd(process)) {
        throw ProcessError("Completion requires READY/RUNNING, final progress and no active loops.");
    }
    process.state = ProcessState::FINISHED;
    process.assignedCore.reset();
    process.finishedAt = std::chrono::system_clock::now();
    process.variables.clear();
    process.remainingSleepTicks = 0;
    process.remainingQuantum = 0;
    process.remainingExecutionDelay = 0;
    return true;
}

} // namespace csopesy
