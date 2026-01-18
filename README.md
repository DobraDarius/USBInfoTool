# USB Information Tool

## Description
This project is a simple console application written in C/C++.  
It reads basic information from a USB flash drive connected to the computer.

When a USB device is detected, the program displays:
- Device name
- Total storage size
- Free available space
- Basic device details

The project was made as a student team assignment to understand how USB storage devices work and how a program can interact with external hardware.

The application runs in the Windows console and is meant only for learning and testing purposes.

---

## Technologies Used
- C / C++
- Windows Console

## RUN in MSYS2 UCRT64

g++ -std=c++17 -O2 USBManager.cpp main_gui.cpp Logger.cpp USBTester.cpp -o USBInfoTool.exe -mwindows -lgdi32 -luser32 -lshell32
---

## Team Members
- Rares – Developer
- Darius – Hardware
- Dragos – Tester
- Ionut – Project Manager & Interface/UX

---

## Notes
- This is a student project created for school
- Removing the USB while the program is running may cause problems
