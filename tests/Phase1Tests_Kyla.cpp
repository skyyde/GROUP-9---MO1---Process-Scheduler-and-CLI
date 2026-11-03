#include "Config_Kyla.h"
#include "Console_Kyla.h"

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace {
using namespace csopesy;
int passed = 0;
int failed = 0;

const std::string Sample =
    "num-cpu 4\nscheduler \"rr\"\nquantum-cycles 5\nbatch-process-freq 1\n"
    "min-ins 1000\nmax-ins 2000\ndelays-per-exec 0\n";

void check(bool condition, const std::string& name) {
    if (condition) {
        ++passed;
    } else {
        ++failed;
        std::cerr << "FAIL: " << name << '\n';
    }
}

bool contains(const std::string& text, const std::string& fragment) {
    return text.find(fragment) != std::string::npos;
}

std::string setting(const std::string& key, const std::string& value,
                    std::string text = Sample) {
    const auto start = text.find(key + " ");
    if (start == std::string::npos) {
        throw std::runtime_error("Test fixture has no setting: " + key);
    }
    const auto end = text.find('\n', start);
    text.replace(start, end - start, key + " " + value);
    return text;
}

Config parse(const std::string& text) {
    std::istringstream input(text);
    return parseConfig(input);
}

void rejects(const std::string& text, const std::string& expected, const std::string& name) {
    try {
        (void)parse(text);
        check(false, name + " should reject");
    } catch (const ConfigError& error) {
        check(contains(error.what(), expected), name + " identifies " + expected);
    }
}

// Tests only touch a uniquely created directory, never the user's config.txt.
class Fixture {
public:
    std::filesystem::path directory;
    std::filesystem::path config;

    Fixture() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        for (int attempt = 0; attempt < 100; ++attempt) {
            directory = std::filesystem::temp_directory_path() /
                ("csopesy-phase1-Kyla-" + std::to_string(stamp) + "-" + std::to_string(attempt));
            if (std::filesystem::create_directory(directory)) {
                config = directory / "config.txt";
                return;
            }
        }
        throw std::runtime_error("Cannot create test fixture directory.");
    }

    ~Fixture() {
        std::error_code ignored;
        // Only remove the exact file and empty directory that this fixture owns.
        std::filesystem::remove(config, ignored);
        std::filesystem::remove(directory, ignored);
    }

    void write(const std::string& text) const {
        std::ofstream file(config);
        file << text;
        file.close();
        if (!file) {
            throw std::runtime_error("Cannot write test configuration.");
        }
    }
};

