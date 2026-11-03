// Enrique - Phase 3 and 4 tests. Exit code is 0 when every check passes.
#include "Instruction_Enrique.h"
#include "Scheduler_Enrique.h"

#ifndef PROGRAM_DIR
#define PROGRAM_DIR "programs"
#endif

#include <atomic>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <thread>

using namespace csopesy;

namespace {
int failures = 0;
int checks = 0;
std::string currentTest;

void check(bool ok, const std::string& what) {
    ++checks;
    if (!ok) {
        ++failures;
        std::cout << "  FAIL [" << currentTest << "] " << what << '\n';
    }
}

template <class A, class B>
void checkEq(const A& actual, const B& expected, const std::string& what) {
    std::ostringstream message;
    message << what << " (expected " << expected << ", got " << actual << ')';
    check(actual == expected, message.str());
}

void test(const std::string& name) {
    currentTest = name;
    std::cout << "- " << name << '\n';
}

// ------------------------------------------------------------ helpers --
// Runs a whole program with the executor alone (no scheduler).
struct RunResult {
    ExecutionState state;
    std::vector<std::string> logs;
    std::vector<int> sleeps;
    std::uint64_t steps = 0;
    bool countedByOne = true;
    std::size_t maxLoopDepth = 0;
};

RunResult runProgram(const InstructionProgram& program, const std::string& name = "p1") {
    RunResult run;
    for (;;) {
        const auto before = run.state.progress.executedInstructions;
        const auto result = executeNext(program, run.state, name, 0, std::chrono::system_clock::now());
        if (!result.executed) break;
        ++run.steps;
        if (run.state.progress.executedInstructions != before + 1) run.countedByOne = false;
        if (result.log) run.logs.push_back(result.log->message);
        if (result.sleepTicks) run.sleeps.push_back(*result.sleepTicks);
        run.maxLoopDepth = std::max(run.maxLoopDepth, run.state.progress.loops.size());
        if (result.finished) break;
    }
    return run;
}

std::uint16_t var(const ExecutionState& state, const std::string& name) {
    const auto found = state.variables.find(name);
    return found == state.variables.end() ? 0xFFFF : found->second;
}

bool hasVar(const ExecutionState& state, const std::string& name) {
    return state.variables.count(name) != 0;
}

// A console initialized from a temporary config gives us a real ProcessRegistry.
struct Emulator {
    std::unique_ptr<Console> console;
    std::filesystem::path configPath;

