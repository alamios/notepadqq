#pragma once

#include <QString>

namespace DiagnosticLog {
// Install once, before worker threads start. stop() restores the previous handler.
bool start(const QString& directory, qint64 maxBytes = 2 * 1024 * 1024);
void stop();
}
