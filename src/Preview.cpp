#include "Preview.h"
#include <Shlwapi.h>
#include <sstream>
#include <atomic>
#include <functional>

// MinGW doesn't always have these libs linked by default via pragma
// CMakeLists should handle -lshlwapi -lole32 etc.

// Manual declaration in case header doesn't provide it properly on MinGW
extern "C" {
    HRESULT __stdcall CreateCoreWebView2EnvironmentWithOptions(
        PCWSTR browserExecutableFolder,
        PCWSTR userDataFolder,
        ICoreWebView2EnvironmentOptions* options,
        ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler* environmentCreatedHandler
    );
}

// -------------------------------------------------------------------------
// Minimal COM Callback Helpers for MinGW (Replacing WRL)
// -------------------------------------------------------------------------

template <typename Interface>
class CallbackBase : public Interface {
    std::atomic<long> m_refCount{1};
public:
    virtual ~CallbackBase() {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override {
        if (!ppvObject) return E_POINTER;
        // Simple implementation - just return this interface
        // In production, you'd check specific IIDs
        *ppvObject = static_cast<Interface*>(this);
        AddRef();
        return S_OK;
    }

    ULONG STDMETHODCALLTYPE AddRef() override {
        return ++m_refCount;
    }

    ULONG STDMETHODCALLTYPE Release() override {
        long res = --m_refCount;
        if (res == 0) delete this;
        return res;
    }
};

// Callback for Environment creation
class EnvironmentCompletedHandler : public CallbackBase<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler> {
    using CallbackFunc = std::function<void(HRESULT, ICoreWebView2Environment*)>;
    CallbackFunc m_callback;
public:
    EnvironmentCompletedHandler(CallbackFunc callback) : m_callback(callback) {}
    
    HRESULT STDMETHODCALLTYPE Invoke(HRESULT result, ICoreWebView2Environment* created_environment) override {
        m_callback(result, created_environment);
        return S_OK;
    }
};

// Callback for Controller creation
class ControllerCompletedHandler : public CallbackBase<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler> {
    using CallbackFunc = std::function<void(HRESULT, ICoreWebView2Controller*)>;
    CallbackFunc m_callback;
public:
    ControllerCompletedHandler(CallbackFunc callback) : m_callback(callback) {}

    HRESULT STDMETHODCALLTYPE Invoke(HRESULT result, ICoreWebView2Controller* controller) override {
        m_callback(result, controller);
        return S_OK;
    }
};

// -------------------------------------------------------------------------
// Preview Class Implementation
// -------------------------------------------------------------------------

Preview::Preview(HWND hParent) 
    : m_hParent(hParent), m_hwnd(NULL), m_isReady(false) {}

Preview::~Preview() {
    if (m_controller) {
        m_controller->Close();
        m_controller->Release();
    }
    if (m_webview) {
        m_webview->Release();
    }
}

void Preview::Create(const std::wstring& assetDir) {
    m_assetDir = assetDir;
    InitializeWebView();
}

void Preview::InitializeWebView() {
    // 1. Create the Environment Handler
    auto envHandler = new EnvironmentCompletedHandler(
        [this](HRESULT result, ICoreWebView2Environment* env) {
            if (FAILED(result)) return;

            // 2. Create the Controller Handler
            auto controllerHandler = new ControllerCompletedHandler(
                [this](HRESULT result, ICoreWebView2Controller* controller) {
                    OnWebViewInit(result, controller);
                }
            );

            // Create Controller
            env->CreateCoreWebView2Controller(m_hParent, controllerHandler);
            
            // Handler is self-deleting on Request/Release phases usually by the caller,
            // but providing it to Create... usually consumes a ref. 
            // Our CallbackBase starts with ref=1. 
            // If CreateCoreWebView2Controller takes ownership (AddRef), we should Release our initial ref if we don't want to hold it.
            // Standard COM pattern: Caller creates (Ref=1), passes to function. Function AddRefs if it keeps it. Caller Releases.
            controllerHandler->Release(); 
        }
    );

    // Create Environment
    // Explicitly cast to the interface pointer to avoid ambiguity
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler* handlerInterface = 
        static_cast<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler*>(envHandler);

    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(nullptr, nullptr, nullptr, handlerInterface);
    envHandler->Release(); // Release our initial reference
}

void Preview::OnWebViewInit(HRESULT result, ICoreWebView2Controller* controller) {
    if (FAILED(result) || !controller) return;

    m_controller = controller;
    m_controller->AddRef(); // Keep it alive

    HRESULT hr = m_controller->get_CoreWebView2(&m_webview);
    if (SUCCEEDED(hr)) {
        // m_webview is an out param, already AddRef'd by get_CoreWebView2
    }

    m_isReady = true;

    // Initial resize to match current request if any
    RECT rc;
    GetClientRect(m_hParent, &rc);
    SetBounds(0, 0, rc.right, rc.bottom);

    // Disable some features for "App" feel
    ICoreWebView2Settings* settings = nullptr;
    if (SUCCEEDED(m_webview->get_Settings(&settings)) && settings) {
        settings->put_AreDefaultContextMenusEnabled(FALSE);
        settings->put_AreDevToolsEnabled(TRUE); // Keep TRUE for debugging
        settings->put_IsStatusBarEnabled(FALSE);
        settings->Release();
    }

    // Navigate to our HTML shell using file:// URL
    std::wstring htmlPath = L"file:///" + m_assetDir + L"/index.html";
    // Replace backslashes with forward slashes for URL
    for (auto& c : htmlPath) {
        if (c == L'\\') c = L'/';
    }
    m_webview->Navigate(htmlPath.c_str());
}

void Preview::SetBounds(int x, int y, int w, int h) {
    if (m_controller) {
        RECT bounds;
        bounds.left = x; bounds.top = y;
        bounds.right = x + w; bounds.bottom = y + h;
        m_controller->put_Bounds(bounds);
    }
}

void Preview::UpdateDiagram(const std::string& mermaidCode) {
    if (!m_webview || !m_isReady) return;

    // Escape the string for JS
    std::string escaped;
    for (char c : mermaidCode) {
        if (c == '\n') escaped += "\\n";
        else if (c == '\r') continue;
        else if (c == '\\') escaped += "\\\\";
        else if (c == '\"') escaped += "\\\"";
        else if (c == '\'') escaped += "\\\'";
        else escaped += c;
    }

    // Call JS function "updateMermaid"
    std::string script = "updateMermaid(\"" + escaped + "\");";
    
    // Convert to wide string for generic execution
    int len = MultiByteToWideChar(CP_UTF8, 0, script.c_str(), -1, NULL, 0);
    std::wstring wscript(len, 0);
    MultiByteToWideChar(CP_UTF8, 0, script.c_str(), -1, &wscript[0], len);

    m_webview->ExecuteScript(wscript.c_str(), nullptr);
}
