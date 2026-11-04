/*
 * CSOPESY Major Output 1
 * Assigned Member: Enrique
 * Phase: 4 - CPU Scheduler
 * File responsibility: Future implementation for CPU Scheduler.
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
#include "Scheduler_Enrique.h"

#include <fstream>
#include <stdexcept>

namespace csopesy {

CpuScheduler::CpuScheduler(const Config& config, ProcessRegistry& registry)
    : config_(config), registry_(registry) {
    cores_.reserve(config_.numCpu);
    for (std::uint64_t id = 0; id < config_.numCpu; ++id) cores_.push_back(CpuCore{id, std::nullopt});
}

CpuScheduler::~CpuScheduler() { stop(); }

void CpuScheduler::start(std::chrono::milliseconds tickInterval) {
    std::lock_guard<std::mutex> lock(threadMutex_);
    if (thread_.joinable()) return;
    if (tickInterval.count() <= 0) tickInterval = std::chrono::milliseconds{1};
    stopRequested_ = false;
    threadRunning_ = true;
    thread_ = std::thread(&CpuScheduler::threadMain, this, tickInterval);
}

void CpuScheduler::stop() noexcept {
    {
        std::lock_guard<std::mutex> lock(threadMutex_);
        stopRequested_ = true;
    }
    wakeThread_.notify_all();
    if (thread_.joinable()) thread_.join();
    std::lock_guard<std::mutex> lock(threadMutex_);
    threadRunning_ = false;
}

bool CpuScheduler::running() const noexcept {
    std::lock_guard<std::mutex> lock(threadMutex_);
    return threadRunning_;
}

void CpuScheduler::threadMain(std::chrono::milliseconds interval) {
    auto next = std::chrono::steady_clock::now() + interval;
    for (;;) {
        {
            std::unique_lock<std::mutex> lock(threadMutex_);
            if (wakeThread_.wait_until(lock, next, [this] { return stopRequested_; })) return;
        }
        try {
            step();
        } catch (const std::exception& error) {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                lastError_ = error.what();
            }
            std::lock_guard<std::mutex> lock(threadMutex_);
            threadRunning_ = false;
            return;
        }
        next += interval;
        const auto now = std::chrono::steady_clock::now();
        if (next < now) next = now;
    }
}

void CpuScheduler::admit(ProcessId pid) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (tasks_.count(pid)) throw ProcessError("Process is already admitted to the scheduler.");
    const auto record = registry_.findByPid(pid);
    if (!record) throw ProcessError("Unknown process ID.");
    if (record->state != ProcessState::READY) throw ProcessError("Only READY processes can be admitted.");
    if (!record->program.handle) throw ProcessError("Attach an instruction program before admitting.");

    Task task;
    task.name = record->name;
    task.program = record->program.handle;
    task.state.progress = record->progress;
    task.state.variables = record->variables;
    task.delayLeft = record->remainingExecutionDelay;
    normalizePosition(*task.program, task.state.progress);
    registry_.setProgress(pid, task.state.progress);

    tasks_.emplace(pid, std::move(task));
    if (isFinished(*tasks_.at(pid).program, tasks_.at(pid).state.progress)) {
        finishLocked(pid);
        return;
    }
    enqueueLocked(pid);
}

void CpuScheduler::enqueueLocked(ProcessId pid) {
    if (!queued_.insert(pid).second) return;
    ready_.push_back(pid);
}

void CpuScheduler::finishLocked(ProcessId pid) {
    registry_.complete(pid);
    tasks_.erase(pid);
}

void CpuScheduler::storeCountersLocked(ProcessId pid, const Task& task) {
    registry_.setExecutionCounters(pid, task.quantumLeft, task.delayLeft);
}

void CpuScheduler::step() {
    std::function<void(std::uint64_t)> observer;
    std::uint64_t tick = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        tickLocked();
        tick = tick_;
        observer = observer_;
    }
    if (observer) observer(tick);
}

std::uint64_t CpuScheduler::runUntilIdle(std::uint64_t maxTicks) {
    std::uint64_t ran = 0;
    while (ran < maxTicks && !idle()) {
        step();
        ++ran;
    }
    return ran;
}

void CpuScheduler::tickLocked() {
    ++tick_;

    // 1. Wake or count down sleepers.
    std::vector<ProcessId> stillSleeping;
    for (const auto pid : sleeping_) {
        auto& task = tasks_.at(pid);
        if (task.sleepLeft == 0) {
            registry_.wake(pid);
            if (isFinished(*task.program, task.state.progress)) finishLocked(pid);
            else enqueueLocked(pid);
        } else {
            --task.sleepLeft;
            registry_.setSleepTicks(pid, task.sleepLeft);
            stillSleeping.push_back(pid);
        }
    }
    sleeping_ = std::move(stillSleeping);

    // 2. Dispatch READY processes to free cores.
    for (auto& core : cores_) {
        if (core.occupied() || ready_.empty()) continue;
        const auto pid = ready_.front();
        ready_.pop_front();
        queued_.erase(pid);
        registry_.dispatch(pid, core.id);
        core.process = pid;
        auto& task = tasks_.at(pid);
        task.quantumLeft = config_.scheduler == SchedulerPolicy::RoundRobin ? config_.quantumCycles : 0;
        storeCountersLocked(pid, task);
    }

    // 3. Run one cycle on every busy core.
    for (auto& core : cores_) {
        if (core.occupied()) runCycleLocked(core);
    }
}

void CpuScheduler::runCycleLocked(CpuCore& core) {
    const auto pid = *core.process;
    auto& task = tasks_.at(pid);

    if (task.delayLeft > 0) {
        // Busy wait: keeps the core, no instruction runs.
        --task.delayLeft;
    } else {
        const auto result = executeNext(*task.program, task.state, task.name, core.id,
                                        std::chrono::system_clock::now());
        for (const auto& write : result.writes) registry_.setVariable(pid, write.first, write.second);
        if (result.log) registry_.appendLog(pid, *result.log);
        registry_.setProgress(pid, task.state.progress);

        if (result.sleepTicks) {
            task.sleepLeft = *result.sleepTicks;
            task.delayLeft = 0;
            task.quantumLeft = 0;
            storeCountersLocked(pid, task);
            registry_.sleep(pid, task.sleepLeft);
            sleeping_.push_back(pid);
            core.process.reset();
            return;
        }
        if (result.finished) {
            core.process.reset();
            finishLocked(pid);
            return;
        }
        task.delayLeft = config_.delaysPerExec;
    }

    if (config_.scheduler == SchedulerPolicy::RoundRobin) {
        // Busy-wait cycles count toward the quantum.
        if (task.quantumLeft > 0) --task.quantumLeft;
        if (task.quantumLeft == 0) {
            storeCountersLocked(pid, task);
            registry_.releaseToReady(pid);
            core.process.reset();
            enqueueLocked(pid);
            return;
        }
    }
    storeCountersLocked(pid, task);
}

std::uint64_t CpuScheduler::tickCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return tick_;
}

std::vector<CpuCore> CpuScheduler::cores() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return cores_;
}

std::vector<ProcessId> CpuScheduler::readyQueue() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return {ready_.begin(), ready_.end()};
}

std::vector<ProcessId> CpuScheduler::sleeping() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sleeping_;
}

bool CpuScheduler::idle() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return tasks_.empty();
}

std::string CpuScheduler::lastError() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return lastError_;
}

void CpuScheduler::setTickObserver(std::function<void(std::uint64_t)> observer) {
    std::lock_guard<std::mutex> lock(mutex_);
    observer_ = std::move(observer);
}

SchedulerBackend::SchedulerBackend(ProgramSourceOptions options, std::chrono::milliseconds tickInterval)
    : options_(std::move(options)), tickInterval_(tickInterval),
      rng_(options_.seed ? *options_.seed : std::random_device{}()) {}

SchedulerBackend::~SchedulerBackend() { shutdown(); }

void SchedulerBackend::initialize(const Config& config, ProcessRegistry& registry) {
    if (scheduler_) throw std::logic_error("Scheduler backend is already initialized.");
    config_ = config;
    registry_ = &registry;
    scheduler_ = std::make_unique<CpuScheduler>(config, registry);
    generator_ = std::make_unique<AutoGenerator>(config.batchProcessFreq, registry,
                                                 [this](const std::string& name) { createProcess(name); });
    scheduler_->setTickObserver([this](std::uint64_t tick) { generator_->onTick(tick); });
    scheduler_->start(tickInterval_);
}

void SchedulerBackend::shutdown() noexcept {
    if (generator_) generator_->stop();
    if (scheduler_) scheduler_->stop();
}

RuntimeStatus SchedulerBackend::status() const {
    RuntimeStatus result;
    if (scheduler_) result.scheduler = scheduler_->running() ? ServiceState::Running : ServiceState::Stopped;
    if (generator_) result.generation = generator_->enabled() ? ServiceState::Running : ServiceState::Stopped;
    return result;
}

std::shared_ptr<const InstructionProgram> SchedulerBackend::programFor(const std::string& name) {
    if (!options_.programDirectory.empty()) {
        const auto path = options_.programDirectory + "/" + name + ".txt";
        if (std::ifstream(path).good()) return loadProgramFile(path);
    }
    return generateProgram(config_->minIns, config_->maxIns, rng_, options_.mode);
}

ProcessId SchedulerBackend::createProcess(const std::string& name) {
    std::lock_guard<std::mutex> lock(createMutex_);
    if (!scheduler_ || !registry_) throw std::logic_error("Initialize the scheduler before creating processes.");
    const auto program = programFor(name);
    const auto pid = registry_->createProcess(name, program->processProgram(program));
    scheduler_->admit(pid);
    return pid;
}

void SchedulerBackend::startGeneration() {
    if (!generator_) throw std::logic_error("Initialize the scheduler before starting generation.");
    generator_->start();
}

void SchedulerBackend::stopGeneration() {
    if (generator_) generator_->stop();
}

}
