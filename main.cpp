
#define _WIN32_IE 0x0501
#define _WIN32_WINNT 0x0501

#include <windows.h>
#include <commctrl.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <stdio.h>
#include <wchar.h>

#ifndef ARRAYSIZE
#define ARRAYSIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

#ifndef LVS_EX_FULLROWSELECT
#define LVS_EX_FULLROWSELECT 0x00000020
#endif
#ifndef LVS_EX_GRIDLINES
#define LVS_EX_GRIDLINES 0x00000001
#endif
#ifndef LVM_SETEXTENDEDLISTVIEWSTYLE
#define LVM_FIRST 0x1000
#define LVM_SETEXTENDEDLISTVIEWSTYLE (LVM_FIRST + 54)
#endif
#ifndef LVM_GETITEMTEXTW
#define LVM_GETITEMTEXTW (LVM_FIRST + 45)
#endif
#ifndef ListView_GetItemTextW
#define ListView_GetItemTextW(hwnd, i, iSubItem, pszText, cchTextMax) \
    do { LVITEMW _lvi = {0}; _lvi.iSubItem = (iSubItem); _lvi.cchTextMax = (cchTextMax); _lvi.pszText = (pszText); \
      SendMessageW((hwnd), LVM_GETITEMTEXTW, (WPARAM)(i), (LPARAM)&_lvi); } while(0)
#endif
#ifndef LVM_INSERTITEMW
#define LVM_INSERTITEMW (LVM_FIRST + 77)
#endif
#ifndef LVM_SETITEMTEXTW
#define LVM_SETITEMTEXTW (LVM_FIRST + 46)
#endif

#define IDC_LISTVIEW 101
#define IDC_RELOAD   102

static int g_sortColumn = 0;
static BOOL g_sortAscending = TRUE;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
void InitListView(HWND hwndList);
void RefreshProcessList(HWND hwndList);
void AddMenus(HWND hwnd);

static ULONGLONG FileTimeToULL(const FILETIME* ft) {
    return (((ULONGLONG)ft->dwHighDateTime) << 32) | ft->dwLowDateTime;
}

DWORD GetProcessMemoryKBLocal(DWORD pid) {
    HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!h) return 0;
    PROCESS_MEMORY_COUNTERS pmc;
    DWORD mem = 0;
    if (GetProcessMemoryInfo(h, &pmc, sizeof(pmc))) {
        mem = (DWORD)(pmc.WorkingSetSize / 1024);
    }
    CloseHandle(h);
    return mem;
}

typedef struct {
    DWORD pid;
    ULONGLONG procTime;
    wchar_t name[260];
} PROC_SAMPLE;

int CALLBACK CompareProc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
    HWND hwndList = (HWND)lParamSort;
    // lParam1/lParam2 are the lParam values we stored for each item (we store the item index at insert time)
    // Find the current item index for that lParam:
    LVFINDINFOW find = {0};
    find.flags = LVFI_PARAM;
    find.lParam = lParam1;
    int idx1 = ListView_FindItem(hwndList, -1, &find);
    find.lParam = lParam2;
    int idx2 = ListView_FindItem(hwndList, -1, &find);

    // If not found, fallback
    if (idx1 == -1) idx1 = (int)lParam1;
    if (idx2 == -1) idx2 = (int)lParam2;

    wchar_t s1[260] = {0}, s2[260] = {0};
    LVITEMW lvi1 = {0};
    lvi1.iSubItem = g_sortColumn;
    lvi1.cchTextMax = ARRAYSIZE(s1);
    lvi1.pszText = s1;
    SendMessageW(hwndList, LVM_GETITEMTEXTW, (WPARAM)idx1, (LPARAM)&lvi1);

    LVITEMW lvi2 = {0};
    lvi2.iSubItem = g_sortColumn;
    lvi2.cchTextMax = ARRAYSIZE(s2);
    lvi2.pszText = s2;
    SendMessageW(hwndList, LVM_GETITEMTEXTW, (WPARAM)idx2, (LPARAM)&lvi2);

    int cmp = 0;
    if (g_sortColumn == 0) {
        cmp = lstrcmpiW(s1, s2);
    } else {
        // numeric compare for PID, Memory, CPU
        int v1 = _wtoi(s1);
        int v2 = _wtoi(s2);
        cmp = (v1 < v2) ? -1 : (v1 > v2) ? 1 : 0;
    }

    return g_sortAscending ? cmp : -cmp;
}

