// FORCE MODERN WINDOWS LOOK
#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#include <windows.h>
#include <dbt.h>
#include <shellapi.h>
#include <cstdio>
#include <algorithm> // Required for std::sort
#include "USBManager.h"

// ID Constants
#define ID_BUTTON_REFRESH 1
#define ID_LISTBOX 2
#define ID_RADIO_NAME 101
#define ID_RADIO_FREE 102
#define ID_RADIO_SIZE 103

USBManager usb;
HWND hListBox;
HFONT hFont; 

// Sorting State
enum SortMode { SORT_NAME, SORT_FREE, SORT_SIZE };
SortMode currentSortMode = SORT_NAME; // Default sort by Name (A-Z)

// Helper: Set font for a control
void SetModernFont(HWND hwndChild) {
    SendMessageA(hwndChild, WM_SETFONT, (WPARAM)hFont, TRUE);
}

void RefreshDriveList(HWND hwnd) {
    SendMessageA(hListBox, LB_RESETCONTENT, 0, 0);

    // 1. Get Drives
    auto drives = usb.detectUSBDevices();

    if (drives.empty()) {
        SendMessageA(hListBox, LB_ADDSTRING, 0, (LPARAM)"No USB devices connected");
        return;
    }

    // 2. SORT THE LIST (Based on selected radio button)
    // We use a "Lambda" function here to define custom sorting rules
    std::sort(drives.begin(), drives.end(), [](const USBDevice& a, const USBDevice& b) {
        if (currentSortMode == SORT_NAME) {
            return a.driveLetter < b.driveLetter; // A before B
        }
        else if (currentSortMode == SORT_FREE) {
            return a.freeSpaceGB > b.freeSpaceGB; // More free space first
        }
        else { // SORT_SIZE
            return a.totalSpaceGB > b.totalSpaceGB; // Larger drives first
        }
    });

    // 3. Display Info
    for (auto& d : drives) {
        char text[256];
        
        // Calculate Percentage Used
        double usedGB = d.totalSpaceGB - d.freeSpaceGB;
        int percent = (d.totalSpaceGB > 0) ? (int)((usedGB / d.totalSpaceGB) * 100) : 0;

        // Format: "E: [KINGSTON] - 25% Used (14GB Free)"
        sprintf(text, " %c: [%s]  -  %d%% Used  (%.1f GB Free)", 
            d.driveLetter, 
            d.label.c_str(), 
            percent,
            d.freeSpaceGB);

        int index = SendMessageA(hListBox, LB_ADDSTRING, 0, (LPARAM)text);
        SendMessageA(hListBox, LB_SETITEMDATA, index, (LPARAM)d.driveLetter);
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {

    case WM_CREATE:
        // Create standard UI Font
        hFont = CreateFontA(19, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, 
                           DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, 
                           CLEARTYPE_QUALITY, FF_SWISS, "Segoe UI");
        
        // Check the "Name" radio button by default
        CheckRadioButton(hwnd, ID_RADIO_NAME, ID_RADIO_SIZE, ID_RADIO_NAME);
        RefreshDriveList(hwnd);
        break;

    case WM_DEVICECHANGE:
        if (wParam == DBT_DEVICEARRIVAL || wParam == DBT_DEVICEREMOVECOMPLETE) {
            RefreshDriveList(hwnd);
        }
        break;

    case WM_COMMAND:
        // If Refresh Button Clicked
        if (LOWORD(wParam) == ID_BUTTON_REFRESH) {
            RefreshDriveList(hwnd);
        }
        
        // If Radio Buttons Clicked
        if (LOWORD(wParam) == ID_RADIO_NAME) {
            currentSortMode = SORT_NAME;
            RefreshDriveList(hwnd); // Re-sort immediately
        }
        if (LOWORD(wParam) == ID_RADIO_FREE) {
            currentSortMode = SORT_FREE;
            RefreshDriveList(hwnd);
        }
        if (LOWORD(wParam) == ID_RADIO_SIZE) {
            currentSortMode = SORT_SIZE;
            RefreshDriveList(hwnd);
        }

        // If ListBox Double Clicked
        if (LOWORD(wParam) == ID_LISTBOX && HIWORD(wParam) == LBN_DBLCLK) {
            int idx = SendMessageA(hListBox, LB_GETCURSEL, 0, 0);
            if (idx != LB_ERR) {
                char driveLetter = (char)SendMessageA(hListBox, LB_GETITEMDATA, idx, 0);
                if (driveLetter >= 'A' && driveLetter <= 'Z') {
                    char path[] = { driveLetter, ':', '\\', 0 };
                    ShellExecuteA(NULL, "open", path, NULL, NULL, SW_SHOWDEFAULT);
                }
            }
        }
        break;

    case WM_DESTROY:
        DeleteObject(hFont);
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProcA(hwnd, msg, wParam, lParam);
    }

    return 0;
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow) {

    WNDCLASSA wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = "USB_GUI_v2";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.style = CS_HREDRAW | CS_VREDRAW;

    RegisterClassA(&wc);

    // Center Window
    int w = 450, h = 420; // Taller window to fit radio buttons
    int x = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;

    HWND hwnd = CreateWindowA("USB_GUI_v2", "Advanced USB Manager",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE,
        x, y, w, h, NULL, NULL, hInst, NULL
    );

    // --- GUI LAYOUT ---

    // 1. Group Box for List
    HWND hGroup = CreateWindowA("BUTTON", "Connected Drives", 
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        20, 10, 395, 250, hwnd, NULL, hInst, NULL);
    SetModernFont(hGroup);

    // 2. The ListBox
    hListBox = CreateWindowA("LISTBOX", "", 
        WS_CHILD | WS_VISIBLE | LBS_NOTIFY | WS_VSCROLL | LBS_NOINTEGRALHEIGHT,
        35, 40, 365, 205, hwnd, (HMENU)ID_LISTBOX, hInst, NULL);
    SetModernFont(hListBox);

    // 3. Sorting Group Box
    HWND hSortGroup = CreateWindowA("BUTTON", "Sort By", 
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        20, 270, 395, 60, hwnd, NULL, hInst, NULL);
    SetModernFont(hSortGroup);

    // 4. Radio Buttons
    HWND r1 = CreateWindowA("BUTTON", "Name (A-Z)", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_GROUP,
        35, 295, 100, 20, hwnd, (HMENU)ID_RADIO_NAME, hInst, NULL);
    
    HWND r2 = CreateWindowA("BUTTON", "Free Space", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
        145, 295, 100, 20, hwnd, (HMENU)ID_RADIO_FREE, hInst, NULL);

    HWND r3 = CreateWindowA("BUTTON", "Total Size", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
        255, 295, 100, 20, hwnd, (HMENU)ID_RADIO_SIZE, hInst, NULL);

    SetModernFont(r1); SetModernFont(r2); SetModernFont(r3);

    // 5. Refresh Button
    HWND hBtn = CreateWindowA("BUTTON", "Refresh List", 
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        150, 345, 130, 30, hwnd, (HMENU)ID_BUTTON_REFRESH, hInst, NULL);
    SetModernFont(hBtn);

    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    return 0;
}