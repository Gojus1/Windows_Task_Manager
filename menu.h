#ifndef MENU_H
#define MENU_H

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#define IDOK 1
#define IDCANCEL 2


#define IDM_FILE_NEW   1
#define IDM_FILE_OPEN  2
#define IDM_FILE_QUIT  3
#define IDM_EDIT_REDO  4
#define IDC_LISTVIEW   1001
#define IDT_REFRESH    2001
#define IDD_DIALOG1    101
#define IDC_STATIC     -1

#include <windows.h>
#include <commctrl.h>
#include <tlhelp32.h>

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK DialogProc(HWND, UINT, WPARAM, LPARAM);
void InitListView(HWND hwndParent);
void RefreshProcessList(HWND hwndList);

#endif
