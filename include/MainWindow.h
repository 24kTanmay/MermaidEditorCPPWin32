#pragma once
#include <windows.h>
#include <commctrl.h>
#include <string>
#include <memory>

class Editor;
class Preview;

class MainWindow {
public:
    MainWindow();
    ~MainWindow();

    bool Create(HINSTANCE hInstance, int nCmdShow);
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

protected:
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    void OnSize(int width, int height);
    void OnCommand(int id);
    void OnCreate();
    void OnDestroy();
    void OnNotify(WPARAM wParam, LPARAM lParam);

    // File operations
    void NewFile();
    void OpenFile();
    void SaveFile();
    void SaveFileAs();
    void ExportPNG();
    void ExportSVG();
    void LoadContent(const std::wstring& path);
    void SaveContent(const std::wstring& path);

    // Edit operations  
    void Undo();
    void Redo();
    void Cut();
    void Copy();
    void Paste();
    void SelectAll();

    // View operations
    void Render();
    void ToggleSplit();

    // UI Creation
    void CreateToolbar();
    void CreateStatusBar();
    void UpdateStatusBar();
    void SetupDarkMenu();

    // Layout
    void RecalculateLayout();

private:
    HWND m_hwnd;
    HWND m_toolbar;
    HWND m_menubar;  // Menu bar in title bar area
    HWND m_statusbar;
    HINSTANCE m_hInstance;
    std::unique_ptr<Editor> m_editor;
    std::unique_ptr<Preview> m_preview;
    
    std::wstring m_currentFile;
    float m_splitRatio;
    int m_toolbarHeight;
    int m_statusbarHeight;
    
    static MainWindow* GetThisFromHandle(HWND hwnd);
};