void configTests() {
    const auto config = parse(Sample);
    check(config.numCpu == 4 && config.scheduler == SchedulerPolicy::RoundRobin &&
          config.quantumCycles == 5 && config.batchProcessFreq == 1 &&
          config.minIns == 1000 && config.maxIns == 2000 && config.delaysPerExec == 0,
          "all seven RR configuration values");
    check(parse(setting("scheduler", "\"fcfs\"")).scheduler == SchedulerPolicy::Fcfs,
          "quoted FCFS");
    check(parse(setting("scheduler", "rr")).scheduler == SchedulerPolicy::RoundRobin,
          "bare RR");
    check(parse(setting("scheduler", "fcfs")).scheduler == SchedulerPolicy::Fcfs,
          "bare FCFS");
    check(parse("\n \t\n" + setting("num-cpu", "\t 4 \t") + " \n").numCpu == 4,
          "blank lines and whitespace");
    std::string crlf;
    for (const char character : Sample) {
        if (character == '\n') crlf += '\r';
        crlf += character;
    }
    check(parse(crlf).maxIns == 2000, "CRLF configuration");
    check(parse(Sample.substr(0, Sample.size() - 1)).maxIns == 2000, "no final newline");
    check(parse("delays-per-exec 0\nmax-ins 2000\nmin-ins 1000\nbatch-process-freq 1\n"
                "quantum-cycles 5\nscheduler \"rr\"\nnum-cpu 4\n").numCpu == 4,
          "arbitrary key order");

    const std::array<std::string, 7> keys{
        "num-cpu", "scheduler", "quantum-cycles", "batch-process-freq",
        "min-ins", "max-ins", "delays-per-exec"
    };
    for (const auto& key : keys) {
        auto missing = Sample;
        const auto start = missing.find(key + " ");
        missing.erase(start, missing.find('\n', start) - start + 1);
        rejects(missing, key, "missing " + key);
        const auto value = key == "scheduler" ? "\"rr\"" : "1";
        rejects(Sample + key + " " + value + "\n", key, "duplicate " + key);
        rejects(setting(key, ""), key, "empty " + key);
        rejects(setting(key, "1 extra"), key, "extra token " + key);
    }
    for (const auto* bad : {"\"priority\"", "\"RR\"", "\"rr", "rr\"", "\"\"", "\"rr\"junk"}) {
        rejects(setting("scheduler", bad), "scheduler", "invalid scheduler");
    }
    rejects(Sample + "unknown 1\n", "unknown", "unknown setting");
    rejects("", "num-cpu", "empty configuration");
    rejects(setting("min-ins", "2001"), "min-ins", "reversed instruction range");

    for (const auto& key : keys) {
        if (key == "scheduler") continue;
        for (const auto* bad : {"-1", "+1", "1.5", "1e2", "abc", "12x", "0x10",
                               "\"4\"", "18446744073709551616",
                               "999999999999999999999999999999999999999"}) {
            rejects(setting(key, bad), key, "invalid numeric " + key + "=" + bad);
        }
        rejects(setting(key, key == "num-cpu" ? "129" : "4294967297"), key,
                "above upper bound " + key);
        if (key != "delays-per-exec") {
            rejects(setting(key, "0"), key, "below lower bound " + key);
        }
    }

    const auto minimum = parse("num-cpu 1\nscheduler \"fcfs\"\nquantum-cycles 1\n"
        "batch-process-freq 1\nmin-ins 1\nmax-ins 1\ndelays-per-exec 0\n");
    check(minimum.numCpu == 1 && minimum.quantumCycles == 1 && minimum.batchProcessFreq == 1 &&
          minimum.minIns == 1 && minimum.maxIns == 1 && minimum.delaysPerExec == 0,
          "all lower boundaries, including min-ins equals max-ins");
    const auto maximum = parse("num-cpu 128\nscheduler \"rr\"\nquantum-cycles 4294967296\n"
        "batch-process-freq 4294967296\nmin-ins 4294967296\nmax-ins 4294967296\n"
        "delays-per-exec 4294967296\n");
    check(maximum.numCpu == 128 && maximum.quantumCycles == MaxConfigCycles &&
          maximum.batchProcessFreq == MaxConfigCycles && maximum.minIns == MaxConfigCycles &&
          maximum.maxIns == MaxConfigCycles && maximum.delaysPerExec == MaxConfigCycles,
          "all inclusive upper boundaries use 64-bit storage");

    std::istringstream broken(Sample);
    broken.setstate(std::ios::badbit);
    try {
        (void)parseConfig(broken);
        check(false, "unreadable configuration stream");
    } catch (const ConfigError& error) {
        check(contains(error.what(), "read"), "unreadable configuration stream");
    }
    Fixture fixture;
    try {
        (void)loadConfig(fixture.config.string());
        check(false, "missing file");
    } catch (const ConfigError& error) {
        check(contains(error.what(), fixture.config.string()), "missing file identifies path");
    }
    fixture.write(Sample);
    check(loadConfig(fixture.config.string()).numCpu == 4, "external configuration file");
}

const std::array<std::pair<const char*, CommandType>, 6> Operational{{
    {"screen -s test", CommandType::ScreenCreate}, {"screen -r test", CommandType::ScreenResume},
    {"screen -ls", CommandType::ScreenList}, {"scheduler-start", CommandType::SchedulerStart},
    {"scheduler-stop", CommandType::SchedulerStop}, {"report-util", CommandType::ReportUtil}
}};

