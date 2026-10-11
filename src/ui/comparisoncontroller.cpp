#include "include/comparisoncontroller.h"
#include "include/linediff.h"
#include <QThread>

namespace {
class DiffThread : public QThread {
public:
    QString left, right;
    std::shared_ptr<std::atomic_bool> canceled;
    LineDiff::Result result;
    void run() override { result = LineDiff::compare(left, right, *canceled); }
};
QVariantList lines(const QVector<int>& input)
{
    QVariantList output;
    for (int line : input) output.append(line);
    return output;
}
}

ComparisonController::ComparisonController(TopEditorContainer* container)
    : QObject(container), m_container(container)
{
    m_timer.setSingleShot(true);
    m_timer.setInterval(200);
    connect(&m_timer, &QTimer::timeout, this, &ComparisonController::calculate);
    // Tab transfers briefly detach an editor; validate after the move completes.
    connect(container, &TopEditorContainer::currentEditorChanged, this, [this] {
        QTimer::singleShot(0, this, [this] {
            if (!active()) return;
            auto* left = m_container->tabWidgetFromEditor(m_left);
            auto* right = m_container->tabWidgetFromEditor(m_right);
            if (!left || !right || left == right) stop();
        });
    });
}
ComparisonController::~ComparisonController()
{ stop(); }
void ComparisonController::clear()
{
    for (auto editor : {m_left, m_right})
        if (editor) editor->sendMessage("C_CMD_DIFF_CLEAR");
}
void ComparisonController::stop()
{
    ++m_generation;
    m_timer.stop();
    if (m_canceled) m_canceled->store(true);
    for (auto connection : m_connections) disconnect(connection);
    m_connections.clear();
    for (auto editor : {m_left, m_right})
        if (editor) editor->sendMessage("C_CMD_DIFF_END");
    m_left.clear(); m_right.clear();
    emit activeChanged(false);
    emit statusChanged(QString());
}
bool ComparisonController::start(Editor* left, Editor* right)
{
#ifdef NQQ_USE_MONACO
    Q_UNUSED(left); Q_UNUSED(right);
    return false;
#else
    if (!left || !right || left == right || !left->supportsCommentToggle() || !right->supportsCommentToggle()) return false;
    auto* sourceLeft = m_container->tabWidgetFromEditor(left);
    auto* sourceRight = m_container->tabWidgetFromEditor(right);
    if (!sourceLeft || !sourceRight) return false;
    stop();
    // Keep live editors and their undo/session metadata; only transfer tabs.
    auto* destinationLeft = m_container->tabWidget(0);
    if (sourceLeft != destinationLeft)
        destinationLeft->transferEditorTab(false, sourceLeft, sourceLeft->indexOf(left));
    sourceRight = m_container->tabWidgetFromEditor(right);
    auto* destinationRight = m_container->count() > 1 ? m_container->tabWidget(1) : m_container->addTabWidget();
    if (sourceRight != destinationRight)
        destinationRight->transferEditorTab(false, sourceRight, sourceRight->indexOf(right));
    destinationLeft->setCurrentIndex(destinationLeft->indexOf(left));
    destinationRight->setCurrentIndex(destinationRight->indexOf(right));
    m_left = left; m_right = right;
    left->sendMessage("C_CMD_DIFF_LOCK");
    right->sendMessage("C_CMD_DIFF_LOCK");
    for (auto* editor : {left, right}) {
        m_connections.append(connect(editor, &Editor::contentChanged, this, &ComparisonController::schedule));
        m_connections.append(connect(editor, &QObject::destroyed, this, &ComparisonController::stop));
    }
    emit activeChanged(true);
    schedule();
    return true;
#endif
}
void ComparisonController::schedule()
{
    ++m_generation;
    if (m_canceled) m_canceled->store(true);
    clear();
    if (active()) m_timer.start();
}
void ComparisonController::calculate()
{
    if (!active()) return;
    const auto generation = m_generation;
    QPointer<ComparisonController> guard(this);
    // Snapshots include the JavaScript revision. Apply commands also validate
    // that revision, covering edits whose throttled change event is in flight.
    m_left->asyncSendMessageWithResultP("C_FUN_DIFF_SNAPSHOT").then([guard, generation](QVariant left) {
        if (!guard || !guard->active() || generation != guard->m_generation) return;
        guard->m_right->asyncSendMessageWithResultP("C_FUN_DIFF_SNAPSHOT").then([guard, generation, left](QVariant right) {
            if (!guard || !guard->active() || generation != guard->m_generation) return;
            auto* worker = new DiffThread;
            worker->left = left.toMap().value("text").toString();
            worker->right = right.toMap().value("text").toString();
            worker->canceled = std::make_shared<std::atomic_bool>(false);
            guard->m_canceled = worker->canceled;
            connect(worker, &QThread::finished, guard, [guard, worker, generation, left, right] {
                if (!guard || !guard->active() || generation != guard->m_generation || worker->canceled->load()) return;
                if (worker->result.limited) {
                    guard->stop();
                    emit guard->statusChanged(tr("Comparison exceeds the size or work limit."));
                } else {
                    guard->m_left->sendMessage("C_CMD_DIFF_MARKS", QVariantMap{
                        {"revision", left.toMap().value("revision")}, {"side", "removed"}, {"lines", lines(worker->result.removed)}});
                    guard->m_right->sendMessage("C_CMD_DIFF_MARKS", QVariantMap{
                        {"revision", right.toMap().value("revision")}, {"side", "added"}, {"lines", lines(worker->result.added)}});
                    emit guard->statusChanged(worker->result.removed.isEmpty() && worker->result.added.isEmpty()
                        ? tr("Documents are identical.") : QString());
                }
                emit guard->updated();
            });
            connect(worker, &QThread::finished, worker, &QObject::deleteLater);
            worker->start();
            emit guard->calculationStarted();
        });
    });
}
