#include "storage.h"
#include "utils.h"
#include <fstream>
#include <sstream>

Storage::Storage()
    : m_maxEntries(1000)
{
}

Storage::~Storage() {
}

bool Storage::Initialize(const std::wstring& directory) {
    m_directory = directory;
    m_filePath = directory + L"\\clipboard_history.json";
    
    // Ensure directory exists
    if (!Utils::EnsureDirectoryExists(directory)) {
        return false;
    }
    
    // Try to read existing entries
    ReadFromFile();
    
    return true;
}

bool Storage::SaveEntry(const ClipboardEntry& entry) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Convert to JSON
    std::string json = EntryToJson(entry);
    
    // Add to list
    m_entries.push_back(json);
    
    // Trim if too many entries
    while (m_entries.size() > m_maxEntries) {
        m_entries.erase(m_entries.begin());
    }
    
    // Write to file
    return WriteToFile();
}

std::string Storage::EntryToJson(const ClipboardEntry& entry) const {
    std::ostringstream json;
    
    json << "  {\n";
    json << "    \"timestamp\": \"" << Utils::EscapeJson(entry.timestamp) << "\",\n";
    json << "    \"content_type\": \"" << Utils::EscapeJson(entry.contentType) << "\",\n";
    json << "    \"content\": \"" << Utils::EscapeJson(Utils::WideToUtf8(entry.content)) << "\",\n";
    json << "    \"content_preview\": \"" << Utils::EscapeJson(Utils::WideToUtf8(entry.contentPreview)) << "\",\n";
    json << "    \"source\": {\n";
    json << "      \"process_name\": \"" << Utils::EscapeJson(Utils::WideToUtf8(entry.source.processName)) << "\",\n";
    json << "      \"process_path\": \"" << Utils::EscapeJson(Utils::WideToUtf8(entry.source.processPath)) << "\",\n";
    json << "      \"window_title\": \"" << Utils::EscapeJson(Utils::WideToUtf8(entry.source.windowTitle)) << "\",\n";
    json << "      \"pid\": " << entry.source.processId << "\n";
    json << "    }";
    
    if (!entry.contextUrl.empty()) {
        json << ",\n    \"context\": {\n";
        json << "      \"url\": \"" << Utils::EscapeJson(Utils::WideToUtf8(entry.contextUrl)) << "\"\n";
        json << "    }";
    }
    
    json << "\n  }";
    
    return json.str();
}

bool Storage::WriteToFile() {
    std::ofstream file(m_filePath, std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        return false;
    }
    
    // Write as JSON array
    file << "{\n";
    file << "\"version\": \"1.0\",\n";
    file << "\"generated\": \"" << Utils::GetTimestamp() << "\",\n";
    file << "\"entries\": [\n";
    
    for (size_t i = 0; i < m_entries.size(); i++) {
        file << m_entries[i];
        if (i < m_entries.size() - 1) {
            file << ",";
        }
        file << "\n";
    }
    
    file << "]\n";
    file << "}\n";
    
    file.close();
    return true;
}

bool Storage::ReadFromFile() {
    std::ifstream file(m_filePath);
    if (!file.is_open()) {
        return false;
    }
    
    // For simplicity, we're not parsing existing JSON
    // In a production app, you'd use a proper JSON library
    // This implementation just starts fresh each time
    
    file.close();
    return true;
}

std::vector<ClipboardEntry> Storage::GetEntries() const {
    // Not implemented - would require JSON parsing
    return std::vector<ClipboardEntry>();
}
