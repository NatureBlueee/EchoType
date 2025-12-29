#pragma once

#include <string>

/**
 * @brief HTML Parser for clipboard CF_HTML format
 *
 * Windows clipboard stores HTML in a special "CF_HTML" format that includes
 * metadata about the HTML content and its source URL.
 *
 * Format example:
 * Version:0.9
 * StartHTML:0000000105
 * EndHTML:0000001234
 * StartFragment:0000000141
 * EndFragment:0000001198
 * SourceURL:https://example.com/page
 * <html>...</html>
 *
 * This parser extracts the SourceURL field, which indicates where the
 * HTML content was copied from (useful for browser context).
 */
class HTMLParser {
public:
    /**
     * @brief Data extracted from CF_HTML format
     */
    struct HTMLClipboardData {
        std::wstring sourceUrl;      // URL where the content was copied from
        std::string htmlContent;     // Raw HTML content
        int startHTML;               // Byte offset of HTML start
        int endHTML;                 // Byte offset of HTML end
        int startFragment;           // Byte offset of fragment start
        int endFragment;             // Byte offset of fragment end

        HTMLClipboardData()
            : startHTML(0)
            , endHTML(0)
            , startFragment(0)
            , endFragment(0)
        {}
    };

    /**
     * @brief Parse CF_HTML format data
     *
     * Extracts metadata (especially SourceURL) from Windows clipboard HTML format.
     *
     * @param cfHtmlData Raw CF_HTML data (UTF-8 or ASCII encoded)
     * @param output Output structure to receive parsed data
     * @return true if SourceURL was successfully extracted, false otherwise
     *
     * Example usage:
     * @code
     * if (OpenClipboard(hwnd)) {
     *     UINT htmlFormat = RegisterClipboardFormatW(L"HTML Format");
     *     if (IsClipboardFormatAvailable(htmlFormat)) {
     *         HANDLE hData = GetClipboardData(htmlFormat);
     *         char* data = static_cast<char*>(GlobalLock(hData));
     *         HTMLParser::HTMLClipboardData result;
     *         if (HTMLParser::ParseCFHTML(data, result)) {
     *             // Use result.sourceUrl
     *         }
     *         GlobalUnlock(hData);
     *     }
     *     CloseClipboard();
     * }
     * @endcode
     */
    static bool ParseCFHTML(const char* cfHtmlData, HTMLClipboardData& output);

    /**
     * @brief Parse CF_HTML format data (string version)
     *
     * @param cfHtmlData CF_HTML data as string
     * @param output Output structure to receive parsed data
     * @return true if SourceURL was successfully extracted, false otherwise
     */
    static bool ParseCFHTML(const std::string& cfHtmlData, HTMLClipboardData& output);

private:
    /**
     * @brief Extract value from metadata line
     *
     * @param line Metadata line (e.g., "SourceURL:https://example.com")
     * @param prefix Prefix to match (e.g., "SourceURL:")
     * @return Value after prefix, or empty string if not found
     */
    static std::string ExtractValue(const std::string& line, const std::string& prefix);

    /**
     * @brief Extract integer value from metadata line
     *
     * @param line Metadata line (e.g., "StartHTML:0000000105")
     * @param prefix Prefix to match
     * @return Integer value, or 0 if not found
     */
    static int ExtractIntValue(const std::string& line, const std::string& prefix);
};
