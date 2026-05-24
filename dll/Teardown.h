#pragma once
#include <Windows.h>

// Orderly DLL unload. The autoclicker thread is the one that ultimately
// FreeLibraryAndExitThread's the DLL out of MC; this module ensures that by
// the time that happens, every OTHER worker thread has cleanly returned and
// the overlay's hooks are gone + drained.
//
// Without this, uninjecting while aim assist or jitter is mid-SendInput
// crashes MC: the DLL gets unmapped while one of the worker threads is
// still executing inside it, and the return address points into freed
// pages. Same hazard for MC's render thread sitting inside our
// wglSwapBuffers hook at the moment of unload.
namespace Teardown
{
    // Hand a duplicated thread handle to the registry. The handle is owned
    // by Teardown afterwards (closed during FinalizeAndUnload). Safe to call
    // from any thread; backed by a small mutex around a fixed-size array.
    void RegisterWorker(HANDLE thread);

    // Drives the full unload sequence. Intended to be called from the
    // autoclicker thread in place of a bare FreeLibraryAndExitThread.
    //
    //   1. Wait for every registered worker except the calling thread to
    //      return (bounded timeout — workers poll the destruct flag on a
    //      ~50ms cadence so this normally completes inside a few hundred
    //      ms; the timeout just prevents a wedged worker from blocking
    //      teardown forever).
    //   2. Shut the overlay down (disables MinHook hooks, restores wndproc,
    //      tears down ImGui).
    //   3. Sleep briefly so any render-thread call already in flight through
    //      our wglSwapBuffers hook has time to finish before the DLL is
    //      unmapped — MinHook's MH_DisableHook only restores the prologue,
    //      it does not drain callers already past it.
    //   4. FreeLibraryAndExitThread on the supplied module + exits the
    //      calling thread atomically.
    [[noreturn]] void FinalizeAndUnload(HMODULE instance);
}
