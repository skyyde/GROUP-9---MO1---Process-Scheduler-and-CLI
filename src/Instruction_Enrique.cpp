/*
 * CSOPESY Major Output 1
 * Assigned Member: Enrique
 * Phase: 3 - Process Instructions
 * File responsibility: Future implementation for Process Instructions.
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

 // Phase 3: process instructions.
#include "Instruction_Enrique.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <limits>
#include <sstream>

namespace csopesy {
namespace {

bool validVariableName(const std::string& name) noexcept {
    if (name.empty() || name.size() > 64) return false;
    const auto first = static_cast<unsigned char>(name.front());
    if (!(std::isalpha(first) || name.front() == '_')) return false;
    for (const char c : name) {
        const auto u = static_cast<unsigned char>(c);
        if (!(std::isalnum(u) || c == '_')) return false;
    }
    return true;
}

void requireVariable(const std::string& name, const char* what) {
    if (!validVariableName(name)) {
        throw InstructionError(std::string(what) + " needs a variable name made of letters, digits or '_' "
                               "that does not start with a digit (got '" + name + "').");
    }
}

void requireOperand(const Operand& operand, const char* what) {
    if (operand.isVariable()) requireVariable(operand.name, what);
}

std::uint64_t checkedMultiply(std::uint64_t a, std::uint64_t b) {
    if (a != 0 && b > std::numeric_limits<std::uint64_t>::max() / a) {
        throw InstructionError("Program is too large: total instruction count overflows 64 bits.");
    }
    return a * b;
}

std::uint64_t checkedAdd(std::uint64_t a, std::uint64_t b) {
    if (b > std::numeric_limits<std::uint64_t>::max() - a) {
        throw InstructionError("Program is too large: total instruction count overflows 64 bits.");
    }
    return a + b;
}

std::uint64_t flatten(const std::vector<Instruction>& list, std::uint64_t depth,
                      std::vector<ProgramSlot>& slots) {
    std::uint64_t total = 0;
    for (const auto& instruction : list) {
        switch (instruction.opcode) {
        case Opcode::Print:
            if (instruction.printVariable) requireVariable(*instruction.printVariable, "PRINT");
            break;
        case Opcode::Declare:
            requireVariable(instruction.target, "DECLARE");
            requireOperand(instruction.first, "DECLARE");
            break;
        case Opcode::Add:
        case Opcode::Subtract:
            requireVariable(instruction.target, opcodeName(instruction.opcode));
            requireOperand(instruction.first, opcodeName(instruction.opcode));
            requireOperand(instruction.second, opcodeName(instruction.opcode));
            break;
        case Opcode::Sleep:
            break;
        case Opcode::For:
            if (depth >= MaxForNesting) {
                throw InstructionError("FOR loops can be nested at most " +
                                       std::to_string(MaxForNesting) + " levels deep.");
            }
            if (instruction.body.empty()) throw InstructionError("FOR needs at least one instruction in its body.");
            if (instruction.repeats == 0) throw InstructionError("FOR needs a repeat count of at least 1.");
            break;
        }

        ProgramSlot slot;
        slot.instruction = instruction;
        slot.instruction.body.clear();
        slot.depth = depth;
        if (instruction.opcode != Opcode::For) {
            slots.push_back(std::move(slot));
            total = checkedAdd(total, 1);
            continue;
        }
        const auto headerIndex = slots.size();
        slots.push_back(std::move(slot));
        const auto bodyTotal = flatten(instruction.body, depth + 1, slots);
        slots[headerIndex].bodyEnd = slots.size();
        total = checkedAdd(total, checkedMultiply(bodyTotal, instruction.repeats));
    }
    return total;
}

void refreshFrameCursors(InstructionProgress& progress) {
    auto& loops = progress.loops;
    for (std::size_t i = 0; i < loops.size(); ++i) {
        loops[i].next = i + 1 < loops.size() ? loops[i + 1].begin - 1 : progress.currentInstruction;
    }
}

std::string quote(const std::string& text) {
    std::string out = "\"";
    for (const char c : text) {
        if (c == '"' || c == '\\') out += '\\';
        out += c;
    }
    return out + '"';
}

std::string operandText(const Operand& operand) {
    return operand.isVariable() ? operand.name : std::to_string(operand.value);
}

void render(const std::vector<Instruction>& list, std::ostream& out, int indent, bool inside) {
    for (std::size_t i = 0; i < list.size(); ++i) {
        const auto& ins = list[i];
        out << std::string(static_cast<std::size_t>(indent) * 2, ' ');
        switch (ins.opcode) {
        case Opcode::Print:
            out << "PRINT(";
            if (!ins.defaultMessage) {
                bool needPlus = false;
                if (!ins.text.empty() || !ins.printVariable) { out << quote(ins.text); needPlus = true; }
                if (ins.printVariable) { out << (needPlus ? " + " : "") << *ins.printVariable; needPlus = true; }
                if (!ins.suffix.empty()) out << " + " << quote(ins.suffix);
            }
            out << ')';
            break;
        case Opcode::Declare:
            out << "DECLARE(" << ins.target << ", " << operandText(ins.first) << ')';
            break;
        case Opcode::Add:
        case Opcode::Subtract:
            out << opcodeName(ins.opcode) << '(' << ins.target << ", " << operandText(ins.first)
                << ", " << operandText(ins.second) << ')';
            break;
        case Opcode::Sleep:
            out << "SLEEP(" << static_cast<unsigned>(ins.sleepTicks) << ')';
            break;
        case Opcode::For:
            out << "FOR([\n";
            render(ins.body, out, indent + 1, true);
            out << std::string(static_cast<std::size_t>(indent) * 2, ' ') << "], " << ins.repeats << ')';
            break;
        }
        if (inside && i + 1 < list.size()) out << ',';
        out << '\n';
    }
}

class Parser {
public:
    explicit Parser(const std::string& text) : text_(text) {}

    std::vector<Instruction> program() {
        auto list = sequence(false);
        skipSpace();
        if (pos_ != text_.size()) fail("Unexpected '" + std::string(1, text_[pos_]) + "'.");
        return list;
    }

private:
    const std::string& text_;
    std::size_t pos_ = 0;

    [[noreturn]] void fail(const std::string& message) const {
        std::size_t line = 1, column = 1;
        for (std::size_t i = 0; i < pos_ && i < text_.size(); ++i) {
            if (text_[i] == '\n') { ++line; column = 1; } else ++column;
        }
        throw InstructionError("Program parse error at line " + std::to_string(line) + ", column " +
                               std::to_string(column) + ": " + message);
    }

    void skipSpace() {
        while (pos_ < text_.size()) {
            const auto c = static_cast<unsigned char>(text_[pos_]);
            if (std::isspace(c)) { ++pos_; continue; }
            if (text_[pos_] == '#') { while (pos_ < text_.size() && text_[pos_] != '\n') ++pos_; continue; }
            break;
        }
    }

    bool peek(char c) { skipSpace(); return pos_ < text_.size() && text_[pos_] == c; }
    bool accept(char c) { if (peek(c)) { ++pos_; return true; } return false; }
    void expect(char c) { if (!accept(c)) fail(std::string("Expected '") + c + "'."); }

    std::string identifier() {
        skipSpace();
        const auto start = pos_;
        while (pos_ < text_.size()) {
            const auto c = static_cast<unsigned char>(text_[pos_]);
            if (!(std::isalnum(c) || text_[pos_] == '_')) break;
            ++pos_;
        }
        if (start == pos_) fail("Expected a name.");
        return text_.substr(start, pos_ - start);
    }

    bool atNumber() {
        skipSpace();
        if (pos_ >= text_.size()) return false;
        const auto c = static_cast<unsigned char>(text_[pos_]);
        return std::isdigit(c) || text_[pos_] == '-' || text_[pos_] == '+';
    }

    long long integer() {
        skipSpace();
        bool negative = false;
        if (text_[pos_] == '-' || text_[pos_] == '+') { negative = text_[pos_] == '-'; ++pos_; }
        if (pos_ >= text_.size() || !std::isdigit(static_cast<unsigned char>(text_[pos_]))) fail("Expected a number.");
        long long value = 0;
        while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) {
            if (value < 1000000000000LL) value = value * 10 + (text_[pos_] - '0');
            ++pos_;
        }
        return negative ? -value : value;
    }

    std::uint64_t unsignedInteger(const char* what) {
        if (!atNumber()) fail(std::string(what) + " needs a whole number.");
        const auto start = pos_;
        const auto value = integer();
        if (value < 0) { pos_ = start; fail(std::string(what) + " cannot be negative."); }
        return static_cast<std::uint64_t>(value);
    }

    std::string stringLiteral() {
        expect('"');
        std::string out;
        while (pos_ < text_.size() && text_[pos_] != '"') {
            if (text_[pos_] == '\\' && pos_ + 1 < text_.size()) ++pos_;
            out += text_[pos_++];
        }
        if (pos_ >= text_.size()) fail("Unterminated string.");
        ++pos_;
        return out;
    }

    Operand operand() {
        if (atNumber()) return Operand::literal(integer());
        return Operand::variable(identifier());
    }

    Instruction printArguments() {
        if (peek(')')) return Instruction::print();
        std::string text, suffix;
        std::optional<std::string> variable;
        do {
            if (peek('"')) {
                (variable ? suffix : text) += stringLiteral();
            } else {
                if (variable) fail("PRINT can show only one variable.");
                variable = identifier();
            }
        } while (accept('+'));
        Instruction ins = variable ? Instruction::print(text, *variable, suffix) : Instruction::print(text);
        return ins;
    }

    Instruction instruction() {
        const auto name = identifier();
        std::string upper = name;
        std::transform(upper.begin(), upper.end(), upper.begin(),
                       [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        expect('(');
        Instruction ins;
        if (upper == "PRINT") {
            ins = printArguments();
        } else if (upper == "DECLARE") {
            const auto target = identifier();
            expect(',');
            if (!atNumber()) fail("DECLARE needs a number as its value.");
            ins = Instruction::declare(target, integer());
        } else if (upper == "ADD" || upper == "SUBTRACT") {
            const auto target = identifier();
            expect(',');
            const auto a = operand();
            expect(',');
            const auto b = operand();
            ins = upper == "ADD" ? Instruction::add(target, a, b) : Instruction::subtract(target, a, b);
        } else if (upper == "SLEEP") {
            const auto ticks = unsignedInteger("SLEEP");
            if (ticks > 255) fail("SLEEP takes a uint8 value (0-255).");
            ins = Instruction::sleep(static_cast<std::uint8_t>(ticks));
        } else if (upper == "FOR") {
            expect('[');
            auto body = sequence(true);
            expect(']');
            expect(',');
            ins = Instruction::forLoop(std::move(body), unsignedInteger("FOR repeat count"));
        } else {
            fail("Unknown instruction '" + name + "'.");
        }
        expect(')');
        return ins;
    }

    std::vector<Instruction> sequence(bool insideBrackets) {
        std::vector<Instruction> list;
        for (;;) {
            while (accept(',') || accept(';')) {}
            skipSpace();
            if (pos_ >= text_.size() || (insideBrackets && peek(']'))) break;
            list.push_back(instruction());
        }
        return list;
    }
};

const std::vector<std::string>& variablePool() {
    static const std::vector<std::string> pool{"x", "y", "z", "count", "total"};
    return pool;
}

std::uint64_t between(std::mt19937_64& rng, std::uint64_t low, std::uint64_t high) {
    return std::uniform_int_distribution<std::uint64_t>(low, high)(rng);
}

std::string pickVariable(std::mt19937_64& rng) {
    const auto& pool = variablePool();
    return pool[static_cast<std::size_t>(between(rng, 0, pool.size() - 1))];
}

Operand pickOperand(std::mt19937_64& rng) {
    if (between(rng, 0, 1) == 0) return Operand::variable(pickVariable(rng));
    return Operand::literal(static_cast<long long>(between(rng, 0, 9) == 0 ? between(rng, 0, MaxUint16)
                                                                             : between(rng, 0, 100)));
}

Instruction randomLeaf(std::mt19937_64& rng) {
    const auto roll = between(rng, 1, 100);
    if (roll <= 30) {
        switch (between(rng, 0, 2)) {
        case 0: return Instruction::print();
        case 1: return Instruction::print("Hello from a generated program");
        default: { const auto v = pickVariable(rng); return Instruction::print("Value from " + v + ": ", v); }
        }
    }
    if (roll <= 45) return Instruction::declare(pickVariable(rng), static_cast<long long>(between(rng, 0, 1000)));
    if (roll <= 70) return Instruction::add(pickVariable(rng), pickOperand(rng), pickOperand(rng));
    if (roll <= 95) return Instruction::subtract(pickVariable(rng), pickOperand(rng), pickOperand(rng));
    const auto ticks = between(rng, 0, 9) == 0 ? between(rng, 0, 255) : between(rng, 1, 10);
    return Instruction::sleep(static_cast<std::uint8_t>(ticks));
}

// Builds instructions whose counted total is exactly `budget`.
std::vector<Instruction> randomBlock(std::uint64_t budget, std::uint64_t depth, std::mt19937_64& rng) {
    std::vector<Instruction> list;
    std::uint64_t remaining = budget;
    while (remaining > 0) {
        const bool canLoop = depth < MaxForNesting && remaining >= 2;
        if (canLoop && between(rng, 1, 100) <= 15) {
            const std::uint64_t repeatCap = remaining > 1000000 ? 1000 : 10;
            const auto repeats = between(rng, 2, std::min(repeatCap, remaining));
            const auto bodyCap = std::min<std::uint64_t>(remaining / repeats, remaining > 1000000 ? remaining : 8);
            const auto bodyBudget = between(rng, 1, std::max<std::uint64_t>(1, bodyCap));
            list.push_back(Instruction::forLoop(randomBlock(bodyBudget, depth + 1, rng), repeats));
            remaining -= repeats * bodyBudget;
        } else {
            list.push_back(randomLeaf(rng));
            --remaining;
        }
    }
    return list;
}

std::vector<Instruction> predeterminedProgram(std::uint64_t total) {
    std::vector<Instruction> list{Instruction::declare("x", 0)};
    std::uint64_t remaining = total - 1;
    if (remaining >= 2) {
        list.push_back(Instruction::forLoop({Instruction::print("Value from: ", "x"),
                                             Instruction::add("x", Operand::variable("x"), Operand::literal(1))},
                                            remaining / 2));
        remaining %= 2;
    }
    if (remaining == 1) list.push_back(Instruction::print("Value from: ", "x"));
    return list;
}

}

const char* opcodeName(Opcode opcode) noexcept {
    switch (opcode) {
    case Opcode::Print: return "PRINT";
    case Opcode::Declare: return "DECLARE";
    case Opcode::Add: return "ADD";
    case Opcode::Subtract: return "SUBTRACT";
    case Opcode::Sleep: return "SLEEP";
    case Opcode::For: return "FOR";
    }
    return "UNKNOWN";
}

std::uint16_t clampToUint16(long long value) noexcept {
    if (value < 0) return 0;
    if (value > MaxUint16) return MaxUint16;
    return static_cast<std::uint16_t>(value);
}

Operand Operand::variable(std::string name) {
    Operand operand;
    operand.kind = Kind::Variable;
    operand.name = std::move(name);
    return operand;
}

Operand Operand::literal(long long value) {
    Operand operand;
    operand.kind = Kind::Literal;
    operand.value = clampToUint16(value);
    return operand;
}

Instruction Instruction::print() {
    Instruction ins;
    ins.opcode = Opcode::Print;
    ins.defaultMessage = true;
    return ins;
}

Instruction Instruction::print(std::string text) {
    Instruction ins;
    ins.opcode = Opcode::Print;
    ins.text = std::move(text);
    return ins;
}

Instruction Instruction::print(std::string text, std::string variable, std::string suffix) {
    Instruction ins = print(std::move(text));
    ins.printVariable = std::move(variable);
    ins.suffix = std::move(suffix);
    return ins;
}

Instruction Instruction::declare(std::string variable, long long value) {
    Instruction ins;
    ins.opcode = Opcode::Declare;
    ins.target = std::move(variable);
    ins.first = Operand::literal(value);
    return ins;
}

Instruction Instruction::add(std::string target, Operand first, Operand second) {
    Instruction ins;
    ins.opcode = Opcode::Add;
    ins.target = std::move(target);
    ins.first = std::move(first);
    ins.second = std::move(second);
    return ins;
}

Instruction Instruction::subtract(std::string target, Operand first, Operand second) {
    Instruction ins = add(std::move(target), std::move(first), std::move(second));
    ins.opcode = Opcode::Subtract;
    return ins;
}

Instruction Instruction::sleep(std::uint8_t ticks) {
    Instruction ins;
    ins.opcode = Opcode::Sleep;
    ins.sleepTicks = ticks;
    return ins;
}

Instruction Instruction::forLoop(std::vector<Instruction> body, std::uint64_t repeats) {
    Instruction ins;
    ins.opcode = Opcode::For;
    ins.body = std::move(body);
    ins.repeats = repeats;
    return ins;
}

std::shared_ptr<const InstructionProgram> InstructionProgram::compile(const std::vector<Instruction>& program) {
    std::shared_ptr<InstructionProgram> compiled(new InstructionProgram());
    compiled->total_ = flatten(program, 0, compiled->slots_);
    compiled->source_ = program;
    return compiled;
}

ProcessProgram InstructionProgram::processProgram(std::shared_ptr<const InstructionProgram> self) const {
    if (self.get() != this) throw InstructionError("processProgram needs the shared handle of this program.");
    ProcessProgram result;
    result.handle = std::move(self);
    result.staticInstructionCount = staticInstructionCount();
    result.totalInstructions = totalInstructions();
    return result;
}

std::string InstructionProgram::toText() const {
    std::ostringstream out;
    render(source_, out, 0, false);
    return out.str();
}

void normalizePosition(const InstructionProgram& program, InstructionProgress& progress) {
    auto& loops = progress.loops;
    auto& position = progress.currentInstruction;
    for (;;) {
        // End of loop body: repeat or leave.
        if (!loops.empty() && position == loops.back().end) {
            auto& frame = loops.back();
            if (frame.remainingIterations > 1) {
                --frame.remainingIterations;
                position = frame.begin;
            } else {
                loops.pop_back();
            }
            continue;
        }
        // FOR header: enter the loop (no CPU cycle).
        if (position < program.staticInstructionCount() &&
            program.slot(position).instruction.opcode == Opcode::For) {
            const auto& slot = program.slot(position);
            loops.push_back(LoopFrame{position + 1, slot.bodyEnd, position + 1, slot.instruction.repeats});
            position += 1;
            continue;
        }
        break;
    }
    refreshFrameCursors(progress);
}

bool isFinished(const InstructionProgram& program, const InstructionProgress& progress) noexcept {
    return progress.loops.empty() && progress.currentInstruction >= program.staticInstructionCount();
}

StepResult executeNext(const InstructionProgram& program, ExecutionState& state,
                       const std::string& processName, std::optional<std::uint64_t> coreId,
                       ProcessTime now) {
    StepResult result;
    normalizePosition(program, state.progress);
    if (isFinished(program, state.progress)) {
        result.finished = true;
        return result;
    }

    // Undeclared variables are created with 0.
    auto read = [&](const Operand& operand) -> long long {
        if (!operand.isVariable()) return operand.value;
        auto found = state.variables.find(operand.name);
        if (found == state.variables.end()) {
            found = state.variables.emplace(operand.name, 0).first;
            result.writes.emplace_back(operand.name, 0);
        }
        return found->second;
    };
    // Results are clamped to 0-65535.
    auto write = [&](const std::string& name, long long value) {
        const auto clamped = clampToUint16(value);
        state.variables[name] = clamped;
        result.writes.emplace_back(name, clamped);
    };

    const auto& ins = program.slot(state.progress.currentInstruction).instruction;
    result.opcode = ins.opcode;
    switch (ins.opcode) {
    case Opcode::Print: {
        std::string message;
        if (ins.defaultMessage) {
            message = "Hello world from " + processName + "!";
        } else {
            message = ins.text;
            if (ins.printVariable) message += std::to_string(read(Operand::variable(*ins.printVariable)));
            message += ins.suffix;
        }
        result.log = PrintLogEntry{now, coreId, std::move(message)};
        break;
    }
    case Opcode::Declare:
        write(ins.target, read(ins.first));
        break;
    case Opcode::Add: {
        const auto a = read(ins.first);
        const auto b = read(ins.second);
        write(ins.target, a + b);
        break;
    }
    case Opcode::Subtract: {
        const auto a = read(ins.first);
        const auto b = read(ins.second);
        write(ins.target, a - b);
        break;
    }
    case Opcode::Sleep:
        result.sleepTicks = ins.sleepTicks;
        break;
    case Opcode::For:
        throw InstructionError("Internal error: executor stopped on a FOR header.");
    }

    state.progress.currentInstruction += 1;
    state.progress.executedInstructions += 1;
    result.executed = true;
    normalizePosition(program, state.progress);
    result.finished = isFinished(program, state.progress);
    return result;
}

std::vector<Instruction> parseProgram(const std::string& text) {
    return Parser(text).program();
}

std::shared_ptr<const InstructionProgram> parseAndCompile(const std::string& text) {
    return InstructionProgram::compile(parseProgram(text));
}

std::shared_ptr<const InstructionProgram> loadProgramFile(const std::string& path) {
    std::ifstream file(path);
    if (!file) throw InstructionError("Cannot open program file '" + path + "'.");
    std::ostringstream text;
    text << file.rdbuf();
    try {
        return parseAndCompile(text.str());
    } catch (const InstructionError& error) {
        throw InstructionError(path + ": " + error.what());
    }
}

std::shared_ptr<const InstructionProgram> generateProgram(std::uint64_t minIns, std::uint64_t maxIns,
                                                          std::mt19937_64& rng, GeneratorMode mode) {
    if (minIns == 0 || minIns > maxIns) throw InstructionError("Generator needs 1 <= min-ins <= max-ins.");
    const auto total = between(rng, minIns, maxIns);
    auto program = InstructionProgram::compile(mode == GeneratorMode::Predetermined
                                                   ? predeterminedProgram(total)
                                                   : randomBlock(total, 0, rng));
    if (program->totalInstructions() != total) {
        throw InstructionError("Internal error: generated program has the wrong length.");
    }
    return program;
}

}

