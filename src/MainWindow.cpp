#include "MainWindow.h"
#include "Editor.h"
#include "Preview.h"
#include "resource.h"
#include <commdlg.h>
#include <fstream>
#include <sstream>
#include <dwmapi.h>
#include <uxtheme.h>
#include <vsstyle.h>

// DWM attributes for dark mode (not defined in older SDKs)
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

// Undocumented UAH messages for menu bar theming
#define WM_UAHDRAWMENU         0x0091
#define WM_UAHDRAWMENUITEM     0x0092

// UAH structures for menu bar painting
typedef struct {
    HMENU hmenu;
    HDC hdc;
    DWORD dwFlags;
} UAHMENU;

typedef struct {
    DRAWITEMSTRUCT dis;
    DWORD dwFlags;
} UAHDRAWMENUITEM;

// Scintilla commands
#define SCI_SELECTALL 2013

MainWindow::MainWindow() 
    : m_hwnd(NULL), m_toolbar(NULL), m_statusbar(NULL), 
      m_hInstance(NULL), m_splitRatio(0.4f),
      m_toolbarHeight(0), m_statusbarHeight(0) {}

MainWindow::~MainWindow() {}

LRESULT CALLBACK MainWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    MainWindow* pThis = NULL;
    if (msg == WM_NCCREATE) {
        CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
        pThis = (MainWindow*)pCreate->lpCreateParams;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
        pThis->m_hwnd = hwnd;
    } else {
        pThis = (MainWindow*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    }

    if (pThis) {
        return pThis->HandleMessage(msg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

bool MainWindow::Create(HINSTANCE hInstance, int nCmdShow) {
    m_hInstance = hInstance;
    
    // Initialize common controls for toolbar
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_BAR_CLASSES | ICC_COOL_CLASSES;
    InitCommonControlsEx(&icex);
    
    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = MainWindow::WndProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON));
    wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszMenuName = NULL;  // No system menu - will use custom dark menu bar
    wc.lpszClassName = L"MermaidNativeClass";
    wc.hIconSm = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON));

    if (!RegisterClassExW(&wc)) return false;

    m_hwnd = CreateWindowExW(
        0, L"MermaidNativeClass", L"",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1200, 800,
        NULL, NULL, hInstance, this
    );

    if (!m_hwnd) return false;

    // Enable immersive dark mode for the title bar (Windows 10 1809+)
    BOOL useDarkMode = TRUE;
    DwmSetWindowAttribute(m_hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDarkMode, sizeof(useDarkMode));
    
    // Set caption color to match menu bar (Windows 11 / 10 22H2+)
    #ifndef DWMWA_CAPTION_COLOR
    #define DWMWA_CAPTION_COLOR 35
    #define DWMWA_BORDER_COLOR 34
    #endif
    
    COLORREF captionColor = RGB(0x25, 0x25, 0x25);  // #252525
    DwmSetWindowAttribute(m_hwnd, DWMWA_CAPTION_COLOR, &captionColor, sizeof(captionColor));
    DwmSetWindowAttribute(m_hwnd, DWMWA_BORDER_COLOR, &captionColor, sizeof(captionColor));
    
    // Extend frame into client area for custom title bar
    MARGINS margins = {0, 0, 32, 0};  // Extend 32 pixels from top
    DwmExtendFrameIntoClientArea(m_hwnd, &margins);

    ShowWindow(m_hwnd, nCmdShow);
    UpdateWindow(m_hwnd);
    return true;
}