// RefreshProcessList does two-snapshot sampling to compute CPU%
void RefreshProcessList(HWND hwndList)
{
    // Clear previous list
    ListView_DeleteAllItems(hwndList);

    // First snapshot: system times and process times
    FILETIME sysIdle1, sysKernel1, sysUser1;
    if (!GetSystemTimes(&sysIdle1, &sysKernel1, &sysUser1)) {
        // fallback: just populate without CPU%
    }
    ULONGLONG sysBusy1 = (FileTimeToULL(&sysKernel1) + FileTimeToULL(&sysUser1)) - FileTimeToULL(&sysIdle1);

    // collect process snapshot into dynamic buffer
    const int CAP = 8192;
    PROC_SAMPLE* arr = (PROC_SAMPLE*)malloc(sizeof(PROC_SAMPLE) * CAP);
    if (!arr) return;
    int count = 0;

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) {
        free(arr);
        return;
    }

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);

    if (Process32FirstW(snap, &pe)) {
        do {
            if (count >= CAP) break;
            arr[count].pid = pe.th32ProcessID;
            wcsncpy(arr[count].name, pe.szExeFile, ARRAYSIZE(arr[count].name) - 1);
            arr[count].name[ARRAYSIZE(arr[count].name)-1] = L'\0';

            // get process times (kernel + user)
            FILETIME ftCreation, ftExit, ftKernel, ftUser;
            ULONGLONG procTotal = 0;
            HANDLE hproc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pe.th32ProcessID);
            if (hproc) {
                if (GetProcessTimes(hproc, &ftCreation, &ftExit, &ftKernel, &ftUser)) {
                    procTotal = FileTimeToULL(&ftKernel) + FileTimeToULL(&ftUser);
                }
                CloseHandle(hproc);
            } else {
                procTotal = 0;
            }
            arr[count].procTime = procTotal;
            count++;
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);

    // Short sleep for sampling interval (blocks UI briefly)
    Sleep(400);

    // Second snapshot
    FILETIME sysIdle2, sysKernel2, sysUser2;
    if (!GetSystemTimes(&sysIdle2, &sysKernel2, &sysUser2)) {
        // fallback: set zeros
    }
    ULONGLONG sysBusy2 = (FileTimeToULL(&sysKernel2) + FileTimeToULL(&sysUser2)) - FileTimeToULL(&sysIdle2);

    ULONGLONG sysDelta = 0;
    if (sysBusy2 > sysBusy1) sysDelta = sysBusy2 - sysBusy1;
    if (sysDelta == 0) sysDelta = 1; // avoid div by zero

    // Now iterate stored processes and get second proc time and compute CPU%
    int index = 0;
    for (int i = 0; i < count; ++i) {
        DWORD pid = arr[i].pid;
        ULONGLONG procTotal2 = 0;
        FILETIME ftCreation, ftExit, ftKernel, ftUser;
        HANDLE hproc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
        if (hproc) {
            if (GetProcessTimes(hproc, &ftCreation, &ftExit, &ftKernel, &ftUser)) {
                procTotal2 = FileTimeToULL(&ftKernel) + FileTimeToULL(&ftUser);
            }
            CloseHandle(hproc);
        }
        ULONGLONG procDelta = 0;
        if (procTotal2 > arr[i].procTime) procDelta = procTotal2 - arr[i].procTime;
        double cpuPercent = ((double)procDelta / (double)sysDelta) * 100.0;
        if (cpuPercent < 0.0) cpuPercent = 0.0;
        if (cpuPercent > 100.0) {
            // On multi-core systems and depending on totals, the value might exceed 100 in some computations.
            // Clamp reasonably (we display integer percent).
            if (cpuPercent > 100.0 * 32) cpuPercent = 100.0; // absurdly large clamp
        }

        // Insert into list view
        LVITEMW item = {0};
        item.mask = LVIF_TEXT | LVIF_PARAM;
        item.iItem = index;
        item.iSubItem = 0;
        item.pszText = arr[i].name;
        item.lParam = index; // unique id for this row
        SendMessageW(hwndList, LVM_INSERTITEMW, 0, (LPARAM)&item);

        // PID
        wchar_t buf[64];
        wsprintfW(buf, L"%u", pid);
        LVITEMW itPid = {0};
        itPid.iItem = index;
        itPid.iSubItem = 1;
        itPid.pszText = buf;
        SendMessageW(hwndList, LVM_SETITEMTEXTW, index, (LPARAM)&itPid);

        // Memory
        DWORD memKB = GetProcessMemoryKBLocal(pid);
        wsprintfW(buf, L"%u", memKB);
        LVITEMW itMem = {0};
        itMem.iItem = index;
        itMem.iSubItem = 2;
        itMem.pszText = buf;
        SendMessageW(hwndList, LVM_SETITEMTEXTW, index, (LPARAM)&itMem);

        // CPU percent (integer display)
        int cpuInt = (int)(cpuPercent + 0.5);
        wsprintfW(buf, L"%d", cpuInt);
        LVITEMW itCpu = {0};
        itCpu.iItem = index;
        itCpu.iSubItem = 3;
        itCpu.pszText = buf;
        SendMessageW(hwndList, LVM_SETITEMTEXTW, index, (LPARAM)&itCpu);

        index++;
    }

    free(arr);
}

