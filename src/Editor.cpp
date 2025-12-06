#include "Editor.h"
#include <vector>
#include <string>
#include <regex>
#include <algorithm>

// Scintilla message definitions
#define SCI_GETTEXT 2182
#define SCI_SETTEXT 2181
#define SCI_GETLENGTH 2006
#define SCI_UNDO 2176
#define SCI_REDO 2011
#define SCI_CUT 2177
#define SCI_COPY 2178
#define SCI_PASTE 2179
#define SCI_SETKEYWORDS 4005
#define SCI_STYLESETFORE 2051
#define SCI_STYLESETBACK 2052
#define SCI_STYLECLEARALL 2050
#define SCI_SETLEXER 4001
#define SCI_SETMARGINTYPEN 2240
#define SCI_SETMARGINWIDTHN 2242
#define SCI_SETCARETFORE 2069
#define SCI_SETSELBACK 2068
#define SCI_SETSELFORE 2067
#define SCI_SETTABWIDTH 2036
#define SCI_SETUSETABS 2124
#define SCI_SETCODEPAGE 2037
#define SCI_STYLESETFONT 2056
#define SCI_STYLESETSIZE 2055
#define SCI_STYLESETCHARACTERSET 2066
#define SCI_STYLESETBOLD 2053
#define SCI_STARTSTYLING 2032
#define SCI_SETSTYLING 2033
#define SCI_COLOURISE 4003
#define SCI_SETILEXER 4033

#define SC_MARGIN_NUMBER 1
#define SCLEX_CONTAINER 0
#define SCLEX_NULL 1
#define STYLE_DEFAULT 32
#define STYLE_LINENUMBER 33
#define SC_CHARSET_DEFAULT 1

// Mermaid token styles (0-7 for custom styling)
#define STYLE_MERMAID_DEFAULT     0
#define STYLE_MERMAID_KEYWORD     1   // graph, flowchart, TD, LR, subgraph, end
#define STYLE_MERMAID_NODE        2   // A, B, C, node identifiers
#define STYLE_MERMAID_CONDITION   3   // Yes, No, OK, Fail
#define STYLE_MERMAID_BRACKET     4   // Text in [], (), {}, |text|
#define STYLE_MERMAID_ARROW       5   // -->, ---, ===, etc.
#define STYLE_MERMAID_COMMENT     6   // %% comments
#define STYLE_MERMAID_STRING      7   // "strings"

// VS Code Dark+ theme colors (BGR format for Scintilla)
#define COLOR_BG            0x1E1E1E
#define COLOR_DEFAULT       0xD4D4D4  // Light gray
#define COLOR_KEYWORD       0xC586C0  // Purple/Magenta - keywords
#define COLOR_NODE          0x9CDCFE  // Light blue - node IDs
#define COLOR_CONDITION     0x4EC9B0  // Teal/Cyan - conditions
#define COLOR_BRACKET       0xCE9178  // Orange - bracketed text
#define COLOR_ARROW         0x808080  // Gray - arrows
#define COLOR_COMMENT       0x6A9955  // Green - comments
#define COLOR_STRING        0xD69D85  // Salmon - strings

// Helper to convert RGB to Scintilla BGR format
#define RGB_TO_BGR(r,g,b) ((b << 16) | (g << 8) | r)

Editor::Editor(HWND hParent, HINSTANCE hInstance) 
    : m_hParent(hParent), m_hwnd(NULL), m_hInstance(hInstance) {}

Editor::~Editor() {}