LRESULT MainWindow::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            OnCreate();
            return 0;
        case WM_SIZE:
            // Resize toolbar and statusbar
            if (m_toolbar) SendMessageW(m_toolbar, WM_SIZE, 0, 0);
            if (m_statusbar) SendMessageW(m_statusbar, WM_SIZE, 0, 0);
            OnSize(LOWORD(lParam), HIWORD(lParam));
            return 0;
        case WM_COMMAND:
            OnCommand(LOWORD(wParam));
            return 0;
        case WM_NOTIFY:
            OnNotify(wParam, lParam);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_TIMER:
            if (wParam == 1) {
                KillTimer(m_hwnd, 1);
                Render();
            }
            return 0;
        
        // Owner-draw menu support for dark theme
        case WM_MEASUREITEM: {
            MEASUREITEMSTRUCT* mis = (MEASUREITEMSTRUCT*)lParam;
            if (mis->CtlType == ODT_MENU) {
                mis->itemWidth = 120;
                mis->itemHeight = 22;
                return TRUE;
            }
            break;
        }
        
        case WM_DRAWITEM: {
            DRAWITEMSTRUCT* dis = (DRAWITEMSTRUCT*)lParam;
            if (dis->CtlType == ODT_MENU) {
                // Dark theme colors
                COLORREF bgColor = (dis->itemState & ODS_SELECTED) ? RGB(0x40, 0x40, 0x40) : RGB(0x2B, 0x2B, 0x2B);
                COLORREF textColor = (dis->itemState & ODS_GRAYED) ? RGB(0x80, 0x80, 0x80) : RGB(0xE0, 0xE0, 0xE0);
                
                // Fill background
                HBRUSH hBrush = CreateSolidBrush(bgColor);
                FillRect(dis->hDC, &dis->rcItem, hBrush);
                DeleteObject(hBrush);
                
                // Draw text
                SetBkMode(dis->hDC, TRANSPARENT);
                SetTextColor(dis->hDC, textColor);
                
                wchar_t* menuText = (wchar_t*)dis->itemData;
                if (menuText) {
                    RECT textRect = dis->rcItem;
                    textRect.left += 20;
                    DrawTextW(dis->hDC, menuText, -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                }
                return TRUE;
            }
            break;
        }
        
        // Handle menu bar background painting
        case WM_NCPAINT: {
            // Let default handle it first
            LRESULT result = DefWindowProcW(m_hwnd, msg, wParam, lParam);
            return result;
        }
        
        case WM_ERASEBKGND: {
            HDC hdc = (HDC)wParam;
            RECT rc;
            GetClientRect(m_hwnd, &rc);
            HBRUSH hBrush = CreateSolidBrush(RGB(0x1E, 0x1E, 0x1E));
            FillRect(hdc, &rc, hBrush);
            DeleteObject(hBrush);
            return 1;
        }
        
        // UAH menu bar background painting
        case WM_UAHDRAWMENU: {
            UAHMENU* pUDM = (UAHMENU*)lParam;
            if (pUDM && pUDM->hdc) {
                MENUBARINFO mbi = {0};
                mbi.cbSize = sizeof(MENUBARINFO);
                if (GetMenuBarInfo(m_hwnd, OBJID_MENU, 0, &mbi)) {
                    RECT rcWindow;
                    GetWindowRect(m_hwnd, &rcWindow);
                    
                    // Convert menu bar rect to window coordinates
                    RECT rcMenuBar = mbi.rcBar;
                    OffsetRect(&rcMenuBar, -rcWindow.left, -rcWindow.top);
                    
                    // Fill with dark color #252525
                    HBRUSH hBrush = CreateSolidBrush(RGB(0x25, 0x25, 0x25));
                    FillRect(pUDM->hdc, &rcMenuBar, hBrush);
                    DeleteObject(hBrush);
                }
            }
            return 0;
        }
        
        // UAH menu bar item painting
        case WM_UAHDRAWMENUITEM: {
            UAHDRAWMENUITEM* pUDMI = (UAHDRAWMENUITEM*)lParam;
            if (pUDMI) {
                DRAWITEMSTRUCT* dis = &pUDMI->dis;
                
                // Dark theme colors for menu bar items
                COLORREF bgColor = RGB(0x25, 0x25, 0x25);  // #252525
                COLORREF textColor = RGB(0xE0, 0xE0, 0xE0);
                
                // Highlight on selection/hover
                if (dis->itemState & (ODS_SELECTED | ODS_HOTLIGHT)) {
                    bgColor = RGB(0x40, 0x40, 0x40);
                }
                
                // Fill background
                HBRUSH hBrush = CreateSolidBrush(bgColor);
                FillRect(dis->hDC, &dis->rcItem, hBrush);
                DeleteObject(hBrush);
                
                // Get menu item text
                wchar_t szText[256] = {0};
                MENUITEMINFOW mii = {0};
                mii.cbSize = sizeof(mii);
                mii.fMask = MIIM_STRING;
                mii.dwTypeData = szText;
                mii.cch = 255;
                
                if (GetMenuItemInfoW((HMENU)dis->hwndItem, dis->itemID, FALSE, &mii)) {
                    // Draw text centered
                    SetBkMode(dis->hDC, TRANSPARENT);
                    SetTextColor(dis->hDC, textColor);
                    DrawTextW(dis->hDC, szText, -1, &dis->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                }
            }
            return 0;
        }
    }
    return DefWindowProcW(m_hwnd, msg, wParam, lParam);
}

