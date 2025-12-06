cmake_minimum_required(VERSION 3.20)
project(MermaidNative LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Include directories
include_directories(include)

# Find packages (or assume vendor libs are present)
find_path(WIL_INCLUDE_DIRS "wil/com.h") 
if (WIL_INCLUDE_DIRS)
    include_directories(${WIL_INCLUDE_DIRS})
endif()

# Source files
file(GLOB_RECURSE SOURCES "src/*.cpp")
file(GLOB_RECURSE HEADERS "include/*.h")

# Resources
set(RESOURCE_FILES resource.rc)

# Executable
add_executable(MermaidNative WIN32 ${SOURCES} ${HEADERS} ${RESOURCE_FILES})

# Compiler options
if(MSVC)
    target_compile_options(MermaidNative PRIVATE /W4 /permissive-)
    target_link_options(MermaidNative PRIVATE /SUBSYSTEM:WINDOWS)
else()
    target_compile_options(MermaidNative PRIVATE -Wall -Wextra)
    target_link_options(MermaidNative PRIVATE -mwindows)
endif()

# Libraries
# Link against system libraries.
target_link_libraries(MermaidNative PRIVATE 
    kernel32 user32 gdi32 comdlg32 advapi32 shell32 ole32 oleaut32 uuid shlwapi version comctl32 dwmapi uxtheme
)

# For MinGW: Link directly against the DLL
if(MINGW)
    # Check vendor folder first (persistent location)
    if(EXISTS "${CMAKE_SOURCE_DIR}/vendor/WebView2Loader.dll")
        file(COPY "${CMAKE_SOURCE_DIR}/vendor/WebView2Loader.dll" DESTINATION "${CMAKE_BINARY_DIR}/bin")
        target_link_libraries(MermaidNative PRIVATE "${CMAKE_BINARY_DIR}/bin/WebView2Loader.dll")
    else()
        # Check build/bin (ephemeral location)
        if(EXISTS "${CMAKE_BINARY_DIR}/bin/WebView2Loader.dll")
             target_link_libraries(MermaidNative PRIVATE "${CMAKE_BINARY_DIR}/bin/WebView2Loader.dll")
        else()
             message(WARNING "WebView2Loader.dll not found in vendor/ or build/bin/. Linking might fail.")
             # Link to a placeholder - might rely on PATH
             target_link_libraries(MermaidNative PRIVATE WebView2Loader)
        endif()
    endif()
endif()

# Scintilla DLL Copy
if(EXISTS "${CMAKE_SOURCE_DIR}/vendor/SciLexer.dll")
    file(COPY "${CMAKE_SOURCE_DIR}/vendor/SciLexer.dll" DESTINATION "${CMAKE_BINARY_DIR}/bin")
endif()


# Custom command to copy necessary DLLs/Assets to output
add_custom_command(TARGET MermaidNative POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/Assets ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Assets
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_SOURCE_DIR}/mermaid.js ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/mermaid.js
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_SOURCE_DIR}/svg-pan-zoom.js ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/svg-pan-zoom.js
)