bool Editor::Create() {
    // Load Scintilla DLL
    HMODULE hMod = LoadLibraryW(L"Scintilla.dll");
    if (!hMod) {
        hMod = LoadLibraryW(L"SciLexer.dll");
    }
    if (!hMod) {
        return false;
    }

    m_hwnd = CreateWindowExW(
        0, L"Scintilla", L"", 
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPCHILDREN | WS_VSCROLL | WS_HSCROLL,
        0, 0, 100, 100,
        m_hParent, NULL, m_hInstance, NULL
    );

    if (!m_hwnd) return false;

    // Set UTF-8 code page
    SendEditor(SCI_SETCODEPAGE, 65001);
    
    // Use container lexer - we will handle styling ourselves
    SendEditor(SCI_SETLEXER, SCLEX_CONTAINER);
    
    // Set font - try JetBrains Mono first, fall back to Consolas
    HFONT hTestFont = CreateFontA(12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, 
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, 
        CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, "JetBrains Mono");
    
    const char* fontName = "Consolas";
    if (hTestFont) {
        HDC hdc = GetDC(m_hwnd);
        HFONT oldFont = (HFONT)SelectObject(hdc, hTestFont);
        char actualName[LF_FACESIZE] = {0};
        GetTextFaceA(hdc, LF_FACESIZE, actualName);
        SelectObject(hdc, oldFont);
        ReleaseDC(m_hwnd, hdc);
        DeleteObject(hTestFont);
        
        if (strcmp(actualName, "JetBrains Mono") == 0) {
            fontName = "JetBrains Mono";
        }
    }
    
    // Configure default style
    SendEditor(SCI_STYLESETFONT, STYLE_DEFAULT, (LPARAM)fontName);
    SendEditor(SCI_STYLESETSIZE, STYLE_DEFAULT, 11);
    SendEditor(SCI_STYLESETCHARACTERSET, STYLE_DEFAULT, SC_CHARSET_DEFAULT);
    SendEditor(SCI_STYLESETBACK, STYLE_DEFAULT, COLOR_BG);
    SendEditor(SCI_STYLESETFORE, STYLE_DEFAULT, COLOR_DEFAULT);
    SendEditor(SCI_STYLECLEARALL);
    
    // Configure Mermaid-specific styles
    // Style 0: Default text
    SendEditor(SCI_STYLESETFORE, STYLE_MERMAID_DEFAULT, COLOR_DEFAULT);
    SendEditor(SCI_STYLESETBACK, STYLE_MERMAID_DEFAULT, COLOR_BG);
    SendEditor(SCI_STYLESETFONT, STYLE_MERMAID_DEFAULT, (LPARAM)fontName);
    SendEditor(SCI_STYLESETSIZE, STYLE_MERMAID_DEFAULT, 11);
    
    // Style 1: Keywords (purple) - graph, flowchart, TD, LR, etc.
    SendEditor(SCI_STYLESETFORE, STYLE_MERMAID_KEYWORD, COLOR_KEYWORD);
    SendEditor(SCI_STYLESETBACK, STYLE_MERMAID_KEYWORD, COLOR_BG);
    SendEditor(SCI_STYLESETFONT, STYLE_MERMAID_KEYWORD, (LPARAM)fontName);
    SendEditor(SCI_STYLESETSIZE, STYLE_MERMAID_KEYWORD, 11);
    SendEditor(SCI_STYLESETBOLD, STYLE_MERMAID_KEYWORD, 1);
    
    // Style 2: Node labels (light blue) - A, B, C
    SendEditor(SCI_STYLESETFORE, STYLE_MERMAID_NODE, COLOR_NODE);
    SendEditor(SCI_STYLESETBACK, STYLE_MERMAID_NODE, COLOR_BG);
    SendEditor(SCI_STYLESETFONT, STYLE_MERMAID_NODE, (LPARAM)fontName);
    SendEditor(SCI_STYLESETSIZE, STYLE_MERMAID_NODE, 11);
    
    // Style 3: Conditions (teal) - Yes, No, OK, Fail
    SendEditor(SCI_STYLESETFORE, STYLE_MERMAID_CONDITION, COLOR_CONDITION);
    SendEditor(SCI_STYLESETBACK, STYLE_MERMAID_CONDITION, COLOR_BG);
    SendEditor(SCI_STYLESETFONT, STYLE_MERMAID_CONDITION, (LPARAM)fontName);
    SendEditor(SCI_STYLESETSIZE, STYLE_MERMAID_CONDITION, 11);
    
    // Style 4: Bracketed text (orange) - [text], (text), {text}, |text|
    SendEditor(SCI_STYLESETFORE, STYLE_MERMAID_BRACKET, COLOR_BRACKET);
    SendEditor(SCI_STYLESETBACK, STYLE_MERMAID_BRACKET, COLOR_BG);
    SendEditor(SCI_STYLESETFONT, STYLE_MERMAID_BRACKET, (LPARAM)fontName);
    SendEditor(SCI_STYLESETSIZE, STYLE_MERMAID_BRACKET, 11);
    
    // Style 5: Arrows (gray)
    SendEditor(SCI_STYLESETFORE, STYLE_MERMAID_ARROW, COLOR_ARROW);
    SendEditor(SCI_STYLESETBACK, STYLE_MERMAID_ARROW, COLOR_BG);
    SendEditor(SCI_STYLESETFONT, STYLE_MERMAID_ARROW, (LPARAM)fontName);
    SendEditor(SCI_STYLESETSIZE, STYLE_MERMAID_ARROW, 11);
    
    // Style 6: Comments (green)
    SendEditor(SCI_STYLESETFORE, STYLE_MERMAID_COMMENT, COLOR_COMMENT);
    SendEditor(SCI_STYLESETBACK, STYLE_MERMAID_COMMENT, COLOR_BG);
    SendEditor(SCI_STYLESETFONT, STYLE_MERMAID_COMMENT, (LPARAM)fontName);
    SendEditor(SCI_STYLESETSIZE, STYLE_MERMAID_COMMENT, 11);
    
    // Style 7: Strings (salmon)
    SendEditor(SCI_STYLESETFORE, STYLE_MERMAID_STRING, COLOR_STRING);
    SendEditor(SCI_STYLESETBACK, STYLE_MERMAID_STRING, COLOR_BG);
    SendEditor(SCI_STYLESETFONT, STYLE_MERMAID_STRING, (LPARAM)fontName);
    SendEditor(SCI_STYLESETSIZE, STYLE_MERMAID_STRING, 11);
    
    // Line number margin styling
    SendEditor(SCI_SETMARGINTYPEN, 0, SC_MARGIN_NUMBER);
    SendEditor(SCI_SETMARGINWIDTHN, 0, 50);
    SendEditor(SCI_STYLESETFONT, STYLE_LINENUMBER, (LPARAM)fontName);
    SendEditor(SCI_STYLESETSIZE, STYLE_LINENUMBER, 10);
    SendEditor(SCI_STYLESETBACK, STYLE_LINENUMBER, RGB_TO_BGR(0x25, 0x25, 0x25));
    SendEditor(SCI_STYLESETFORE, STYLE_LINENUMBER, RGB_TO_BGR(0x85, 0x85, 0x85));
    
    // Caret and selection colors
    SendEditor(SCI_SETCARETFORE, RGB_TO_BGR(0xFF, 0xFF, 0xFF));
    SendEditor(SCI_SETSELBACK, 1, RGB_TO_BGR(0x26, 0x4F, 0x78));
    
    // Tab settings
    SendEditor(SCI_SETTABWIDTH, 4);
    SendEditor(SCI_SETUSETABS, 0);

    // Give it a unique ID for notifications
    SetWindowLongPtr(m_hwnd, GWLP_ID, 2001);

    return true;
}

