#include "include/comparisoncontroller.h"
#include "include/linediff.h"
#include <QThread>
#include <QTabBar>
#include <QMap>
#include <algorithm>

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
    const auto left = m_left, right = m_right;
    // Clear the pair before transfers emit selection signals. Queued validation
    // must not mistake restoration for another active comparison move.
    m_left.clear(); m_right.clear();
    restoreDocuments(left, right);
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
    if (!m_container || !m_container->tabWidgetFromEditor(left) || !m_container->tabWidgetFromEditor(right)) return false;
    stop();
    auto* sourceLeft = m_container->tabWidgetFromEditor(left);
    auto* sourceRight = m_container->tabWidgetFromEditor(right);
    m_leftOrigin = {sourceLeft, m_container->indexOf(sourceLeft), sourceLeft->indexOf(left)};
    m_rightOrigin = {sourceRight, m_container->indexOf(sourceRight), sourceRight->indexOf(right)};
    // Keep live editors and their undo/session metadata; only transfer tabs.
    auto* destinationLeft = m_container->tabWidget(0);
    if (sourceLeft != destinationLeft)
        destinationLeft->transferEditorTab(false, sourceLeft, sourceLeft->indexOf(left));
    sourceRight = m_container->tabWidgetFromEditor(right);
    const bool createPane = m_container->count() == 1;
    auto* destinationRight = createPane ? m_container->addTabWidget() : m_container->tabWidget(1);
    if (createPane) m_temporaryPane = destinationRight;
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
void ComparisonController::restoreDocuments(QPointer<Editor> left, QPointer<Editor> right)
{
    if (!m_container) return;
    struct ReturnTab { QPointer<Editor> editor; Origin origin; };
    QList<ReturnTab> tabs = {{left, m_leftOrigin}, {right, m_rightOrigin}};
    // When both tabs shared a pane, restore lower indices first after transfers.
    std::sort(tabs.begin(), tabs.end(), [](const ReturnTab& a, const ReturnTab& b) {
        if (a.origin.paneIndex != b.origin.paneIndex) return a.origin.paneIndex < b.origin.paneIndex;
        return a.origin.tabIndex < b.origin.tabIndex;
    });
    QMap<int, QPointer<EditorTabWidget>> recreated;
    for (auto& tab : tabs) {
        if (!tab.editor) continue;
        auto* source = m_container->tabWidgetFromEditor(tab.editor);
        // A document moved to another window belongs there; never pull it back.
        if (!source) continue;
        if (!tab.origin.pane) {
            auto pane = recreated.value(tab.origin.paneIndex);
            if (!pane) {
                pane = m_container->addTabWidget();
                m_container->insertWidget(qMin(tab.origin.paneIndex, m_container->count() - 1), pane);
                recreated.insert(tab.origin.paneIndex, pane);
            }
            tab.origin.pane = pane;
        }
        if (source != tab.origin.pane)
            tab.origin.pane->transferEditorTab(false, source, source->indexOf(tab.editor));
    }
    for (const auto& tab : tabs) {
        if (!tab.editor || !tab.origin.pane || m_container->tabWidgetFromEditor(tab.editor) != tab.origin.pane) continue;
        const int current = tab.origin.pane->indexOf(tab.editor);
        tab.origin.pane->tabBar()->moveTab(current, qBound(0, tab.origin.tabIndex, tab.origin.pane->count() - 1));
    }
    // Select the reference on return, and update the container's active pointer
    // before removing an empty comparison-only pane.
    EditorTabWidget* focus = left ? m_container->tabWidgetFromEditor(left) : nullptr;
    if (focus) focus->setCurrentIndex(focus->indexOf(left));
    else if (right) {
        focus = m_container->tabWidgetFromEditor(right);
        if (focus) focus->setCurrentIndex(focus->indexOf(right));
    }
    if (left || right || m_temporaryPane) {
        if (!focus) focus = m_container->tabWidget(0);
        m_container->activateTabWidget(focus);
    }
    if (m_temporaryPane && m_temporaryPane->count() == 0 && m_container->count() > 1) {
        delete m_temporaryPane.data();
    }
    m_temporaryPane.clear();
    m_leftOrigin = {}; m_rightOrigin = {};
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
