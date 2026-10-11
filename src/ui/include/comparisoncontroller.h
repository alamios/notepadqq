#ifndef COMPARISONCONTROLLER_H
#define COMPARISONCONTROLLER_H
#include "topeditorcontainer.h"
#include <QPointer>
#include <QTimer>
#include <atomic>
#include <memory>

// Owns one temporarily read-only comparison pair. Decorations never modify document contents.
class ComparisonController : public QObject {
    Q_OBJECT
public:
    explicit ComparisonController(TopEditorContainer* container);
    ~ComparisonController() override;
    bool active() const { return m_left && m_right; }
    bool start(Editor* left, Editor* right);
    void stop();
signals:
    void activeChanged(bool active);
    void statusChanged(const QString& message);
    void updated();
    void calculationStarted();
private:
    void schedule();
    void calculate();
    void clear();
    struct Origin {
        QPointer<EditorTabWidget> pane;
        int paneIndex = 0;
        int tabIndex = 0;
    };
    void restoreDocuments(QPointer<Editor> left, QPointer<Editor> right);
    QPointer<TopEditorContainer> m_container;
    Origin m_leftOrigin, m_rightOrigin;
    QPointer<EditorTabWidget> m_temporaryPane;
    QPointer<Editor> m_left, m_right;
    QTimer m_timer;
    quint64 m_generation = 0;
    QList<QMetaObject::Connection> m_connections;
    std::shared_ptr<std::atomic_bool> m_canceled;
};
#endif