void MainWindow::OnNotify(WPARAM wParam, LPARAM lParam) {
    LPNMHDR pnmh = (LPNMHDR)lParam;
    
    // Scintilla notifications
    if (m_editor && pnmh->hwndFrom == m_editor->GetHwnd()) {
        // Handle SCN_STYLENEEDED for container lexer
        #define SCN_STYLENEEDED 2000
        if (pnmh->code == SCN_STYLENEEDED) {
            // Container lexer needs styling - call our custom lexer
            m_editor->StyleText(0, 0);
        }
        
        // Debounce render updates on text change
        SetTimer(m_hwnd, 1, 500, NULL);
    }
}

void MainWindow::CreateToolbar() {
    // Create menu bar positioned in the title bar area (beside app icon)
    // Position: After icon (approx 32px from left), in the extended frame area (top 30px)
    m_menubar = CreateWindowExW(
        0, TOOLBARCLASSNAMEW, NULL,
        WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT | TBSTYLE_LIST | TBSTYLE_TRANSPARENT | CCS_NODIVIDER | CCS_NORESIZE | CCS_NOPARENTALIGN,
        32, 4, 400, 24,  // Position in title bar: x=32 (after icon), y=4, width=400, height=24
        m_hwnd, (HMENU)1001, m_hInstance, NULL
    );
    
    SendMessageW(m_menubar, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);
    SendMessageW(m_menubar, TB_SETBITMAPSIZE, 0, MAKELPARAM(0, 0));  // No bitmaps
    SendMessageW(m_menubar, TB_SETPADDING, 0, MAKELPARAM(8, 4));  // Add padding
    
    // Add menu items as text buttons
    TBBUTTON menuButtons[] = {
        { I_IMAGENONE, ID_FILE_NEW + 1000, TBSTATE_ENABLED, BTNS_DROPDOWN | BTNS_AUTOSIZE, {0}, 0, (INT_PTR)L"File" },
        { I_IMAGENONE, ID_EDIT_UNDO + 1000, TBSTATE_ENABLED, BTNS_DROPDOWN | BTNS_AUTOSIZE, {0}, 0, (INT_PTR)L"Edit" },
        { I_IMAGENONE, ID_VIEW_RENDER + 1000, TBSTATE_ENABLED, BTNS_DROPDOWN | BTNS_AUTOSIZE, {0}, 0, (INT_PTR)L"View" },
        { I_IMAGENONE, ID_SEARCH_FIND + 1000, TBSTATE_ENABLED, BTNS_DROPDOWN | BTNS_AUTOSIZE, {0}, 0, (INT_PTR)L"Search" },
        { I_IMAGENONE, ID_HELP_ABOUT + 1000, TBSTATE_ENABLED, BTNS_DROPDOWN | BTNS_AUTOSIZE, {0}, 0, (INT_PTR)L"Help" },
    };
    
    SendMessageW(m_menubar, TB_ADDBUTTONSW, 5, (LPARAM)menuButtons);
    SendMessageW(m_menubar, TB_AUTOSIZE, 0, 0);
    
    // Create icon toolbar below the title bar (at y=32, below the extended frame)
    m_toolbar = CreateWindowExW(
        0, TOOLBARCLASSNAMEW, NULL,
        WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT | TBSTYLE_TOOLTIPS | CCS_NODIVIDER | CCS_NORESIZE | CCS_NOPARENTALIGN,
        0, 32, 400, 28,  // Position below title bar area
        m_hwnd, (HMENU)IDC_TOOLBAR, m_hInstance, NULL
    );
    
    if (!m_toolbar) return;
    
    SendMessageW(m_toolbar, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);
    SendMessageW(m_toolbar, TB_SETBUTTONSIZE, 0, MAKELPARAM(24, 24));
    
    TBADDBITMAP tbab = { HINST_COMMCTRL, IDB_STD_SMALL_COLOR };
    SendMessageW(m_toolbar, TB_ADDBITMAP, 0, (LPARAM)&tbab);
    
    TBBUTTON buttons[] = {
        { STD_FILENEW, ID_FILE_NEW, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, 0 },
        { STD_FILEOPEN, ID_FILE_OPEN, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, 0 },
        { STD_FILESAVE, ID_FILE_SAVE, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, 0 },
        { 0, 0, TBSTATE_ENABLED, BTNS_SEP, {0}, 0, 0 },
        { STD_REDOW, ID_VIEW_RENDER, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, 0 },
        { 0, 0, TBSTATE_ENABLED, BTNS_SEP, {0}, 0, 0 },
        { STD_COPY, ID_FILE_EXPORTPNG, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, 0 },
        { STD_PASTE, ID_FILE_EXPORTSVG, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, 0 },
    };
    
    SendMessageW(m_toolbar, TB_ADDBUTTONSW, 8, (LPARAM)buttons);
    SendMessageW(m_toolbar, TB_AUTOSIZE, 0, 0);
    
    // Set toolbar height (title bar area + toolbar)
    m_toolbarHeight = 60;  // 32 (title bar extension) + 28 (toolbar)
}

