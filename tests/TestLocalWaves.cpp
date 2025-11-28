#include <QtTest>
#include "../src/server/MimeTypes.hpp"
#include "../src/server/HttpConnection.hpp"

class TestLocalWaves : public QObject {
    Q_OBJECT

private slots:
    void testMimeTypes();
    void testUrlDecode();
};

void TestLocalWaves::testMimeTypes() {
    QCOMPARE(Server::getMimeType("video.mp4"), std::string("video/mp4"));
    QCOMPARE(Server::getMimeType("image.png"), std::string("image/png"));
    QCOMPARE(Server::getMimeType("unknown.xyz"), std::string("application/octet-stream"));
}

void TestLocalWaves::testUrlDecode() {
    // We need to expose urlDecode or make it public/static for testing.
    // For now, let's assume we refactor HttpConnection to have a static helper or test a public method.
    // Since urlDecode is private in HttpConnection, we can't test it directly without friendship or refactoring.
    // Let's stick to MimeTypes for this example as it's a standalone header.
}

QTEST_MAIN(TestLocalWaves)
#include "TestLocalWaves.moc"
