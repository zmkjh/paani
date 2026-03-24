// paani_colors.hpp - Cross-platform colored console output
// Uses Windows API on Windows, ANSI escape codes on Unix-like systems
// No special characters, works on all platforms

#ifndef PAANI_COLORS_HPP
#define PAANI_COLORS_HPP

#include <iostream>
#include <string>

#ifdef _WIN32
    #include <windows.h>
#endif

namespace paani {

// Color codes for ANSI terminals
namespace AnsiColor {
    constexpr const char* RESET      = "\033[0m";
    constexpr const char* BOLD       = "\033[1m";
    constexpr const char* DIM        = "\033[2m";
    constexpr const char* UNDERLINE  = "\033[4m";
    
    // Foreground colors
    constexpr const char* BLACK      = "\033[30m";
    constexpr const char* RED        = "\033[31m";
    constexpr const char* GREEN      = "\033[32m";
    constexpr const char* YELLOW     = "\033[33m";
    constexpr const char* BLUE       = "\033[34m";
    constexpr const char* MAGENTA    = "\033[35m";
    constexpr const char* CYAN       = "\033[36m";
    constexpr const char* WHITE      = "\033[37m";
    constexpr const char* GRAY       = "\033[90m";
    
    // Bright colors
    constexpr const char* BRIGHT_RED     = "\033[91m";
    constexpr const char* BRIGHT_GREEN   = "\033[92m";
    constexpr const char* BRIGHT_YELLOW  = "\033[93m";
    constexpr const char* BRIGHT_BLUE    = "\033[94m";
    constexpr const char* BRIGHT_MAGENTA = "\033[95m";
    constexpr const char* BRIGHT_CYAN    = "\033[96m";
    constexpr const char* BRIGHT_WHITE   = "\033[97m";
}

// Windows console color constants
#ifdef _WIN32
namespace WinColor {
    constexpr WORD RESET      = 7;   // Light Gray
    constexpr WORD BOLD       = FOREGROUND_INTENSITY;
    
    constexpr WORD BLACK      = 0;
    constexpr WORD DARK_RED   = FOREGROUND_RED;
    constexpr WORD DARK_GREEN = FOREGROUND_GREEN;
    constexpr WORD DARK_YELLOW= FOREGROUND_RED | FOREGROUND_GREEN;
    constexpr WORD DARK_BLUE  = FOREGROUND_BLUE;
    constexpr WORD DARK_MAGENTA= FOREGROUND_RED | FOREGROUND_BLUE;
    constexpr WORD DARK_CYAN  = FOREGROUND_GREEN | FOREGROUND_BLUE;
    constexpr WORD GRAY       = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
    
    constexpr WORD RED        = FOREGROUND_RED | FOREGROUND_INTENSITY;
    constexpr WORD GREEN      = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
    constexpr WORD YELLOW     = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
    constexpr WORD BLUE       = FOREGROUND_BLUE | FOREGROUND_INTENSITY;
    constexpr WORD MAGENTA    = FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
    constexpr WORD CYAN       = FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
    constexpr WORD WHITE      = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
}
#endif

class ConsoleColor {
public:
    enum class Color {
        Reset,
        Error,      // Red
        Success,    // Green  
        Warning,    // Yellow
        Info,       // Blue
        Highlight,  // Cyan
        Dim,        // Gray
        Bold,
        Normal
    };

private:
    static bool initialized;
    static bool useAnsi;
    static bool enabled;
    
#ifdef _WIN32
    static HANDLE hStdout;
    static HANDLE hStderr;
    static WORD originalAttrs;
#endif

public:
    // Initialize color support (auto-detect platform)
    static void init();
    
    // Enable/disable colors
    static void setEnabled(bool enable) { enabled = enable; }
    static bool isEnabled() { return enabled; }
    
    // Set color for stdout
    static void set(Color color, bool toStderr = false);
    
    // Reset to default
    static void reset(bool toStderr = false);
    
    // Print with color (convenience methods)
    static void printError(const std::string& msg);
    static void printSuccess(const std::string& msg);
    static void printWarning(const std::string& msg);
    static void printInfo(const std::string& msg);
    static void printHighlight(const std::string& msg);
    static void printDim(const std::string& msg);
    