void MainWindow::CreateStatusBar() {
    m_statusbar = CreateWindowExW(
        0, STATUSCLASSNAMEW, NULL,
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
        0, 0, 0, 0,
        m_hwnd, (HMENU)IDC_STATUSBAR, m_hInstance, NULL
    );
    
    if (!m_statusbar) return;
    
    // Set parts
    int parts[] = { 150, 300, -1 };
    SendMessageW(m_statusbar, SB_SETPARTS, 3, (LPARAM)parts);
    
    UpdateStatusBar();
    
    // Get statusbar height
    RECT rcStatus;
    GetWindowRect(m_statusbar, &rcStatus);
    m_statusbarHeight = rcStatus.bottom - rcStatus.top;
}

void MainWindow::UpdateStatusBar() {
    if (m_statusbar) {
        SendMessageW(m_statusbar, SB_SETTEXTW, 0, (LPARAM)L"Ready");
        SendMessageW(m_statusbar, SB_SETTEXTW, 1, (LPARAM)L"Sync Active");
        SendMessageW(m_statusbar, SB_SETTEXTW, 2, (LPARAM)L"Mermaid v10.4.0");
    }
}

// Helper to recursively make menu items owner-drawn
static void MakeMenuOwnerDrawn(HMENU hMenu) {
    if (!hMenu) return;
    
    int count = GetMenuItemCount(hMenu);
    for (int i = 0; i < count; i++) {
        MENUITEMINFOW mii = {0};
        mii.cbSize = sizeof(MENUITEMINFOW);
        mii.fMask = MIIM_FTYPE | MIIM_STRING | MIIM_SUBMENU | MIIM_DATA;
        
        // Get current item info
        wchar_t buffer[256] = {0};
        mii.dwTypeData = buffer;
        mii.cch = 255;
        
        if (GetMenuItemInfoW(hMenu, i, TRUE, &mii)) {
            // Skip separators
            if (mii.fType & MFT_SEPARATOR) continue;
            
            // Store the text in itemData (allocate memory for it)
            size_t len = wcslen(buffer) + 1;
            wchar_t* textCopy = new wchar_t[len];
            wcscpy_s(textCopy, len, buffer);
            
            // Make owner-drawn
            mii.fMask = MIIM_FTYPE | MIIM_DATA;
            mii.fType |= MFT_OWNERDRAW;
            mii.dwItemData = (ULONG_PTR)textCopy;
            
            SetMenuItemInfoW(hMenu, i, TRUE, &mii);
            
            // Recurse into submenus
            if (mii.hSubMenu) {
                MakeMenuOwnerDrawn(mii.hSubMenu);
            }
        }
    }
}

