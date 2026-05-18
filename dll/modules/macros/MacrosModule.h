#ifndef MacrosModule_H
#define MacrosModule_H

#include <Windows.h>

namespace MacrosModule
{
    // JNI-attached worker thread. Watches each configured macro key; on press
    // edge, finds the item by display-name substring in the hotbar (slots 0-8),
    // switches to that slot, right-clicks, then restores the previously-held
    // slot. Slot lookup is cached per-macro and re-scanned on miss.
    DWORD WINAPI init(LPVOID lpParam);
}

#endif
