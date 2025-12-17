#define UNICODE
#define _UNICODE
#define _WIN32_IE 0x0501
#define _WIN32_WINNT 0x0501

#include <windows.h>
#include <commctrl.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <map>

#pragma comment(lib, "comctl32")
#pragma comment(lib, "psapi")

#define IDC_LISTVIEW 101
#define IDC_RELOAD   102
#define IDT_REFRESH  1

static int  g_sortColumn = 0;
static BOOL g_sortAsc    = TRUE;

/* ---------------- Helpers ---------------- */
DWORD GetMemKB(DWORD pid) {
    HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!h) return 0;

    PROCESS_MEMORY_COUNTERS pmc;
    DWORD mem = 0;
    if (GetProcessMemoryInfo(h, &pmc, sizeof(pmc)))
        mem = (DWORD)(pmc.WorkingSetSize / 1024);

    CloseHandle(h);
    return mem;
}

int FindItemByPID(HWND lv, DWORD pid) {
    LVFINDINFO fi;
    ZeroMemory(&fi, sizeof(fi));
    fi.flags  = LVFI_PARAM;
    fi.lParam = pid;
    return ListView_FindItem(lv, -1, &fi);
}

/* ---------------- Sorting ---------------- */
int CALLBACK CompareProc(LPARAM p1, LPARAM p2, LPARAM sortParam) {
    HWND lv = (HWND)sortParam;
    wchar_t a[64], b[64];
    ListView_GetItemText(lv, FindItemByPID(lv,(DWORD)p1), g_sortColumn, a, 64);
    ListView_GetItemText(lv, FindItemByPID(lv,(DWORD)p2), g_sortColumn, b, 64);

    int r;
    if (g_sortColumn == 0) // Name
        r = lstrcmpi(a, b);
    else { // numeric columns
        int n1 = _wtoi(a);
        int n2 = _wtoi(b);
        r = (n1 > n2) ? 1 : (n1 < n2) ? -1 : 0;
    }
    return g_sortAsc ? r : -r;
}

/* ---------------- CPU Tracking ---------------- */
struct PROCINFO {
    ULONGLONG lastCPU;
    FILETIME lastSysTime;
};

std::map<DWORD, PROCINFO> g_procCPU; // previous CPU snapshot
int g_numCores = 0;

void InitCPUCores() {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    g_numCores = si.dwNumberOfProcessors;
}

/* ---------------- Refresh Logic ---------------- */
void RefreshProcessList(HWND lv) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return;

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);

    // mark all items unseen
    int count = ListView_GetItemCount(lv);
    for (int i = 0; i < count; i++) {
        LVITEMW it;
        ZeroMemory(&it, sizeof(it));
        it.mask = LVIF_PARAM;
        it.iItem = i;
        ListView_GetItem(lv, &it);
        it.lParam |= 0x80000000;
        ListView_SetItem(lv, &it);
    }

    FILETIME sysIdle, sysKernel, sysUser;
    GetSystemTimes(&sysIdle, &sysKernel, &sysUser);

    if (Process32FirstW(snap, &pe)) {
        do {
            DWORD pid = pe.th32ProcessID;
            int idx = FindItemByPID(lv, pid);

            wchar_t buf[64];

            if (idx == -1) {
                // insert new process
                LVITEMW it;
                ZeroMemory(&it, sizeof(it));
                it.mask   = LVIF_TEXT | LVIF_PARAM;
                it.iItem  = ListView_GetItemCount(lv);
                it.lParam = pid;
                it.pszText = pe.szExeFile;
                idx = ListView_InsertItem(lv, &it);

                wsprintfW(buf, L"%u", pid);
                ListView_SetItemText(lv, idx, 1, buf);
            } else {
                // clear seen flag
                LVITEMW it;
                ZeroMemory(&it, sizeof(it));
                it.mask = LVIF_PARAM;
                it.iItem = idx;
                ListView_GetItem(lv, &it);
                it.lParam &= ~0x80000000;
                ListView_SetItem(lv, &it);
            }

            // Memory
            wsprintfW(buf, L"%u", GetMemKB(pid));
            ListView_SetItemText(lv, idx, 2, buf);

            // CPU %
            HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
            int cpuPercent = 0;
            if (h) {
                FILETIME creation, exit, kernel, user;
                if (GetProcessTimes(h, &creation, &exit, &kernel, &user)) {
                    ULONGLONG totalTime = ((ULONGLONG)kernel.dwHighDateTime << 32 | kernel.dwLowDateTime)
                                        + ((ULONGLONG)user.dwHighDateTime << 32 | user.dwLowDateTime);

                    if (g_procCPU.find(pid) != g_procCPU.end()) {
                        ULONGLONG prevTime = g_procCPU[pid].lastCPU;
                        ULONGLONG delta = totalTime - prevTime;

                        // compute delta time in ms
                        FILETIME now;
                        GetSystemTimeAsFileTime(&now);
                        ULONGLONG sysDelta = ((ULONGLONG)now.dwHighDateTime << 32 | now.dwLowDateTime)
                                           - ((ULONGLONG)g_procCPU[pid].lastSysTime.dwHighDateTime << 32 | g_procCPU[pid].lastSysTime.dwLowDateTime);
                        if (sysDelta > 0)
                            cpuPercent = (int)((delta * 100) / sysDelta / g_numCores);
                    }

                    g_procCPU[pid].lastCPU = totalTime;
                    GetSystemTimeAsFileTime(&g_procCPU[pid].lastSysTime);
                }
                CloseHandle(h);
            }

            wsprintfW(buf, L"%d", cpuPercent);
            ListView_SetItemText(lv, idx, 3, buf);

        } while (Process32NextW(snap, &pe));
    }

    CloseHandle(snap);

    // remove exited processes
    for (int i = ListView_GetItemCount(lv) - 1; i >= 0; i--) {
        LVITEMW it;
        ZeroMemory(&it, sizeof(it));
        it.mask = LVIF_PARAM;
        it.iItem = i;
        ListView_GetItem(lv, &it);
        if (it.lParam & 0x80000000)
            ListView_DeleteItem(lv, i);
    }

    ListView_SortItemsEx(lv, CompareProc, (LPARAM)lv);
}

