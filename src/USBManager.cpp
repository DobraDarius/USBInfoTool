#include "USBManager.h"
#include <windows.h>
#include <iostream>
#include <filesystem>
#include <cmath> // For rounding if needed

namespace fs = std::filesystem;

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

    // 1. GET SPACE INFO
    ULARGE_INTEGER freeBytesAvailable, totalBytes, freeBytes;
    if (GetDiskFreeSpaceExA(root.c_str(), &freeBytesAvailable, &totalBytes, &freeBytes)) {
        dev.totalSpaceGB = totalBytes.QuadPart / (1024.0 * 1024.0 * 1024.0);
        dev.freeSpaceGB  = freeBytes.QuadPart / (1024.0 * 1024.0 * 1024.0);
    } else {
        dev.totalSpaceGB = 0;
        dev.freeSpaceGB  = 0;
    }

    // 2. GET VOLUME LABEL (NEW)
    char volumeName[MAX_PATH + 1] = {0};
    char fileSysName[MAX_PATH + 1] = {0};
    
    // This Windows function fetches the drive name (Volume Label)
    if (GetVolumeInformationA(
            root.c_str(), 
            volumeName, 
            MAX_PATH, 
            NULL, NULL, NULL, 
            fileSysName, 
            MAX_PATH)) {
        
        dev.label = volumeName;
    }
    
    // Fallback if name is empty
    if (dev.label.empty()) {
        dev.label = "USB Drive"; 
    }

    return dev;
}

std::vector<USBDevice> USBManager::detectUSBDevices() {
    std::vector<USBDevice> usbList;
    DWORD drives = GetLogicalDrives();

    for (char c = 'A'; c <= 'Z'; c++) {
        if (drives & (1 << (c - 'A'))) {
            if (isUSBDrive(c)) {
                usbList.push_back(getDriveInfo(c));
            }
        }
    }
    return usbList;
}

void USBManager::listFiles(char driveLetter) {
    // (Keep your existing listFiles code here if you still need the console version)
}