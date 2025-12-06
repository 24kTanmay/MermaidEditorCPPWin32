#include <windows.h>
#include "MainWindow.h"

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow)
{
    // Ignore unused parameters
    (void)hPrevInstance;
    (void)lpCmdLine;

    // Use HeapSetInformation to specify that the process heap should be efficient
    HeapSetInformation(NULL, HeapEnableTerminationOnCorruption, NULL, 0);

    // Initialize COM (needed for WebView2 and other shell apis)
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(hr)) return 1;

    MainWindow window;
    if (!window.Create(hInstance, nCmdShow))
    {
        return 0;
    }

    // Standard message loop
    MSG msg = {};
    while (GetMessageW(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    CoUninitialize();
    return (int)msg.wParam;
}
