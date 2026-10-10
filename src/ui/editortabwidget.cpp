#include "include/editortabwidget.h"

#include "include/iconprovider.h"
#include "include/nqqsettings.h"

#include <QApplication>
#include <QFileInfo>
#include <QTabBar>
#include <QPainter>

namespace {

// Keep Qt's native hit testing, close buttons, drag handling and active-tab style.
class EditorTabBar : public QTabBar {
public:
    explicit EditorTabBar(QWidget* parent) : QTabBar(parent) {}

    void refreshAppearance()
    {
        const auto& appearance = NqqSettings::getInstance().Appearance;
        m_compact = appearance.getCompactTabs();
        m_maximumWidth = qBound(96, appearance.getMaximumTabWidth(), 320);
        setExpanding(false);
        // Changing the elision policy invalidates Qt's cached tab size hints.
        setElideMode(Qt::ElideNone);
        setElideMode(Qt::ElideMiddle);
        updateGeometry();
        update();
    }

protected:
    QSize tabSizeHint(int index) const override
    {
        QSize size = QTabBar::tabSizeHint(index);
        if (m_compact) {
            size.setWidth(qBound(72, size.width() - 12, m_maximumWidth));
            size.setHeight(qMax(qMax(fontMetrics().height(), iconSize().height()) + 8,
                                size.height() - 4));
        } else {
            size.setWidth(qMax(100, size.width()));
        }
        return size;
    }

    void paintEvent(QPaintEvent* event) override
    {
        QTabBar::paintEvent(event);
        QPainter painter(this);
        const QStringList ids = Editor::tabColorIds();
        const QColor colors[] = {QColor("#e05252"), QColor("#e58b36"), QColor("#d5b52e"),
                                 QColor("#48a868"), QColor("#438bd5"), QColor("#a477cc")};
        for (int index = 0; index < count(); ++index) {
            const int colorIndex = ids.indexOf(tabData(index).toString());
            if (colorIndex < 0)
                continue;
            // A color band leaves native text, focus and dirty indicators readable.
            const QRect rect = tabRect(index).adjusted(4, 2, -4, 0);
            painter.fillRect(QRect(rect.left(), rect.top(), rect.width(), 3), colors[colorIndex]);
        }
    }

private:
    bool m_compact = true;
    int m_maximumWidth = 160;
};

} // namespace

#ifdef QT_DEBUG
#include <QElapsedTimer>
#endif

EditorTabWidget::EditorTabWidget(QWidget* parent)
    : QTabWidget(parent)
{
    setTabBar(new EditorTabBar(this));
    this->setContextMenuPolicy(Qt::CustomContextMenu);
    this->setDocumentMode(true);
    this->setTabsClosable(true);
    this->setMovable(true);

    this->setTabBarHidden(false);
    this->setTabBarHighlight(false);

    connect(this, &EditorTabWidget::currentChanged, this, &EditorTabWidget::on_currentTabChanged);

    this->setUsesScrollButtons(true);
    refreshTabAppearance();
}

void EditorTabWidget::refreshTabAppearance()
{ static_cast<EditorTabBar*>(tabBar())->refreshAppearance(); }

void EditorTabWidget::on_tabColorChanged()
{
    auto* editor = qobject_cast<Editor*>(sender());
    const int index = indexOf(editor);
    if (index >= 0) {
        tabBar()->setTabData(index, editor->tabColor());
        tabBar()->update();
    }
}

int EditorTabWidget::addEditorTab(bool setFocus, const QString& title)
{ return this->rawAddEditorTab(setFocus, title, 0, 0); }

void EditorTabWidget::connectEditorSignals(Editor* editor)
{
    connect(editor, &Editor::cleanChanged, this, &EditorTabWidget::on_cleanChanged);
    connect(editor, &Editor::tabColorChanged, this, &EditorTabWidget::on_tabColorChanged);

    connect(editor, &Editor::gotFocus, this, &EditorTabWidget::gotFocus);

    connect(editor, &Editor::mouseWheel, this, &EditorTabWidget::on_editorMouseWheel);

    connect(editor, &Editor::fileNameChanged, this, &EditorTabWidget::on_fileNameChanged);
}

