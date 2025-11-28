# LocalWaves 🌊

**High-Performance C++ LAN Video Streamer**

LocalWaves is a lightweight, ultra-fast desktop application that turns your PC into a local video streaming server. Built with **modern C++17** and **Qt 6**, it delivers raw performance with a custom-built HTTP server engine designed specifically for reliable media streaming over WiFi.

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![Platform](https://img.shields.io/badge/platform-Windows-lightgrey.svg)
![Build](https://img.shields.io/badge/build-CMake-green.svg)

## ✨ Key Features

### 🚀 Core Performance
*   **Custom HTTP Engine**: Built from scratch using raw Winsock2 sockets for maximum throughput.
*   **Zero-Copy Streaming**: Optimized buffer management for smooth 4K/1080p playback.
*   **Multi-Threaded**: Handles multiple concurrent connections effortlessly.
*   **Range Request Support**: Full support for seeking/skipping in videos (HTTP 206 Partial Content).

### 💻 Modern Web Interface (Client)
*   **Responsive Design**: Beautiful, touch-friendly UI that works perfectly on Mobile and Desktop.
*   **Dark Mode**: Built-in toggle for comfortable night-time viewing.
*   **Smart Resume**: Remembers exactly where you left off in every video.
*   **Search & Filter**: Instantly find files in large libraries.
*   **File Upload**: Wirelessly transfer files from your phone to your PC.

### 🛡️ Security & Control
*   **Password Protection**: Optional login system to secure your files.
*   **Connection Monitor**: Real-time counter of active clients.
*   **Custom Port**: Configurable server port (default: 4142).
*   **QR Code Connect**: Generate a QR code instantly to connect mobile devices without typing IPs.

## 🛠️ Tech Stack

*   **Language**: C++17
*   **GUI Framework**: Qt 6.8 (Widgets)
*   **Build System**: CMake
*   **Networking**: Native Winsock2 (Windows) / BSD Sockets (Linux ready)
*   **Frontend**: HTML5, CSS3 (Modern Variables), Vanilla JS

## 📦 Build Instructions

### Prerequisites
*   **C++ Compiler**: MinGW-w64 (GCC 11+) or MSVC 2019+
*   **CMake**: Version 3.16 or higher
*   **Qt 6**: Core, Gui, Widgets, Network modules

### Building on Windows (MSYS2 / MinGW)

1.  **Clone the repository**
    ```bash
    git clone https://github.com/Airyshtoteles/localWaves.git
    cd localWaves
    ```

2.  **Create build directory**
    ```bash
    mkdir build
    cd build
    ```

3.  **Configure & Build**
    ```bash
    cmake .. -G "MinGW Makefiles"
    cmake --build .
    ```

4.  **Run**
    ```bash
    ./CppVideoLan.exe
    ```

## 🤝 Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