void commandTests() {
    check(parseCommand("initialize").type == CommandType::Initialize, "parse initialize");
    check(parseCommand("exit").type == CommandType::Exit, "parse exit");
    check(parseCommand(" \t\r ").type == CommandType::Empty, "parse whitespace");
    for (const auto& entry : Operational) {
        check(parseCommand(entry.first).type == entry.second, std::string("parse ") + entry.first);
    }
    const auto spaced = parseCommand(" \t screen   -s \t process01  \r");
    check(spaced.type == CommandType::ScreenCreate && spaced.processName == "process01",
          "full line whitespace and process argument");
    check(parseCommand("screen -r nonexistent").processName == "nonexistent",
          "no process existence checks");
    for (const auto* invalid : {"screen", "screen -s", "screen -r", "screen -ls test",
                               "screen -x test", "screen -s one two", "screen -r one two",
                               "initialize hello", "exit extra", "scheduler-start test",
                               "scheduler-stop test", "report-util extra", "bogus", "INITIALIZE"}) {
        const auto command = parseCommand(invalid);
        check(command.type == CommandType::Invalid && !command.error.empty(),
              std::string("reject command ") + invalid);
    }
}

std::string execute(Console& console, const std::string& line) {
    std::ostringstream output;
    check(console.executeLine(line, output), "continues after " + line);
    return output.str();
}

void consoleTests() {
    Fixture fixture;
    Console console(fixture.config.string());
    check(!console.isInitialized() && console.configuration() == nullptr, "starts uninitialized");
    for (const auto& entry : Operational) {
        check(contains(execute(console, entry.first), "Please initialize"),
              std::string("blocked before initialization: ") + entry.first);
    }
    const auto missing = execute(console, "initialize");
    check(contains(missing, "Error:") && !contains(missing, "successfully") &&
          !console.isInitialized() && console.configuration() == nullptr,
          "missing configuration leaves no partial state");
    fixture.write(setting("delays-per-exec", "-1"));
    const auto invalid = execute(console, "initialize");
    check(contains(invalid, "delays-per-exec") && !contains(invalid, "emulator initialized") &&
          !console.isInitialized() && console.configuration() == nullptr,
          "late validation error leaves no partial state");
    check(contains(execute(console, "screen -ls"), "Please initialize"), "still gated after failure");
    fixture.write(Sample);
    const auto initialized = execute(console, "initialize");
    check(contains(initialized, "Configuration loaded successfully.") &&
          contains(initialized, "CSOPESY emulator initialized.") && console.isInitialized() &&
          console.configuration() && console.configuration()->numCpu == 4,
          "retry succeeds after file correction");

    for (const auto& entry : Operational) {
        const auto output = execute(console, entry.first);
        check(output == "Command recognized.\nThis feature will be implemented in a later phase.\n",
              std::string("honest placeholder response: ") + entry.first);
    }
    fixture.write(setting("num-cpu", "8"));
    check(contains(execute(console, "initialize"), "already initialized") &&
          console.configuration()->numCpu == 4, "repeat initialize does not reload changed file");
    Console restarted(fixture.config.string());
    (void)execute(restarted, "initialize");
    check(restarted.configuration() && restarted.configuration()->numCpu == 8,
          "new session loads changed file without recompilation");
    fixture.write("broken");
    check(contains(execute(console, "initialize"), "already initialized") &&
          console.configuration()->numCpu == 4, "repeat initialize does not read invalid file");
    check(contains(execute(console, "exit extra"), "Usage:"), "invalid exit keeps loop alive");
    check(contains(execute(console, "nonsense"), "Unknown command"), "unknown command error");
    check(execute(console, " \t ").empty(), "blank command harmless");

    std::ostringstream exitOutput;
    check(!console.executeLine("exit", exitOutput) && contains(exitOutput.str(), "Exiting"),
          "exit after initialization");
    Console fresh(fixture.config.string());
    check(!fresh.executeLine("exit", exitOutput), "exit before initialization");

    std::istringstream input("\nunknown\nscreen -s\n  screen   -s   process01  \nexit extra\nexit\ninitialize\n");
    std::ostringstream output;
    fresh.run(input, output);
    const auto transcript = output.str();
    check(contains(transcript, "C S O P E S Y") &&
          contains(transcript, "Kyla | Enrique | Nicos | Ced") &&
          contains(transcript, "root:\\>"), "startup heading, group and root prompt");
    check(contains(transcript, "Unknown command") && contains(transcript, "Usage: screen") &&
          contains(transcript, "Please initialize") && contains(transcript, "Usage: exit") &&
          contains(transcript, "Exiting"), "input loop continues through blanks and invalid lines");
    check(!contains(transcript, "Reading configuration"), "exit stops before trailing commands");

    for (const auto* text : {"", "\n", "unknown"}) {
        std::istringstream eofInput(text);
        std::ostringstream eofOutput;
        fresh.run(eofInput, eofOutput);
        check(contains(eofOutput.str(), "root:\\>") && !contains(eofOutput.str(), "Unable to read"),
              "EOF terminates cleanly");
    }
    std::istringstream badInput;
    badInput.setstate(std::ios::badbit);
    std::ostringstream badOutput;
    fresh.run(badInput, badOutput);
    check(contains(badOutput.str(), "Unable to read console input"), "failed input terminates loop");

    fixture.write(Sample);
    std::istringstream fullInput("initialize\nscreen -ls\ninitialize\nexit\n");
    std::ostringstream fullOutput;
    Console fullSession(fixture.config.string());
    fullSession.run(fullInput, fullOutput);
    check(contains(fullOutput.str(), "emulator initialized.") &&
          contains(fullOutput.str(), "later phase") && contains(fullOutput.str(), "already initialized") &&
          contains(fullOutput.str(), "Exiting"), "complete initialized input loop");
}