// Check if a word is a Mermaid keyword
static bool IsMermaidKeyword(const std::string& word) {
    static const char* keywords[] = {
        "graph", "flowchart", "sequenceDiagram", "classDiagram", "stateDiagram",
        "erDiagram", "gantt", "pie", "gitGraph", "mindmap", "timeline",
        "TD", "TB", "BT", "RL", "LR",
        "subgraph", "end", "class", "style", "linkStyle", "click",
        "participant", "actor", "Note", "activate", "deactivate", "loop", "alt", "else", "opt", "par", "and", "rect",
        "direction", "title", "section", "dateFormat", "axisFormat"
    };
    for (const auto& kw : keywords) {
        if (word == kw) return true;
    }
    return false;
}

// Check if a word is a condition
static bool IsCondition(const std::string& word) {
    static const char* conditions[] = {
        "Yes", "No", "yes", "no", "OK", "ok", "Fail", "fail", 
        "True", "true", "False", "false", "Success", "Error"
    };
    for (const auto& cond : conditions) {
        if (word == cond) return true;
    }
    return false;
}

// Check if a word looks like a node ID (uppercase letter or letter followed by letters/digits)
static bool IsNodeId(const std::string& word) {
    if (word.empty()) return false;
    // Single uppercase letter or starts with uppercase
    if (word.length() == 1 && isupper(word[0])) return true;
    // Multi-char identifier starting with letter
    if (isalpha(word[0]) && word.length() <= 10) {
        bool allAlnum = true;
        for (char c : word) {
            if (!isalnum(c) && c != '_') { allAlnum = false; break; }
        }
        if (allAlnum && !IsMermaidKeyword(word) && !IsCondition(word)) return true;
    }
    return false;
}

