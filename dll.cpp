#include <windows.h>

extern "C" __declspec(dllexport) int GetMemLvl(DWORD memKB) {
    if (memKB >= 1024 * 1024) return 2; // MEM_HIGH
    if (memKB >= 256 * 1024)  return 1; // MEM_MED
    return 0; // MEM_LOW
}
