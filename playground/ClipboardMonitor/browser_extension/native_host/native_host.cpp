// ClipboardMonitor Native Messaging Host
// Receives context data from browser extension and writes to shared file

#include <iostream>
#include <fstream>
#include <string>
#include <cstdint>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shlobj.h>

// Read a message from stdin (Chrome native messaging protocol)
std::string ReadMessage() {
    // First 4 bytes: message length (little-endian)
    uint32_t length = 0;
    if (std::cin.read(reinterpret_cast<char*>(&length), 4).gcount() != 4) {
        return "";
    }
    
    // Sanity check
    if (length > 1024 * 1024) {  // 1MB max
        return "";
    }
    
    // Read the message
    std::string message(length, '\0');
    if (std::cin.read(&message[0], length).gcount() != static_cast<std::streamsize>(length)) {
        return "";
    }
    
    return message;
}

// Write a message to stdout (Chrome native messaging protocol)
void WriteMessage(const std::string& message) {
    uint32_t length = static_cast<uint32_t>(message.length());
    std::cout.write(reinterpret_cast<char*>(&length), 4);
    std::cout.write(message.c_str(), length);
    std::cout.flush();
}

// Get AppData path
std::wstring GetAppDataPath() {
    wchar_t path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, path))) {
        return std::wstring(path) + L"\\ClipboardMonitor";
    }
    return L".";
}

// Write context to shared file
void WriteContextToFile(const std::string& context) {
    std::wstring appDataPath = GetAppDataPath();
    std::wstring filePath = appDataPath + L"\\browser_context.json";
    
    // Convert to narrow string for ofstream
    char narrowPath[MAX_PATH];
    WideCharToMultiByte(CP_UTF8, 0, filePath.c_str(), -1, narrowPath, MAX_PATH, NULL, NULL);
    
    std::ofstream file(narrowPath, std::ios::out | std::ios::trunc);
    if (file.is_open()) {
        file << context;
        file.close();
    }
}

int main() {
    // Set stdin/stdout to binary mode
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
    
    // Read messages in a loop
    while (true) {
        std::string message = ReadMessage();
        if (message.empty()) {
            break;
        }
        
        // Write context to file for ClipboardMonitor to read
        WriteContextToFile(message);
        
        // Send acknowledgment
        WriteMessage("{\"status\":\"ok\"}");
    }
    
    return 0;
}