void MainWindow::SetupDarkMenu() {
    HMENU hMenu = GetMenu(m_hwnd);
    if (hMenu) {
        MakeMenuOwnerDrawn(hMenu);
        DrawMenuBar(m_hwnd);
    }
}

void MainWindow::OnCreate() {
    // Setup dark theme for menu bar
    SetupDarkMenu();
    
    // Create toolbar first
    CreateToolbar();
    
    // Create status bar
    CreateStatusBar();
    
    // Create Editor
    m_editor = std::make_unique<Editor>(m_hwnd, m_hInstance);
    if (!m_editor->Create()) {
        MessageBoxW(m_hwnd, L"Failed to create Scintilla Editor. Ensure Scintilla.dll is present.", L"Error", MB_OK | MB_ICONERROR);
    } else {
        m_editor->SetText("graph TD;\n    A-->B;\n    A-->C;\n    B-->D;\n    C-->D;");
    }

    // Create Preview
    m_preview = std::make_unique<Preview>(m_hwnd);
    
    WCHAR path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);
    std::wstring exePath(path);
    std::wstring dir = exePath.substr(0, exePath.find_last_of(L"\\/"));
    
    m_preview->Create(dir + L"\\Assets");
    
    // Initial layout
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    OnSize(rc.right, rc.bottom);
    
    // Initial render after a delay
    SetTimer(m_hwnd, 1, 1000, NULL);
}

void MainWindow::OnSize(int width, int height) {
    if (!m_editor || !m_preview) return;
    
    int contentTop = m_toolbarHeight;
    int contentHeight = height - m_toolbarHeight - m_statusbarHeight;
    
    int splitX = (int)(width * m_splitRatio);
    m_editor->SetBounds(0, contentTop, splitX, contentHeight);
    m_preview->SetBounds(splitX, contentTop, width - splitX, contentHeight);
}

void MainWindow::OnCommand(int id) {
    switch (id) {
        // File menu
        case ID_FILE_NEW:
            NewFile();
            break;
        case ID_FILE_OPEN:
            OpenFile();
            break;
        case ID_FILE_SAVE:
            SaveFile();
            break;
        case ID_FILE_SAVEAS:
            SaveFileAs();
            break;
        case ID_FILE_EXPORTPNG:
            ExportPNG();
            break;
        case ID_FILE_EXPORTSVG:
            ExportSVG();
            break;
        case ID_FILE_EXIT:
            DestroyWindow(m_hwnd);
            break;
            
        // Edit menu
        case ID_EDIT_UNDO:
            Undo();
            break;
        case ID_EDIT_REDO:
            Redo();
            break;
        case ID_EDIT_CUT:
            Cut();
            break;
        case ID_EDIT_COPY:
            Copy();
            break;
        case ID_EDIT_PASTE:
            Paste();
            break;
        case ID_EDIT_SELECTALL:
            SelectAll();
            break;
            
        // View menu
        case ID_VIEW_RENDER:
            Render();
            break;
        case ID_VIEW_SPLIT:
            ToggleSplit();
            break;
            
        // Help menu
        case ID_HELP_ABOUT:
            MessageBoxW(m_hwnd, 
                L"Mermaid Editor Native\n\nA lightweight native Mermaid diagram editor.\n\nBuilt with Win32 API, Scintilla, and WebView2.",
                L"About", MB_OK | MB_ICONINFORMATION);
            break;
    }
}

