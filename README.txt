CSOPESY MAJOR OUTPUT 1 - PROCESS SCHEDULER AND CLI
================================================

TEMPORARY RUN GUIDE FOR THE GROUP
See README.md for Windows setup, build commands and commands to try.

Phase 1 is implemented: terminal console, command parsing, external configuration,
initialization, error handling and clean exit. Phase 2 is also implemented: process
records, a synchronized registry, lifecycle operations and safe snapshots.
Phases 3-8 remain unimplemented.
This project will eventually simulate a process multiplexer and multicore FCFS/RR
scheduler. The CLI does not create processes, execute instructions, schedule work,
start threads, generate reports or display invented CPU statistics.

Configuration validation follows the Phase 1-2 checklist, including the inclusive
2^32 limit. The other members' modules are still placeholders for their phases.

FILE OWNERSHIP AND GROUP RESPONSIBILITIES
=======================================
Member   Phases   Assigned responsibilities and files
Kyla     1-2      Project setup, CLI, configuration, initialization; process representation.
                 src/main_Kyla.cpp, src/Config_Kyla.h/.cpp,
                 src/Console_Kyla.h/.cpp;
                 Phase 2: src/Process_Kyla.h/.cpp.
Enrique  3-4      Instructions and CPU scheduler.
                 src/Instruction_Enrique.h/.cpp, src/Scheduler_Enrique.h/.cpp.
Nicos    5-6      Screens and automatic process generation.
                 src/Screen_Nicos.h/.cpp, src/AutoGenerator_Nicos.h/.cpp.
Ced      7-8      CPU reporting, final integration, testing and submission.
                 src/Report_Ced.h/.cpp; Phase 8 integration/testing work later.

Kyla maintains config.txt, CMakeLists.txt, .gitignore and this README for Phases 1-2.
Shared project metadata keeps its standard filenames. Keep member suffixes on
source files. Each future-phase header/source identifies its owner,
phase, responsibility and planned features with TODO: NOT IMPLEMENTED YET.

FOLDER STRUCTURE
================
Repository root (keep the existing repository name)
  src/
    main_Kyla.cpp
    Config_Kyla.h / Config_Kyla.cpp
    Console_Kyla.h / Console_Kyla.cpp
    Process_Kyla.h / Process_Kyla.cpp                 [Phase 2 implemented]
    Instruction_Enrique.h / Instruction_Enrique.cpp   [Phase 3 placeholder]
    Scheduler_Enrique.h / Scheduler_Enrique.cpp       [Phase 4 placeholder]
    Screen_Nicos.h / Screen_Nicos.cpp                [Phase 5 placeholder]
    AutoGenerator_Nicos.h / AutoGenerator_Nicos.cpp   [Phase 6 placeholder]
    Report_Ced.h / Report_Ced.cpp                    [Phase 7 placeholder]
  config.txt
  README.txt
  README.md                                         [temporary run guide]
  CMakeLists.txt
  .gitignore
  build/                                            [generated, ignored]

BUILD AND EXECUTION
===================
Requirements: a C++17 compiler and CMake 3.16 or newer. Run commands from the repository root.

Windows: open an x64 Native Tools/Developer Command Prompt for Visual Studio with
the C++ build tools, CMake and Ninja installed, then run:

  cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
  cmake --build build
  .\build\csopesy.exe

If using a Visual Studio multi-configuration generator instead, use a separate
build directory and select the configuration for build and execution:

  cmake -S . -B build-vs
  cmake --build build-vs --config Debug
  .\build-vs\Debug\csopesy.exe

Linux/macOS with a C++17 toolchain:

  cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
  cmake --build build
  ./build/csopesy

The runtime config.txt is read from the CURRENT WORKING DIRECTORY, not the
executable directory. Run from the repository root to use the supplied file.
When launching from an IDE, set the working directory to the repository root.
The build deliberately does not copy config.txt, avoiding stale build copies.
To run elsewhere, place your configuration in that working directory.

CMake builds phase1_Kyla from Config_Kyla.cpp and Console_Kyla.cpp, linked to
phase2_Kyla (Process_Kyla.cpp and standard Threads::Threads). csopesy adds only
main_Kyla.cpp. Phase 3-8 placeholders are not compiled.
There is exactly one application main(): src/main_Kyla.cpp.

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

initialize validates a temporary Config, constructs an empty ProcessRegistry,
and publishes both on success. Failure leaves no partial configuration and allows
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

