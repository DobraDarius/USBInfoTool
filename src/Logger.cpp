#include "Logger.h"
#include <fstream>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#endif

static std::mutex g_logMutex;
static std::string g_logFile = "usbtool.log";

static std::string now_timestamp() {
    using namespace std::chrono;
    auto t = system_clock::now();
    auto tt = system_clock::to_time_t(t);

    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &tt);
#else
    tm = *std::localtime(&tt);
#endif

    auto ms = duration_cast<milliseconds>(t.time_since_epoch()) % 1000;

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S")
        << '.' << std::setw(3) << std::setfill('0') << ms.count();
    return oss.str();
}

void Logger::Init(const std::string& filename) {
    std::lock_guard<std::mutex> lock(g_logMutex);
    g_logFile = filename.empty() ? "usbtool.log" : filename;

    // Touch the file so we know it’s writable
    std::ofstream out(g_logFile, std::ios::app);
    out << now_timestamp() << " [INFO] Logger initialized: " << g_logFile << "\n";
}

void Logger::Write(const char* level, const std::string& msg) {
    std::lock_guard<std::mutex> lock(g_logMutex);
    std::ofstream out(g_logFile, std::ios::app);
    out << now_timestamp() << " [" << level << "] " << msg << "\n";
}

void Logger::Info(const std::string& msg)  { Write("INFO", msg); }
void Logger::Warn(const std::string& msg)  { Write("WARN", msg); }
void Logger::Error(const std::string& msg) { Write("ERROR", msg); }

void Logger::WinError(const std::string& context) {
#ifdef _WIN32
    DWORD err = GetLastError();

    LPSTR buffer = nullptr;
    DWORD size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        err,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&buffer,
        0,
        nullptr
    );

    std::string msg = context + " | GetLastError=" + std::to_string(err);
    if (size && buffer) {
        // Trim trailing newlines from FormatMessage
        std::string sys(buffer, buffer + size);
        while (!sys.empty() && (sys.back() == '\n' || sys.back() == '\r')) sys.pop_back();
        msg += " | " + sys;
    }
    if (buffer) LocalFree(buffer);

    Error(msg);
#else
    Error(context + " | (WinError called on non-Windows)");
#endif
}
