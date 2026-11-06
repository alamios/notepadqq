#include "include/diagnosticlog.h"

#include <QDir>
#include <QFile>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QtTest>
#include <atomic>
#include <thread>
#include <vector>

namespace {
std::atomic<int> forwarded{0};
void sink(QtMsgType, const QMessageLogContext&, const QString&) { ++forwarded; }
QByteArray read(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}
}

class DiagnosticLogTest : public QObject {
    Q_OBJECT
private slots:
    void persistenceAndFiltering()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto previous = qInstallMessageHandler(sink);
        auto cleanup = qScopeGuard([&] { DiagnosticLog::stop(); qInstallMessageHandler(previous); });
        QVERIFY(DiagnosticLog::start(directory.path()));
        qInfo("startup-marker");
        qWarning("warning-marker\nsecond-line");
        qDebug("private-debug-payload");
        const QString path = directory.filePath("notepadqq.log");
        const auto data = read(path); // Visible even before orderly shutdown.
        QVERIFY(data.contains(" INFO pid="));
        QVERIFY(data.contains(" tid="));
        QVERIFY(data.contains("warning-marker\\nsecond-line"));
        QVERIFY(!data.contains("private-debug-payload"));
        QVERIFY(!(QFile::permissions(path) & (QFileDevice::ReadGroup | QFileDevice::ReadOther)));
        DiagnosticLog::stop();
        QVERIFY(DiagnosticLog::start(directory.path()));
        qCritical("restart-marker");
        QVERIFY(read(path).contains("startup-marker"));
        QVERIFY(read(path).contains("restart-marker"));
    }

    void concurrentRotation()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto previous = qInstallMessageHandler(sink);
        auto cleanup = qScopeGuard([&] { DiagnosticLog::stop(); qInstallMessageHandler(previous); });
        QVERIFY(DiagnosticLog::start(directory.path(), 32768));
        const int before = forwarded;
        std::vector<std::thread> threads;
        for (int t = 0; t < 4; ++t) {
            threads.emplace_back([t] {
                for (int i = 0; i < 80; ++i)
                    qInfo().noquote() << "record" << t << i << QString(900, 'x') << "END";
            });
        }
        for (auto& thread : threads)
            thread.join();
        QCOMPARE(forwarded.load() - before, 320);
        const auto files = QDir(directory.path()).entryList({"notepadqq.log*"}, QDir::Files);
        QCOMPARE(files.size(), 4);
        for (const auto& file : files) {
            const auto data = read(directory.filePath(file));
            QVERIFY(data.size() <= 32768);
            for (const auto& line : data.split('\n')) {
                if (!line.isEmpty())
                    QVERIFY(line.endsWith("END"));
            }
        }
    }

    void rotationFailureKeepsExistingData()
    {
        QTemporaryDir directory;
        const auto previous = qInstallMessageHandler(sink);
        auto cleanup = qScopeGuard([&] { DiagnosticLog::stop(); qInstallMessageHandler(previous); });
        QVERIFY(DiagnosticLog::start(directory.path(), 32768));
        qInfo("preserved-marker");
        // A nonempty directory at the rotation destination forces a safe failure.
        QVERIFY(QDir().mkpath(directory.filePath("notepadqq.log.3")));
        QFile obstacle(directory.filePath("notepadqq.log.3/keep"));
        QVERIFY(obstacle.open(QIODevice::WriteOnly));
        obstacle.close();
        const int before = forwarded;
        for (int i = 0; i < 50; ++i)
            qInfo().noquote() << QString(1000, 'x');
        QCOMPARE(forwarded.load() - before, 50);
        QVERIFY(read(directory.filePath("notepadqq.log")).contains("preserved-marker"));
        QVERIFY(QFileInfo::exists(obstacle.fileName()));
    }

    void unavailableDirectory()
    {
        QTemporaryDir directory;
        QFile blocker(directory.filePath("file"));
        QVERIFY(blocker.open(QIODevice::WriteOnly));
        QVERIFY(!DiagnosticLog::start(blocker.fileName() + "/logs"));
        DiagnosticLog::stop();
    }
};

QTEST_GUILESS_MAIN(DiagnosticLogTest)
#include "tst_diagnosticlog.moc"
