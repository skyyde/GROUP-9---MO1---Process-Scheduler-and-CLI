/*
 * CSOPESY Major Output 1
 * Assigned Member: Enrique
 * Phase: 3 - Process Instructions
 * File responsibility: Future public interfaces for Process Instructions.
 *
 * Features to implement:
 * - PRINT(message), DECLARE(variable, value)
 * - ADD(var1, var2/value, var3/value), SUBTRACT(var1, var2/value, var3/value)
 * - SLEEP(X), FOR(instructions, repeats); nesting up to three levels
 * - Process-local uint16 variables; clamp arithmetic to 0-65535
 * - Initialize undeclared variables to zero where specified
 * - Preserve instruction progress; predetermined/randomized instruction generation
 *
 * TODO: IMPLEMENTED 
 * Reserved for Phase 3; excluded from the Phase 1 build.
 */
#pragma once

#include "Process_Kyla.h"

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace csopesy {

inline constexpr std::uint16_t MaxUint16 = 65535;
inline constexpr std::size_t MaxForNesting = 3;

class InstructionError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

enum class Opcode { Print, Declare, Add, Subtract, Sleep, For };

const char* opcodeName(Opcode opcode) noexcept;
std::uint16_t clampToUint16(long long value) noexcept;

struct Operand {
    enum class Kind { Variable, Literal };
    Kind kind = Kind::Literal;
    std::string name;
    std::uint16_t value = 0;

    static Operand variable(std::string name);
    static Operand literal(long long value);
    bool isVariable() const noexcept { return kind == Kind::Variable; }
};

struct Instruction {
    Opcode opcode = Opcode::Print;

    // PRINT: text + variable + suffix, or the default message.
    bool defaultMessage = false;
    std::string text;
    std::optional<std::string> printVariable;
    std::string suffix;

    // DECLARE / ADD / SUBTRACT
    std::string target;
    Operand first;
    Operand second;

    // SLEEP
    std::uint8_t sleepTicks = 0;

    // FOR
    std::vector<Instruction> body;
    std::uint64_t repeats = 0;

    static Instruction print();
    static Instruction print(std::string text);
    static Instruction print(std::string text, std::string variable, std::string suffix = {});
    static Instruction declare(std::string variable, long long value);
    static Instruction add(std::string target, Operand first, Operand second);
    static Instruction subtract(std::string target, Operand first, Operand second);
    static Instruction sleep(std::uint8_t ticks);
    static Instruction forLoop(std::vector<Instruction> body, std::uint64_t repeats);
};

struct ProgramSlot {
    Instruction instruction;
    std::uint64_t bodyEnd = 0; // FOR only
    std::uint64_t depth = 0;
};

class InstructionProgram {
public:
    static std::shared_ptr<const InstructionProgram> compile(const std::vector<Instruction>& program);

    std::uint64_t staticInstructionCount() const noexcept { return slots_.size(); }
    std::uint64_t totalInstructions() const noexcept { return total_; }
    const ProgramSlot& slot(std::uint64_t index) const { return slots_.at(index); }
    const std::vector<Instruction>& source() const noexcept { return source_; }
    ProcessProgram processProgram(std::shared_ptr<const InstructionProgram> self) const;
    std::string toText() const;

private:
    InstructionProgram() = default;
    std::vector<ProgramSlot> slots_;
    std::vector<Instruction> source_;
    std::uint64_t total_ = 0;
};

struct ExecutionState {
    InstructionProgress progress;
    std::map<std::string, std::uint16_t> variables;
};

struct StepResult {
    bool executed = false;
    Opcode opcode = Opcode::Print;
    std::optional<PrintLogEntry> log;
    std::vector<std::pair<std::string, std::uint16_t>> writes;
    std::optional<std::uint8_t> sleepTicks;
    bool finished = false;
};

void normalizePosition(const InstructionProgram& program, InstructionProgress& progress);
bool isFinished(const InstructionProgram& program, const InstructionProgress& progress) noexcept;

// Runs one counted instruction (one CPU cycle).
StepResult executeNext(const InstructionProgram& program, ExecutionState& state,
                       const std::string& processName, std::optional<std::uint64_t> coreId,
                       ProcessTime now);

// Fixed test programs
std::vector<Instruction> parseProgram(const std::string& text);
std::shared_ptr<const InstructionProgram> parseAndCompile(const std::string& text);
std::shared_ptr<const InstructionProgram> loadProgramFile(const std::string& path);

enum class GeneratorMode { Random, Predetermined };

// totalInstructions is between minIns and maxIns.
std::shared_ptr<const InstructionProgram> generateProgram(std::uint64_t minIns, std::uint64_t maxIns,
                                                          std::mt19937_64& rng,
                                                          GeneratorMode mode = GeneratorMode::Random);

} 
