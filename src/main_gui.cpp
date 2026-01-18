#undef UNICODE
#undef _UNICODE

#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <cstdio>
#include <algorithm>
#include "USBManager.h"
#include "USBTester.h"
#include "Logger.h"
#include <thread>



#pragma comment(lib, "Comctl32.lib")

#define ID_LISTVIEW 100
#define ID_BTN_BACK 101
#define ID_BTN_DELETE 102
#define ID_BTN_MKDIR 103
#define ID_BTN_RENAME 104
#define ID_SEARCH_BOX 105
#define ID_BTN_MKFILE 106 // ID NOU
#define IDC_SPEEDTEST 2001


// --- INPUT BOX FIX (Varianta Anti-Freeze) ---
char inputBuffer[256];
HWND hInputEdit;

LRESULT CALLBACK InputWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch(msg) {
        case WM_CREATE:
            CreateWindow("STATIC", "Enter Name:", WS_VISIBLE | WS_CHILD, 10, 10, 200, 20, hwnd, NULL, NULL, NULL);
            hInputEdit = CreateWindow("EDIT", "", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL, 10, 35, 260, 25, hwnd, NULL, NULL, NULL);
            CreateWindow("BUTTON", "OK", WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 190, 70, 80, 25, hwnd, (HMENU)1, NULL, NULL);
            SetFocus(hInputEdit); 
            break;
        case WM_COMMAND:
            if (LOWORD(wParam) == 1) { 
                GetWindowText(hInputEdit, inputBuffer, 256);
                SendMessage(hwnd, WM_CLOSE, 0, 0); 
            }
            break;
        case WM_CLOSE: DestroyWindow(hwnd); break;
        default: return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

bool GetUserInput(HWND parent, const char* title, char* outBuf) {
    WNDCLASSA wc = {0};
    if (!GetClassInfoA(GetModuleHandle(NULL), "SimpleInputBox", &wc)) {
        wc.lpfnWndProc = InputWndProc; wc.hInstance = GetModuleHandle(NULL);
        wc.lpszClassName = "SimpleInputBox"; wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
        RegisterClassA(&wc);
    }
    EnableWindow(parent, FALSE); 
    HWND hDlg = CreateWindow("SimpleInputBox", title, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, 
        GetSystemMetrics(SM_CXSCREEN)/2 - 150, GetSystemMetrics(SM_CYSCREEN)/2 - 70, 300, 140, parent, NULL, NULL, NULL);
    memset(inputBuffer, 0, 256);
    MSG msg; while(IsWindow(hDlg) && GetMessage(&msg, NULL, 0, 0)) { TranslateMessage(&msg); DispatchMessage(&msg); }
    EnableWindow(parent, TRUE); SetForegroundWindow(parent); 
    strcpy(outBuf, inputBuffer);
    return strlen(outBuf) > 0;
}
// -----------------------------------------------------------

USBManager usb;
HWND hListView;
HWND hSearchBox;
std::string currentPath = ""; 
int sortColumn = 0;      
bool sortAscending = true; 

// Helpers
std::string FormatSize(uintmax_t bytes) {
    char buf[64]; double size = (double)bytes; const char* units[] = { "B", "KB", "MB", "GB", "TB" };
    int i = 0; while (size > 1024 && i < 4) { size /= 1024; i++; }
    sprintf(buf, "%.1f %s", size, units[i]); return std::string(buf);
}
std::string CreateProgressBar(double percent) {
    std::string bar = "["; int bars = (int)(percent / 10);
    for (int i = 0; i < 10; i++) { if (i < bars) bar += "|"; else bar += "."; }
    bar += "]"; return bar;
}
std::string ToLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); });
    return s;
}
void AddCol(HWND hList, int index, const char* text, int width) {
    LVCOLUMN lvc; lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
    lvc.fmt = LVCFMT_LEFT; lvc.cx = width; lvc.pszText = (LPSTR)text; ListView_InsertColumn(hList, index, &lvc);
}
void AddItem(HWND hList, int row, int col, const char* text) {
    LVITEM lvi; lvi.mask = LVIF_TEXT; lvi.iItem = row; lvi.iSubItem = col; lvi.pszText = (LPSTR)text;
    if (col == 0) ListView_InsertItem(hList, &lvi); else ListView_SetItem(hList, &lvi);
}

