#include "src/runtime/runtime.hpp"
#include <Windows.h>

namespace { DWORD WINAPI Bootstrap(void* module) { const HMODULE self = static_cast<HMODULE>(module); vectra::Runtime::Instance().Run(self); FreeLibraryAndExitThread(self, 0); } }

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) { DisableThreadLibraryCalls(module); const HANDLE thread = CreateThread(nullptr, 0, Bootstrap, module, 0, nullptr); if (thread) CloseHandle(thread); }
    if (reason == DLL_PROCESS_DETACH) vectra::Runtime::Instance().RequestStop();
    return TRUE;
}
