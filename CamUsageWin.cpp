// CamUsageWin.cpp
// Windows Desktop app (Win32) that shows current & recent webcam usage.
// Reads HKCU\Software\Microsoft\Windows\CurrentVersion\CapabilityAccessManager\ConsentStore\webcam
// and ...\NonPackaged\ for classic desktop apps.
//
// UI:
//  - ListView with columns: Kind, App, EXE, Active, Last Start, Last Stop
//  - [Refresh] button
//  - [ ] Current only (filters to active sessions)
//  - Status bar: "Ready - Bob Paydar"
//
// Build: Visual Studio 2022 → Win32 Project (Empty), add this file, set /DUNICODE /D_UNICODE.
// Programmer: Bob Paydar

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <algorithm>

#pragma comment(lib, "comctl32.lib")

// ---------------------- Constants & IDs ----------------------
const wchar_t kAppClass[] = L"CamUsageWin32App";
const wchar_t kAppTitle[] = L"Camera Usage Viewer (Win32)";

#define IDC_LIST        1001
#define IDC_REFRESH     1002
#define IDC_CURONLY     1003
#define IDC_STATUS      1004

// Registry base
const wchar_t* const REG_WEBCAM_BASE =
L"Software\\Microsoft\\Windows\\CurrentVersion\\CapabilityAccessManager\\ConsentStore\\webcam";

// ---------------------- Data Model --------------------------
struct CamRow {
    std::wstring kind;         // "Packaged" | "Desktop"
    std::wstring app;          // App key or friendly name
    std::wstring exe;          // Full path for Desktop (NonPackaged) apps
    bool         activeNow{ false };
    ULONGLONG    startFt{ 0 }; // FILETIME (100ns since 1601), UTC
    ULONGLONG    stopFt{ 0 };  // FILETIME (0 => still active)
};

// Globals
HINSTANCE g_hInst = nullptr;
HWND g_hList = nullptr, g_hBtnRefresh = nullptr, g_hChkCurrent = nullptr, g_hStatus = nullptr;
std::vector<CamRow> g_rows;

// ---------------------- Helpers -----------------------------
static std::wstring ReplaceAll(const std::wstring& s, wchar_t from, wchar_t to) {
    std::wstring r = s;
    std::replace(r.begin(), r.end(), from, to);
    return r;
}

static std::wstring LeafName(const std::wstring& path) {
    size_t pos = path.find_last_of(L"\\/");
    return (pos == std::wstring::npos) ? path : path.substr(pos + 1);
}

static std::wstring FtToLocalString(ULONGLONG ft) {
    if (ft == 0) return L"";
    FILETIME ftUtc{};
    ftUtc.dwLowDateTime = static_cast<DWORD>(ft & 0xFFFFFFFFULL);
    ftUtc.dwHighDateTime = static_cast<DWORD>(ft >> 32);

    SYSTEMTIME stUtc{};
    if (!FileTimeToSystemTime(&ftUtc, &stUtc)) return L"";

    SYSTEMTIME stLocal{};
    if (!SystemTimeToTzSpecificLocalTime(nullptr, &stUtc, &stLocal)) return L"";

    wchar_t buf[64];
    swprintf_s(buf, L"%04u-%02u-%02u %02u:%02u:%02u",
        stLocal.wYear, stLocal.wMonth, stLocal.wDay,
        stLocal.wHour, stLocal.wMinute, stLocal.wSecond);
    return buf;
}

static bool RegGetQword(HKEY hKey, const wchar_t* valueName, ULONGLONG& out) {
    DWORD type = 0;
    ULONGLONG val = 0;
    DWORD cb = sizeof(val);
    LONG r = RegGetValueW(hKey, nullptr, valueName, RRF_RT_QWORD, &type, &val, &cb);
    if (r == ERROR_SUCCESS) { out = val; return true; }
    return false;
}

