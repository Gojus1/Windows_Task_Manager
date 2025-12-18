#define UNICODE
#define _UNICODE
#define _WIN32_IE 0x0501
#define _WIN32_WINNT 0x0501

#include <windows.h>
#include <commctrl.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <shellapi.h>
#include <map>

#pragma comment(lib, "comctl32")
#pragma comment(lib, "psapi")
#pragma comment(lib, "shell32")

#define IDC_LISTVIEW 101
#define IDT_REFRESH  1

static int  g_sortColumn = 0;
static BOOL g_sortAsc    = TRUE;

HIMAGELIST g_hImageList = NULL;
int g_defaultIconIndex = -1;

/* ================= MEMORY LEVEL ================= */
enum MEM_LEVEL { MEM_LOW = 0, MEM_MED = 1, MEM_HIGH = 2 };

int GetMemLevel(DWORD memKB) {
    if (memKB >= 1024 * 1024) return MEM_HIGH; // ≥ 1 GB
    if (memKB >= 256 * 1024)  return MEM_MED;  // ≥ 256 MB
    return MEM_LOW;
}

/* ================= HELPERS ================= */
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

bool GetProcessPath(DWORD pid, wchar_t* path) {
    HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!h) return false;
    bool ok = GetModuleFileNameExW(h, NULL, path, MAX_PATH);
    CloseHandle(h);
    return ok;
}

int FindItemByPID(HWND lv, DWORD pid) {
    LVFINDINFO fi = {};
    fi.flags = LVFI_PARAM;
    fi.lParam = pid;
    return ListView_FindItem(lv, -1, &fi);
}

int GetProcessIconIndex(DWORD pid, const wchar_t* fallback) {
    wchar_t path[MAX_PATH];
    const wchar_t* use = fallback;

    if (GetProcessPath(pid, path))
        use = path;

    SHFILEINFO sfi = {};
    if (SHGetFileInfoW(use, 0, &sfi, sizeof(sfi),
        SHGFI_ICON | SHGFI_SMALLICON)) {

        int idx = ImageList_AddIcon(g_hImageList, sfi.hIcon);
        DestroyIcon(sfi.hIcon);
        return idx;
    }
    return g_defaultIconIndex;
}

/* ================= SORTING ================= */
int CALLBACK CompareProc(LPARAM p1, LPARAM p2, LPARAM param) {
    HWND lv = (HWND)param;
    wchar_t a[64], b[64];

    ListView_GetItemText(lv, FindItemByPID(lv,(DWORD)p1), g_sortColumn, a, 64);
    ListView_GetItemText(lv, FindItemByPID(lv,(DWORD)p2), g_sortColumn, b, 64);

    int r;
    if (g_sortColumn == 0)
        r = lstrcmpiW(a, b);
    else
        r = _wtoi(a) - _wtoi(b);

    return g_sortAsc ? r : -r;
}

/* ================= CPU TRACKING ================= */
struct CPUINFO {
    ULONGLONG cpu;
    FILETIME  time;
};

std::map<DWORD, CPUINFO> g_cpu;
int g_cores = 1;

void InitCPU() {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    g_cores = si.dwNumberOfProcessors;
}

/* ================= REFRESH ================= */
void RefreshProcessList(HWND lv) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return;

    PROCESSENTRY32W pe = { sizeof(pe) };
    FILETIME now;
    GetSystemTimeAsFileTime(&now);

    if (Process32FirstW(snap, &pe)) {
        do {
            DWORD pid = pe.th32ProcessID;
            int idx = FindItemByPID(lv, pid);

            if (idx == -1) {
                LVITEMW it = {};
                it.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE;
                it.iItem = ListView_GetItemCount(lv);
                it.lParam = pid;
                it.iImage = GetProcessIconIndex(pid, pe.szExeFile);
                it.pszText = pe.szExeFile;
                idx = ListView_InsertItem(lv, &it);
            }

            wchar_t buf[64];

            wsprintfW(buf, L"%u", pid);
            ListView_SetItemText(lv, idx, 1, buf);

            DWORD mem = GetMemKB(pid);
            wsprintfW(buf, L"%u", mem);
            ListView_SetItemText(lv, idx, 2, buf);

            int cpuPct = 0;
            HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
            if (h) {
                FILETIME c,e,k,u;
                if (GetProcessTimes(h,&c,&e,&k,&u)) {
                    ULONGLONG total =
                        ((ULONGLONG)k.dwHighDateTime << 32 | k.dwLowDateTime) +
                        ((ULONGLONG)u.dwHighDateTime << 32 | u.dwLowDateTime);

                    if (g_cpu.count(pid)) {
                        ULONGLONG dt = total - g_cpu[pid].cpu;
                        ULONGLONG ds =
                            ((ULONGLONG)now.dwHighDateTime << 32 | now.dwLowDateTime) -
                            ((ULONGLONG)g_cpu[pid].time.dwHighDateTime << 32 |
                             g_cpu[pid].time.dwLowDateTime);

                        if (ds)
                            cpuPct = (int)((dt * 100) / ds / g_cores);
                    }
                    g_cpu[pid] = { total, now };
                }
                CloseHandle(h);
            }

            wsprintfW(buf, L"%d", cpuPct);
            ListView_SetItemText(lv, idx, 3, buf);

            int memLevel = GetMemLevel(mem);

            LVITEMW it = {};
            it.mask = LVIF_PARAM;
            it.iItem = idx;
            ListView_GetItem(lv, &it);

            if (cpuPct > 255) cpuPct = 255;

            it.lParam =
                (pid & 0xFFFF) |
                ((memLevel & 0xFF) << 16) |
                ((cpuPct & 0xFF) << 24);

            ListView_SetItem(lv, &it);

        } while (Process32NextW(snap, &pe));
    }

    CloseHandle(snap);
    ListView_SortItemsEx(lv, CompareProc, (LPARAM)lv);
}

