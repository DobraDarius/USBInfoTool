#include <iostream>
#include "USBManager.h"

int main() {
    std::cout << "=== USB Storage Management Tool ===\n\n";

    USBManager usbManager;
    auto devices = usbManager.detectUSBDevices();

    if (devices.empty()) {
        std::cout << "No USB devices detected.\n";
    } else {
        for (auto& dev : devices) {
            std::cout << "USB Drive " << dev.driveLetter << ":\\" << "\n";
            std::cout << "  Total Space: " << dev.totalSpaceGB << " GB\n";
            std::cout << "  Free Space:  " << dev.freeSpaceGB  << " GB\n";
            std::cout << "  Used Space:  " << dev.totalSpaceGB - dev.freeSpaceGB << " GB\n\n";
        }
    }

    return 0;
}
