// Windows Liquid Glass - Demo Entry Point
#include "demo.h"

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int nShow) {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    DemoApp app;
    if (!app.Init(hInst, nShow)) { CoUninitialize(); return 1; }
    int r = app.Run();
    app.Shutdown();
    printf("[Exit] Code %d\n", r);
    CoUninitialize();
    return r;
}
