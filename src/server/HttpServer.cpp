#include "HttpServer.hpp"
#include "HttpConnection.hpp"
#include <iostream>

namespace Server {

HttpServer::HttpServer() : m_running(false), m_serverSocket(-1), m_activeConnections(0) {
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
}

HttpServer::~HttpServer() {
    stop();
#ifdef _WIN32
    WSACleanup();
#endif
}

bool HttpServer::start(int port, const std::string& rootDir, const std::string& password) {
    if (m_running) return false;

    m_port = port;
    m_rootDir = rootDir;
    m_password = password;
    m_activeConnections = 0;
    if (m_clientCountCallback) m_clientCountCallback(0);

    m_serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (m_serverSocket == -1) {
        if (m_logCallback) m_logCallback("Failed to create socket");
        return false;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    if (bind(m_serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        if (m_logCallback) m_logCallback("Failed to bind to port " + std::to_string(port));
        return false;
    }

    if (listen(m_serverSocket, 10) < 0) {
        if (m_logCallback) m_logCallback("Failed to listen");
        return false;
    }

    m_running = true;
    m_acceptThread = std::thread(&HttpServer::acceptLoop, this);
    
    if (m_logCallback) m_logCallback("Server started on port " + std::to_string(port));
    return true;
}

void HttpServer::stop() {
    if (!m_running) return;
    m_running = false;

#ifdef _WIN32
    closesocket(m_serverSocket);
#else
    close(m_serverSocket);
#endif

    if (m_acceptThread.joinable()) {
        m_acceptThread.join();
    }
    
    if (m_logCallback) m_logCallback("Server stopped");
}

bool HttpServer::isRunning() const {
    return m_running;
}

void HttpServer::setLogCallback(std::function<void(const std::string&)> callback) {
    m_logCallback = callback;
}

void HttpServer::setClientCountCallback(std::function<void(int)> callback) {
    m_clientCountCallback = callback;
}

void HttpServer::acceptLoop() {
    while (m_running) {
        sockaddr_in clientAddr;
#ifdef _WIN32
        int clientLen = sizeof(clientAddr);
#else
        socklen_t clientLen = sizeof(clientAddr);
#endif
        
        SocketType clientSocket = accept(m_serverSocket, (struct sockaddr*)&clientAddr, &clientLen);
        
        if (!m_running) break; // Check again after unblocking accept

        if (clientSocket != -1) {
            m_activeConnections++;
            if (m_clientCountCallback) m_clientCountCallback(m_activeConnections.load());

            // Spawn a new thread for each client
            std::thread([this, clientSocket]() {
                HttpConnection conn(clientSocket, m_rootDir, m_password, m_logCallback);
                conn.handle();
                
                m_activeConnections--;
                if (m_clientCountCallback) m_clientCountCallback(m_activeConnections.load());
            }).detach();
        }
    }
}

}
