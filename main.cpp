#define UNICODE
#define _UNICODE
#define _WIN32_IE 0x0501
#define _WIN32_WINNT 0x0501

#include <windows.h>
#include <commctrl.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <map>

#pragma comment(lib, "comctl32")
#pragma comment(lib, "psapi")
#pragma comment(lib, "shell32")
#pragma comment(lib, "kernel32")

#define IDC_LISTVIEW 101
#define IDC_RELOAD   102
#define IDT_REFRESH  1

static int  g_sortColumn = 0;
static BOOL g_sortAsc    = TRUE;

HIMAGELIST g_hImageList = NULL;

int g_defaultIconIndex = -1;
enum MEM_LEVEL {MEM_LOW = 0, MEM_MED = 1, MEM_HIGH = 2};


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

bool GetProcessImagePath(DWORD pid, wchar_t* path, DWORD size) {
    HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!h) return false;

    DWORD len = GetModuleFileNameExW(h, NULL, path, size);
    CloseHandle(h);
    return len > 0;
}

int FindItemByPID(HWND lv, DWORD pid) {
    LVFINDINFO fi = {};
    fi.flags  = LVFI_PARAM;
    fi.lParam = pid;
    return ListView_FindItem(lv, -1, &fi);
}

int GetProcessIconIndex(DWORD pid, const wchar_t* fallbackName) {
    wchar_t fullPath[MAX_PATH] = {};

    const wchar_t* pathToUse = fallbackName;

    if (GetProcessImagePath(pid, fullPath, MAX_PATH)) {
        pathToUse = fullPath;
    }

    SHFILEINFO sfi = {};
    if (SHGetFileInfoW(
            pathToUse,
            0,
            &sfi,
            sizeof(sfi),
            SHGFI_ICON | SHGFI_SMALLICON)) {

        int idx = ImageList_AddIcon(g_hImageList, sfi.hIcon);
        DestroyIcon(sfi.hIcon);
        return idx;
    }

    return g_defaultIconIndex; //default icon
}

/* ---------------- Sorting ---------------- */
int CALLBACK CompareProc(LPARAM p1, LPARAM p2, LPARAM sortParam) {
    HWND lv = (HWND)sortParam;
    wchar_t a[64], b[64];

    ListView_GetItemText(lv, FindItemByPID(lv,(DWORD)p1), g_sortColumn, a, 64);
    ListView_GetItemText(lv, FindItemByPID(lv,(DWORD)p2), g_sortColumn, b, 64);

    int r;
    if (g_sortColumn == 0)
        r = lstrcmpi(a, b);
    else {
        int n1 = _wtoi(a);
        int n2 = _wtoi(b);
        r = (n1 > n2) ? 1 : (n1 < n2) ? -1 : 0;
    }
    return g_sortAsc ? r : -r;
}

/* ---------------- CPU Tracking ---------------- */
struct PROCINFO {
    ULONGLONG lastCPU;
    FILETIME  lastSysTime;
};

