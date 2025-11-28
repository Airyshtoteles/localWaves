#include "MainWindow.hpp"
#include "../utils/NetworkUtils.hpp"
#include <QFileDialog>
#include <QMessageBox>
#include <QGroupBox>
#include <QFormLayout>
#include <QSettings>
#include <QDesktopServices>
#include <QUrl>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_server(std::make_unique<Server::HttpServer>()), m_qrDialog(nullptr), m_qrLabel(nullptr) {
    setupUi();
    
    // Load Settings
    QSettings settings("CppVideoLan", "Server");
    QString lastPath = settings.value("lastPath").toString();
    if (!lastPath.isEmpty()) {
        m_pathInput->setText(lastPath);
    }
    m_portInput->setText(settings.value("port", "4142").toString());
    m_passwordInput->setText(settings.value("password", "").toString());

    m_netManager = new QNetworkAccessManager(this);
    connect(m_netManager, &QNetworkAccessManager::finished, this, &MainWindow::onQrImageLoaded);

    // Set up logging callback
    m_server->setLogCallback([this](const std::string& msg) {
        QMetaObject::invokeMethod(this, "appendLog", Qt::QueuedConnection, 
                                  Q_ARG(QString, QString::fromStdString(msg)));
    });
    
    m_server->setClientCountCallback([this](int count) {
        QMetaObject::invokeMethod(this, "updateClientCount", Qt::QueuedConnection, 
                                  Q_ARG(int, count));
    });
}

MainWindow::~MainWindow() {
    // Save Settings
    QSettings settings("CppVideoLan", "Server");
    settings.setValue("lastPath", m_pathInput->text());
    settings.setValue("port", m_portInput->text());
    settings.setValue("password", m_passwordInput->text());

    if (m_server->isRunning()) {
        m_server->stop();
    }
}

void MainWindow::setupUi() {
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

    // Configuration Group
    QGroupBox *configGroup = new QGroupBox("Server Configuration", this);
    QFormLayout *formLayout = new QFormLayout(configGroup);

    QHBoxLayout *pathLayout = new QHBoxLayout();
    m_pathInput = new QLineEdit(this);
    m_pathInput->setPlaceholderText("Select folder to stream...");
    m_pathInput->setReadOnly(true);
    m_browseBtn = new QPushButton("Browse...", this);
    connect(m_browseBtn, &QPushButton::clicked, this, &MainWindow::onBrowseClicked);
    
    pathLayout->addWidget(m_pathInput);
    pathLayout->addWidget(m_browseBtn);
    formLayout->addRow("Root Folder:", pathLayout);

    m_portInput = new QLineEdit(this);
    m_portInput->setPlaceholderText("4142");
    formLayout->addRow("Port:", m_portInput);

    m_passwordInput = new QLineEdit(this);
    m_passwordInput->setPlaceholderText("Optional (Leave empty for no password)");
    m_passwordInput->setEchoMode(QLineEdit::PasswordEchoOnEdit);
    formLayout->addRow("Password:", m_passwordInput);

    m_statusLabel = new QLabel("Stopped", this);
    m_statusLabel->setStyleSheet("color: red; font-weight: bold;");
    formLayout->addRow("Status:", m_statusLabel);

    m_clientCountLabel = new QLabel("0", this);
    formLayout->addRow("Active Clients:", m_clientCountLabel);

    m_urlCombo = new QComboBox(this);
    m_urlCombo->setEditable(true); // Allow user to copy text
    m_urlCombo->setPlaceholderText("Start server to see URLs");
    formLayout->addRow("Local URL:", m_urlCombo);

    mainLayout->addWidget(configGroup);

    // Controls
    QHBoxLayout *btnLayout = new QHBoxLayout();
    m_startStopBtn = new QPushButton("Start Server", this);
    m_startStopBtn->setMinimumHeight(40);
    connect(m_startStopBtn, &QPushButton::clicked, this, &MainWindow::onStartStopClicked);
    
    m_qrBtn = new QPushButton("Show QR Code", this);
    m_qrBtn->setMinimumHeight(40);
    m_qrBtn->setEnabled(false);
    connect(m_qrBtn, &QPushButton::clicked, this, &MainWindow::onShowQrClicked);

    btnLayout->addWidget(m_startStopBtn);
    btnLayout->addWidget(m_qrBtn);
    mainLayout->addLayout(btnLayout);

    // Logs
    QGroupBox *logGroup = new QGroupBox("Server Logs", this);
    QVBoxLayout *logLayout = new QVBoxLayout(logGroup);
    m_logOutput = new QTextEdit(this);
    m_logOutput->setReadOnly(true);
    logLayout->addWidget(m_logOutput);
    mainLayout->addWidget(logGroup);

    resize(600, 400);
    setWindowTitle("LAN Video Streamer (C++ Qt6)");
}

