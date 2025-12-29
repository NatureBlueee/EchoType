#pragma once

#include <string>
#include <fstream>
#include <mutex>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

// Forward declaration
namespace Utils {
    std::string GetTimestamp();
    std::string WideToUtf8(const std::wstring& wstr);
    std::wstring GetAppDataPath();
}

class DebugLog {
public:
    static DebugLog& Instance() {
        static DebugLog instance;
        return instance;
    }
    
    void Initialize(const std::wstring& directory) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        std::wstring logPath = directory + L"\\debug.log";
        m_file.open(logPath, std::ios::out | std::ios::app);
        if (m_file.is_open()) {
            m_initialized = true;
            WriteLog("=== ClipboardMonitor Started ===");
        }
    }
    
    void Log(const std::string& message) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        WriteLog(message);
    }
    
    void Log(const std::wstring& message) {
        Log(Utils::WideToUtf8(message));
    }
    
    void Close() {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (m_file.is_open()) {
            WriteLog("=== ClipboardMonitor Stopped ===");
            m_file.close();
            m_initialized = false;
        }
    }
    
private:
    void WriteLog(const std::string& message) {
        if (!m_initialized || !m_file.is_open()) return;
        m_file << "[" << Utils::GetTimestamp() << "] " << message << std::endl;
        m_file.flush();
    }
    
    DebugLog() : m_initialized(false) {}
    ~DebugLog() { 
        if (m_file.is_open()) {
            m_file << "[SHUTDOWN] ClipboardMonitor exiting" << std::endl;
            m_file.close();
        }
    }
    
    std::ofstream m_file;
    std::recursive_mutex m_mutex;
    bool m_initialized;
};

#define DEBUG_LOG(msg) DebugLog::Instance().Log(msg)

