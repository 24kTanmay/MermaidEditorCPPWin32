#pragma once
#include <windows.h>
#include <string>

class Editor {
public:
    Editor(HWND hParent, HINSTANCE hInstance);
    ~Editor();

    bool Create();
    HWND GetHwnd() const { return m_hwnd; }
    void SetBounds(int x, int y, int w, int h);
    
    std::string GetText();
    void SetText(const std::string& text);
    
    // Commands
    void Undo();
    void Redo();
    void Cut();
    void Copy();
    void Paste();
    
    // Syntax highlighting
    void StyleText(int startPos, int endPos);

private:
    HWND m_hParent;
    HWND m_hwnd;
    HINSTANCE m_hInstance;
    
    // Helper to send Scintilla messages
    LRESULT SendEditor(UINT msg, WPARAM wParam = 0, LPARAM lParam = 0);
};