    // Print colored prefix
    static void printErrorPrefix();
    static void printSuccessPrefix();
    static void printWarningPrefix();
    static void printInfoPrefix();
};

// Static member definitions
bool ConsoleColor::initialized = false;
bool ConsoleColor::useAnsi = false;
bool ConsoleColor::enabled = true;

#ifdef _WIN32
HANDLE ConsoleColor::hStdout = nullptr;
HANDLE ConsoleColor::hStderr = nullptr;
WORD ConsoleColor::originalAttrs = 0;
#endif

inline void ConsoleColor::init() {
    if (initialized) return;
    
#ifdef _WIN32
    hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
    hStderr = GetStdHandle(STD_ERROR_HANDLE);
    
    // Try to enable ANSI escape codes on Windows 10+
    DWORD mode = 0;
    if (GetConsoleMode(hStdout, &mode)) {
        mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        if (SetConsoleMode(hStdout, mode)) {
            useAnsi = true;
        }
    }
    
    // Save original attributes
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (GetConsoleScreenBufferInfo(hStdout, &info)) {
        originalAttrs = info.wAttributes;
    }
#else
    // Unix-like systems always support ANSI
    useAnsi = true;
#endif
    
    initialized = true;
}

inline void ConsoleColor::set(Color color, bool toStderr) {
    if (!enabled) return;
    if (!initialized) init();
    
    const char* ansiCode = AnsiColor::RESET;
    
#ifdef _WIN32
    WORD winColor = WinColor::RESET;
#endif
    
    switch (color) {
        case Color::Error:
            ansiCode = AnsiColor::BRIGHT_RED;
#ifdef _WIN32
            winColor = WinColor::RED;
#endif
            break;
        case Color::Success:
            ansiCode = AnsiColor::BRIGHT_GREEN;
#ifdef _WIN32
            winColor = WinColor::GREEN;
#endif
            break;
        case Color::Warning:
            ansiCode = AnsiColor::BRIGHT_YELLOW;
#ifdef _WIN32
            winColor = WinColor::YELLOW;
#endif
            break;
        case Color::Info:
            ansiCode = AnsiColor::BRIGHT_BLUE;
#ifdef _WIN32
            winColor = WinColor::BLUE;
#endif
            break;
        case Color::Highlight:
            ansiCode = AnsiColor::BRIGHT_CYAN;
#ifdef _WIN32
            winColor = WinColor::CYAN;
#endif
            break;
        case Color::Dim:
            ansiCode = AnsiColor::GRAY;
#ifdef _WIN32
            winColor = WinColor::GRAY;
#endif
            break;
        case Color::Bold:
            ansiCode = AnsiColor::BOLD;
#ifdef _WIN32
            winColor = WinColor::BOLD;
#endif
            break;
        case Color::Normal:
        case Color::Reset:
        default:
            ansiCode = AnsiColor::RESET;
#ifdef _WIN32
            winColor = originalAttrs ? originalAttrs : WinColor::RESET;
#endif
            break;
    }
    
    if (useAnsi) {
        if (toStderr) {
            std::cerr << ansiCode;
        } else {
            std::cout << ansiCode;
        }
    }
#ifdef _WIN32
    else {
        HANDLE h = toStderr ? hStderr : hStdout;
        if (h != nullptr && h != INVALID_HANDLE_VALUE) {
            SetConsoleTextAttribute(h, winColor);
        }
    }
#endif
}

inline void ConsoleColor::reset(bool toStderr) {
    if (!enabled) return;
    set(Color::Reset, toStderr);
}

inline void ConsoleColor::printError(const std::string& msg) {
    set(Color::Error, true);
    std::cerr << msg;
    reset(true);
}

inline void ConsoleColor::printSuccess(const std::string& msg) {
    set(Color::Success);
    std::cout << msg;
    reset();
}

inline void ConsoleColor::printWarning(const std::string& msg) {
    set(Color::Warning, true);
    std::cerr << msg;
    reset(true);
}

inline void ConsoleColor::printInfo(const std::string& msg) {
    set(Color::Info);
    std::cout << msg;
    reset();
}

inline void ConsoleColor::printHighlight(const std::string& msg) {
    set(Color::Highlight);
    std::cout << msg;
    reset();
}

inline void ConsoleColor::printDim(const std::string& msg) {
    set(Color::Dim);
    std::cout << msg;
    reset();
}

inline void ConsoleColor::printErrorPrefix() {
    set(Color::Error, true);
    std::cerr << "[ERROR]";
    reset(true);
}

inline void ConsoleColor::printSuccessPrefix() {
    set(Color::Success);
    std::cout << "[OK]";
    reset();
}

inline void ConsoleColor::printWarningPrefix() {
    set(Color::Warning, true);
    std::cerr << "[WARNING]";
    reset(true);
}

inline void ConsoleColor::printInfoPrefix() {
    set(Color::Info);
    std::cout << "[INFO]";
    reset();
}

// RAII helper for scoped color
class ScopedColor {
    bool toStderr_;
public:
    ScopedColor(ConsoleColor::Color color, bool toStderr = false) : toStderr_(toStderr) {
        ConsoleColor::set(color, toStderr);
    }
    ~ScopedColor() {
        ConsoleColor::reset(toStderr_);
    }
};

} // namespace paani

#endif // PAANI_COLORS_HPP