static void LoadConsentStore(std::vector<CamRow>& out) {
    out.clear();

    HKEY hBase = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_WEBCAM_BASE, 0, KEY_READ, &hBase) != ERROR_SUCCESS) {
        return;
    }

    DWORD index = 0;
    wchar_t name[512];
    DWORD nameLen;
    FILETIME ftDummy{};
    while (true) {
        nameLen = static_cast<DWORD>(std::size(name));
        LONG rr = RegEnumKeyExW(hBase, index++, name, &nameLen, nullptr, nullptr, nullptr, &ftDummy);
        if (rr == ERROR_NO_MORE_ITEMS) break;
        if (rr != ERROR_SUCCESS) continue;

        std::wstring subkey = name;
        if (_wcsicmp(subkey.c_str(), L"NonPackaged") == 0) {
            // Desktop apps
            HKEY hNp = nullptr;
            if (RegOpenKeyExW(hBase, L"NonPackaged", 0, KEY_READ, &hNp) == ERROR_SUCCESS) {
                DWORD idx2 = 0;
                wchar_t n2[1024]; DWORD n2len;
                while (true) {
                    n2len = static_cast<DWORD>(std::size(n2));
                    LONG r2 = RegEnumKeyExW(hNp, idx2++, n2, &n2len, nullptr, nullptr, nullptr, &ftDummy);
                    if (r2 == ERROR_NO_MORE_ITEMS) break;
                    if (r2 != ERROR_SUCCESS) continue;

                    HKEY hItem = nullptr;
                    if (RegOpenKeyExW(hNp, n2, 0, KEY_READ, &hItem) == ERROR_SUCCESS) {
                        ULONGLONG start = 0, stop = 0;
                        RegGetQword(hItem, L"LastUsedTimeStart", start);
                        RegGetQword(hItem, L"LastUsedTimeStop", stop);
                        RegCloseKey(hItem);

                        std::wstring raw = n2;
                        std::wstring exe = ReplaceAll(raw, L'#', L'\\');
                        CamRow row;
                        row.kind = L"Desktop";
                        row.exe = exe;
                        row.app = LeafName(exe);
                        row.startFt = start;
                        row.stopFt = stop;
                        row.activeNow = (start != 0) && (stop == 0);
                        out.push_back(std::move(row));
                    }
                }
                RegCloseKey(hNp);
            }
        }
        else {
            // Packaged apps
            HKEY hChild = nullptr;
            if (RegOpenKeyExW(hBase, subkey.c_str(), 0, KEY_READ, &hChild) == ERROR_SUCCESS) {
                ULONGLONG start = 0, stop = 0;
                RegGetQword(hChild, L"LastUsedTimeStart", start);
                RegGetQword(hChild, L"LastUsedTimeStop", stop);
                RegCloseKey(hChild);

                CamRow row;
                row.kind = L"Packaged";
                row.app = subkey;
                row.exe = L"";
                row.startFt = start;
                row.stopFt = stop;
                row.activeNow = (start != 0) && (stop == 0);
                out.push_back(std::move(row));
            }
        }
    }

    RegCloseKey(hBase);

    std::sort(out.begin(), out.end(), [](const CamRow& a, const CamRow& b) {
        if (a.activeNow != b.activeNow) return a.activeNow > b.activeNow;
        return a.startFt > b.startFt;
        });
}

static void ListView_SetupColumns(HWND hList) {
    ListView_DeleteAllItems(hList);
    while (ListView_DeleteColumn(hList, 0)) {}

    LVCOLUMNW col{};
    col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;

    col.pszText = const_cast<wchar_t*>(L"Kind"); col.cx = 90; col.iSubItem = 0;
    ListView_InsertColumn(hList, 0, &col);

    col.pszText = const_cast<wchar_t*>(L"App"); col.cx = 200; col.iSubItem = 1;
    ListView_InsertColumn(hList, 1, &col);

    col.pszText = const_cast<wchar_t*>(L"EXE"); col.cx = 360; col.iSubItem = 2;
    ListView_InsertColumn(hList, 2, &col);

    col.pszText = const_cast<wchar_t*>(L"Active"); col.cx = 70; col.iSubItem = 3;
    ListView_InsertColumn(hList, 3, &col);

    col.pszText = const_cast<wchar_t*>(L"Last Start"); col.cx = 140; col.iSubItem = 4;
    ListView_InsertColumn(hList, 4, &col);

    col.pszText = const_cast<wchar_t*>(L"Last Stop"); col.cx = 140; col.iSubItem = 5;
    ListView_InsertColumn(hList, 5, &col);
}