void Editor::StyleText(int startPos, int endPos) {
    if (!m_hwnd) return;
    
    // Get text to style
    int length = (int)SendEditor(SCI_GETLENGTH);
    if (length == 0) return;
    
    std::vector<char> buffer(length + 1);
    SendEditor(SCI_GETTEXT, length + 1, (LPARAM)buffer.data());
    std::string text(buffer.data());
    
    // Start styling from position 0
    SendEditor(SCI_STARTSTYLING, 0);
    
    int i = 0;
    int len = (int)text.length();
    
    while (i < len) {
        char c = text[i];
        
        // Check for comment (%%)
        if (c == '%' && i + 1 < len && text[i + 1] == '%') {
            int start = i;
            while (i < len && text[i] != '\n' && text[i] != '\r') i++;
            SendEditor(SCI_STARTSTYLING, start);
            SendEditor(SCI_SETSTYLING, i - start, STYLE_MERMAID_COMMENT);
            continue;
        }
        
        // Check for strings "..."
        if (c == '"') {
            int start = i;
            i++;
            while (i < len && text[i] != '"' && text[i] != '\n') i++;
            if (i < len && text[i] == '"') i++;
            SendEditor(SCI_STARTSTYLING, start);
            SendEditor(SCI_SETSTYLING, i - start, STYLE_MERMAID_STRING);
            continue;
        }
        
        // Check for bracketed text [...], {...}, (...), |...|
        if (c == '[' || c == '{' || c == '(' || c == '|') {
            char closing = (c == '[') ? ']' : (c == '{') ? '}' : (c == '(') ? ')' : '|';
            int start = i;
            i++;
            // Handle nested brackets for [[ ]]
            int depth = 1;
            while (i < len && depth > 0) {
                if (text[i] == c && c != '|') depth++;
                else if (text[i] == closing) depth--;
                else if (text[i] == '\n' || text[i] == '\r') break;
                if (depth > 0) i++;
            }
            if (i < len && text[i] == closing) i++;
            SendEditor(SCI_STARTSTYLING, start);
            SendEditor(SCI_SETSTYLING, i - start, STYLE_MERMAID_BRACKET);
            continue;
        }
        
        // Check for arrows: -->, --->, --, ===>, ==>, ==>
        if ((c == '-' || c == '=' || c == '<' || c == '>') && i + 1 < len) {
            int start = i;
            while (i < len && (text[i] == '-' || text[i] == '=' || text[i] == '>' || text[i] == '<' || text[i] == '.')) {
                i++;
            }
            if (i > start + 1) {  // At least 2 chars for arrow
                SendEditor(SCI_STARTSTYLING, start);
                SendEditor(SCI_SETSTYLING, i - start, STYLE_MERMAID_ARROW);
                continue;
            }
            i = start;  // Reset if not an arrow
        }
        
        // Check for words (identifiers/keywords)
        if (isalpha(c) || c == '_') {
            int start = i;
            while (i < len && (isalnum(text[i]) || text[i] == '_')) i++;
            std::string word = text.substr(start, i - start);
            
            int style = STYLE_MERMAID_DEFAULT;
            if (IsMermaidKeyword(word)) {
                style = STYLE_MERMAID_KEYWORD;
            } else if (IsCondition(word)) {
                style = STYLE_MERMAID_CONDITION;
            } else if (IsNodeId(word)) {
                style = STYLE_MERMAID_NODE;
            }
            
            SendEditor(SCI_STARTSTYLING, start);
            SendEditor(SCI_SETSTYLING, i - start, style);
            continue;
        }
        
        // Default: style single character as default
        SendEditor(SCI_STARTSTYLING, i);
        SendEditor(SCI_SETSTYLING, 1, STYLE_MERMAID_DEFAULT);
        i++;
    }
}

void Editor::SetBounds(int x, int y, int w, int h) {
    if (m_hwnd) {
        MoveWindow(m_hwnd, x, y, w, h, TRUE);
    }
}

std::string Editor::GetText() {
    if (!m_hwnd) return "";
    int len = (int)SendEditor(SCI_GETLENGTH);
    if (len <= 0) return "";
    
    std::vector<char> buffer(len + 1);
    SendEditor(SCI_GETTEXT, len + 1, (LPARAM)buffer.data());
    return std::string(buffer.data());
}

void Editor::SetText(const std::string& text) {
    if (m_hwnd) {
        SendEditor(SCI_SETTEXT, 0, (LPARAM)text.c_str());
        // Apply syntax highlighting
        StyleText(0, (int)text.length());
    }
}

void Editor::Undo() { SendEditor(SCI_UNDO); }
void Editor::Redo() { SendEditor(SCI_REDO); }
void Editor::Cut() { SendEditor(SCI_CUT); }
void Editor::Copy() { SendEditor(SCI_COPY); }
void Editor::Paste() { SendEditor(SCI_PASTE); }

LRESULT Editor::SendEditor(UINT msg, WPARAM wParam, LPARAM lParam) {
    return SendMessage(m_hwnd, msg, wParam, lParam);
}
