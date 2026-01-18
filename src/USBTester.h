#ifndef USB_TESTER_H
#define USB_TESTER_H

#include <string>
#include <windows.h>


struct SpeedResult {
    double writeMBps = 0.0;
    double readMBps  = 0.0;
    bool ok = false;
    std::string error;
    std::string testFilePath;
};

class USBTester {
public:
    // driveLetter example: 'E'
    // megabytes example: 256 (creates a ~256MB temp file)
    static SpeedResult TestSpeed(char driveLetter, size_t megabytes = 256, bool verify = false);
};

#endif