/* ---------------- Window Proc ---------------- */
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    static HWND lv;

    switch (msg) {
    case WM_CREATE: {
        CreateWindowW(L"BUTTON", L"Reload",
            WS_CHILD | WS_VISIBLE,
            10, 10, 80, 25,
            hwnd, (HMENU)IDC_RELOAD, NULL, NULL);

        lv = CreateWindowW(WC_LISTVIEWW, L"",
            WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL,
            10, 45, 760, 500,
            hwnd, (HMENU)IDC_LISTVIEW, NULL, NULL);

        ListView_SetExtendedListViewStyle(lv,
            LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

        LVCOLUMNW c;
        ZeroMemory(&c, sizeof(c));
        c.mask = LVCF_TEXT | LVCF_WIDTH;

        c.cx = 300; c.pszText = (LPWSTR)L"Process";
        ListView_InsertColumn(lv, 0, &c);

        c.cx = 100; c.pszText = (LPWSTR)L"PID";
        ListView_InsertColumn(lv, 1, &c);

        c.cx = 120; c.pszText = (LPWSTR)L"Memory KB";
        ListView_InsertColumn(lv, 2, &c);

        c.cx = 80; c.pszText = (LPWSTR)L"CPU %";
        ListView_InsertColumn(lv, 3, &c);

        InitCPUCores();
        SetTimer(hwnd, IDT_REFRESH, 1500, NULL);
        RefreshProcessList(lv);
        break;
    }

    case WM_TIMER:
        RefreshProcessList(lv);
        break;

    case WM_COMMAND:
        if (LOWORD(w) == IDC_RELOAD)
            RefreshProcessList(lv);
        break;

    case WM_NOTIFY: {
        NMLISTVIEW* n = (NMLISTVIEW*)l;
        if (n->hdr.code == LVN_COLUMNCLICK) {
            if (g_sortColumn == n->iSubItem)
                g_sortAsc = !g_sortAsc;
            else {
                g_sortColumn = n->iSubItem;
                g_sortAsc = TRUE;
            }
            ListView_SortItemsEx(lv, CompareProc, (LPARAM)lv);
        }
        break;
    }

    case WM_DESTROY:
        KillTimer(hwnd, IDT_REFRESH);
        PostQuitMessage(0);
        break;
    }

    return DefWindowProcW(hwnd, msg, w, l);
}

/* ---------------- Entry ---------------- */
int WINAPI WinMain(HINSTANCE h, HINSTANCE, LPSTR, int) {
    INITCOMMONCONTROLSEX ic;
    ic.dwSize = sizeof(ic);
    ic.dwICC  = ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&ic);

    WNDCLASSW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = h;
    wc.lpszClassName = L"TaskMgrLite";
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassW(&wc);

    CreateWindowW(wc.lpszClassName, L"Task Manager (WinAPI)",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        100, 100, 800, 600,
        NULL, NULL, h, NULL);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
