#include "storage.h"
#include "utils.h"
#include "context/context_data.h"
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

    // Serialize context data if available
    if (entry.contextData) {
        const auto& ctx = entry.contextData;
        json << ",\n    \"context\": {\n";
        json << "      \"adapter_type\": \"" << Utils::EscapeJson(ctx->adapterType) << "\",\n";
        json << "      \"success\": " << (ctx->success ? "true" : "false") << ",\n";
        json << "      \"fetch_time_ms\": " << ctx->fetchTimeMs;

        // Add common fields
        if (!ctx->url.empty()) {
            json << ",\n      \"url\": \"" << Utils::EscapeJson(Utils::WideToUtf8(ctx->url)) << "\"";
        }
        if (!ctx->title.empty()) {
            json << ",\n      \"title\": \"" << Utils::EscapeJson(Utils::WideToUtf8(ctx->title)) << "\"";
        }
        if (!ctx->error.empty()) {
            json << ",\n      \"error\": \"" << Utils::EscapeJson(Utils::WideToUtf8(ctx->error)) << "\"";
        }

        // Serialize adapter-specific fields
        if (ctx->adapterType == "browser") {
            const BrowserContext* browserCtx = static_cast<const BrowserContext*>(ctx.get());
            if (!browserCtx->sourceUrl.empty()) {
                json << ",\n      \"source_url\": \"" << Utils::EscapeJson(Utils::WideToUtf8(browserCtx->sourceUrl)) << "\"";
            }
            if (!browserCtx->addressBarUrl.empty()) {
                json << ",\n      \"address_bar_url\": \"" << Utils::EscapeJson(Utils::WideToUtf8(browserCtx->addressBarUrl)) << "\"";
            }
            if (!browserCtx->pageTitle.empty()) {
                json << ",\n      \"page_title\": \"" << Utils::EscapeJson(Utils::WideToUtf8(browserCtx->pageTitle)) << "\"";
            }
        }

        // Serialize metadata if present
        if (!ctx->metadata.empty()) {
            json << ",\n      \"metadata\": {\n";
            bool firstMeta = true;
            for (const auto& pair : ctx->metadata) {
                if (!firstMeta) {
                    json << ",\n";
                }
                json << "        \"" << Utils::EscapeJson(Utils::WideToUtf8(pair.first)) << "\": \""
                     << Utils::EscapeJson(Utils::WideToUtf8(pair.second)) << "\"";
                firstMeta = false;
            }
            json << "\n      }";
        }

        json << "\n    }";
    }
    // Fallback: serialize old contextUrl field if present
    else if (!entry.contextUrl.empty()) {
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