void RefreshView(HWND hwnd) {
    ListView_DeleteAllItems(hListView);
    char searchBuf[256]; GetWindowText(hSearchBox, searchBuf, 256);
    std::string searchQuery = ToLower(std::string(searchBuf));

    if (currentPath == "") {
        SetWindowText(hwnd, "USB Manager - Select Drive");
        EnableWindow(GetDlgItem(hwnd, ID_BTN_BACK), FALSE);
        EnableWindow(GetDlgItem(hwnd, ID_BTN_DELETE), FALSE);
        EnableWindow(GetDlgItem(hwnd, ID_BTN_MKDIR), FALSE);
        EnableWindow(GetDlgItem(hwnd, ID_BTN_MKFILE), FALSE); // Disable File
        EnableWindow(GetDlgItem(hwnd, ID_BTN_RENAME), FALSE); 

        auto drives = usb.detectUSBDevices();
        std::sort(drives.begin(), drives.end(), [](const USBDevice& a, const USBDevice& b) {
            if (sortColumn == 2) return sortAscending ? (a.freeSpaceGB < b.freeSpaceGB) : (a.freeSpaceGB > b.freeSpaceGB);
            return sortAscending ? (a.driveLetter < b.driveLetter) : (a.driveLetter > b.driveLetter);
        });

        int row = 0;
        for (auto& d : drives) {
            if (!searchQuery.empty()) {
                std::string fullInfo = std::string(1, d.driveLetter) + d.label;
                if (ToLower(fullInfo).find(searchQuery) == std::string::npos) continue; 
            }
            std::string name = std::string(1, d.driveLetter) + ": (" + d.label + ")";
            double percent = (d.totalSpaceGB > 0) ? ((d.totalSpaceGB - d.freeSpaceGB) / d.totalSpaceGB) * 100.0 : 0;
            std::string sizeInfo = FormatSize((uintmax_t)(d.freeSpaceGB * 1024 * 1024 * 1024)) + " Free " + CreateProgressBar(percent);
            AddItem(hListView, row, 0, name.c_str()); AddItem(hListView, row, 1, "Drive"); AddItem(hListView, row, 2, sizeInfo.c_str()); row++;
        }
    } else {
        SetWindowText(hwnd, ("Browsing: " + currentPath).c_str());
        EnableWindow(GetDlgItem(hwnd, ID_BTN_BACK), TRUE);
        EnableWindow(GetDlgItem(hwnd, ID_BTN_DELETE), TRUE);
        EnableWindow(GetDlgItem(hwnd, ID_BTN_MKDIR), TRUE);
        EnableWindow(GetDlgItem(hwnd, ID_BTN_MKFILE), TRUE); // Enable File
        EnableWindow(GetDlgItem(hwnd, ID_BTN_RENAME), TRUE); 

        auto files = usb.getFiles(currentPath);
        std::sort(files.begin(), files.end(), [](const FileEntry& a, const FileEntry& b) {
            if (sortColumn == 2) return sortAscending ? (a.size < b.size) : (a.size > b.size);
            if (sortColumn == 1) return sortAscending ? (a.isDirectory > b.isDirectory) : (a.isDirectory < b.isDirectory);
            return sortAscending ? (a.name < b.name) : (a.name > b.name);
        });

        int row = 0;
        for (auto& f : files) {
            if (!searchQuery.empty()) {
                if (ToLower(f.name).find(searchQuery) == std::string::npos) continue;
            }
            AddItem(hListView, row, 0, f.name.c_str());
            if (f.isDirectory) { AddItem(hListView, row, 1, "Folder"); AddItem(hListView, row, 2, "<DIR>"); }
            else { AddItem(hListView, row, 1, "File"); AddItem(hListView, row, 2, FormatSize(f.size).c_str()); }
            row++;
        }
    }
}

