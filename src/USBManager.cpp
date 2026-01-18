#include "USBManager.h"
#include <windows.h>
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <fstream>


#include "Logger.h"


namespace fs = std::filesystem;

// ---------------------------------------------------------
// PARTEA 1: Detectare USB (Codul vechi)
// ---------------------------------------------------------

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
        dev.totalSpaceGB = totalBytes.QuadPart / (1024.0 * 1024.0 * 1024.0);
        dev.freeSpaceGB  = freeBytes.QuadPart / (1024.0 * 1024.0 * 1024.0);
    }

    char volumeName[MAX_PATH + 1] = {0};
    char fileSysName[MAX_PATH + 1] = {0};
    if (GetVolumeInformationA(root.c_str(), volumeName, MAX_PATH, NULL, NULL, NULL, fileSysName, MAX_PATH)) {
        dev.label = volumeName;
    }
    if (dev.label.empty()) dev.label = "USB Drive"; 

    return dev;
}

std::vector<USBDevice> USBManager::detectUSBDevices() {
    std::vector<USBDevice> usbList;


    Logger::Info("Detecting USB devices...");

    DWORD drives = GetLogicalDrives();
    for (char c = 'A'; c <= 'Z'; c++) {
        if (drives & (1 << (c - 'A'))) {
            if (isUSBDrive(c)) usbList.push_back(getDriveInfo(c));
        }
    }
    return usbList;
}

// ---------------------------------------------------------
// PARTEA 2: Logica File Manager (NOU)
// ---------------------------------------------------------

std::vector<FileEntry> USBManager::getFiles(std::string path) {
    std::vector<FileEntry> files;
    
    try {
        if (fs::exists(path) && fs::is_directory(path)) {
            for (const auto& entry : fs::directory_iterator(path)) {
                FileEntry fe;
                fe.path = entry.path().string();
                fe.name = entry.path().filename().string();
                fe.isDirectory = entry.is_directory();
                
                if (!fe.isDirectory) fe.size = entry.file_size();
                else fe.size = 0;
                
                files.push_back(fe);
            }
        }
    } catch (...) { }

    std::sort(files.begin(), files.end(), [](const FileEntry& a, const FileEntry& b) {
        if (a.isDirectory != b.isDirectory) return a.isDirectory > b.isDirectory;
        return a.name < b.name;
    });

    return files;
}

bool USBManager::deleteFile(std::string path) {
    try {
        if (fs::exists(path)) return fs::remove_all(path) > 0;
    } catch (...) { return false; }
    return false;
}
std::string USBManager::createAutoFolder(std::string currentPath) {
    try {
        std::string baseName = "NewFolder";
        std::string newFolderPath = currentPath + "\\" + baseName;
        int counter = 1;

        while (fs::exists(newFolderPath)) {
            newFolderPath = currentPath + "\\" + baseName + std::to_string(counter);
            counter++;
        }

        if (fs::create_directory(newFolderPath)) {
            return fs::path(newFolderPath).filename().string();
        }
    } catch (...) { }
    return "";
}

bool USBManager::renameEntry(std::string oldPath, std::string newName) {
    try {
        fs::path source(oldPath);
        fs::path target = source.parent_path() / newName;
        
        fs::rename(source, target);
        return true;
    } catch (...) {
        return false;
    }
}

bool USBManager::createFile(std::string currentPath, std::string fileName) {
    try {
        fs::path target = fs::path(currentPath) / fileName;
        
        if (fs::exists(target)) return false; 

        std::ofstream outfile(target);
        
        outfile.close();
        
        return true;
    } catch (...) {
        return false;
    }
}