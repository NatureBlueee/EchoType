cmake_minimum_required(VERSION 3.15)
project(ClipboardMonitor VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows specific settings
if(WIN32)
    add_definitions(-DUNICODE -D_UNICODE)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS")
endif()

# Source files
set(SOURCES
    main.cpp
    clipboard_monitor.cpp
    storage.cpp
    context/async_executor.cpp
    context/context_manager.cpp
    context/adapters/browser_adapter.cpp
    context/adapters/wechat_adapter.cpp
    context/adapters/vscode_adapter.cpp
    context/adapters/notion_adapter.cpp
    context/utils/ui_automation_helper.cpp
    context/utils/html_parser.cpp
)

set(HEADERS
    clipboard_monitor.h
    storage.h
    utils.h
    debug_log.h
    context/context_data.h
    context/context_adapter.h
    context/async_executor.h
    context/context_manager.h
    context/utils/ui_automation_helper.h
    context/utils/html_parser.h
)

# Create executable (WIN32 for no console window)
add_executable(${PROJECT_NAME} WIN32 ${SOURCES} ${HEADERS})

# Link Windows libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    user32
    gdi32
    shell32
    ole32
    oleaut32            # OLE Automation (for BSTR functions)
    shlwapi
    oleacc              # MSAA (already used in code)
    uiautomationcore    # UI Automation (for Phase 2)
)

# Set output directory
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