PHASE 2 PROCESS MODEL AND SERVICE
================================
After successful initialization, console.processRegistry() returns the service.
Both const and mutable access throw std::logic_error before initialization, even
after a failed attempt. Only Console can construct a registry, using its validated
Config. Repeated initialize preserves the same service, records and configuration.
Console operations belong to the console thread. Obtain the registry reference
after initialization, then publish it to future worker threads. Join all workers
before destroying Console; a service reference must not outlive its owner.

Process is a detached value snapshot containing PID/name, creation and finish
system-clock timestamps, state, optional current and last core IDs, program
metadata, instruction progress, uint16 variables, loop frames, PRINT logs and
sleep/quantum/execution-delay counters. Editing a copy cannot mutate live records.
Snapshots remain valid after completion or registry destruction. Historical
snapshots may retain old variables; completing a process clears its live variables,
not previously returned caller-owned copies.

All registry reads and writes use one mutex. No mutable record/container references
escape. Every operation is atomic; a sequence of separate API calls is not a
transaction. No user callbacks are invoked while holding the lock. A future worker
must exclusively own execution of its dispatched process: a read/compute/write
sequence for an instruction needs that Phase 4 ownership discipline. The registry
is noncopyable and owns its records with standard containers. Finished records
stay until registry destruction; there is intentionally no erase/reuse API.

PIDs are nonzero uint64 values allocated atomically across all registries in the
application process. They are never reused, including after registry destruction.
The maximum PID can be allocated once; subsequent requests throw ProcessError
instead of wrapping. Zero is reserved. Allocation failures can leave PID gaps.
PIDs restart with the application. The registry keeps its PID sequence private. Names are case-sensitive, 1-64 ASCII characters
from [A-Za-z0-9_-], including process01 and p01. Duplicate names, including finished
names, are rejected for the registry's entire lifetime. Names may repeat in a new
application session. The console accepts one token syntactically; future screen
creation must also apply this registry name policy.

createProcess(name, ProcessProgram) validates inputs, allocates identity/time,
initializes READY with zero counters and progress, and registers the record plus
name index under the same lock. Index allocation failure rolls back the record.
READY snapshots are non-consuming: new, released and awakened records remain
visible until dispatch or completion. Phase 4 must maintain its actual shared
READY queue, deduplicate admission and use dispatch() to revalidate stale entries.
The snapshot order is PID order, not an FCFS or Round Robin scheduling policy.
An empty/final READY process is visible for finalization; dispatch rejects it.
No scheduler, ready-queue drain, worker thread or timing loop is implemented here.

LEGAL LIFECYCLE AND CORE INVARIANTS
=================================
Creation -> READY
READY -> RUNNING                 dispatch(pid, coreId)
RUNNING -> READY                 releaseToReady(pid)
RUNNING -> SLEEPING              sleep(pid, ticks)
SLEEPING -> READY                setSleepTicks(pid, 0), then wake(pid)
RUNNING or READY -> FINISHED     complete(pid), only with final progress

READY/SLEEPING/FINISHED never own an active core. RUNNING owns exactly one core
from 0 through validated numCpu-1. dispatch rejects redispatch, an occupied core,
out-of-range cores and final progress. The single registry lock also prevents two
records from claiming the same core. This is registry bookkeeping, not a real CPU
worker/core table or global allocation across independent application sessions.
Release, sleep and finish clear the active core; lastCore preserves reporting
history. Sleep ticks are unsigned, externally supplied and cannot increase once
sleeping. wake requires zero ticks; sleep(0) can wake explicitly. No ticking occurs.

setExecutionCounters stores quantum and execution delay bounded by the configured
quantumCycles/delaysPerExec. Both start at zero; Phase 4 initializes/resets them when
appropriate. They survive release, sleep, wake and migration between cores.
Execution delay does not release a core. Sleep is separate from execution delay.
Completion clears all active counters and variables, stamps finishedAt, preserves
identity, creation time, final progress/program, lastCore and logs, and removes
active/READY eligibility by state. Repeated completion returns false without
changing the record. Unknown PIDs and other invalid mutations throw ProcessError;
missing lookups return std::nullopt. There is no generic unrestricted state setter.

INSTRUCTION PROGRAM, PROGRESS, VARIABLES AND LOGS
================================================
ProcessProgram contains shared_ptr<const csopesy::InstructionProgram>, a forward
declaration only; Enrique defines the program in Phase 3. It is NOT a competing
instruction class or interpreter. Phase 2 links without any Phase 3 definition.
Supply a real immutable program and metadata to createProcess, or reserve a record
with a null handle and known metadata, then attachProgram(pid, sharedProgram) once
while READY, before any progress/loop state. A null handle means explicitly pending
attachment. Phase 4 must not execute a record whose program handle is null.
Freeze a program after publication; do not retain a mutable alias and modify it
concurrently. Shared ownership keeps attached programs alive through snapshots.