    Emulator(std::uint64_t cpus, const std::string& policy, std::uint64_t quantum, std::uint64_t delay,
             std::shared_ptr<ConsoleBackend> backend = nullptr, std::uint64_t minIns = 1,
             std::uint64_t maxIns = 50) {
        static int counter = 0;
        configPath = std::filesystem::temp_directory_path() /
                     ("csopesy_test_config_" + std::to_string(++counter) + ".txt");
        std::ofstream file(configPath);
        file << "num-cpu " << cpus << "\nscheduler \"" << policy << "\"\nquantum-cycles " << quantum
             << "\nbatch-process-freq 1\nmin-ins " << minIns << "\nmax-ins " << maxIns
             << "\ndelays-per-exec " << delay << '\n';
        file.close();
        console = std::make_unique<Console>(configPath.string());
        if (backend) console->setBackend(backend);
        std::ostringstream out;
        console->executeLine("initialize", out);
        if (!console->isInitialized()) throw std::runtime_error("init failed: " + out.str());
    }
    ~Emulator() {
        console.reset();
        std::error_code ignored;
        std::filesystem::remove(configPath, ignored);
    }
    ProcessRegistry& registry() { return console->processRegistry(); }
    const Config& config() { return *console->configuration(); }
};

ProcessId addProcess(ProcessRegistry& registry, const std::string& name, const std::string& programText) {
    const auto program = parseAndCompile(programText);
    return registry.createProcess(name, program->processProgram(program));
}

Process record(ProcessRegistry& registry, ProcessId pid) { return *registry.findByPid(pid); }

// Which process is on each core after the current tick ("-" when free).
std::string coreLine(const CpuScheduler& scheduler, ProcessRegistry& registry) {
    std::string line;
    for (const auto& core : scheduler.cores()) {
        if (!line.empty()) line += ' ';
        line += core.process ? record(registry, *core.process).name : "-";
    }
    return line;
}

// Runs `ticks` ticks and records the core line after each one.
std::vector<std::string> trace(CpuScheduler& scheduler, ProcessRegistry& registry, int ticks) {
    std::vector<std::string> lines;
    for (int i = 0; i < ticks; ++i) {
        // A process that ran this tick but then finished/slept/was preempted
        // is no longer on the core, so record who executed via progress.
        std::map<ProcessId, std::uint64_t> before;
        for (const auto& p : registry.allSnapshots()) before[p.pid] = p.progress.executedInstructions;
        scheduler.step();
        std::string line;
        for (const auto& p : registry.allSnapshots()) {
            if (p.progress.executedInstructions != before[p.pid]) line += (line.empty() ? "" : " ") + p.name;
        }
        lines.push_back(line.empty() ? "-" : line);
    }
    return lines;
}

std::string join(const std::vector<std::string>& parts) {
    std::string out;
    for (const auto& part : parts) out += (out.empty() ? "" : " | ") + part;
    return out;
}

std::vector<std::string> messages(const Process& process) {
    std::vector<std::string> out;
    for (const auto& log : process.printLogs) out.push_back(log.message);
    return out;
}

// =================================================== Phase 3 tests ====
void testPrint() {
    test("PRINT: default message, plain text, one variable with text");
    const auto program = parseAndCompile(R"(PRINT() PRINT("plain text") PRINT("v=" + v + "!") DECLARE(n, 5) PRINT(n))");
    const auto run = runProgram(*program, "p01");
    checkEq(run.logs.size(), 4u, "four PRINT logs");
    checkEq(run.logs.at(0), std::string("Hello world from p01!"), "default message uses the process name");
    checkEq(run.logs.at(1), std::string("plain text"), "text message");
    checkEq(run.logs.at(2), std::string("v=0!"), "undeclared variable prints as 0");
    check(hasVar(run.state, "v"), "PRINT of an undeclared variable creates it with 0");
    checkEq(run.logs.at(3), std::string("5"), "variable-only PRINT");

    const auto result = [] {
        ExecutionState state;
        const auto p = parseAndCompile("PRINT(\"x\")");
        const auto when = std::chrono::system_clock::now();
        return std::make_pair(executeNext(*p, state, "p", 3, when), when);
    }();
    check(result.first.log && result.first.log->coreId == std::optional<std::uint64_t>(3),
          "PRINT log stores the executing core ID");
    check(result.first.log && result.first.log->timestamp == result.second, "PRINT log stores the timestamp");
}

void testDeclare() {
    test("DECLARE: uint16 storage, clamping, reassignment");
    const auto run = runProgram(*parseAndCompile(
        "DECLARE(a, 123) DECLARE(b, -5) DECLARE(c, 70000) DECLARE(d, 65535) DECLARE(e, 0) DECLARE(a, 7)"));
    checkEq(var(run.state, "a"), 7, "reassigned value");
    checkEq(var(run.state, "b"), 0, "negative clamps to 0");
    checkEq(var(run.state, "c"), 65535, "70000 clamps to 65535");
    checkEq(var(run.state, "d"), 65535, "65535 is kept");
    checkEq(var(run.state, "e"), 0, "0 is kept");
}

void testAdd() {
    test("ADD: literals, variables, undeclared = 0, clamp at 65535");
    const auto run = runProgram(*parseAndCompile(R"(
        DECLARE(a, 10)
        ADD(b, a, 5)          # var + literal = 15
        ADD(c, a, b)          # var + var = 25
        ADD(d, 3, 4)          # literal + literal = 7
        ADD(e, missing, 3)    # undeclared operand -> 0
        ADD(f, 65000, 1000)   # clamp
        ADD(g, 65535, 65535)  # clamp
        ADD(h, nothing, alsoNothing)
        ADD(a, a, 1)          # self update
    )"));
    checkEq(var(run.state, "b"), 15, "var + literal");
    checkEq(var(run.state, "c"), 25, "var + var");
    checkEq(var(run.state, "d"), 7, "literal + literal");
    checkEq(var(run.state, "e"), 3, "undeclared operand treated as 0");
    check(hasVar(run.state, "missing") && var(run.state, "missing") == 0, "undeclared operand created with 0");
    checkEq(var(run.state, "f"), 65535, "65000 + 1000 clamps");
    checkEq(var(run.state, "g"), 65535, "65535 + 65535 clamps");
    checkEq(var(run.state, "h"), 0, "two undeclared operands = 0");
    checkEq(var(run.state, "a"), 11, "a = a + 1");
}

void testSubtract() {
    test("SUBTRACT: literals, variables, undeclared = 0, clamp at 0");
    const auto run = runProgram(*parseAndCompile(R"(
        DECLARE(a, 10)
        SUBTRACT(b, a, 3)       # 7
        SUBTRACT(c, 3, a)       # -7 -> 0
        SUBTRACT(d, unknown, 1) # 0 - 1 -> 0
        SUBTRACT(e, 65535, 1)   # 65534
        SUBTRACT(a, a, a)       # 0
        SUBTRACT(f, b, unknown2)
    )"));
    checkEq(var(run.state, "b"), 7, "var - literal");
    checkEq(var(run.state, "c"), 0, "negative clamps to 0");
    checkEq(var(run.state, "d"), 0, "undeclared - 1 clamps to 0");
    check(hasVar(run.state, "unknown"), "undeclared operand created");
    checkEq(var(run.state, "e"), 65534, "literal - literal");
    checkEq(var(run.state, "a"), 0, "a - a");
    checkEq(var(run.state, "f"), 7, "subtracting an undeclared variable subtracts 0");
}

void testSleepInstruction() {
    test("SLEEP: uint8 argument, counted once");
    const auto program = parseAndCompile("SLEEP(0) SLEEP(7) SLEEP(255)");
    const auto run = runProgram(*program);
    checkEq(run.sleeps.size(), 3u, "three sleeps reported");
    check(run.sleeps == std::vector<int>{0, 7, 255}, "sleep durations 0, 7, 255");
    checkEq(program->totalInstructions(), 3u, "each SLEEP counts as one instruction");
    bool rejected = false;
    try { parseAndCompile("SLEEP(256)"); } catch (const InstructionError&) { rejected = true; }
    check(rejected, "SLEEP(256) is rejected (not uint8)");
}

void testForLoops() {
    test("FOR: repetition counts and three-level nesting");
    {
        const auto program = parseAndCompile("FOR([ADD(x, x, 1)], 5)");
        const auto run = runProgram(*program);
        checkEq(program->staticInstructionCount(), 2u, "FOR header + 1 body slot");
        checkEq(program->totalInstructions(), 5u, "total counts repetitions, not the FOR header");
        checkEq(var(run.state, "x"), 5, "body ran 5 times");
        checkEq(run.steps, 5u, "5 CPU cycles");
    }
    {
        const auto program = parseAndCompile(R"(
            FOR([
              FOR([
                FOR([ADD(x, x, 1)], 4),
                ADD(mid, mid, 1)
              ], 3),
              ADD(outer, outer, 1)
            ], 2))");
        const auto run = runProgram(*program);
        checkEq(var(run.state, "x"), 24, "innermost ran 2*3*4 = 24 times");
        checkEq(var(run.state, "mid"), 6, "middle ran 2*3 = 6 times");
        checkEq(var(run.state, "outer"), 2, "outer ran 2 times");
        checkEq(program->totalInstructions(), 32u, "total = 24 + 6 + 2");
        checkEq(run.state.progress.executedInstructions, 32u, "executed equals total at the end");
        checkEq(run.maxLoopDepth, 3u, "three loop frames active at the deepest point");
        check(run.countedByOne, "progress always moves by exactly 1 per cycle");
    }
    {
        const auto run = runProgram(*parseAndCompile(R"(FOR([PRINT("i=" + i), ADD(i, i, 1)], 3))"));
        check(run.logs == std::vector<std::string>{"i=0", "i=1", "i=2"}, "PRINT inside FOR sees updated variables");
    }
    {
        // Loop as the very first and very last instruction.
        const auto program = parseAndCompile("FOR([FOR([ADD(x,x,1)],2)],2)");
        const auto run = runProgram(*program);
        checkEq(var(run.state, "x"), 4, "nested loops at program start and end");
        check(isFinished(*program, run.state.progress), "finished right after the last counted instruction");
    }
    bool rejected = false;
    try { parseAndCompile("FOR([FOR([FOR([FOR([PRINT()],2)],2)],2)],2)"); }
    catch (const InstructionError&) { rejected = true; }
    check(rejected, "four nesting levels are rejected");
    rejected = false;
    try { parseAndCompile("FOR([PRINT()], 0)"); } catch (const InstructionError&) { rejected = true; }
    check(rejected, "repeat count 0 is rejected");
    rejected = false;
    try { parseAndCompile("FOR([], 3)"); } catch (const InstructionError&) { rejected = true; }
    check(rejected, "empty FOR body is rejected");
}

void testResumeEquivalence() {
    test("Saved position: pausing at every cycle gives the same result");
    const auto program = parseAndCompile(R"(
        DECLARE(x, 1)
        FOR([
          FOR([ADD(x, x, 2), PRINT("x=" + x), SLEEP(1)], 3),
          FOR([FOR([SUBTRACT(x, x, 1)], 2)], 2)
        ], 2)
        PRINT("done " + x))");
    const auto straight = runProgram(*program);

    // Copy the state out and back in after every single cycle, the way the
    // registry stores it during SLEEP / preemption / core changes.
    ExecutionState state;
    std::vector<std::string> logs;
    for (;;) {
        ExecutionState saved = state; // "Save to process record".
        state = ExecutionState{};     // "Process leaves the CPU".
        state = saved;                // "Resume, possibly on another core".
        const auto result = executeNext(*program, state, "p1", 1, std::chrono::system_clock::now());
        if (!result.executed) break;
        if (result.log) logs.push_back(result.log->message);
        if (result.finished) break;
    }
    check(logs == straight.logs, "same PRINT sequence");
    check(state.variables == straight.state.variables, "same variables");
    checkEq(state.progress.executedInstructions, program->totalInstructions(), "same final count");
}

void testParserRoundTrip() {
    test("Parser: toText() output parses back to the same program");
    const auto original = parseAndCompile(R"(PRINT() PRINT("say \"hi\"") PRINT("a" + b + "c")
        DECLARE(x, 9) ADD(y, x, 3) SUBTRACT(z, 1, y) SLEEP(4) FOR([PRINT(x), FOR([ADD(x,x,1)], 2)], 3))");
    const auto text = original->toText();
    const auto again = parseAndCompile(text);
    checkEq(again->toText(), text, "round trip is stable");
    checkEq(again->totalInstructions(), original->totalInstructions(), "same total");
    bool rejected = false;
    try { parseAndCompile("JUMP(3)"); } catch (const InstructionError&) { rejected = true; }
    check(rejected, "unknown instruction rejected");
}

void testSamplePrograms() {
    test("Sample fixed programs in programs/ load and give the documented results");
    const std::string dir = PROGRAM_DIR;
    const auto print = runProgram(*loadProgramFile(dir + "/demo_print.txt"), "demo_print");
    check(print.logs == std::vector<std::string>{"Hello world from demo_print!", "Plain text message", "x = 42",
                                                 "big = 65535 (clamped)", "neg = 0"}, "demo_print logs");
    const auto math = runProgram(*loadProgramFile(dir + "/demo_math.txt"));
    check(math.logs == std::vector<std::string>{"a=10", "b=15", "c=0", "d=7", "undeclared=0", "max=65535"},
          "demo_math logs");
    const auto forProgram = loadProgramFile(dir + "/demo_for.txt");
    const auto loops = runProgram(*forProgram);
    checkEq(forProgram->totalInstructions(), 34u, "demo_for total = 1 + 2*(3*(4+1)+1) + 1");
    checkEq(loops.logs.back(), std::string("final count = 24"), "demo_for final count");
    checkEq(runProgram(*loadProgramFile(dir + "/demo_sleep.txt")).sleeps.size(), 1u, "demo_sleep sleeps once");
}

void testGenerator() {
    test("Generator: lengths within min-ins..max-ins, valid, deterministic per seed");
    bool allInRange = true, allRunClean = true, sawFor = false, sawEveryOpcode = true;
    std::set<Opcode> seen;
    for (std::uint64_t seed = 1; seed <= 300; ++seed) {
        std::mt19937_64 rng(seed);
        const auto program = generateProgram(1000, 2000, rng);
        const auto total = program->totalInstructions();
        if (total < 1000 || total > 2000) allInRange = false;
        const auto run = runProgram(*program);
        if (run.steps != total || !run.countedByOne) allRunClean = false;
        for (std::uint64_t i = 0; i < program->staticInstructionCount(); ++i) {
            seen.insert(program->slot(i).instruction.opcode);
            if (program->slot(i).instruction.opcode == Opcode::For) sawFor = true;
        }
    }
    for (auto op : {Opcode::Print, Opcode::Declare, Opcode::Add, Opcode::Subtract, Opcode::Sleep, Opcode::For})
        if (!seen.count(op)) sawEveryOpcode = false;
    check(allInRange, "300 random programs all have 1000-2000 instructions");
    check(allRunClean, "every generated program runs exactly totalInstructions cycles");
    check(sawFor && sawEveryOpcode, "generator uses all six instruction types");

    std::mt19937_64 a(42), b(42);
    checkEq(generateProgram(1, 500, a)->toText(), generateProgram(1, 500, b)->toText(), "same seed, same program");

    bool exact = true;
    for (std::uint64_t n = 1; n <= 60; ++n) {
        std::mt19937_64 rng(n);
        if (generateProgram(n, n, rng)->totalInstructions() != n) exact = false;
    }
    check(exact, "min-ins == max-ins gives exactly that many instructions (1..60)");

    std::mt19937_64 rng(7);
    const auto fixed = generateProgram(11, 11, rng, GeneratorMode::Predetermined);
    const auto run = runProgram(*fixed);
    checkEq(fixed->totalInstructions(), 11u, "predetermined length");
    checkEq(var(run.state, "x"), 5, "predetermined: DECLARE + 5 x (PRINT, ADD)");
    checkEq(run.logs.front(), std::string("Value from: 0"), "predetermined first PRINT");
    checkEq(run.logs.back(), std::string("Value from: 4"), "predetermined last PRINT (before the final ADD)");

    std::mt19937_64 big(3);
    const auto huge = generateProgram(1000000000ULL, 4000000000ULL, big);
    check(huge->staticInstructionCount() < 2000000, "huge totals stay compact thanks to FOR loops");
}

// =================================================== Phase 4 tests ====
const char* Count3 = "ADD(x,x,1) ADD(x,x,1) ADD(x,x,1)";

void testCores() {
    test("Cores: exact count from num-cpu, IDs, availability");
    Emulator emu(4, "fcfs", 1, 0);
    CpuScheduler scheduler(emu.config(), emu.registry());
    const auto cores = scheduler.cores();
    checkEq(cores.size(), 4u, "4 cores created");
    bool ok = true;
    for (std::size_t i = 0; i < cores.size(); ++i) ok = ok && cores[i].id == i && !cores[i].occupied();
    check(ok, "IDs 0-3, all available");
    const auto pid = addProcess(emu.registry(), "p1", Count3);
    scheduler.admit(pid);
    bool duplicate = false;
    try { scheduler.admit(pid); } catch (const ProcessError&) { duplicate = true; }
    check(duplicate, "admitting the same process twice is rejected");
    checkEq(scheduler.readyQueue().size(), 1u, "only one READY queue entry");
}

void testFcfsOneCore() {
    test("FCFS, 1 core: arrival order, runs to completion, releases the CPU");
    Emulator emu(1, "fcfs", 2, 0);
    CpuScheduler scheduler(emu.config(), emu.registry());
    for (auto name : {"p1", "p2", "p3"}) scheduler.admit(addProcess(emu.registry(), name, Count3));
    const auto lines = trace(scheduler, emu.registry(), 10);
    checkEq(join(lines), std::string("p1 | p1 | p1 | p2 | p2 | p2 | p3 | p3 | p3 | -"),
            "each process keeps the CPU until it finishes (quantum ignored)");
    check(!scheduler.cores()[0].occupied(), "core released after the last process");
    bool allDone = true;
    for (const auto& p : emu.registry().allSnapshots()) allDone = allDone && p.state == ProcessState::FINISHED;
    check(allDone, "all processes FINISHED");
    const auto finished = emu.registry().finishedSnapshots();
    check(finished[0].finishedAt <= finished[1].finishedAt && finished[1].finishedAt <= finished[2].finishedAt,
          "finish times follow arrival order");
}

void testFcfsFourCores() {
    test("FCFS, 4 cores: concurrent execution and correct release");
    Emulator emu(4, "fcfs", 2, 0);
    CpuScheduler scheduler(emu.config(), emu.registry());
    std::vector<ProcessId> pids;
    for (auto name : {"p1", "p2", "p3", "p4", "p5"})
        pids.push_back(addProcess(emu.registry(), name, "FOR([ADD(x,x,1)], 5)"));
    for (auto pid : pids) scheduler.admit(pid);

    scheduler.step();
    checkEq(coreLine(scheduler, emu.registry()), std::string("p1 p2 p3 p4"), "tick 1: p1-p4 on cores 0-3");
    bool allOne = true;
    for (int i = 0; i < 4; ++i) allOne = allOne && record(emu.registry(), pids[i]).progress.executedInstructions == 1;
    check(allOne, "all four advanced in the same tick");
    check(record(emu.registry(), pids[4]).state == ProcessState::READY, "p5 waits in the READY queue");
    for (int i = 0; i < 4; ++i) scheduler.step();
    checkEq(coreLine(scheduler, emu.registry()), std::string("- - - -"), "tick 5: p1-p4 finished, cores free");
    scheduler.step();
    checkEq(coreLine(scheduler, emu.registry()), std::string("p5 - - -"), "tick 6: p5 takes core 0");
    checkEq(record(emu.registry(), pids[4]).lastCore.value_or(99), 0u, "p5 ran on core 0");
    scheduler.runUntilIdle(100);
    checkEq(emu.registry().finishedSnapshots().size(), 5u, "all five finished");
}

void testRoundRobin(std::uint64_t quantum, int length, const std::string& expected) {
    test("Round Robin, quantum " + std::to_string(quantum) + ": preemption and queue rotation");
    Emulator emu(1, "rr", quantum, 0);
    CpuScheduler scheduler(emu.config(), emu.registry());
    const std::string program = "FOR([ADD(x, x, 1), PRINT(\"x=\" + x)], " + std::to_string(length) + ")";
    std::vector<ProcessId> pids;
    for (auto name : {"p1", "p2", "p3"}) pids.push_back(addProcess(emu.registry(), name, program));
    for (auto pid : pids) scheduler.admit(pid);
    const auto total = static_cast<int>(3 * 2 * length);
    const auto lines = trace(scheduler, emu.registry(), total);
    checkEq(join(lines), expected, "tick-by-tick schedule");
    check(scheduler.idle(), "all processes finished after exactly 3 x total ticks");
    std::vector<std::string> expectedLogs;
    for (int i = 1; i <= length; ++i) expectedLogs.push_back("x=" + std::to_string(i));
    bool logsOk = true;
    for (auto pid : pids) {
        const auto p = record(emu.registry(), pid);
        logsOk = logsOk && messages(p) == expectedLogs &&
                 p.progress.executedInstructions == p.program.totalInstructions;
    }
    check(logsOk, "no skipped or duplicated instructions (each PRINT sequence is x=1..x=" +
                      std::to_string(length) + ")");
}

void testRoundRobinCoreMigration() {
    test("Round Robin, 2 cores: loop state and variables survive core changes");
    Emulator emu(2, "rr", 2, 0);
    CpuScheduler scheduler(emu.config(), emu.registry());
    const std::string program = R"(
        FOR([
          FOR([ADD(n, n, 1), PRINT("n=" + n)], 3),
          SLEEP(1)
        ], 3))";
    std::vector<ProcessId> pids;
    for (auto name : {"a", "b", "c"}) pids.push_back(addProcess(emu.registry(), name, program));
    for (auto pid : pids) scheduler.admit(pid);
    std::map<ProcessId, std::set<std::uint64_t>> coresUsed;
    std::size_t maxRunning = 0;
    while (!scheduler.idle() && scheduler.tickCount() < 500) {
        scheduler.step();
        std::size_t running = 0;
        for (const auto& p : emu.registry().allSnapshots()) {
            for (const auto& log : p.printLogs) if (log.coreId) coresUsed[p.pid].insert(*log.coreId);
            if (p.state == ProcessState::RUNNING) ++running;
        }
        maxRunning = std::max(maxRunning, running);
    }
    std::vector<std::string> expected;
    for (int i = 1; i <= 9; ++i) expected.push_back("n=" + std::to_string(i));
    bool ok = true, migrated = false;
    for (auto pid : pids) {
        ok = ok && messages(record(emu.registry(), pid)) == expected;
        migrated = migrated || coresUsed[pid].size() > 1;
    }
    check(ok, "every process printed n=1..n=9 in order");
    check(migrated, "at least one process resumed on a different core");
    check(maxRunning <= 2, "never more RUNNING processes than cores");
}

void testSleepFreesCpu(const std::string& policy) {
    test("SLEEP under " + policy + ": frees the core for exactly X ticks");
    Emulator emu(1, policy, 2, 0);
    CpuScheduler scheduler(emu.config(), emu.registry());
    const auto a = addProcess(emu.registry(), "a", R"(PRINT("a1") SLEEP(3) PRINT("a2"))");
    const auto b = addProcess(emu.registry(), "b", "FOR([ADD(x,x,1)], 6)");
    scheduler.admit(a);
    scheduler.admit(b);
    scheduler.step(); // a: PRINT
    scheduler.step(); // a: SLEEP(3)
    check(record(emu.registry(), a).state == ProcessState::SLEEPING, "a is SLEEPING after SLEEP");
    check(!record(emu.registry(), a).assignedCore, "a has no core while sleeping");
    bool coreBusy = true, stillAsleep = true;
    for (int t = 3; t <= 5; ++t) {
        scheduler.step();
        // b executes on the freed core every tick (under RR it may be
        // preempted and re-dispatched in between, which is fine).
        coreBusy = coreBusy && record(emu.registry(), b).progress.executedInstructions ==
                                   static_cast<std::uint64_t>(t - 2);
        stillAsleep = stillAsleep && record(emu.registry(), a).state == ProcessState::SLEEPING;
    }
    check(coreBusy, "b uses the freed core during ticks 3-5");
    check(stillAsleep, "a stays asleep for 3 ticks");
    checkEq(record(emu.registry(), a).remainingSleepTicks, 0u, "sleep counter reached 0");
    scheduler.step(); // tick 6: a wakes
    check(record(emu.registry(), a).state != ProcessState::SLEEPING, "a is READY/RUNNING again at tick 6");
    scheduler.runUntilIdle(50);
    check(messages(record(emu.registry(), a)) == std::vector<std::string>{"a1", "a2"}, "a resumes after SLEEP");
}

void testBusyWait() {
    test("delays-per-exec: busy wait keeps the core and counts toward the quantum");
    {
        Emulator emu(1, "rr", 4, 2);
        CpuScheduler scheduler(emu.config(), emu.registry());
        const auto p = addProcess(emu.registry(), "p", "FOR([ADD(x,x,1)], 3)");
        const auto q = addProcess(emu.registry(), "q", "ADD(y,y,1)");
        scheduler.admit(p);
        scheduler.admit(q);
        scheduler.step(); // tick 1: p executes #1
        checkEq(record(emu.registry(), p).progress.executedInstructions, 1u, "tick 1 executes");
        scheduler.step();
        scheduler.step(); // ticks 2-3: busy wait
        check(scheduler.cores()[0].process == std::optional<ProcessId>(p), "p still holds the core while waiting");
        checkEq(record(emu.registry(), p).progress.executedInstructions, 1u, "no instruction during busy wait");
        scheduler.step(); // tick 4: p executes #2 and its quantum (4 cycles) is used up
        checkEq(record(emu.registry(), p).progress.executedInstructions, 2u, "tick 4 executes #2");
        check(record(emu.registry(), p).state == ProcessState::READY, "quantum of 4 included the 2 busy cycles");
        checkEq(record(emu.registry(), p).remainingExecutionDelay, 2u, "pending wait saved across preemption");
        scheduler.step(); // tick 5: q runs
        check(record(emu.registry(), q).state == ProcessState::FINISHED, "q ran while p was preempted");
        scheduler.step();
        scheduler.step(); // ticks 6-7: p finishes its saved wait on the core
        checkEq(record(emu.registry(), p).progress.executedInstructions, 2u, "saved wait is completed first");
        scheduler.step(); // tick 8: #3
        check(record(emu.registry(), p).state == ProcessState::FINISHED, "p finishes with no trailing wait");
    }
    {
        Emulator emu(1, "fcfs", 1, 2);
        CpuScheduler scheduler(emu.config(), emu.registry());
        scheduler.admit(addProcess(emu.registry(), "p", "FOR([ADD(x,x,1)], 3)"));
        checkEq(scheduler.runUntilIdle(100), 7u, "FCFS: 3 instructions + 2 waits of 2 cycles = 7 ticks");
    }
    {
        Emulator emu(1, "fcfs", 1, 3);
        CpuScheduler scheduler(emu.config(), emu.registry());
        const auto p = addProcess(emu.registry(), "p", "SLEEP(2) ADD(x,x,1)");
        const auto q = addProcess(emu.registry(), "q", "FOR([ADD(y,y,1)], 2)");
        scheduler.admit(p);
        scheduler.admit(q);
        scheduler.step(); // p sleeps -> core free immediately (no busy wait after SLEEP)
        check(!scheduler.cores()[0].occupied(), "SLEEP releases the core instead of busy waiting");
        scheduler.step();
        check(scheduler.cores()[0].process == std::optional<ProcessId>(q), "another process takes the core");
    }
}

void testBackgroundExecution() {
    test("Background tick loop: processes run while the console waits for input");
    ProgramSourceOptions options;
    options.programDirectory.clear();
    options.seed = 11;
    auto backend = std::make_shared<SchedulerBackend>(options, std::chrono::milliseconds(1));
    Emulator emu(2, "rr", 3, 0, backend, 200, 300);
    std::ostringstream out;
    emu.console->executeLine("screen -s p01", out); // Attaches to the process screen.
    check(out.str().find("Created process p01") != std::string::npos, "screen -s created and attached p01");
    const auto before = emu.registry().findByName("p01")->progress.executedInstructions;
    // The console thread does nothing here, like a blocked std::getline.
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    const auto after = emu.registry().findByName("p01")->progress.executedInstructions;
    check(after > before, "progress advanced with no console input (" + std::to_string(before) + " -> " +
                              std::to_string(after) + ")");
    std::ostringstream screen;
    emu.console->executeLine("process-smi", screen);
    check(screen.str().find("PRINT LOGS") != std::string::npos, "process-smi shows the attached screen");
    emu.console->executeLine("exit", screen);
    const auto status = emu.console->runtimeStatus();
    check(status.scheduler == ServiceState::Running, "scheduler status reports Running");
}

void testThreadSafety() {
    test("Thread safety: snapshots stay consistent while the tick thread runs");
    Emulator emu(4, "rr", 1, 1);
    CpuScheduler scheduler(emu.config(), emu.registry());
    std::mt19937_64 rng(5);
    for (int i = 0; i < 16; ++i) {
        const auto program = generateProgram(50, 120, rng);
        scheduler.admit(emu.registry().createProcess("t" + std::to_string(i), program->processProgram(program)));
    }
    scheduler.start(std::chrono::milliseconds(0));
    bool consistent = true;
    std::atomic<bool> done{false};
    std::thread admitter([&] { // Admit more processes from another thread meanwhile.
        std::mt19937_64 local(9);
        for (int i = 0; i < 8; ++i) {
            const auto program = generateProgram(20, 40, local);
            scheduler.admit(emu.registry().createProcess("late" + std::to_string(i), program->processProgram(program)));
        }
        done = true;
    });
    for (int i = 0; i < 2000 && !(done && scheduler.idle()); ++i) {
        std::set<std::uint64_t> used;
        for (const auto& p : emu.registry().allSnapshots()) {
            if (p.state == ProcessState::RUNNING && (!p.assignedCore || !used.insert(*p.assignedCore).second))
                consistent = false;
            if (p.state != ProcessState::RUNNING && p.assignedCore) consistent = false;
            if (p.progress.executedInstructions > p.program.totalInstructions) consistent = false;
        }
        std::this_thread::sleep_for(std::chrono::microseconds(200));
    }
    admitter.join();
    for (int i = 0; i < 2000 && !scheduler.idle(); ++i) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    scheduler.stop();
    check(consistent, "no shared cores, no stray assignments, counters in range");
    check(scheduler.lastError().empty(), "tick thread had no errors: " + scheduler.lastError());
    checkEq(emu.registry().finishedSnapshots().size(), 24u, "all 24 processes finished");
}
} // namespace

int main() {
    std::cout << "Phase 3 - Process Instructions\n";
    testPrint();
    testDeclare();
    testAdd();
    testSubtract();
    testSleepInstruction();
    testForLoops();
    testResumeEquivalence();
    testParserRoundTrip();
    testSamplePrograms();
    testGenerator();

    std::cout << "\nPhase 4 - CPU Scheduler\n";
    testCores();
    testFcfsOneCore();
    testFcfsFourCores();
    testRoundRobin(1, 2, "p1 | p2 | p3 | p1 | p2 | p3 | p1 | p2 | p3 | p1 | p2 | p3");
    testRoundRobin(5, 4, "p1 | p1 | p1 | p1 | p1 | p2 | p2 | p2 | p2 | p2 | p3 | p3 | p3 | p3 | p3 | "
                         "p1 | p1 | p1 | p2 | p2 | p2 | p3 | p3 | p3");
    testRoundRobinCoreMigration();
    testSleepFreesCpu("fcfs");
    testSleepFreesCpu("rr");
    testBusyWait();
    testBackgroundExecution();
    testThreadSafety();

    std::cout << '\n' << (checks - failures) << " / " << checks << " checks passed\n";
    return failures == 0 ? 0 : 1;
}
