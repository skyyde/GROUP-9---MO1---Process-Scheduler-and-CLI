// Kyla - Phase 1: starts the main console.
#include "Console_Kyla.h"

#include <iostream>
#include <cstdlib>
#include <optional>
#include <string>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace {
std::optional<std::string> environment(const char* name) {
#ifdef _MSC_VER
    std::size_t size = 0;
    if (getenv_s(&size, nullptr, 0, name) != 0 || size == 0) return std::nullopt;
    std::string value(size, '\0');
    if (getenv_s(&size, value.data(), value.size(), name) != 0) return std::nullopt;
    value.resize(size ? size - 1 : 0);
    return value;
#else
    const auto value = std::getenv(name);
    return value ? std::optional<std::string>(value) : std::nullopt;
#endif
}

// Restore console settings on exit; redirected output stays plain text.
class TerminalSession {
public:
    TerminalSession() {
        const auto term = environment("TERM");
        bool color = interactive() && !environment("NO_COLOR") &&
                     (!term || *term != "dumb");
#ifdef _WIN32
        inputPage_ = GetConsoleCP();
        outputPage_ = GetConsoleOutputCP();
        if (inputPage_) SetConsoleCP(CP_UTF8);
        if (outputPage_) SetConsoleOutputCP(CP_UTF8);
        if (color) {
            const auto handle = GetStdHandle(STD_OUTPUT_HANDLE);
            restoreMode_ = GetConsoleMode(handle, &outputMode_) &&
                SetConsoleMode(handle, outputMode_ | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
            color = restoreMode_;
        }
#endif
        csopesy::terminal::enableColor(std::cout, color);
    }
    ~TerminalSession() {
#ifdef _WIN32
        if (restoreMode_) SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), outputMode_);
        if (inputPage_) SetConsoleCP(inputPage_);
        if (outputPage_) SetConsoleOutputCP(outputPage_);
#endif
        csopesy::terminal::enableColor(std::cout, false);
    }
    static bool interactive() {
#ifdef _WIN32
        DWORD mode;
        return GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), &mode) &&
               GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &mode);
#else
        return isatty(STDIN_FILENO) && isatty(STDOUT_FILENO);
#endif
    }
    static void clear() {
#ifdef _WIN32
        const auto handle = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO info;
        if (!GetConsoleScreenBufferInfo(handle, &info)) return;
        const COORD origin{0, 0};
        const auto cells = static_cast<DWORD>(info.dwSize.X) * info.dwSize.Y;
        DWORD written;
        FillConsoleOutputCharacterW(handle, L' ', cells, origin, &written);
        FillConsoleOutputAttribute(handle, info.wAttributes, cells, origin, &written);
        SetConsoleCursorPosition(handle, origin);
#else
        std::cout << "\033[2J\033[H" << std::flush;
#endif
    }
private:
#ifdef _WIN32
    UINT inputPage_, outputPage_;
    DWORD outputMode_ = 0;
    bool restoreMode_ = false;
#endif
};
}

int main() {
    try {
        [[maybe_unused]] TerminalSession terminal;
        csopesy::Console console;
        if (TerminalSession::interactive()) console.setClearScreen(TerminalSession::clear);
        console.run(std::cin, std::cout);
        return 0;
    } catch (const std::exception& error) {
        csopesy::terminal::notice(std::cerr, "Error: " + std::string(error.what()), "ERROR");
        return 1;
    }
}
