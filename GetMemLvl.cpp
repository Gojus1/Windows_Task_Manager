#include <windows.h>

extern "C" __declspec(dllexport) int GetMemLvl(DWORD memKB) {
    if (memKB >= 1024 * 1024) return 2; // HIGH
    if (memKB >= 256 * 1024)  return 1; // MID
    return 0;
}