// File operations
void MainWindow::NewFile() {
    m_currentFile.clear();
    if (m_editor) {
        m_editor->SetText("graph TD;\n    A-->B;");
    }
    SetWindowTextW(m_hwnd, L"Mermaid Editor Native - Untitled");
    Render();
}

void MainWindow::OpenFile() {
    WCHAR szFile[260] = {0};
    OPENFILENAMEW ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile) / sizeof(WCHAR);
    ofn.lpstrFilter = L"Mermaid Files\0*.mmd;*.mermaid;*.txt\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileNameW(&ofn) == TRUE) {
        LoadContent(szFile);
    }
}

void MainWindow::SaveFile() {
    if (m_currentFile.empty()) {
        SaveFileAs();
    } else {
        SaveContent(m_currentFile);
    }
}

void MainWindow::SaveFileAs() {
    WCHAR szFile[260] = {0};
    OPENFILENAMEW ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile) / sizeof(WCHAR);
    ofn.lpstrFilter = L"Mermaid Files\0*.mmd\0All Files\0*.*\0";
    ofn.lpstrDefExt = L"mmd";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;

    if (GetSaveFileNameW(&ofn) == TRUE) {
        m_currentFile = szFile;
        SaveContent(m_currentFile);
    }
}

void MainWindow::ExportPNG() {
    MessageBoxW(m_hwnd, L"PNG Export: This feature will be implemented to export the diagram as PNG.", L"Export PNG", MB_OK);
}

void MainWindow::ExportSVG() {
    MessageBoxW(m_hwnd, L"SVG Export: This feature will be implemented to export the diagram as SVG.", L"Export SVG", MB_OK);
}

void MainWindow::LoadContent(const std::wstring& path) {
    int size = WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, NULL, 0, NULL, NULL);
    std::string narrowPath(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, &narrowPath[0], size, NULL, NULL);
    
    std::ifstream file(narrowPath);
    if (file) {
        std::stringstream buffer;
        buffer << file.rdbuf();
        m_editor->SetText(buffer.str());
        m_currentFile = path;
        
        std::wstring title = L"Mermaid Editor Native - " + path;
        SetWindowTextW(m_hwnd, title.c_str());
        
        Render();
    }
}

void MainWindow::SaveContent(const std::wstring& path) {
    int size = WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, NULL, 0, NULL, NULL);
    std::string narrowPath(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, &narrowPath[0], size, NULL, NULL);
    
    std::ofstream file(narrowPath);
    if (file) {
        file << m_editor->GetText();
    }
}

// Edit operations
void MainWindow::Undo() {
    if (m_editor) m_editor->Undo();
}

void MainWindow::Redo() {
    if (m_editor) m_editor->Redo();
}

void MainWindow::Cut() {
    if (m_editor) m_editor->Cut();
}

void MainWindow::Copy() {
    if (m_editor) m_editor->Copy();
}

void MainWindow::Paste() {
    if (m_editor) m_editor->Paste();
}

void MainWindow::SelectAll() {
    if (m_editor) {
        SendMessage(m_editor->GetHwnd(), SCI_SELECTALL, 0, 0);
    }
}

// View operations
void MainWindow::Render() {
    if (m_editor && m_preview) {
        m_preview->UpdateDiagram(m_editor->GetText());
    }
    if (m_statusbar) {
        SendMessageW(m_statusbar, SB_SETTEXTW, 0, (LPARAM)L"Rendered");
    }
}

void MainWindow::ToggleSplit() {
    m_splitRatio = (m_splitRatio == 0.4f) ? 0.0f : 0.4f;
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    OnSize(rc.right, rc.bottom);
}

void MainWindow::RecalculateLayout() {
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    OnSize(rc.right, rc.bottom);
}
