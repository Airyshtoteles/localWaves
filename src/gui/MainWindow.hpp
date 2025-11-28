#pragma once

#include <QMainWindow>
#include <QLineEdit>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QVBoxLayout>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QDialog>
#include <QLabel>
#include <memory>
#include "../server/HttpServer.hpp"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onBrowseClicked();
    void onStartStopClicked();
    void onShowQrClicked();
    void onQrImageLoaded(QNetworkReply *reply);
    void appendLog(const QString& message);

private:
    void setupUi();
    void updateServerStatus();
    void updateClientCount(int count);

    QLineEdit *m_pathInput;
    QLineEdit *m_portInput;
    QLineEdit *m_passwordInput;
    QPushButton *m_browseBtn;
    QPushButton *m_startStopBtn;
    QPushButton *m_qrBtn;
    
    QNetworkAccessManager *m_netManager;
    QDialog *m_qrDialog;
    QLabel *m_qrLabel;
    QTextEdit *m_logOutput;
    QLabel *m_statusLabel;
    QLabel *m_clientCountLabel;
    QComboBox *m_urlCombo;

    std::unique_ptr<Server::HttpServer> m_server;
};