void GoUp() {
    if (currentPath.empty()) return;
    if (currentPath.length() <= 3) currentPath = ""; 
    else {
        size_t lastSlash = currentPath.find_last_of("\\");
        if (lastSlash == currentPath.length() - 1) {
            currentPath = currentPath.substr(0, lastSlash);
            lastSlash = currentPath.find_last_of("\\");
        }
        currentPath = currentPath.substr(0, lastSlash + 1);
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        hListView = CreateWindow(WC_LISTVIEW, "", WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS, 10, 50, 560, 300, hwnd, (HMENU)ID_LISTVIEW, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
        AddCol(hListView, 0, "Name", 250); AddCol(hListView, 1, "Type", 80); AddCol(hListView, 2, "Size / Usage", 150);
        ListView_SetExtendedListViewStyle(hListView, LVS_EX_FULLROWSELECT);

        // --- LAYOUT BUTOANE ---
        CreateWindow("BUTTON", "Back", WS_CHILD | WS_VISIBLE, 10, 10, 50, 30, hwnd, (HMENU)ID_BTN_BACK, NULL, NULL);
        CreateWindow("BUTTON", "+ Folder", WS_CHILD | WS_VISIBLE, 65, 10, 70, 30, hwnd, (HMENU)ID_BTN_MKDIR, NULL, NULL);
        
        // NOU: Butonul File
        CreateWindow("BUTTON", "+ File", WS_CHILD | WS_VISIBLE, 140, 10, 60, 30, hwnd, (HMENU)ID_BTN_MKFILE, NULL, NULL);

        CreateWindow("BUTTON", "Rename", WS_CHILD | WS_VISIBLE, 205, 10, 65, 30, hwnd, (HMENU)ID_BTN_RENAME, NULL, NULL);
        CreateWindow("BUTTON", "Delete", WS_CHILD | WS_VISIBLE, 275, 10, 65, 30, hwnd, (HMENU)ID_BTN_DELETE, NULL, NULL);

        // Search Box
        CreateWindow("STATIC", "Search:", WS_CHILD | WS_VISIBLE, 350, 15, 50, 20, hwnd, NULL, NULL, NULL);
        hSearchBox = CreateWindow("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 400, 12, 170, 25, hwnd, (HMENU)ID_SEARCH_BOX, NULL, NULL);

        CreateWindowA(
    "BUTTON",
    "Speed Test",
    WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
    20, 380, 120, 30,      // x, y, width, height (adjust)
    hwnd,
    (HMENU)IDC_SPEEDTEST,
    (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
    NULL
);


        RefreshView(hwnd);
        break;

    case WM_NOTIFY: {
        LPNMHDR header = (LPNMHDR)lParam;
        if (header->code == LVN_COLUMNCLICK) {
            LPNMLISTVIEW pNMLV = (LPNMLISTVIEW)lParam;
            if (sortColumn == pNMLV->iSubItem) sortAscending = !sortAscending;
            else { sortColumn = pNMLV->iSubItem; sortAscending = true; }
            RefreshView(hwnd);
        }
        if (header->code == NM_DBLCLK) {
            int iItem = ((LPNMITEMACTIVATE)lParam)->iItem;
            if (iItem != -1) {
                char text[256]; ListView_GetItemText(hListView, iItem, 0, text, sizeof(text));
                if (currentPath == "") currentPath = std::string(1, text[0]) + ":\\";
                else {
                    std::string newPath = currentPath + text;
                    if (GetFileAttributes(newPath.c_str()) & FILE_ATTRIBUTE_DIRECTORY) {
                        currentPath = newPath + "\\";
                        SetWindowText(hSearchBox, ""); 
                    }
                    else ShellExecute(NULL, "open", newPath.c_str(), NULL, NULL, SW_SHOW);
                }
                RefreshView(hwnd);
            }
        }
        break;
    }

    case WM_COMMAND:
        if (HIWORD(wParam) == EN_CHANGE && LOWORD(wParam) == ID_SEARCH_BOX) RefreshView(hwnd);
        if (LOWORD(wParam) == ID_BTN_BACK) { GoUp(); SetWindowText(hSearchBox, ""); RefreshView(hwnd); }
        if (LOWORD(wParam) == ID_BTN_MKDIR) {
            std::string created = usb.createAutoFolder(currentPath);
            if (!created.empty()) { RefreshView(hwnd); MessageBox(hwnd, ("Created: " + created).c_str(), "Success", MB_OK); }
            else MessageBox(hwnd, "Failed.", "Error", MB_ICONERROR);
        }
        
        if (LOWORD(wParam) == ID_BTN_MKFILE) {
            char nameBuf[256];
            if (GetUserInput(hwnd, "New File Name", nameBuf)) {
                if (usb.createFile(currentPath, nameBuf)) {
                    RefreshView(hwnd);
                } else {
                    MessageBox(hwnd, "Failed. File already exists?", "Error", MB_ICONERROR);
                }
            }
        }

        if (LOWORD(wParam) == ID_BTN_RENAME) {
            int iItem = ListView_GetNextItem(hListView, -1, LVNI_SELECTED);
            if (iItem != -1) {
                char text[256]; char newName[256];
                ListView_GetItemText(hListView, iItem, 0, text, sizeof(text));
                if (GetUserInput(hwnd, "Rename Entry", newName)) {
                    std::string fullPath = currentPath + text;
                    if (usb.renameEntry(fullPath, newName)) RefreshView(hwnd);
                    else MessageBox(hwnd, "Rename failed.", "Error", MB_ICONERROR);
                }
            } else MessageBox(hwnd, "Select an item first.", "Info", MB_OK);
        }
        if (LOWORD(wParam) == ID_BTN_DELETE) {
            int iItem = ListView_GetNextItem(hListView, -1, LVNI_SELECTED);
            if (iItem != -1) {
                char text[256]; ListView_GetItemText(hListView, iItem, 0, text, sizeof(text));
                std::string fullPath = currentPath + text;
                if (MessageBox(hwnd, ("Delete " + std::string(text) + "?").c_str(), "Confirm", MB_YESNO | MB_ICONWARNING) == IDYES) {
                    if (usb.deleteFile(fullPath)) RefreshView(hwnd);
                    else MessageBox(hwnd, "Failed to delete.", "Error", MB_OK);
                }
            } else MessageBox(hwnd, "Select an item first.", "Info", MB_OK);
        }

        if (LOWORD(wParam) == IDC_SPEEDTEST) {

    // currentPath already exists in your file, like "E:\\some\\folder\\"
    // So drive letter is currentPath[0]
    if (currentPath.size() < 2 || currentPath[1] != ':') {
        MessageBoxA(hwnd, "No drive selected.", "Speed Test", MB_OK | MB_ICONWARNING);
        break;
    }

    char drive = currentPath[0];

    EnableWindow(GetDlgItem(hwnd, IDC_SPEEDTEST), FALSE);
    Logger::Info(std::string("GUI: Speed test requested for drive ") + drive);

    std::thread([hwnd, drive]() {
        auto res = USBTester::TestSpeed(drive, 256, false);

        // Re-enable button on UI thread
        PostMessage(hwnd, WM_APP + 1, 0, 0);

        if (!res.ok) {
            std::string msg = "Speed test failed:\n" + res.error;
            Logger::Error("GUI speed test failed: " + res.error);
            MessageBoxA(hwnd, msg.c_str(), "Speed Test", MB_OK | MB_ICONERROR);
        } else {
            char buf[256];
            snprintf(buf, sizeof(buf),
                     "Drive %c:\\\n\nWrite: %.2f MB/s\nRead:  %.2f MB/s",
                     drive, res.writeMBps, res.readMBps);

            Logger::Info("GUI speed test OK");
            MessageBoxA(hwnd, buf, "Speed Test Result", MB_OK | MB_ICONINFORMATION);
        }
    }).detach();

    break; // IMPORTANT: stop processing WM_COMMAND
}


        break;

    case WM_DEVICECHANGE: RefreshView(hwnd); break;
    case WM_DESTROY: PostQuitMessage(0); break;

    case WM_APP + 1:
{
    EnableWindow(GetDlgItem(hwnd, IDC_SPEEDTEST), TRUE);
    return 0;
}

    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow) {
    Logger::Init("usbtool.log");
    Logger::Info("GUI app started");

    WNDCLASSA wc = {0}; wc.lpfnWndProc = WndProc; wc.hInstance = hInst; wc.lpszClassName = "USBManMax";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1); RegisterClassA(&wc);
    int x = (GetSystemMetrics(SM_CXSCREEN)-600)/2; int y = (GetSystemMetrics(SM_CYSCREEN)-450)/2;
    CreateWindow("USBManMax", "USB Manager Ultimate (+File)", WS_VISIBLE|WS_SYSMENU|WS_CAPTION|WS_MINIMIZEBOX, x, y, 600, 450, NULL, NULL, hInst, NULL);
    MSG msg; while (GetMessage(&msg, NULL, 0, 0)) { TranslateMessage(&msg); DispatchMessage(&msg); }
    return 0;
}
