#pragma once

#include <string>
#include <unordered_map>
#include <algorithm>

namespace Server {

inline std::string getMimeType(const std::string& path) {
    size_t dotPos = path.find_last_of('.');
    if (dotPos == std::string::npos) return "application/octet-stream";

    std::string ext = path.substr(dotPos);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    static const std::unordered_map<std::string, std::string> mimeTypes = {
        // Web & Text
        {".html", "text/html"}, {".htm", "text/html"},
        {".css", "text/css"}, {".js", "application/javascript"},
        {".json", "application/json"}, {".xml", "application/xml"},
        {".txt", "text/plain"}, {".srt", "text/plain"}, {".vtt", "text/vtt"},

        // Video
        {".mp4", "video/mp4"}, {".m4v", "video/mp4"},
        {".mkv", "video/x-matroska"}, {".webm", "video/webm"},
        {".avi", "video/x-msvideo"}, {".mov", "video/quicktime"},
        {".wmv", "video/x-ms-wmv"}, {".flv", "video/x-flv"},
        {".mpg", "video/mpeg"}, {".mpeg", "video/mpeg"},
        {".ts", "video/mp2t"}, {".3gp", "video/3gpp"},

        // Audio
        {".mp3", "audio/mpeg"}, {".wav", "audio/wav"},
        {".ogg", "audio/ogg"}, {".flac", "audio/flac"},
        {".aac", "audio/aac"}, {".m4a", "audio/mp4"},

        // Images
        {".jpg", "image/jpeg"}, {".jpeg", "image/jpeg"},
        {".png", "image/png"}, {".gif", "image/gif"},
        {".svg", "image/svg+xml"}, {".ico", "image/x-icon"},
        {".webp", "image/webp"}
    };

    auto it = mimeTypes.find(ext);
    if (it != mimeTypes.end()) {
        return it->second;
    }
    return "application/octet-stream";
}

}