/* ================= WINDOW ================= */
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    static HWND lv;

    switch (msg) {

    case WM_CREATE: {
        lv = CreateWindowW(WC_LISTVIEWW, L"",
            WS_CHILD | WS_VISIBLE | LVS_REPORT,
            10, 10, 780, 540,
            hwnd, (HMENU)IDC_LISTVIEW, NULL, NULL);

        ListView_SetExtendedListViewStyle(lv,
            LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

        g_hImageList = ImageList_Create(
            GetSystemMetrics(SM_CXSMICON),
            GetSystemMetrics(SM_CYSMICON),
            ILC_COLOR32 | ILC_MASK,
            1, 64);

        ListView_SetImageList(lv, g_hImageList, LVSIL_SMALL);

        g_defaultIconIndex = ImageList_AddIcon(
            g_hImageList, LoadIconW(NULL, IDI_APPLICATION));

        LVCOLUMNW c = { LVCF_TEXT | LVCF_WIDTH };

        c.cx = 300; c.pszText = (LPWSTR)L"Process";
        ListView_InsertColumn(lv, 0, &c);

        c.cx = 100; c.pszText = (LPWSTR)L"PID";
        ListView_InsertColumn(lv, 1, &c);

        c.cx = 120; c.pszText = (LPWSTR)L"Memory KB";
        ListView_InsertColumn(lv, 2, &c);

        c.cx = 80; c.pszText = (LPWSTR)L"CPU %";
        ListView_InsertColumn(lv, 3, &c);

        InitCPU();
        SetTimer(hwnd, IDT_REFRESH, 1500, NULL);
        RefreshProcessList(lv);
        return 0;
    }

    case WM_TIMER:
        RefreshProcessList(lv);
        return 0;

    case WM_NOTIFY: {
        NMHDR* hdr = (NMHDR*)l;

        if (hdr->hwndFrom == lv && hdr->code == NM_CUSTOMDRAW) {
            LPNMLVCUSTOMDRAW cd = (LPNMLVCUSTOMDRAW)l;

            if (cd->nmcd.dwDrawStage == CDDS_PREPAINT)
                return CDRF_NOTIFYITEMDRAW;

            if (cd->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
                LVITEMW it = {};
                it.mask = LVIF_PARAM;
                it.iItem = (int)cd->nmcd.dwItemSpec;
                ListView_GetItem(lv, &it);

                int cpu = (it.lParam >> 24) & 0xFF;
                int mem = (it.lParam >> 16) & 0xFF;

                if (cpu >= 20)
                    cd->clrTextBk = RGB(255,180,180);
                else if (cpu >= 1)
                    cd->clrTextBk = RGB(255,240,170);
                else if (mem == MEM_HIGH)
                    cd->clrTextBk = RGB(200,200,255);
                else if (mem == MEM_MED)
                    cd->clrTextBk = RGB(240,240,255);

                return CDRF_DODEFAULT;
            }
        }

        if (hdr->code == LVN_COLUMNCLICK) {
            NMLISTVIEW* n = (NMLISTVIEW*)l;
            g_sortAsc = (g_sortColumn == n->iSubItem) ? !g_sortAsc : TRUE;
            g_sortColumn = n->iSubItem;
            ListView_SortItemsEx(lv, CompareProc, (LPARAM)lv);
        }
        return 0;
    }

    case WM_DESTROY:
        KillTimer(hwnd, IDT_REFRESH);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, w, l);
}

/* ================= ENTRY ================= */
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
        100, 100, 820, 600,
        NULL, NULL, h, NULL);

    MSG msg;
    while (GetMessage(&msg,NULL,0,0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
