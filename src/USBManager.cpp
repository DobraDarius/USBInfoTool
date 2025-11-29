#include "USBManager.h"
#include <windows.h>
#include <iostream>

bool USBManager::isUSBDrive(char driveLetter) {
    std::string path = "";
    path += driveLetter;
    path += ":\\";
    UINT type = GetDriveTypeA(path.c_str());
    return type == DRIVE_REMOVABLE;
}

USBDevice USBManager::getDriveInfo(char driveLetter) {
    USBDevice dev{};
    dev.driveLetter = driveLetter;

    std::string root = "";
    root += driveLetter;
    root += ":\\";

    ULARGE_INTEGER freeBytesAvailable, totalBytes, freeBytes;

    if (GetDiskFreeSpaceExA(root.c_str(), &freeBytesAvailable, &totalBytes, &freeBytes)) {
        dev.totalSpaceGB = totalBytes.QuadPart / (1024ULL * 1024ULL * 1024ULL);
        dev.freeSpaceGB  = freeBytes.QuadPart / (1024ULL * 1024ULL * 1024ULL);
    } else {
        dev.totalSpaceGB = 0;
        dev.freeSpaceGB  = 0;
    }

    return dev;
}

std::vector<USBDevice> USBManager::detectUSBDevices() {
    std::vector<USBDevice> usbList;

    DWORD drives = GetLogicalDrives(); // Bitmask of available drives

    for (char c = 'A'; c <= 'Z'; c++) {
        if (drives & (1 << (c - 'A'))) {
            if (isUSBDrive(c)) {
                usbList.push_back(getDriveInfo(c));
            }
        }
    }

    return usbList;
}