staticInstructionCount is the number of addressable positions in a flattened
program. currentInstruction is a position in [0, staticInstructionCount], with the
upper value representing the end; it may move backwards for a loop. totalInstructions
is the expanded operation budget including loop repetitions. executedInstructions
is monotonic and must be <= totalInstructions; it may exceed the static count.
Phase 3 defines whether FOR control operations count and calculates totals using
that same rule. It must detect overflow when expanding nested loops. Phase 2 stores
uint64 counts without allocating instruction objects. Empty programs use zero for
both counts and can finish directly from READY. Nonempty programs require both
counts positive. Totals are fixed at creation; metadata must match the real program.
minIns/maxIns constrain future generation, not storage of explicit/empty programs.
No instruction count is randomly selected or hardcoded in the registry.

setProgress stores position, executed count and the full nested loop stack together.
Frames are outermost first and store [begin,end), next and remainingIterations.
Bounds/nesting and positive remaining iterations are checked. Phase 3 removes
finished frames and supplies its updated state; Phase 2 never advances a loop.
Completion requires executed == total, current position == static count and an
empty loop stack. This prevents incomplete records from being silently finalized.

setVariable(pid,name,value) stores/updates one process-local uint16 value. It rejects
empty names and values above 65535; variable(pid,name) returns an optional copied
value without implicitly declaring missing variables. DECLARE/ADD/SUBTRACT,
expression evaluation, variable-name grammar and arithmetic clamping are Phase 3.
Variables persist across active state changes and are cleared on completion.

appendLog(pid, PrintLogEntry{timestamp, optionalCoreId, message}) stores explicit
caller-supplied log data. Core IDs, if present, must be within configured bounds.
Append order is preserved even if wall-clock timestamps arrive out of order.
A supplied core may describe historical execution; it is not required to match the
current assignment. Logs do not print to the main console. Finished history is
readable but immutable. Display/report snapshots include ordered log entries.

FUTURE INTEGRATION GUIDE (EXPLANATIONS ONLY)
===========================================
Enrique - Phase 3:
  Define csopesy::InstructionProgram in your instruction module. Create immutable
  shared programs and calculate static/expanded counts; supply ProcessProgram to
  createProcess or attach a real handle to a record with matching metadata.
  Use variable/setVariable for uint16 storage, setProgress for position/count/loop
  context, and appendLog for PRINT results. Implement instructions only in your
  own files. A real program's attachment/execution cannot be exercised until that
  type exists; this stage verifies the compile-safe structure and metadata API.

Enrique - Phase 4:
  Obtain the initialized registry once. Admit readySnapshots into your own shared
  queue with deduplication; final-progress READY records should be completed.
  dispatch atomically checks state/core availability; catch ProcessError for stale
  queue entries. Use releaseToReady for release, sleep for blocking, setSleepTicks
  and wake after your ticks expire, setExecutionCounters for saved quantum/delay,
  and complete after final progress. Do not execute null program handles.
  Coordinate worker ownership across separate registry calls and join workers on
  exit/EOF before Console destruction. Implement algorithms, ticks and workers in
  Scheduler_Enrique files, using validated config values; none exist yet.

Nicos - Phase 5:
  findActiveByName supports attachment lookup; findByPid/findByName retain finished
  reporting records. Snapshots expose name/PID/state, progress, totals and logs.
  A screen already attached can keep its PID and display the final snapshot after
  completion. Refuse new reattachment when state is FINISHED. Add process-smi and
  screen-local exit in your future screen context; main-console commands stay
  deferred until you install their handlers.

Nicos - Phase 6:
  Use createProcess with your unique generated name and Enrique's real program.
  Duplicate-name ProcessError includes finished-name conflicts; never overwrite.
  Use validated minIns/maxIns and batchProcessFreq. Implement naming and generation
  timing in AutoGenerator_Nicos; scheduler-start/stop remain deferred now.

Ced - Phase 7:
  activeSnapshots includes READY/RUNNING/SLEEPING; filter state for RUNNING.
  finishedSnapshots supplies retained finished records. Both contain progress,
  program totals, creation/finish timestamps, current/last core and logs. These
  are storage views, not utilization percentages or live CPU statistics. Combine
  them with Phase 4's actual scheduler data to implement your reports later.

Phase 8 remains Ced's later full-system integration, algorithm tests and submission.
Add each completed module's .cpp files to CMake when it is ready to be integrated.
