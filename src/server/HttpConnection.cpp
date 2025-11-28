#include "HttpConnection.hpp"
#include "MimeTypes.hpp"
#include <iostream>
#include <sstream>
#include <vector>
#include <fstream>
#include <filesystem>
#include <cstring>
#include <cstdio>

#ifdef _WIN32
    #include <mswsock.h>
    #pragma comment(lib, "Mswsock.lib")
#else
    #include <sys/sendfile.h>
    #include <sys/stat.h>
    #include <fcntl.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
#endif

namespace fs = std::filesystem;

namespace Server {

HttpConnection::HttpConnection(SocketType socket, const std::string& rootDir, const std::string& password, std::function<void(const std::string&)> logCallback)
    : m_socket(socket), m_rootDir(rootDir), m_password(password), m_log(logCallback) {
    
    // OPTIMIZATION: Enable TCP_NODELAY to disable Nagle's algorithm for lower latency
    int flag = 1;
    setsockopt(m_socket, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(flag));

    // OPTIMIZATION: Increase Send Buffer Size (SO_SNDBUF)
    int sendBuff = 1024 * 1024; // 1MB Kernel Buffer
    setsockopt(m_socket, SOL_SOCKET, SO_SNDBUF, (char*)&sendBuff, sizeof(sendBuff));
}

HttpConnection::~HttpConnection() {
#ifdef _WIN32
    closesocket(m_socket);
#else
    close(m_socket);
#endif
}

std::string HttpConnection::urlDecode(const std::string& str) {
    std::string ret;
    char ch;
    int i, ii;
    for (i = 0; i < str.length(); i++) {
        if (str[i] != '%') {
            if (str[i] == '+')
                ret += ' ';
            else
                ret += str[i];
        } else {
            sscanf(str.substr(i + 1, 2).c_str(), "%x", &ii);
            ch = static_cast<char>(ii);
            ret += ch;
            i = i + 2;
        }
    }
    return ret;
}

bool HttpConnection::checkAuth(const std::string& request) {
    if (m_password.empty()) return true;
    if (request.find("Cookie: auth=1") != std::string::npos) return true;
    return false;
}

void HttpConnection::sendLogin() {
    std::string html = 
        "<!DOCTYPE html><html><head><title>Login</title>"
        "<meta name='viewport' content='width=device-width, initial-scale=1'>"
        "<style>"
        "body{font-family:'Segoe UI',sans-serif;background:#f0f2f5;display:flex;justify-content:center;align-items:center;height:100vh;margin:0;}"
        ".card{background:white;padding:30px;border-radius:10px;box-shadow:0 4px 12px rgba(0,0,0,0.1);width:100%;max-width:350px;text-align:center;}"
        "input{width:100%;padding:12px;margin:15px 0;border:1px solid #ddd;border-radius:6px;box-sizing:border-box;}"
        "button{width:100%;padding:12px;background:#0078d4;color:white;border:none;border-radius:6px;cursor:pointer;font-weight:bold;}"
        "button:hover{background:#006cbd;}"
        "</style></head><body>"
        "<div class='card'><h2>Locked</h2><p>Please enter password to access.</p>"
        "<form method='POST' action='/login'>"
        "<input type='password' name='password' placeholder='Password' required autofocus>"
        "<button type='submit'>Unlock</button>"
        "</form></div></body></html>";
    
    std::ostringstream response;
    response << "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: " << html.length() 
             << "\r\nConnection: keep-alive\r\n\r\n" << html;
    sendResponse(response.str());
}

void HttpConnection::handle() {
    // OPTIMIZATION: Set Receive Timeout (20s) to handle Keep-Alive without zombies
#ifdef _WIN32
    DWORD timeout = 20000;
    setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
#else
    struct timeval tv;
    tv.tv_sec = 20;
    tv.tv_usec = 0;
    setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
#endif

    char buffer[8192]; // Larger request buffer

    while (true) {
        int bytesRead = recv(m_socket, buffer, sizeof(buffer) - 1, 0);
        if (bytesRead <= 0) break; // Connection closed, timeout, or error

        buffer[bytesRead] = '\0';
        std::string request(buffer);
        std::istringstream iss(request);
        std::string method, path, protocol;
        iss >> method >> path >> protocol;

        if (method.empty()) break;

        // --- AUTHENTICATION ---
        if (!m_password.empty()) {
            if (method == "POST" && path == "/login") {
                // Parse body for password
                size_t bodyPos = request.find("\r\n\r\n");
                if (bodyPos != std::string::npos) {
                    std::string body = request.substr(bodyPos + 4);
                    std::string passPrefix = "password=";
                    if (body.find(passPrefix) == 0) {
                        std::string providedPass = urlDecode(body.substr(passPrefix.length()));
                        // Trim whitespace
                        providedPass.erase(providedPass.find_last_not_of(" \n\r\t") + 1);
                        
                        if (providedPass == m_password) {
                            std::ostringstream response;
                            response << "HTTP/1.1 302 Found\r\n"
                                     << "Set-Cookie: auth=1; Path=/\r\n"
                                     << "Location: /\r\n"
                                     << "Content-Length: 0\r\n\r\n";
                            sendResponse(response.str());
                            continue;
                        }
                    }
                }
                sendLogin(); // Fail
                continue;
            }

            if (!checkAuth(request)) {
                sendLogin();
                continue;
            }
        }

        if (method == "POST" && path.find("/upload") == 0) {
            std::string filename = "uploaded_file";
            size_t qPos = path.find("name=");
            if (qPos != std::string::npos) {
                filename = urlDecode(path.substr(qPos + 5));
                size_t endPos = filename.find('&'); // In case of other params
                if (endPos != std::string::npos) filename = filename.substr(0, endPos);
                // Safety: remove path separators
                size_t lastSlash = filename.find_last_of("/\\");
                if (lastSlash != std::string::npos) filename = filename.substr(lastSlash + 1);
            }

            // Find Content-Length
            size_t clPos = request.find("Content-Length: ");
            int contentLength = 0;
            if (clPos != std::string::npos) {
                // Simple parse, assumes header is well formed
                size_t endCl = request.find("\r\n", clPos);
                if (endCl != std::string::npos) {
                    contentLength = std::stoi(request.substr(clPos + 16, endCl - (clPos + 16)));
                }
            }

            // Find start of body (after \r\n\r\n)
            size_t bodyPos = request.find("\r\n\r\n");
            if (bodyPos == std::string::npos) { sendError(400, "Bad Request"); continue; }
            bodyPos += 4;

            std::string body = request.substr(bodyPos);
            int bytesReceived = body.length();

            // Open file
            std::ofstream outfile(fs::path(m_rootDir) / filename, std::ios::binary);
            outfile.write(body.c_str(), body.length());

            // Read remaining bytes
            int bytesLeft = contentLength - bytesReceived;
            char upBuf[8192];
            while (bytesLeft > 0) {
                int r = recv(m_socket, upBuf, std::min((int)sizeof(upBuf), bytesLeft), 0);
                if (r <= 0) break;
                outfile.write(upBuf, r);
                bytesLeft -= r;
            }
            outfile.close();

            sendResponse("HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n");
            m_log("Uploaded: " + filename);
            continue;
        }

        if (method != "GET") {
            sendError(405, "Method Not Allowed");
            break; // Close on error
        }

        path = urlDecode(path);
        if (path.find("..") != std::string::npos) {
            sendError(403, "Forbidden");
            break;
        }

        // Remove query string
        size_t queryPos = path.find('?');
        if (queryPos != std::string::npos) {
            path = path.substr(0, queryPos);
        }

        // --- FEATURE: HTML5 Video Player Wrapper (/view/...) ---
        if (path.rfind("/view/", 0) == 0) { 
            // ... (Keep existing player logic, but return to loop? No, usually browsers load page then close)
            // For simplicity, we'll just process it and break/return as it's a small page.
            // Copy-paste the player logic here or refactor. 
            // Let's keep the player logic simple: send and continue.
            
            std::string realPathStr = path.substr(5);
            fs::path realPath = fs::path(m_rootDir) / (realPathStr.substr(1));
            
            if (fs::exists(realPath) && !fs::is_directory(realPath)) {
                std::string filename = realPath.filename().string();
                std::string srtPath = realPathStr.substr(0, realPathStr.find_last_of('.')) + ".srt";
                fs::path fullSrtPath = fs::path(m_rootDir) / (srtPath.substr(1));
                bool hasSrt = fs::exists(fullSrtPath);

                std::ostringstream html;
                html << "<html><head><title>" << filename << "</title>"
                     << "<meta name='viewport' content='width=device-width, initial-scale=1'>"
                     << "<style>body{margin:0;background:#000;display:flex;justify-content:center;align-items:center;height:100vh;}"
                     << "video{max-width:100%;max-height:100%;box-shadow:0 0 20px #000;}"
                     << ".back{position:absolute;top:20px;left:20px;color:white;text-decoration:none;background:rgba(0,0,0,0.5);padding:10px;border-radius:5px;font-family:sans-serif;}"
                     << "</style>"
                     << "<script>"
                     << "window.onload = function() {"
                     << "  var vid = document.querySelector('video');"
                     << "  var key = 'vid_pos_" << filename << "';"
                     << "  var saved = localStorage.getItem(key);"
                     << "  if(saved) vid.currentTime = parseFloat(saved);"
                     << "  setInterval(function(){ localStorage.setItem(key, vid.currentTime); }, 1000);"
                     << "};"
                     << "</script>"
                     << "</head><body>"
                     << "<a href='/' class='back'>&larr; Back</a>"
                     << "<video controls autoplay playsinline>"
                     << "<source src=\"" << realPathStr << "\" type=\"" << getMimeType(realPathStr) << "\">";
                if (hasSrt) html << "<track label=\"Subtitle\" kind=\"subtitles\" srclang=\"en\" src=\"" << srtPath << "\" default>";
                html << "Your browser does not support the video tag.</video></body></html>";

                std::string body = html.str();
                std::ostringstream response;
                response << "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: " << body.length() 
                         << "\r\nConnection: keep-alive\r\n\r\n" << body;
                sendResponse(response.str());
                m_log("Serving Player for: " + filename);
                continue; // Keep alive
            }
        }

        fs::path fullPath = fs::path(m_rootDir) / (path == "/" ? "" : path.substr(1));
        
        if (!fs::exists(fullPath)) {
            sendError(404, "Not Found");
            m_log("404 Not Found: " + path);
            continue;
        }

        if (fs::is_directory(fullPath)) {
            std::ostringstream html;
            html << "<!DOCTYPE html><html lang='en'><head>"
                 << "<meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no'>"
                 << "<title>LAN Streamer</title>"
                 << "<style>"
                 << ":root { --primary: #0078d4; --bg: #f5f7fa; --card: #ffffff; --text: #333; --border: #e1e4e8; }"
                 << "[data-theme='dark'] { --primary: #4da6ff; --bg: #121212; --card: #1e1e1e; --text: #e0e0e0; --border: #333; }"
                 << "body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif; margin: 0; padding: 0; background: var(--bg); color: var(--text); -webkit-tap-highlight-color: transparent; transition: background 0.3s, color 0.3s; }"
                 << ".container { max-width: 800px; margin: 0 auto; padding: 20px; }"
                 << "header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 20px; }"
                 << "h1 { margin: 0; font-size: 1.5rem; color: var(--primary); }"
                 << ".upload-area { background: var(--card); padding: 15px; border-radius: 12px; box-shadow: 0 2px 8px rgba(0,0,0,0.05); margin-bottom: 20px; display: flex; gap: 10px; align-items: center; }"
                 << ".upload-area input[type='file'] { flex: 1; font-size: 14px; color: var(--text); }"
                 << ".btn { background: var(--primary); color: white; border: none; padding: 10px 20px; border-radius: 8px; cursor: pointer; font-weight: 600; transition: opacity 0.2s; white-space: nowrap; }"
                 << ".btn:disabled { opacity: 0.6; cursor: not-allowed; }"
                 << ".search-box { width: 100%; padding: 12px 15px; border: 2px solid var(--border); border-radius: 12px; font-size: 16px; box-sizing: border-box; margin-bottom: 20px; transition: border-color 0.2s; -webkit-appearance: none; background: var(--card); color: var(--text); }"
                 << ".search-box:focus { border-color: var(--primary); outline: none; }"
                 << ".file-list { background: var(--card); border-radius: 12px; box-shadow: 0 2px 8px rgba(0,0,0,0.05); overflow: hidden; }"
                 << ".file-item { display: flex; align-items: center; padding: 16px; border-bottom: 1px solid var(--border); text-decoration: none; color: var(--text); transition: background 0.1s; }"
                 << ".file-item:last-child { border-bottom: none; }"
                 << ".file-item:active { background: rgba(0,0,0,0.05); }"
                 << ".icon { font-size: 24px; margin-right: 16px; width: 30px; text-align: center; flex-shrink: 0; }"
                 << ".name { font-size: 16px; font-weight: 500; word-break: break-word; }"
                 << "@media (max-width: 600px) { .container { padding: 15px; } h1 { font-size: 1.25rem; } .upload-area { flex-direction: column; align-items: stretch; } .btn { width: 100%; } }"
                 << "</style>"
                 << "<script>"
                 << "function toggleTheme() { const body = document.body; const current = body.getAttribute('data-theme'); const next = current === 'dark' ? 'light' : 'dark'; body.setAttribute('data-theme', next); localStorage.setItem('theme', next); }"
                 << "function initTheme() { const saved = localStorage.getItem('theme'); if(saved) document.body.setAttribute('data-theme', saved); }"
                 << "function filterList() { const filter = document.getElementById('search').value.toUpperCase(); const items = document.getElementsByClassName('file-item'); for (let item of items) { const txt = item.innerText; item.style.display = txt.toUpperCase().includes(filter) ? '' : 'none'; } }"
                 << "function upload() { const file = document.getElementById('upfile').files[0]; if(!file) return; const btn = document.getElementById('upbtn'); btn.innerText = 'Uploading...'; btn.disabled = true; const xhr = new XMLHttpRequest(); xhr.open('POST', '/upload?name=' + encodeURIComponent(file.name), true); xhr.onload = function() { if(xhr.status == 200) { location.reload(); } else { alert('Error'); btn.innerText = 'Upload'; btn.disabled = false; } }; xhr.send(file); }"
                 << "</script>"
                 << "</head><body onload='initTheme()'>"
                 << "<div class='container'>"
                 << "<header><h1>LAN Streamer</h1><button class='btn' onclick='toggleTheme()'>&#9790;</button></header>"
                 << "<div class='upload-area'>"
                 << "<input type='file' id='upfile'>"
                 << "<button id='upbtn' class='btn' onclick='upload()'>Upload</button>"
                 << "</div>"
                 << "<input type='text' id='search' class='search-box' onkeyup='filterList()' placeholder='Search files...'>"
                 << "<div class='file-list'>";
            
            // Add "Up Directory" link if not root
            if (path != "/") {
                std::string parentPath = path.substr(0, path.find_last_of('/'));
                if (parentPath.empty()) parentPath = "/";
                html << "<a href=\"" << parentPath << "\" class='file-item'><span class='icon'>&#11013;</span><span class='name'>.. (Parent Directory)</span></a>";
            }

            for (const auto& entry : fs::directory_iterator(fullPath)) {
                std::string filename = entry.path().filename().string();
                if (filename[0] == '.') continue;
                std::string linkPath = (path == "/" ? "" : path) + "/" + filename;
                std::string ext = filename.substr(filename.find_last_of('.') + 1);
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                
                std::string icon = "&#128196;";
                bool isVideo = false;
                if (ext == "mp4" || ext == "mkv" || ext == "webm" || ext == "avi" || ext == "mov") { icon = "&#127916;"; isVideo = true; }
                else if (ext == "mp3" || ext == "wav" || ext == "flac") icon = "&#127925;";
                else if (ext == "jpg" || ext == "png" || ext == "gif") icon = "&#127912;";
                else if (fs::is_directory(entry.path())) icon = "&#128193;";

                if (isVideo) html << "<a href=\"/view" << linkPath << "\" class='file-item'><span class='icon'>" << icon << "</span><span class='name'>" << filename << "</span></a>";
                else html << "<a href=\"" << linkPath << "\" class='file-item'><span class='icon'>" << icon << "</span><span class='name'>" << filename << "</span></a>";
            }
            html << "</div></div></body></html>";
            
            std::string body = html.str();
            std::ostringstream response;
            response << "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: " << body.length() 
                     << "\r\nConnection: keep-alive\r\n\r\n" << body;
            sendResponse(response.str());
            m_log("Serving Directory Listing");
            continue;
        }

        uintmax_t fileSize = fs::file_size(fullPath);
        std::string mimeType = getMimeType(fullPath.string());

        // Parse Range Header
        size_t rangePos = request.find("Range: bytes=");
        int64_t start = 0;
        int64_t end = fileSize - 1;
        bool isPartial = false;

        if (rangePos != std::string::npos) {
            isPartial = true;
            size_t eol = request.find("\r\n", rangePos);
            std::string rangeVal = request.substr(rangePos + 13, eol - (rangePos + 13));
            size_t dashPos = rangeVal.find('-');
            try {
                start = std::stoll(rangeVal.substr(0, dashPos));
                if (dashPos + 1 < rangeVal.length()) end = std::stoll(rangeVal.substr(dashPos + 1));
            } catch (...) { isPartial = false; start = 0; end = fileSize - 1; }
        }

        if (end >= fileSize) end = fileSize - 1;
        int64_t contentLength = end - start + 1;

        std::ostringstream response;
        if (isPartial) {
            response << "HTTP/1.1 206 Partial Content\r\n";
            response << "Content-Range: bytes " << start << "-" << end << "/" << fileSize << "\r\n";
        } else {
            response << "HTTP/1.1 200 OK\r\n";
        }
        
        response << "Content-Type: " << mimeType << "\r\n";
        response << "Content-Length: " << contentLength << "\r\n";
        response << "Accept-Ranges: bytes\r\n";
        response << "Connection: close\r\n";
        response << "\r\n";

        sendResponse(response.str());
        m_log("Serving: " + path + (isPartial ? " (Partial)" : ""));

        // --- STABLE SEND LOOP (Fixes Freezing) ---
        FILE* fp = fopen(fullPath.string().c_str(), "rb");
        if (fp) {
            #ifdef _WIN32
            _fseeki64(fp, start, SEEK_SET);
            #else
            fseeko(fp, start, SEEK_SET);
            #endif
            
            int64_t remaining = contentLength;
            const size_t bufSize = 65536; // 64KB Buffer (Stable for WiFi)
            std::vector<char> buffer(bufSize);

            while (remaining > 0) {
                size_t toRead = std::min((int64_t)bufSize, remaining);
                size_t bytesRead = fread(buffer.data(), 1, toRead, fp);
                
                if (bytesRead > 0) {
                    // Ensure ALL bytes are sent
                    char* p = buffer.data();
                    size_t bytesLeftToSend = bytesRead;
                    
                    while (bytesLeftToSend > 0) {
                        int bytesSent = send(m_socket, p, (int)bytesLeftToSend, 0);
                        if (bytesSent <= 0) {
                            fclose(fp);
                            return; // Client disconnected
                        }
                        p += bytesSent;
                        bytesLeftToSend -= bytesSent;
                    }
                    
                    remaining -= bytesRead;
                } else {
                    break; // EOF or error
                }
            }
            fclose(fp);
        }
        break; // Close connection after serving file (More stable than Keep-Alive for now)
    }
}

void HttpConnection::sendError(int code, const std::string& message) {
    std::ostringstream response;
    response << "HTTP/1.1 " << code << " " << message << "\r\n";
    response << "Content-Type: text/plain\r\n";
    response << "Content-Length: " << message.length() << "\r\n";
    response << "Connection: close\r\n";
    response << "\r\n";
    response << message;
    sendResponse(response.str());
}

void HttpConnection::sendResponse(const std::string& header) {
    send(m_socket, header.c_str(), header.length(), 0);
}

}
