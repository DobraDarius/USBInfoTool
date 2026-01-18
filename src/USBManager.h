#ifndef USB_MANAGER_H
#define USB_MANAGER_H

#include <vector>
#include <string>
#include <cstdint>

struct USBDevice {
    char driveLetter;
    std::string label;
    double totalSpaceGB;
    double freeSpaceGB;
};

struct FileEntry {
    std::string name;
    std::string path;
    uintmax_t size;
    bool isDirectory;
};

class USBManager {
public:
    std::vector<USBDevice> detectUSBDevices();
    
    std::vector<FileEntry> getFiles(std::string path);
    bool deleteFile(std::string path);
    std::string createAutoFolder(std::string currentPath);
    bool renameEntry(std::string oldPath, std::string newName);
    bool createFile(std::string currentPath, std::string fileName);

private:
    bool isUSBDrive(char driveLetter);
    USBDevice getDriveInfo(char driveLetter);
};

#endif