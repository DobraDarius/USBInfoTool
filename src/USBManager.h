#ifndef USB_MANAGER_H
#define USB_MANAGER_H

#include <vector>
#include <string>

struct USBDevice {
    char driveLetter;                // Example: 'E'
    unsigned long long totalSpaceGB; // Total size in GB
    unsigned long long freeSpaceGB;  // Free size in GB
};

class USBManager {
public:
    // Detects all USB devices connected
    std::vector<USBDevice> detectUSBDevices();

private:
    bool isUSBDrive(char driveLetter);
    USBDevice getDriveInfo(char driveLetter);
};

#endif
