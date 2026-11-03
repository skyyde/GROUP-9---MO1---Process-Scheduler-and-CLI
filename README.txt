CSOPESY MAJOR OUTPUT 1 - PROCESS SCHEDULER AND CLI
================================================

Phase 1 is implemented: terminal console, command parsing, external configuration,
initialization, error handling and clean exit. Phases 2-8 remain unimplemented.
This project will eventually simulate a process multiplexer and multicore FCFS/RR
scheduler. Phase 1 does not create processes, execute instructions, schedule work,
start threads, generate reports or display invented CPU statistics.

The existing repository and its original README.md are preserved. There was no
existing source code, build configuration or professor specification in this
checkout. Validation follows the supplied Phase 1 checklist, including inclusive
2^32 limits. No repository replacement or file renaming was necessary.

FILE OWNERSHIP AND GROUP RESPONSIBILITIES
=======================================
Member   Phases   Assigned responsibilities and files
Kyla     1-2      Project setup, CLI, configuration, initialization; later processes.
                 src/main_Kyla.cpp, src/Config_Kyla.h/.cpp,
                 src/Console_Kyla.h/.cpp, tests/Phase1Tests_Kyla.cpp;
                 Phase 2: src/Process_Kyla.h/.cpp.
Enrique  3-4      Instructions and CPU scheduler.
                 src/Instruction_Enrique.h/.cpp, src/Scheduler_Enrique.h/.cpp.
Nicos    5-6      Screens and automatic process generation.
                 src/Screen_Nicos.h/.cpp, src/AutoGenerator_Nicos.h/.cpp.
Ced      7-8      CPU reporting, final integration, testing and submission.
                 src/Report_Ced.h/.cpp; Phase 8 integration/testing work later.

Kyla maintains config.txt, CMakeLists.txt, .gitignore and this README for Phase 1.
Shared project metadata keeps its standard filenames. Keep member suffixes on
source and test files. Each future-phase header/source identifies its owner,
phase, responsibility and planned features with TODO: NOT IMPLEMENTED YET.

FOLDER STRUCTURE
================
Repository root (keep the existing repository name)
  src/
    main_Kyla.cpp
    Config_Kyla.h / Config_Kyla.cpp
    Console_Kyla.h / Console_Kyla.cpp
    Process_Kyla.h / Process_Kyla.cpp                 [Phase 2 placeholder]
    Instruction_Enrique.h / Instruction_Enrique.cpp   [Phase 3 placeholder]
    Scheduler_Enrique.h / Scheduler_Enrique.cpp       [Phase 4 placeholder]
    Screen_Nicos.h / Screen_Nicos.cpp                [Phase 5 placeholder]
    AutoGenerator_Nicos.h / AutoGenerator_Nicos.cpp   [Phase 6 placeholder]
    Report_Ced.h / Report_Ced.cpp                    [Phase 7 placeholder]
  tests/Phase1Tests_Kyla.cpp
  config.txt
  README.txt
  README.md                                         [original, preserved]
  CMakeLists.txt
  .gitignore
  build/                                            [generated, ignored]

BUILD AND EXECUTION
===================
Requirements: a C++17 compiler and CMake 3.16 or newer. No external libraries or
test framework are required. Run commands from the repository root.

Windows: open an x64 Native Tools/Developer Command Prompt for Visual Studio with
the C++ build tools, CMake and Ninja installed, then run:

  cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
  cmake --build build
  ctest --test-dir build --output-on-failure -V
  .\build\csopesy.exe

If using a Visual Studio multi-configuration generator instead, use a separate
build directory and select the configuration for build, tests and execution:

  cmake -S . -B build-vs
  cmake --build build-vs --config Debug
  ctest --test-dir build-vs -C Debug --output-on-failure -V
  .\build-vs\Debug\csopesy.exe

Linux/macOS with a C++17 toolchain:

  cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
  cmake --build build
  ctest --test-dir build --output-on-failure -V
  ./build/csopesy

