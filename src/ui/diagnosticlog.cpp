#include "include/diagnosticlog.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QThread>
#include <QtLogging>

#include <cstdio>

namespace {
struct State {
    QMutex mutex;
    QFile file;
    QString path;
    qint64 limit = 0;
    QtMessageHandler previous = nullptr;
    bool installed = false;
};

State& state()
{
    // Qt can log during static destruction, after main has returned.
    static auto* instance = new State;
    return *instance;
}

bool openLog(State& s)
{
    s.file.setFileName(s.path);
    if (!s.file.open(QIODevice::WriteOnly | QIODevice::Append))
        return false;
    return s.file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
}

bool rotate(State& s)
{
    s.file.close();
    const QString oldest = s.path + ".3";
    if (QFile::exists(oldest) && !QFile::remove(oldest))
        return false;
    for (int i = 2; i >= 0; --i) {
        const QString source = i == 0 ? s.path : s.path + "." + QString::number(i);
        if (QFile::exists(source) && !QFile::rename(source, s.path + "." + QString::number(i + 1)))
            return false;
    }
    return openLog(s);
}

void handler(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    auto& s = state();
    const char* level = "DEBUG";
    switch (type) {
    case QtInfoMsg: level = "INFO"; break;
    case QtWarningMsg: level = "WARN"; break;
    case QtCriticalMsg: level = "ERROR"; break;
    case QtFatalMsg: level = "FATAL"; break;
    case QtDebugMsg: break;
    }
    QString text = message.left(8192);
    text.replace('\r', "\\r").replace('\n', "\\n");
    const QByteArray record = QString("%1 %2 pid=%3 tid=%4 [%5] %6\n")
        .arg(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs), QString::fromLatin1(level))
        .arg(QCoreApplication::applicationPid())
        .arg(reinterpret_cast<quintptr>(QThread::currentThreadId()), 0, 16)
        .arg(QString::fromLatin1(context.category ? context.category : "default"), text).toUtf8();
    QtMessageHandler previous;
    {
        QMutexLocker lock(&s.mutex);
        previous = s.previous;
        // Legacy debug messages may include bridge payloads or extension manifests.
        if (type != QtDebugMsg && s.file.isOpen()) {
            bool ok = true;
            if (s.file.size() + record.size() > s.limit)
                ok = rotate(s);
            if (ok)
                ok = s.file.write(record) == record.size() && s.file.flush();
            if (!ok) {
                s.file.close();
                std::fputs("Notepadqq: diagnostic log write/rotation failed; using stderr only.\n", stderr);
            }
        }
    }
    // Never call Qt logging from inside the handler.
    if (previous)
        previous(type, context, message);
    else {
        std::fwrite(record.constData(), 1, record.size(), stderr);
        std::fflush(stderr);
    }
}
}

bool DiagnosticLog::start(const QString& directory, qint64 maxBytes)
{
    auto& s = state();
    QMutexLocker lock(&s.mutex);
    if (s.installed)
        return true;
    s.path = QDir(directory).filePath("notepadqq.log");
    s.limit = qMax<qint64>(maxBytes, 32768);
    if (!QDir().mkpath(directory) || !openLog(s)) {
        s.file.close();
        std::fputs("Notepadqq: could not open diagnostic log; using stderr only.\n", stderr);
        return false;
    }
    s.previous = qInstallMessageHandler(handler);
    s.installed = true;
    return true;
}

void DiagnosticLog::stop()
{
    auto& s = state();
    QMutexLocker lock(&s.mutex);
    if (!s.installed)
        return;
    qInstallMessageHandler(s.previous);
    s.file.close();
    s.installed = false;
}
