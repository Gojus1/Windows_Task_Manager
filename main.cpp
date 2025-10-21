#include "menu.h"
#include <windows.h>
#include <commctrl.h>
#pragma comment(lib, "comctl32.lib")
#include <tlhelp32.h>
// #include <sfc.h>

void AddMenus(HWND hwnd);

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
    //AddMenus(hwnd);
}

// SfcIsFileProtected(
//     NULL, 
//     FILE_NAME
// );

// SfcIsKeyProtected(
//     HKEY_LOCAL_MACHINE,
//     KEY_NAME,
//     KEY_READ
// );



void InitListView(HWND hwndList) {
    LVCOLUMNW lvCol = {0};
    lvCol.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;

    lvCol.pszText = (LPWSTR)L"Process Name";
    lvCol.cx = 300;
    ListView_InsertColumn(hwndList, 0, &lvCol);

    lvCol.pszText = (LPWSTR)L"PID";
    lvCol.cx = 100;
    ListView_InsertColumn(hwndList, 1, &lvCol);

    lvCol.pszText = (LPWSTR)L"Memory Usage";
    lvCol.cx = 100;
    ListView_InsertColumn(hwndList, 2, &lvCol);

}

void AddMenus(HWND hwnd) {
    HMENU hMenubar = CreateMenu();
    HMENU hMenu    = CreateMenu();

    AppendMenuW(hMenu, MF_STRING, IDM_FILE_NEW,  L"&New");
    AppendMenuW(hMenu, MF_STRING, IDM_FILE_OPEN, L"&Open");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, IDM_FILE_QUIT, L"&Quit");

    AppendMenuW(hMenu, MF_STRING, IDM_EDIT_REDO, L"&Redo");

    AppendMenuW(hMenubar, MF_POPUP, (UINT_PTR)hMenu, L"&File");
    AppendMenuW(hMenubar, MF_STRING, 0, L"&Edit");
    SetMenu(hwnd, hMenubar);
}

void RefreshProcessList(HWND hwndList) {
    ListView_DeleteAllItems(hwndList);

    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return;

    PROCESSENTRY32 pe;  
    pe.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(hSnapshot, &pe)) {
        int index = 0;
        do {
            LVITEMW lvItem = {0};
            lvItem.mask = LVIF_TEXT;
            lvItem.iItem = index;
            lvItem.pszText = pe.szExeFile;
            ListView_InsertItem(hwndList, &lvItem);
            wchar_t pidBuf[32];
            wsprintf(pidBuf, L"%u", pe.th32ProcessID);
            ListView_SetItemText(hwndList, index, 1, pidBuf);

            index++;
        } while (Process32Next(hSnapshot, &pe));
    }

    CloseHandle(hSnapshot);
}


// #include "menu.h"
// #include <windows.h>


// void AddMenus(HWND);

// LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
//     switch (msg) {
//         case WM_CREATE:
//             AddMenus(hwnd);
//             break;

//         case WM_COMMAND:
//             switch (LOWORD(wParam)) {
//                 case IDM_FILE_NEW:
//                 case IDM_FILE_OPEN:
//                     MessageBeep(MB_ICONINFORMATION);
//                     break;
//                 case IDM_FILE_QUIT:
//                     SendMessage(hwnd, WM_CLOSE, 0, 0);
//                     break;
//             }
//             break;

//         case WM_DESTROY:
//             PostQuitMessage(0);
//             break;
//     }

//     return DefWindowProcW(hwnd, msg, wParam, lParam);
// }

// INT_PTR CALLBACK DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
//     switch (uMsg) {
//         case WM_INITDIALOG:
//             return TRUE;
//         case WM_COMMAND:
//             if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
//                 EndDialog(hwndDlg, LOWORD(wParam));
//                 return TRUE;
//             }
//             break;
//     }
//     return FALSE;
// }

// int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
//                    LPSTR lpCmdLine, int nCmdShow) {
//     MSG msg;    
//     WNDCLASSW wc = {0};

//     wc.lpszClassName = L"SimpleMenuClass";
//     wc.hInstance     = hInstance;
//     wc.hbrBackground = (HBRUSH)(COLOR_3DFACE+1);
//     wc.lpfnWndProc   = WndProc;
//     wc.hCursor       = LoadCursor(0, IDC_ARROW);

//     RegisterClassW(&wc);

//     HWND hwndMain = CreateWindowW(wc.lpszClassName, L"Simple menu",
//                        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
//                        100, 100, 350, 250,
//                        0, 0, hInstance, 0);


//     HWND hwndDialog = CreateDialogParamW(
//     hInstance,
//     MAKEINTRESOURCEW(IDD_DIALOG1),
//     hwndMain,
//     DialogProc,
//     0
// );

//     if (!hwndDialog) {
//         MessageBoxW(NULL, L"Dialog creation failed!", L"Error", MB_OK | MB_ICONERROR);
//     } else {
//         ShowWindow(hwndDialog, SW_SHOW); // 👈 this makes it visible
// }

//     if (!hwndMain) {
//         MessageBoxW(NULL, L"Window creation failed!", L"Error", MB_OK);
//         return 0;
//     }

//     CreateWindowW(
//         L"BUTTON",  
//         L"OK",      
//         WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,  
//         10, 10, 100, 100,  
//         hwndMain,   
//         NULL,       
//         hInstance,  
//         NULL);      

//     // Message loop
//     while (GetMessage(&msg, NULL, 0, 0)) {
//     if (!IsDialogMessage(hwndDialog, &msg)) {
//         TranslateMessage(&msg);
//         DispatchMessage(&msg);
//     }
// }

//     return (int) msg.wParam;
// }


// void AddMenus(HWND hwnd) {
//     HMENU hMenubar = CreateMenu();
//     HMENU hMenu    = CreateMenu();

//     AppendMenuW(hMenu, MF_STRING, IDM_FILE_NEW,  L"&New");
//     AppendMenuW(hMenu, MF_STRING, IDM_FILE_OPEN, L"&Open");
//     AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
//     AppendMenuW(hMenu, MF_STRING, IDM_FILE_QUIT, L"&Quit");

//     AppendMenuW(hMenu, MF_STRING, IDM_EDIT_REDO, L"&Redo");

//     AppendMenuW(hMenubar, MF_POPUP, (UINT_PTR)hMenu, L"&File");
//     AppendMenuW(hMenubar, MF_STRING, 0, L"&Edit");
//     SetMenu(hwnd, hMenubar);
// }