The runtime config.txt is read from the CURRENT WORKING DIRECTORY, not the
executable directory. Run from the repository root to use the supplied file.
When launching from an IDE, set the working directory to the repository root.
The build deliberately does not copy config.txt, avoiding stale build copies.
To run elsewhere, place your configuration in that working directory.

CMake builds phase1_Kyla from Config_Kyla.cpp and Console_Kyla.cpp. The csopesy
executable adds only main_Kyla.cpp. Future-phase placeholders are not compiled.
The test runner is a separate executable with its own main(), never linked into
csopesy. There is exactly one application main(): src/main_Kyla.cpp.
Use -DBUILD_TESTING=OFF when configuring an application-only build.

CONFIGURATION FORMAT
====================
One space-separated key/value pair per nonblank line, in any order. Leading and
trailing whitespace, tabs, blank lines, LF and CRLF are accepted. All seven keys
are mandatory. Keys and scheduler values are case-sensitive. Comments, unknown
keys, duplicate keys and extra values are rejected. Numeric values contain only
decimal digits (no sign, quotes, exponent notation, fractional part or suffix).
Scheduler values may be double-quoted or bare: "fcfs", "rr", fcfs, rr.

Sample config.txt:
  num-cpu 4
  scheduler "rr"
  quantum-cycles 5
  batch-process-freq 1
  min-ins 1000
  max-ins 2000
  delays-per-exec 0

Setting              Meaning                              Inclusive range
num-cpu              Logical CPU cores                    1-128
scheduler            Scheduling policy                    fcfs or rr
quantum-cycles       Round Robin time quantum             1-4294967296
batch-process-freq   CPU ticks between process generation 1-4294967296
min-ins              Minimum process instruction count   1-4294967296
max-ins              Maximum process instruction count   1-4294967296
delays-per-exec      CPU delay cycles between instructions 0-4294967296

min-ins must be <= max-ins. All seven settings are required even for FCFS.
4294967296 is 2^32; numeric fields use std::uint64_t. Parsing detects overflow
before range checks. There are no fallback defaults for missing/invalid values.
Edit config.txt and restart to change an initialized session's configuration;
no recompilation is required. Failed initialization can be retried after editing
the file without restarting.

COMMANDS AND INITIALIZATION
===========================
The heading appears immediately, followed by the root:\> prompt. Commands are
case-sensitive; input is read as full lines with std::getline. Leading/trailing
whitespace, multiple spaces, tabs and blank input are handled safely.

Command                    Phase 1 behavior
initialize                 Load/validate config.txt and initialize once.
exit                       End the main console normally, even before initialize.
screen -s <process_name>    Validate one name; later Nicos creates/attaches screens.
screen -r <process_name>    Validate one name; later Nicos resumes process screens.
screen -ls                 No arguments; later Ced lists utilization/processes.
scheduler-start            No arguments; later Nicos starts process generation.
scheduler-stop             No arguments; later Nicos stops process generation.
report-util                No arguments; later Ced writes the utilization report.

Process names are a single whitespace-delimited token. Phase 1 does not validate
name existence or implement quoted names containing spaces. initialize and exit
accept no arguments. Unknown commands and incorrect arguments report useful
errors and leave the CLI running. Invalid syntax is reported before checking
initialization, so an invalid command never runs a handler.

The application starts UNINITIALIZED. Only valid initialize and exit commands
operate in that state; operational commands say:
  Error: Please initialize the emulator first.

initialize reads every setting into a temporary validated Config before storing
it. Failure reports the setting/path, leaves no partial configuration and allows
another attempt. Success prints:
  Reading configuration...
  Configuration loaded successfully.
  CSOPESY emulator initialized.

Repeated initialize reports "CSOPESY emulator is already initialized." and never
reloads the file. After initialization, all six future operational commands say:
  Command recognized.
  This feature will be implemented in a later phase.

exit prints "Exiting CSOPESY Emulator..." and stops the loop. EOF also ends the
loop cleanly; a non-EOF input failure reports an error and stops without spinning.
No process-screen exit behavior is implemented in this phase.

PHASE 1 TESTING
===============
Build and run CTest using the commands above. To see the check count directly:
  .\build\phase1_tests.exe

