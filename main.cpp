#define _WIN32_IE 0x0500
#include <windows.h>
#include <commctrl.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <string>
//#pragma comment(lib, "comctl32.lib") // Can be removed; linked via -lcomctl32

#define IDC_LISTVIEW 101
#define IDT_REFRESH  1

// Forward declarations
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
void InitListView(HWND hwndList);
void RefreshProcessList(HWND hwndList);
void AddMenus(HWND hwnd);

// Optional: function to get memory usage if plugin not used
DWORD GetProcessMemoryKBLocal(DWORD pid) {
HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
if (!hProcess) return 0;

PROCESS_MEMORY_COUNTERS pmc;
DWORD mem = 0;
if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
    mem = pmc.WorkingSetSize / 1024; // in KB
}
CloseHandle(hProcess);
return mem;

}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
LPSTR lpCmdLine, int nCmdShow) {
MSG msg;
WNDCLASSW wc = {0};

wc.lpszClassName = L"TaskMgrClass";
wc.hInstance     = hInstance;
wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
wc.lpfnWndProc   = WndProc;
wc.hCursor       = LoadCursor(NULL, IDC_ARROW);

RegisterClassW(&wc);

HWND hwnd = CreateWindowW(wc.lpszClassName, L"Task Manager",
                          WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                          100, 100, 500, 400,
                          NULL, NULL, hInstance, NULL);

if (!hwnd) {
    MessageBoxW(NULL, L"Failed to create main window!", L"Error", MB_OK | MB_ICONERROR);
    return 1;
}
InitCommonControls();

while (GetMessage(&msg, NULL, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
}
return (int) msg.wParam;

}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
static HWND hwndList;

switch (msg) {
case WM_CREATE:
    hwndList = CreateWindowW(WC_LISTVIEWW, L"",
                             WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL,
                             10, 10, 460, 330,
                             hwnd, (HMENU)IDC_LISTVIEW,
                             ((LPCREATESTRUCT)lParam)->hInstance, NULL);

    InitListView(hwndList);
    RefreshProcessList(hwndList);

    // Add menus
    AddMenus(hwnd);

    // Set timer to refresh every 2 seconds
    SetTimer(hwnd, IDT_REFRESH, 2000, NULL);
    break;

case WM_TIMER:
    if (wParam == IDT_REFRESH) {
        RefreshProcessList(hwndList);
    }
    break;

case WM_DESTROY:
    KillTimer(hwnd, IDT_REFRESH);
    PostQuitMessage(0);
    break;
}

return DefWindowProcW(hwnd, msg, wParam, lParam);


}

void InitListView(HWND hwndList) {
// Set extended styles (full row select + grid lines)
SendMessage(hwndList,
LVM_SETEXTENDEDLISTVIEWSTYLE,
0,
LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

// Insert columns
LVCOLUMNW col = {0};
col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;

col.cx = 300;
col.pszText = (LPWSTR)L"Process Name";
ListView_InsertColumn(hwndList, 0, &col);

col.cx = 100;
col.pszText = (LPWSTR)L"PID";
ListView_InsertColumn(hwndList, 1, &col);

col.cx = 100;
col.pszText = (LPWSTR)L"Memory (KB)";
ListView_InsertColumn(hwndList, 2, &col);

}

void AddMenus(HWND hwnd) {
HMENU hMenubar = CreateMenu();
HMENU hMenu    = CreateMenu();

AppendMenuW(hMenu, MF_STRING, 1, L"&Refresh");
AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
AppendMenuW(hMenu, MF_STRING, 2, L"&Quit");

AppendMenuW(hMenubar, MF_POPUP, (UINT_PTR)hMenu, L"&File");

SetMenu(hwnd, hMenubar);

}

void RefreshProcessList(HWND hwndList) {
ListView_DeleteAllItems(hwndList);

HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
if (hSnapshot == INVALID_HANDLE_VALUE) return;

PROCESSENTRY32W pe;
pe.dwSize = sizeof(PROCESSENTRY32W);

if (Process32FirstW(hSnapshot, &pe)) {
    int index = 0;
    do {
        // First column: process name
        LVITEMW lvItem0 = {0};
        lvItem0.mask = LVIF_TEXT;
        lvItem0.iItem = index;
        lvItem0.iSubItem = 0;
        lvItem0.pszText = (LPWSTR)pe.szExeFile;
        SendMessageW(hwndList, LVM_INSERTITEMW, 0, (LPARAM)&lvItem0);

        // PID column
        wchar_t pidBuf[32];
        wsprintfW(pidBuf, L"%u", pe.th32ProcessID);
        LVITEMW lvItem1 = {0};
        lvItem1.mask = LVIF_TEXT;
        lvItem1.iItem = index;
        lvItem1.iSubItem = 1;
        lvItem1.pszText = pidBuf;
        SendMessageW(hwndList, LVM_SETITEMTEXTW, (WPARAM)index, (LPARAM)&lvItem1);

        // Memory column
        DWORD memKB = GetProcessMemoryKBLocal(pe.th32ProcessID);
        wchar_t memBuf[32];
        wsprintfW(memBuf, L"%u", memKB);
        LVITEMW lvItem2 = {0};
        lvItem2.mask = LVIF_TEXT;
        lvItem2.iItem = index;
        lvItem2.iSubItem = 2;
        lvItem2.pszText = memBuf;
        SendMessageW(hwndList, LVM_SETITEMTEXTW, (WPARAM)index, (LPARAM)&lvItem2);

        index++;
    } while (Process32NextW(hSnapshot, &pe));
}


CloseHandle(hSnapshot);

}