static void ListView_Populate(HWND hList, const std::vector<CamRow>& src, bool currentOnly) {
    ListView_DeleteAllItems(hList);

    int i = 0;
    for (const auto& r : src) {
        if (currentOnly && !r.activeNow) continue;

        std::wstring active = r.activeNow ? L"Yes" : L"No";
        std::wstring startS = FtToLocalString(r.startFt);
        std::wstring stopS = FtToLocalString(r.stopFt);

        LVITEMW item{};
        item.mask = LVIF_TEXT;
        item.iItem = i;
        item.pszText = const_cast<wchar_t*>(r.kind.c_str());
        int idx = ListView_InsertItem(hList, &item);

        ListView_SetItemText(hList, idx, 1, const_cast<wchar_t*>(r.app.c_str()));
        ListView_SetItemText(hList, idx, 2, const_cast<wchar_t*>(r.exe.c_str()));
        ListView_SetItemText(hList, idx, 3, const_cast<wchar_t*>(active.c_str()));
        ListView_SetItemText(hList, idx, 4, const_cast<wchar_t*>(startS.c_str()));
        ListView_SetItemText(hList, idx, 5, const_cast<wchar_t*>(stopS.c_str()));

        ++i;
    }
}

static void DoRefresh(HWND hWnd) {
    LoadConsentStore(g_rows);
    BOOL curOnly = (Button_GetCheck(g_hChkCurrent) == BST_CHECKED);
    ListView_Populate(g_hList, g_rows, curOnly);
    int parts[1] = { -1 };
    SendMessageW(g_hStatus, SB_SETPARTS, 1, (LPARAM)parts);
    SendMessageW(g_hStatus, SB_SETTEXT, 0, (LPARAM)L"Ready - Bob Paydar");
}

static void ResizeLayout(HWND hWnd) {
    RECT rc{}; GetClientRect(hWnd, &rc);

    SendMessageW(g_hStatus, WM_SIZE, 0, 0);
    RECT rcStatus{}; GetWindowRect(g_hStatus, &rcStatus);
    int sbHeight = rcStatus.bottom - rcStatus.top;

    int padding = 8;
    int btnW = 100, btnH = 28;
    int chkW = 140, chkH = 24;

    int topBarH = btnH + padding * 2;

    MoveWindow(g_hBtnRefresh, padding, padding, btnW, btnH, TRUE);
    MoveWindow(g_hChkCurrent, padding + btnW + 10, padding + 2, chkW, chkH, TRUE);

    int listY = topBarH;
    int listH = rc.bottom - listY - sbHeight;
    MoveWindow(g_hList, 0, listY, rc.right - rc.left, listH, TRUE);
}

static void InitListView(HWND hList) {
    DWORD ex = ListView_GetExtendedListViewStyle(hList);
    ex |= LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES;
    ListView_SetExtendedListViewStyle(hList, ex);
    ListView_SetupColumns(hList);
}

// ---------------------- Win32 Boilerplate -------------------
static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        g_hBtnRefresh = CreateWindowExW(0, L"BUTTON", L"Refresh",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            0, 0, 0, 0, hWnd, (HMENU)IDC_REFRESH, g_hInst, nullptr);

        g_hChkCurrent = CreateWindowExW(0, L"BUTTON", L"Current only",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP,
            0, 0, 0, 0, hWnd, (HMENU)IDC_CURONLY, g_hInst, nullptr);

        g_hList = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
            WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS,
            0, 0, 0, 0, hWnd, (HMENU)IDC_LIST, g_hInst, nullptr);

        g_hStatus = CreateWindowExW(0, STATUSCLASSNAMEW, L"",
            WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hWnd, (HMENU)IDC_STATUS, g_hInst, nullptr);

        InitListView(g_hList);
        ResizeLayout(hWnd);
        DoRefresh(hWnd);
        return 0;

    case WM_SIZE:
        ResizeLayout(hWnd);
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_REFRESH:
        case IDC_CURONLY:
            DoRefresh(hWnd);
            return 0;
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow) {
    g_hInst = hInstance;

    INITCOMMONCONTROLSEX icc{ sizeof(icc), ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES };
    InitCommonControlsEx(&icc);

    WNDCLASSEXW wc{ sizeof(wc) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hIconSm = wc.hIcon;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = kAppClass;

    if (!RegisterClassExW(&wc)) return 0;

    HWND hWnd = CreateWindowExW(0, kAppClass, kAppTitle,
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 980, 560,
        nullptr, nullptr, hInstance, nullptr);

    if (!hWnd) return 0;

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}
