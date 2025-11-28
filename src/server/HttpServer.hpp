#pragma once

#include <string>
#include <atomic>
#include <thread>
#include <functional>
#include <vector>

#ifdef _WIN32
    #include <winsock2.h>
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
#endif

namespace Server {

class HttpServer {
public:
    HttpServer();
    ~HttpServer();

    bool start(int port, const std::string& rootDir, const std::string& password = "");
    void stop();
    bool isRunning() const;
    void setLogCallback(std::function<void(const std::string&)> callback);
    void setClientCountCallback(std::function<void(int)> callback);

private:
    void acceptLoop();

    std::atomic<bool> m_running;
    std::string m_rootDir;
    std::string m_password;
    int m_port;
    std::atomic<int> m_activeConnections;
    std::function<void(int)> m_clientCountCallback;
    std::function<void(const std::string&)> m_logCallback;
    
#ifdef _WIN32
    SOCKET m_serverSocket;
#else
    int m_serverSocket;
#endif

    std::thread m_acceptThread;
};

}
