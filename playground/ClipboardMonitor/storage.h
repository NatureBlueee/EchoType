#pragma once

#include "clipboard_monitor.h"
#include <string>
#include <vector>
#include <mutex>

class Storage {
public:
    Storage();
    ~Storage();
    
    // Initialize storage with directory path
    bool Initialize(const std::wstring& directory);
    
    // Save a clipboard entry
    bool SaveEntry(const ClipboardEntry& entry);
    
    // Get all entries (optional, for future use)
    std::vector<ClipboardEntry> GetEntries() const;
    
    // Get storage file path
    std::wstring GetFilePath() const { return m_filePath; }
    
    // Set maximum entries to keep
    void SetMaxEntries(size_t max) { m_maxEntries = max; }

private:
    // Convert entry to JSON string
    std::string EntryToJson(const ClipboardEntry& entry) const;
    
    // Write entries to file
    bool WriteToFile();
    
    // Read entries from file
    bool ReadFromFile();
    
private:
    std::wstring m_directory;
    std::wstring m_filePath;
    std::vector<std::string> m_entries;  // Store as JSON strings for simplicity
    size_t m_maxEntries;
    mutable std::mutex m_mutex;
};