void EditorTabWidget::disconnectEditorSignals(Editor* editor)
{
    disconnect(editor, &Editor::cleanChanged, this, &EditorTabWidget::on_cleanChanged);
    disconnect(editor, &Editor::tabColorChanged, this, &EditorTabWidget::on_tabColorChanged);
    disconnect(editor, &Editor::gotFocus, this, &EditorTabWidget::gotFocus);
    disconnect(editor, &Editor::mouseWheel, this, &EditorTabWidget::on_editorMouseWheel);
    disconnect(editor, &Editor::fileNameChanged, this, &EditorTabWidget::on_fileNameChanged);
}

int EditorTabWidget::indexOf(Editor* editor) const
{ return QTabWidget::indexOf(editor); }

int EditorTabWidget::indexOf(QWidget* widget) const
{ return QTabWidget::indexOf(widget); }

QString EditorTabWidget::tabText(Editor* editor) const
{ return editor->tabName(); }

QString EditorTabWidget::tabText(int index) const
{ return editor(index)->tabName(); }

void EditorTabWidget::setTabText(int index, const QString& text)
{
    QTabWidget::setTabText(index, text);
    editor(index)->setTabName(text);
    if (editor(index)->filePath().isEmpty())
        setTabToolTip(index, text);
}

void EditorTabWidget::setTabText(Editor* editor, const QString& text)
{
    int idx = indexOf(editor);

    if (idx >= 0) {
        QTabWidget::setTabText(idx, text);
        editor->setTabName(text);
        if (editor->filePath().isEmpty())
            setTabToolTip(idx, text);
    }
}

int EditorTabWidget::transferEditorTab(bool setFocus, EditorTabWidget* source, int tabIndex)
{ return this->rawAddEditorTab(setFocus, QString(), source, tabIndex); }

void EditorTabWidget::closeEditorTab(int index)
{
    Editor* closingEditor = editor(index);
    removeTab(index);
    if (closingEditor)
        closingEditor->deleteLater();
}

/**
 * @brief Do NOT directly connect to Editor signals within this method,
 *        or they'll remain attached to this EditorTabWidget whenever the
 *        tab gets moved to another container. Use connectEditorSignals()
 *        and disconnectEditorSignals() methods instead.
 */
int EditorTabWidget::rawAddEditorTab(
    const bool setFocus, const QString& title, EditorTabWidget* source, const int sourceTabIndex)
{
#ifdef QT_DEBUG
    QElapsedTimer __aet_timer;
    __aet_timer.start();
#endif

    bool create = (source == 0);

    this->setUpdatesEnabled(false);

    Editor* editor;

    QString oldText;
    QIcon oldIcon;
    QString oldTooltip;

    if (create) {
        editor = Editor::takeNewEditor();
    } else {
        editor = source->editor(sourceTabIndex);

        oldText = source->tabText(sourceTabIndex);
        oldIcon = source->tabIcon(sourceTabIndex);
        oldTooltip = source->tabToolTip(sourceTabIndex);
    }

    // Calling adTab() triggers MainWindow::refreshEditorUiInfo. We want to set the tab title
    // before that happens so it can be displayed properly.
    const QString& tabTitle = create ? title : oldText;
    editor->setTabName(tabTitle);
    if (!create) {
        source->disconnectEditorSignals(editor);
        source->removeTab(sourceTabIndex);
    }
    int index = addTab(editor, tabTitle);
    tabBar()->setTabData(index, editor->tabColor());
    setTabToolTip(index, tabTitle);

    this->connectEditorSignals(editor);

    if (setFocus) {
        this->setCurrentIndex(index);
        editor->setFocus();
    }

    if (create) {
        this->setSavedIcon(index, true);
    } else {
        this->setTabIcon(index, oldIcon);
        this->setTabToolTip(index, oldTooltip);
    }

    // Common setup
    editor->setZoomFactor(m_zoomFactor);

    this->setUpdatesEnabled(true);

    emit editorAdded(index);

#ifdef QT_DEBUG
    qint64 __aet_elapsed = __aet_timer.nsecsElapsed();
    qDebug() << QString("Tab opened in " + QString::number(__aet_elapsed / 1000 / 1000) + "msec").toStdString().c_str();
#endif

    return index;
}

