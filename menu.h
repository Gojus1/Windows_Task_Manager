#ifndef MENU_H
#define MENU_H

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif


#include <windows.h>
#include <commctrl.h>
#include <tlhelp32.h>
#include <string>

// Control IDs
#define IDC_LISTVIEW 1001
#define IDT_REFRESH  2001

// Function declarations
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void InitListView(HWND hwndParent);
void RefreshProcessList(HWND hwndList);

#endif // TASKMGR_H
