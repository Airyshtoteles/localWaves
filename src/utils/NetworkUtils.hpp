#pragma once

#include <QString>
#include <QList>
#include <QNetworkInterface>

namespace Utils {

inline QStringList getAllIPAddresses() {
    QStringList list;
    const QHostAddress &localhost = QHostAddress(QHostAddress::LocalHost);
    for (const QNetworkInterface &netInterface : QNetworkInterface::allInterfaces()) {
        // Skip loopback and non-running interfaces
        if (!(netInterface.flags() & QNetworkInterface::IsUp) || 
            (netInterface.flags() & QNetworkInterface::IsLoopBack)) {
            continue;
        }

        for (const QNetworkAddressEntry &entry : netInterface.addressEntries()) {
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol && entry.ip() != localhost) {
                list.append(entry.ip().toString());
            }
        }
    }
    if (list.isEmpty()) list.append("127.0.0.1");
    return list;
}

}
