#pragma once
#include <windows.h>
#include <string>
#include "WebView2.h"

// Forward declaration if header not present yet to avoid IntelliSense errors in editor
struct ICoreWebView2Controller;
struct ICoreWebView2;

class Preview {
public:
    Preview(HWND hParent);
    ~Preview();

    // Asynchronous init
    void Create(const std::wstring& assetPath);
    
    // Layout
    void SetBounds(int x, int y, int w, int h);
    HWND GetHwnd() const { return m_hwnd; } 

    // Commands
    void UpdateDiagram(const std::string& mermaidCode);

private:
    HWND m_hParent;
    HWND m_hwnd;
    
    ICoreWebView2Controller* m_controller = nullptr;
    ICoreWebView2* m_webview = nullptr;
    
    std::wstring m_assetDir;
    bool m_isReady;

    void InitializeWebView();
    void OnWebViewInit(HRESULT result, ICoreWebView2Controller* controller);
};
