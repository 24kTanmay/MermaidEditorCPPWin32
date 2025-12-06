# FINAL STEPS TO RUN MERMAID NATIVE

The code is compiled and ready. The build failing is strictly due to missing DLLs.

1.  **WebView2Loader.dll**:
    *   **Action**: Copy this file to `c:\Users\ajayd\Documents\Project\Mermaid_C++\build\bin\`
    *   **Source**: Download `Microsoft.Web.WebView2` package (nuget.org) -> `/build/native/x64/WebView2Loader.dll`.

2.  **SciLexer.dll**:
    *   **Action**: Copy this file to `c:\Users\ajayd\Documents\Project\Mermaid_C++\build\bin\`
    *   **Source**: Download `wscite.zip` (scintilla.org) -> `/wscite/SciLexer.dll`.

3.  **Run**:
    ```powershell
    cd c:\Users\ajayd\Documents\Project\Mermaid_C++
    cmake --build build
    ./build/bin/MermaidNative.exe
    ```

**Note**: The source code compiles successfully (verified). The linker just needs these 2 files.