void integrationInterfaceTests() {
    Fixture fixture;
    fixture.write(Sample);
    Console console(fixture.config.string());
    int calls = 0;
    console.registerHandler(CommandType::ScreenCreate,
        [&calls](const Command& command, const Config& config, std::ostream& output) {
            ++calls;
            check(command.processName == "test" && config.numCpu == 4,
                  "handler receives parsed command and validated configuration");
            output << "Test handler called.\n";
        });
    (void)execute(console, "screen -s test");
    check(calls == 0, "registered handler blocked before initialization");
    (void)execute(console, "initialize");
    (void)execute(console, "screen -s");
    check(calls == 0, "invalid syntax never calls handler");
    check(execute(console, "screen -s test") == "Test handler called.\n" && calls == 1,
          "registered handler replaces placeholder response");
    for (const auto type : {CommandType::Initialize, CommandType::Exit, CommandType::Empty,
                            CommandType::Invalid}) {
        try {
            console.registerHandler(type, [](const Command&, const Config&, std::ostream&) {});
            check(false, "protect built-in command");
        } catch (const std::invalid_argument&) {
            check(true, "protect built-in command");
        }
    }
    try {
        console.registerHandler(CommandType::ReportUtil, {});
        check(false, "empty handler rejected");
    } catch (const std::invalid_argument&) {
        check(true, "empty handler rejected");
    }
}
} // namespace

// Separate test executable; this is never part of the csopesy application target.
int main() {
    try {
        configTests();
        commandTests();
        consoleTests();
        integrationInterfaceTests();
    } catch (const std::exception& error) {
        check(false, std::string("Unexpected test exception: ") + error.what());
    }
    std::cout << "Phase 1: " << passed << " checks passed, " << failed << " failed.\n";
    return failed == 0 ? 0 : 1;
}
