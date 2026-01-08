#ifndef USB_MANAGER_H
#define USB_MANAGER_H

#include <vector>
#include <string>

struct USBDevice {
    char driveLetter;
    std::string label;   // NEW: Stores "KINGSTON", "USB_STICK", etc.
    double totalSpaceGB;
    double freeSpaceGB;
};

class USBManager {
public:
    std::vector<USBDevice> detectUSBDevices();
    void listFiles(char driveLetter);

private:
    bool isUSBDrive(char driveLetter);
    USBDevice getDriveInfo(char driveLetter);
};

#endif