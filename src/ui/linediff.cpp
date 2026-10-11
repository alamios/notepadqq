#include "include/linediff.h"
#include <QStringList>
#include <vector>

LineDiff::Result LineDiff::compare(QString left, QString right, const std::atomic_bool& canceled)
{
    Result result;
    if (left.size() + right.size() > 4 * 1024 * 1024) {
        result.limited = true;
        return result;
    }
    left.replace("\r\n", "\n"); left.replace('\r', '\n');
    right.replace("\r\n", "\n"); right.replace('\r', '\n');
    const auto a = left.isEmpty() ? QStringList{} : left.split('\n');
    const auto b = right.isEmpty() ? QStringList{} : right.split('\n');
    if (a.size() + b.size() > 100000) {
        result.limited = true;
        return result;
    }
    int start = 0, ae = a.size(), be = b.size();
    while (start < ae && start < be && a[start] == b[start]) ++start;
    while (ae > start && be > start && a[ae - 1] == b[be - 1]) { --ae; --be; }
    const int n = ae - start, m = be - start;
    // Trim equal edges before allocating the LCS table. Limit both memory and
    // worst-case CPU work rather than freezing on unrelated large documents.
    if (qint64(n + 1) * (m + 1) > 4000000 || n + m > 20000) {
        result.limited = true;
        return result;
    }
    const int stride = m + 1;
    std::vector<int> table(size_t(n + 1) * stride, 0);
    for (int i = n - 1; i >= 0; --i) {
        if (canceled.load()) return {};
        for (int j = m - 1; j >= 0; --j) {
            table[size_t(i) * stride + j] = a[start + i] == b[start + j]
                ? 1 + table[size_t(i + 1) * stride + j + 1]
                : qMax(table[size_t(i + 1) * stride + j], table[size_t(i) * stride + j + 1]);
        }
    }
    int i = 0, j = 0;
    while (i < n || j < m) {
        if (canceled.load()) return {};
        if (i < n && j < m && a[start + i] == b[start + j]) { ++i; ++j; }
        else if (i < n && (j == m || table[size_t(i + 1) * stride + j] >= table[size_t(i) * stride + j + 1]))
            result.removed.append(start + i++);
        else result.added.append(start + j++);
    }
    return result;
}