int EditorTabWidget::findOpenEditorByUrl(const QUrl& filename)
{
    QUrl absFileName = filename;
    if (absFileName.isLocalFile())
        absFileName = QUrl::fromLocalFile(QFileInfo(filename.toLocalFile()).absoluteFilePath());

    for (int i = 0; i < count(); i++) {
        auto editor = this->editor(i);
        if (editor->filePath() == filename)
            return i;
    }

    return -1;
}

Editor* EditorTabWidget::editor(int index) const
{ return qobject_cast<Editor*>(widget(index)); }

Editor* EditorTabWidget::currentEditor()
{ return editor(currentIndex()); }

QString EditorTabWidget::tabTextFromEditor(Editor* ed)
{
    for (int i = 0; i < count(); ++i)
        if (editor(i) == ed)
            return tabText(i);

    return QString();
}

qreal EditorTabWidget::zoomFactor() const
{ return m_zoomFactor; }

void EditorTabWidget::setZoomFactor(const qreal& zoomFactor)
{
    m_zoomFactor = zoomFactor;

    for (int i = 0; i < count(); i++) {
        editor(i)->setZoomFactor(zoomFactor);
    }
}

void EditorTabWidget::deleteIfEmpty()
{ EditorTabWidget::deleteIfEmpty(this); }

void EditorTabWidget::deleteIfEmpty(EditorTabWidget* tabWidget)
{
    if (tabWidget->count() == 0) {
        delete tabWidget;
    }
}

void EditorTabWidget::setSavedIcon(int index, bool saved)
{
    if (saved)
        this->setTabIcon(index, IconProvider::fromTheme("document-saved"));
    else
        this->setTabIcon(index, IconProvider::fromTheme("document-unsaved"));
}

void EditorTabWidget::setTabBarHidden(bool yes)
{ tabBar()->setHidden(yes); }

void EditorTabWidget::setTabBarHighlight(bool yes)
{
    // Get colors from palette so it doesn't look fugly.
    QPalette palette = tabBar()->palette();
    palette.setColor(QPalette::Highlight,
        yes ? QApplication::palette().highlight().color() : QApplication::palette().light().color());
    tabBar()->setPalette(palette);
}

void EditorTabWidget::on_cleanChanged(bool isClean)
{
    Editor* editor = dynamic_cast<Editor*>(sender());
    if (!editor)
        return;

    int index = indexOf(editor);
    if (index >= 0)
        setSavedIcon(index, isClean);
}

void EditorTabWidget::on_editorMouseWheel(QWheelEvent* ev)
{
    Editor* editor = dynamic_cast<Editor*>(sender());
    if (!editor)
        return;

    emit editorMouseWheel(indexOf(editor), ev);
}

void EditorTabWidget::mouseReleaseEvent(QMouseEvent* ev)
{
    if (ev->button() == Qt::MiddleButton) {
        int index = tabBar()->tabAt(ev->pos());

        if (index != -1) {
            emit tabCloseRequested(index);
            ev->accept();
            return;
        }
    }

    QTabWidget::mouseReleaseEvent(ev);
}

QString EditorTabWidget::generateTabTitleForUrl(const QUrl& filename) const
{
    QString fileName =
        QFileInfo(filename.toDisplayString(QUrl::RemoveScheme | QUrl::RemovePassword | QUrl::RemoveUserInfo |
                                           QUrl::RemovePort | QUrl::RemoveAuthority | QUrl::RemoveQuery |
                                           QUrl::RemoveFragment | QUrl::PreferLocalFile))
            .fileName();
    return fileName;
}

void EditorTabWidget::on_fileNameChanged(const QUrl& /*oldFileName*/, const QUrl& newFileName)
{
    Editor* editor = dynamic_cast<Editor*>(sender());
    if (!editor)
        return;

    int index = indexOf(editor);

    QString fullFileName = newFileName.toDisplayString(QUrl::PreferLocalFile | QUrl::RemovePassword);

    setTabText(index, generateTabTitleForUrl(newFileName));
    setTabToolTip(index, fullFileName);
}

int EditorTabWidget::formerTabIndex()
{ return m_formerTabIndex; }

void EditorTabWidget::on_currentTabChanged(int index)
{
    // Store current index to become former index on next tab change.
    if (m_mostRecentTabIndex != index) {
        m_formerTabIndex = m_mostRecentTabIndex;
        m_mostRecentTabIndex = index;
    }
}
