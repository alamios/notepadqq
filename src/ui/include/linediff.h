#ifndef LINEDIFF_H
#define LINEDIFF_H

#include <QString>
#include <QVector>
#include <atomic>

namespace LineDiff {
struct Result {
    QVector<int> removed;
    QVector<int> added;
    QVector<int> modifiedLeft;
    QVector<int> modifiedRight;
    bool limited = false;
};
// Compare normalized lines, including the final newline, with bounded work.
Result compare(QString left, QString right, const std::atomic_bool& canceled);
}
#endif