// Window procedure and UI
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static HWND hwndList = NULL;
    static HWND hwndReload = NULL;

    switch (msg) {
    case WM_CREATE:
    {
        // Create reload button
        hwndReload = CreateWindowW(L"BUTTON", L"Reload",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            10, 10, 100, 30, hwnd, (HMENU)IDC_RELOAD,
            ((LPCREATESTRUCT)lParam)->hInstance, NULL);

        // Create listview (large)
        hwndList = CreateWindowW(WC_LISTVIEWW, L"",
            WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | WS_BORDER,
            10, 50, 720, 480, hwnd, (HMENU)IDC_LISTVIEW,
            ((LPCREATESTRUCT)lParam)->hInstance, NULL);

        // ensure common controls are initialized for listview
        INITCOMMONCONTROLSEX icx = { sizeof(icx), ICC_LISTVIEW_CLASSES };
        InitCommonControlsEx(&icx);

        // set extended style
        SendMessageW(hwndList, LVM_SETEXTENDEDLISTVIEWSTYLE, 0,
            (LPARAM)(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES));

        // columns (use const strings to avoid const->nonconst warnings)
        static const wchar_t *c0 = L"Process Name";
        static const wchar_t *c1 = L"PID";
        static const wchar_t *c2 = L"Memory (KB)";
        static const wchar_t *c3 = L"CPU %";

        LVCOLUMNW col = {0};
        col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
        col.cx = 340; col.pszText = (LPWSTR)c0; ListView_InsertColumn(hwndList, 0, &col);
        col.cx = 100; col.pszText = (LPWSTR)c1; ListView_InsertColumn(hwndList, 1, &col);
        col.cx = 120; col.pszText = (LPWSTR)c2; ListView_InsertColumn(hwndList, 2, &col);
        col.cx = 80;  col.pszText = (LPWSTR)c3; ListView_InsertColumn(hwndList, 3, &col);

        AddMenus(hwnd);
        // initial population
        RefreshProcessList(hwndList);
        return 0;
    }

    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_RELOAD) {
            HWND lv = GetDlgItem(hwnd, IDC_LISTVIEW);
            if (lv) RefreshProcessList(lv);
        }
        // File->Reload and File->Quit menu IDs are 1 and 2 respectively (AddMenus below)
        if (LOWORD(wParam) == 1) {
            HWND lv = GetDlgItem(hwnd, IDC_LISTVIEW);
            if (lv) RefreshProcessList(lv);
        } else if (LOWORD(wParam) == 2) {
            PostQuitMessage(0);
        }
        return 0;

    case WM_NOTIFY:
    {
        LPNMHDR hdr = (LPNMHDR)lParam;
        if (hdr->idFrom == IDC_LISTVIEW && hdr->code == LVN_COLUMNCLICK) {
            NMLISTVIEW* nmlv = (NMLISTVIEW*)lParam;
            int col = nmlv->iSubItem;
            if (g_sortColumn == col) g_sortAscending = !g_sortAscending;
            else { g_sortColumn = col; g_sortAscending = TRUE; }

            HWND lv = GetDlgItem(hwnd, IDC_LISTVIEW);
            ListView_SortItemsEx(lv, CompareProc, (LPARAM)lv);
        }
        return 0;
    }

    case WM_SIZE:
        if (hwndList) {
            MoveWindow(hwndList, 10, 50, LOWORD(lParam) - 20, HIWORD(lParam) - 60, TRUE);
        }
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// Add a simple File menu with Reload and Quit
void AddMenus(HWND hwnd)
{
    HMENU menuBar = CreateMenu();
    HMENU menuFile = CreateMenu();

    AppendMenuW(menuFile, MF_STRING, 1, L"&Reload");
    AppendMenuW(menuFile, MF_STRING, 2, L"&Quit");

    AppendMenuW(menuBar, MF_POPUP, (UINT_PTR)menuFile, L"&File");
    SetMenu(hwnd, menuBar);
}

// WinMain
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nShow)
{
    WNDCLASSW wc = {0};
    wc.lpszClassName = L"TaskMgrClass";
    wc.hInstance = hInst;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    wc.lpfnWndProc = WndProc;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClassW(&wc);

    // create bigger window
    HWND hwnd = CreateWindowW(wc.lpszClassName, L"Task Manager (Sample)", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                              100, 100, 780, 600, NULL, NULL, hInst, NULL);
    if (!hwnd) return 1;

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}