Tests use standard C++ checks that remain active in release builds, plus temporary
configuration files in uniquely created directories. They do not edit the user's
config.txt. They cover startup/prompt, full-line input, whitespace, invalid syntax,
all eight command forms, initialization gating/retry/idempotence, actual file
loading, both policies, all seven fields, missing/duplicate/unknown keys, malformed
values, numeric overflow, every numeric limit, instruction ordering, configuration
changes across sessions, exit before/after initialization, EOF, stream read errors
and the future handler interface. Test handlers are test-only callbacks and do not
implement future-phase features. A failing check makes the test runner exit nonzero.

For a real executable smoke test in PowerShell, run from the repository root:
  @('screen -ls', 'initialize', 'screen -s test', 'screen -r test', 'screen -ls',
    'scheduler-start', 'scheduler-stop', 'report-util', 'screen -s',
    'unknown', 'initialize', 'exit') | .\build\csopesy.exe

Verify the heading/prompt, pre-initialization error, successful initialization,
honest later-phase responses, usage/unknown-command errors, repeated initialization
message and clean exit. A final integration/black-box submission suite belongs
to Ced's Phase 8 and is not part of this implementation.

INTEGRATION INSTRUCTIONS FOR GROUPMATES
======================================
Config_Kyla.h exposes csopesy::Config, SchedulerPolicy::{Fcfs, RoundRobin},
MaxConfigCycles, loadConfig(path) and parseConfig(stream). The parser is independent
of the console. The console stores a validated Config and exposes configuration()
as a const pointer: nullptr before success, stable read-only settings afterwards.
Do not retain that pointer beyond the Console object's lifetime. Config field
names: numCpu, scheduler, quantumCycles, batchProcessFreq, minIns, maxIns,
delaysPerExec. Future modules should consume the validated configuration instead
of re-reading config.txt or inventing defaults.

Console_Kyla.h exposes parseCommand(line), Command{type, processName, error}, and:
  console.registerHandler(CommandType, callable);
The callable has signature:
  void(const csopesy::Command&, const csopesy::Config&, std::ostream&)
Register each future handler during application wiring before console.run(). The
console performs syntax validation and initialization gating, then invokes that
command's handler instead of the later-phase message. Initialize/Exit/Empty/Invalid
cannot be overridden, and empty callables are rejected. Capture dependencies with
lifetimes that cover console.run(); use the provided output stream for CLI output.
Registration/console calls currently belong to the console thread. Future background
state and synchronization must be designed by the responsible members.

Kyla (Phase 2): implement Process_Kyla.h/.cpp, including the process registry and
state model. Coordinate its public interface with instruction/scheduler/screen
owners. Phase 1 supplies no fake process types.

Enrique (Phases 3-4): implement Instruction_Enrique.h/.cpp and Scheduler_Enrique.h/.cpp.
Use Config fields for CPU count, policy, quantum and execution delays. Coordinate
one-time scheduler startup after successful initialization with Kyla, and shutdown
on both exit and EOF. The scheduler must later run while console input waits.
There are currently no background resources or lifecycle hooks to manage.

Nicos (Phases 5-6): implement Screen_Nicos.h/.cpp and AutoGenerator_Nicos.h/.cpp.
Register CommandType::ScreenCreate, ScreenResume, SchedulerStart and SchedulerStop.
Use command.processName and validated config.batchProcessFreq. Add process-smi
and process-screen exit inside the future screen context. Stopping the generator
must eventually leave existing processes running.

Ced (Phase 7): implement Report_Ced.h/.cpp. Register CommandType::ScreenList and
ReportUtil, using real process/scheduler snapshots for utilization and reports.
Phase 8 later covers integration, FCFS/RR tests with 1/4/128 CPUs, configuration
variations, black-box cases, source/GitHub submission, presentation and MP4 demos.

As each phase becomes complete, explicitly add its .cpp files to the appropriate
CMake target and add meaningful tests. Do not glob all sources, add a second main
to csopesy, or wire incomplete placeholders into the active build. Keep ownership
suffixes and agree on shared interfaces before changing another member's module.
