#include <windows.h>
#include "menu.h"

void AddMenus(HWND hwnd);

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            AddMenus(hwnd);
            break;

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDM_FILE_NEW:
                case IDM_FILE_OPEN:
                    MessageBeep(MB_ICONINFORMATION);
                    break;
                case IDM_FILE_QUIT:
                    SendMessage(hwnd, WM_CLOSE, 0, 0);
                    break;
            }
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

INT_PTR CALLBACK DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_INITDIALOG:
            return TRUE;

        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
                EndDialog(hwndDlg, LOWORD(wParam));
                return TRUE;
            }
            break;
    }
    return FALSE;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow) {

    MSG msg;
    WNDCLASSW wc = {0};
    wc.lpszClassName = L"SimpleMenuClass";
    wc.hInstance = hInstance;
    wc.hbrBackground = (HBRUSH)(COLOR_3DFACE+1);
    wc.lpfnWndProc = WndProc;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClassW(&wc);

    HWND hwndMain = CreateWindowW(wc.lpszClassName, L"Simple Menu",
                                  WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                  100, 100, 350, 250,
                                  NULL, NULL, hInstance, NULL);

    if (!hwndMain) {
        MessageBoxW(NULL, L"Main window creation failed!", L"Error", MB_OK | MB_ICONERROR);
        return 0;
    }

    // Create a modeless dialog
    HWND hwndDialog = CreateDialogParamW(
        hInstance,
        MAKEINTRESOURCEW(IDD_DIALOG1),
        hwndMain,
        DialogProc,
        0
    );

    if (!hwndDialog) {
        MessageBoxW(NULL, L"Dialog creation failed!", L"Error", MB_OK | MB_ICONERROR);
        return 0;
    }

    // Make it visible
    ShowWindow(hwndDialog, SW_SHOW);

    // Message loop
    while (GetMessage(&msg, NULL, 0, 0)) {
        if (!IsDialogMessage(hwndDialog, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int)msg.wParam;
}

void AddMenus(HWND hwnd) {
    HMENU hMenubar = CreateMenu();
    HMENU hMenu = CreateMenu();

    AppendMenuW(hMenu, MF_STRING, IDM_FILE_NEW,  L"&New");
    AppendMenuW(hMenu, MF_STRING, IDM_FILE_OPEN, L"&Open");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, IDM_FILE_QUIT, L"&Quit");

    AppendMenuW(hMenubar, MF_POPUP, (UINT_PTR)hMenu, L"&File");
    SetMenu(hwnd, hMenubar);
}
