#ifndef LOGGER_H
#define LOGGER_H

#include <string>

class Logger {
public:
    // Optional: call once at startup. If not called, it defaults to "usbtool.log".
    static void Init(const std::string& filename = "usbtool.log");

    static void Info(const std::string& msg);
    static void Warn(const std::string& msg);
    static void Error(const std::string& msg);

    // Convenience: logs last WinAPI error with context
    static void WinError(const std::string& context);

private:
    static void Write(const char* level, const std::string& msg);
};

#endif
