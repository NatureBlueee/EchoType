#include "html_parser.h"
#include "../../utils.h"
#include "../../debug_log.h"
#include <sstream>
#include <algorithm>

bool HTMLParser::ParseCFHTML(const char* cfHtmlData, HTMLClipboardData& output) {
    if (!cfHtmlData) {
        return false;
    }

    return ParseCFHTML(std::string(cfHtmlData), output);
}

bool HTMLParser::ParseCFHTML(const std::string& cfHtmlData, HTMLClipboardData& output) {
    if (cfHtmlData.empty()) {
        return false;
    }

    // CF_HTML format is line-based with metadata at the top
    // We need to extract:
    // - SourceURL: The URL where the content was copied from
    // - StartHTML/EndHTML: Byte offsets for HTML content
    // - StartFragment/EndFragment: Byte offsets for selected fragment

    std::istringstream stream(cfHtmlData);
    std::string line;

    while (std::getline(stream, line)) {
        // Remove carriage return if present (Windows line endings)
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        // Check if line is empty or starts with '<' (HTML content starts)
        if (line.empty() || line[0] == '<') {
            break;  // End of metadata section
        }

        // Parse metadata fields
        if (line.find("SourceURL:") == 0) {
            std::string url = ExtractValue(line, "SourceURL:");
            output.sourceUrl = Utils::Utf8ToWide(url);
            DEBUG_LOG("HTMLParser: Found SourceURL: " + url);
        }
        else if (line.find("StartHTML:") == 0) {
            output.startHTML = ExtractIntValue(line, "StartHTML:");
        }
        else if (line.find("EndHTML:") == 0) {
            output.endHTML = ExtractIntValue(line, "EndHTML:");
        }
        else if (line.find("StartFragment:") == 0) {
            output.startFragment = ExtractIntValue(line, "StartFragment:");
        }
        else if (line.find("EndFragment:") == 0) {
            output.endFragment = ExtractIntValue(line, "EndFragment:");
        }
    }

    // Extract HTML content if offsets are available
    if (output.startHTML > 0 && output.endHTML > output.startHTML &&
        output.endHTML <= static_cast<int>(cfHtmlData.size())) {
        output.htmlContent = cfHtmlData.substr(output.startHTML,
                                               output.endHTML - output.startHTML);
    }

    // Return true if we successfully extracted a source URL
    return !output.sourceUrl.empty();
}

std::string HTMLParser::ExtractValue(const std::string& line, const std::string& prefix) {
    if (line.size() <= prefix.size()) {
        return "";
    }

    if (line.compare(0, prefix.size(), prefix) != 0) {
        return "";
    }

    std::string value = line.substr(prefix.size());

    // Trim whitespace
    size_t start = value.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }

    size_t end = value.find_last_not_of(" \t\r\n");
    return value.substr(start, end - start + 1);
}

int HTMLParser::ExtractIntValue(const std::string& line, const std::string& prefix) {
    std::string valueStr = ExtractValue(line, prefix);
    if (valueStr.empty()) {
        return 0;
    }

    try {
        return std::stoi(valueStr);
    } catch (...) {
        return 0;
    }
}
