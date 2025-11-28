#pragma once

#include <string>
#include <memory>
#include <functional>

#ifdef _WIN32
    #include <winsock2.h>
    #include <windows.h>
    using SocketType = SOCKET;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <unistd.h>
    using SocketType = int;
#endif

namespace Server {

class HttpConnection {
public:
    HttpConnection(SocketType socket, const std::string& rootDir, const std::string& password, std::function<void(const std::string&)> logCallback);
    ~HttpConnection();

    void handle();

private:
    bool checkAuth(const std::string& request);
    void sendLogin();

    SocketType m_socket;
    std::string m_rootDir;
    std::string m_password;
    std::function<void(const std::string&)> m_log;

    void sendError(int code, const std::string& message);
    void sendResponse(const std::string& header);
    std::string urlDecode(const std::string& str);
};

}