void MainWindow::onBrowseClicked() {
    QString dir = QFileDialog::getExistingDirectory(this, "Select Root Folder",
                                                    QDir::homePath(),
                                                    QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (!dir.isEmpty()) {
        m_pathInput->setText(dir);
    }
}

void MainWindow::onStartStopClicked() {
    if (m_server->isRunning()) {
        m_server->stop();
        updateServerStatus();
    } else {
        QString path = m_pathInput->text();
        if (path.isEmpty()) {
            QMessageBox::warning(this, "Error", "Please select a folder first.");
            return;
        }
        
        int port = m_portInput->text().toInt();
        if (port <= 0 || port > 65535) port = 4142;
        
        std::string password = m_passwordInput->text().toStdString();

        if (m_server->start(port, path.toStdString(), password)) {
            updateServerStatus();
        } else {
            QMessageBox::critical(this, "Error", "Failed to start server. Check logs.");
        }
    }
}

void MainWindow::updateServerStatus() {
    if (m_server->isRunning()) {
        m_startStopBtn->setText("Stop Server");
        m_statusLabel->setText("Running");
        m_statusLabel->setStyleSheet("color: green; font-weight: bold;");
        
        m_urlCombo->clear();
        QStringList ips = Utils::getAllIPAddresses();
        int port = m_portInput->text().toInt();
        if (port <= 0) port = 4142;

        for(const QString& ip : ips) {
            m_urlCombo->addItem(QString("http://%1:%2").arg(ip).arg(port));
        }
        
        m_pathInput->setEnabled(false);
        m_portInput->setEnabled(false);
        m_passwordInput->setEnabled(false);
        m_browseBtn->setEnabled(false);
        m_qrBtn->setEnabled(true);
    } else {
        m_startStopBtn->setText("Start Server");
        m_statusLabel->setText("Stopped");
        m_statusLabel->setStyleSheet("color: red; font-weight: bold;");
        
        m_urlCombo->clear();
        m_urlCombo->setPlaceholderText("Start server to see URLs");
        
        m_pathInput->setEnabled(true);
        m_portInput->setEnabled(true);
        m_passwordInput->setEnabled(true);
        m_browseBtn->setEnabled(true);
        m_qrBtn->setEnabled(false);
    }
}

void MainWindow::updateClientCount(int count) {
    m_clientCountLabel->setText(QString::number(count));
}

void MainWindow::appendLog(const QString& message) {
    m_logOutput->append(message);
}

void MainWindow::onShowQrClicked() {
    QString urlStr = m_urlCombo->currentText();
    if (urlStr.isEmpty()) return;
    
    // Create Dialog if not exists
    if (!m_qrDialog) {
        m_qrDialog = new QDialog(this);
        m_qrDialog->setWindowTitle("Scan QR Code");
        QVBoxLayout *layout = new QVBoxLayout(m_qrDialog);
        m_qrLabel = new QLabel("Loading QR Code...", m_qrDialog);
        m_qrLabel->setAlignment(Qt::AlignCenter);
        m_qrLabel->setMinimumSize(300, 300);
        layout->addWidget(m_qrLabel);
    }
    
    m_qrLabel->setText("Loading...");
    m_qrDialog->show();
    m_qrDialog->raise();
    m_qrDialog->activateWindow();

    QString qrApiUrl = "http://api.qrserver.com/v1/create-qr-code/?size=300x300&data=" + urlStr;
    m_netManager->get(QNetworkRequest(QUrl(qrApiUrl)));
}

void MainWindow::onQrImageLoaded(QNetworkReply *reply) {
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();
        QPixmap pixmap;
        pixmap.loadFromData(data);
        if (m_qrLabel) {
            m_qrLabel->setPixmap(pixmap);
        }
    } else {
        if (m_qrLabel) {
            m_qrLabel->setText("Error loading QR Code.\nCheck internet connection.");
        }
    }
    reply->deleteLater();
}