std::map<DWORD, PROCINFO> g_procCPU;
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

    PROCESSENTRY32W pe = { sizeof(pe) };

    int count = ListView_GetItemCount(lv);
    for (int i = 0; i < count; i++) {
        LVITEMW it = {};
        it.mask = LVIF_PARAM;
        it.iItem = i;
        ListView_GetItem(lv, &it);
        it.lParam |= 0x80000000;
        ListView_SetItem(lv, &it);
    }

    if (Process32FirstW(snap, &pe)) {
        do {
            DWORD pid = pe.th32ProcessID;
            int idx = FindItemByPID(lv, pid);

            wchar_t buf[64];

            if (idx == -1) {
                int iconIndex = GetProcessIconIndex(pid, pe.szExeFile);

                LVITEMW it = {};
                it.mask   = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE;
                it.iItem  = ListView_GetItemCount(lv);
                it.lParam = pid;
                it.iImage = iconIndex;
                it.pszText = pe.szExeFile;

                idx = ListView_InsertItem(lv, &it);

                wsprintfW(buf, L"%u", pid);
                ListView_SetItemText(lv, idx, 1, buf);
            } else {
                LVITEMW it = {};
                it.mask = LVIF_PARAM;
                it.iItem = idx;
                ListView_GetItem(lv, &it);
                it.lParam &= ~0x80000000;
                ListView_SetItem(lv, &it);
            }

            wsprintfW(buf, L"%u", GetMemKB(pid));
            ListView_SetItemText(lv, idx, 2, buf);

            int cpuPercent = 0;
            HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
            if (h) {
                FILETIME c,e,k,u;
                if (GetProcessTimes(h,&c,&e,&k,&u)) {
                    ULONGLONG total =
                        ((ULONGLONG)k.dwHighDateTime << 32 | k.dwLowDateTime) +
                        ((ULONGLONG)u.dwHighDateTime << 32 | u.dwLowDateTime);

                    FILETIME now;
                    GetSystemTimeAsFileTime(&now);

                    if (g_procCPU.count(pid)) {
                        ULONGLONG dt = total - g_procCPU[pid].lastCPU;
                        ULONGLONG ds =
                            ((ULONGLONG)now.dwHighDateTime << 32 | now.dwLowDateTime) -
                            ((ULONGLONG)g_procCPU[pid].lastSysTime.dwHighDateTime << 32 |
                             g_procCPU[pid].lastSysTime.dwLowDateTime);

                        if (ds)
                            cpuPercent = (int)((dt * 100) / ds / g_numCores);
                    }

                    g_procCPU[pid] = { total, now };
                }
                CloseHandle(h);
            }

            wsprintfW(buf, L"%d", cpuPercent);
            ListView_SetItemText(lv, idx, 3, buf);

            LVITEMW it = {};
            it.mask = LVIF_PARAM;
            it.iItem = idx;
            ListView_GetItem(lv, &it);
            it.lParam = (it.lParam & 0xFFFF) | (cpuPercent << 16);
            ListView_SetItem(lv, &it);


        } while (Process32NextW(snap, &pe));
    }

    CloseHandle(snap);

    for (int i = ListView_GetItemCount(lv) - 1; i >= 0; i--) {
        LVITEMW it = {};
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
        lv = CreateWindowW(WC_LISTVIEWW, L"",
            WS_CHILD | WS_VISIBLE | LVS_REPORT,
            10, 45, 760, 500,
            hwnd, (HMENU)IDC_LISTVIEW, NULL, NULL);

        ListView_SetExtendedListViewStyle(lv,
            LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

        g_hImageList = ImageList_Create(
            GetSystemMetrics(SM_CXSMICON),
            GetSystemMetrics(SM_CYSMICON),
            ILC_COLOR32 | ILC_MASK,
            1, 64);

        ListView_SetImageList(lv, g_hImageList, LVSIL_SMALL);

        HICON hDefault = LoadIconW(NULL, IDI_APPLICATION);
        g_defaultIconIndex = ImageList_AddIcon(g_hImageList, hDefault);

        LVCOLUMNW c = { LVCF_TEXT | LVCF_WIDTH };

        c.cx = 300; c.pszText = (LPWSTR)L"Process";   ListView_InsertColumn(lv, 0, &c);
        c.cx = 100; c.pszText = (LPWSTR)L"PID";       ListView_InsertColumn(lv, 1, &c);
        c.cx = 120; c.pszText = (LPWSTR)L"Memory KB"; ListView_InsertColumn(lv, 2, &c);
        c.cx = 80;  c.pszText = (LPWSTR)L"CPU %";     ListView_InsertColumn(lv, 3, &c);

        InitCPUCores();
        SetTimer(hwnd, IDT_REFRESH, 1500, NULL);
        RefreshProcessList(lv);
        break;
    }

    case WM_TIMER:
        RefreshProcessList(lv);
        break;

    case WM_NOTIFY: {
        NMHDR* hdr = (NMHDR*)l;

        if (hdr->hwndFrom == lv && hdr->code == NM_CUSTOMDRAW) {
            LPNMLVCUSTOMDRAW cd = (LPNMLVCUSTOMDRAW)l;

            switch (cd->nmcd.dwDrawStage) {

            case CDDS_PREPAINT:
                return CDRF_NOTIFYITEMDRAW;

            case CDDS_ITEMPREPAINT: {
                int item = (int)cd->nmcd.dwItemSpec;

                LVITEMW it = {};
                it.mask = LVIF_PARAM;
                it.iItem = item;
                ListView_GetItem(lv, &it);

                int cpu = (int)((it.lParam >> 16) & 0xFFFF);

            //Cpu usage to colors
                if (cpu >= 30) {
                    cd->clrTextBk = RGB(255, 150, 150);
                } else if (cpu >= 10) {
                    cd->clrTextBk = RGB(255, 200, 120);
                } else if (cpu >= 1) {
                    cd->clrTextBk = RGB(255, 255, 180);
                }

                return CDRF_DODEFAULT;
            }
        }
    }

    if (hdr->code == LVN_COLUMNCLICK) {
        NMLISTVIEW* n = (NMLISTVIEW*)l;
        g_sortAsc = (g_sortColumn == n->iSubItem) ? !g_sortAsc : TRUE;
        g_sortColumn = n->iSubItem;
        ListView_SortItemsEx(lv, CompareProc, (LPARAM)lv);
    }
    break;
}

        break;

    case WM_DESTROY:
        KillTimer(hwnd, IDT_REFRESH);
        PostQuitMessage(0);
        break;
    }
    return DefWindowProcW(hwnd, msg, w, l);
}

/* ---------------- Entry ---------------- */
int WINAPI WinMain(HINSTANCE h, HINSTANCE, LPSTR, int) {
    INITCOMMONCONTROLSEX ic = { sizeof(ic), ICC_LISTVIEW_CLASSES };
    InitCommonControlsEx(&ic);

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = h;
    wc.lpszClassName = L"TaskMgrLite";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
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
