#include "include/editortabwidget.h"

#include "include/iconprovider.h"
#include "include/nqqsettings.h"

#include <QApplication>
#include <QFileInfo>
#include <QTabBar>
#include <QPainter>
#include <QTimer>
#include <QKeyEvent>
#include <QSignalBlocker>
#include <QStylePainter>
#include <QStyleOptionTab>

namespace {

struct TabAppearance {
    bool compact = true;
    bool fullTitles = true;
    bool wrap = true;
    int maximumWidth = 160;
};

class SizedTabBar : public QTabBar {
public:
    explicit SizedTabBar(QWidget* parent) : QTabBar(parent) {}
    TabAppearance appearance;
    int activeTab = -1;
    bool customSelection = false;
    QSize naturalSize(int index) const { return tabSizeHint(index); }

protected:
    QSize tabSizeHint(int index) const override
    {
        // Native label painting reserves its own horizontal margins. Reducing
        // those only in the size hint clips text even with elision disabled.
        const int nativePadding = qMax(24, style()->pixelMetric(QStyle::PM_TabBarTabHSpace, nullptr, this));
        const int padding = nativePadding + (appearance.compact ? 0 : 12);
        int width = fontMetrics().horizontalAdvance(tabText(index)) + padding;
        int height = fontMetrics().height();
        if (!tabIcon(index).isNull()) {
            width += iconSize().width() + 6;
            height = qMax(height, iconSize().height());
        }
        for (auto side : {QTabBar::LeftSide, QTabBar::RightSide}) {
            if (auto* button = tabButton(index, side)) {
                width += button->sizeHint().width() + 6;
                height = qMax(height, button->sizeHint().height());
            }
        }
        if (appearance.compact && !appearance.fullTitles)
            width = qMin(width, appearance.maximumWidth);
        // Reserve a separate color band and two pixels before label/icon content.
        return QSize(qMax(72, width), height + (appearance.compact ? 10 : 14));
    }

    QSize minimumTabSizeHint(int index) const override { return tabSizeHint(index); }

    void paintEvent(QPaintEvent* event) override
    {
        Q_UNUSED(event)
        QStylePainter native(this);
        for (int index = 0; index < count(); ++index) {
            QStyleOptionTab option;
            initStyleOption(&option, index);
            if (customSelection) {
                option.state.setFlag(QStyle::State_Selected, index == activeTab);
                option.state.setFlag(QStyle::State_HasFocus, index == activeTab && hasFocus());
                option.selectedPosition = index == activeTab - 1 ? QStyleOptionTab::NextIsSelected :
                    index == activeTab + 1 ? QStyleOptionTab::PreviousIsSelected : QStyleOptionTab::NotAdjacent;
            }
            native.drawControl(QStyle::CE_TabBarTabShape, option);
            option.rect.adjust(0, 6, 0, 0);
            native.drawControl(QStyle::CE_TabBarTabLabel, option);
        }
        native.end();
        QPainter painter(this);
        const QStringList ids = Editor::tabColorIds();
        const QColor colors[] = {QColor("#e05252"), QColor("#e58b36"), QColor("#d5b52e"),
                                 QColor("#48a868"), QColor("#438bd5"), QColor("#a477cc")};
        for (int index = 0; index < count(); ++index) {
            const int colorIndex = ids.indexOf(tabData(index).toString());
            if (colorIndex < 0)
                continue;
            const QRect rect = tabRect(index).adjusted(4, 2, -4, 0);
            painter.fillRect(QRect(rect.left(), rect.top(), rect.width(), 3), colors[colorIndex]);
        }
    }
};

class TabRow : public SizedTabBar {
public:
    explicit TabRow(QWidget* parent) : SizedTabBar(parent) {}
    int first = 0;
    std::function<void(int, QPoint)> dropped;
    std::function<void(int)> middleClosed;
    std::function<void(int)> traversed;
protected:
    void mousePressEvent(QMouseEvent* event) override
    {
        m_pressed = event->button() == Qt::LeftButton ? tabAt(event->position().toPoint()) : -1;
        m_origin = event->position().toPoint();
        SizedTabBar::mousePressEvent(event);
    }
    void mouseReleaseEvent(QMouseEvent* event) override
    {
        const int pressed = m_pressed;
        m_pressed = -1;
        if (event->button() == Qt::MiddleButton) {
            const int tab = tabAt(event->position().toPoint());
            if (tab >= 0)
                middleClosed(first + tab);
            return;
        }
        if (pressed >= 0 && (event->position().toPoint() - m_origin).manhattanLength() >= QApplication::startDragDistance()) {
            dropped(first + pressed, mapToParent(event->position().toPoint()));
            return;
        }
        SizedTabBar::mouseReleaseEvent(event);
    }
    void keyPressEvent(QKeyEvent* event) override
    {
        if (event->key() == Qt::Key_Left || event->key() == Qt::Key_Right) {
            int delta = event->key() == Qt::Key_Right ? 1 : -1;
            if (layoutDirection() == Qt::RightToLeft)
                delta = -delta;
            traversed(delta);
            event->accept();
            return;
        }
        SizedTabBar::keyPressEvent(event);
    }
private:
    int m_pressed = -1;
    QPoint m_origin;
};

// The parent bar remains the authoritative flat model used by QTabWidget.
// Child bars only present successive ranges; every event maps to a logical index.
class EditorTabBar : public SizedTabBar {
public:
    explicit EditorTabBar(QWidget* parent) : SizedTabBar(parent)
    {
        connect(this, &QTabBar::currentChanged, this, [this] { scheduleLayout(); });
        connect(this, &QTabBar::tabMoved, this, [this] { scheduleLayout(); });
    }

    void refreshAppearance()
    {
        const auto& settings = NqqSettings::getInstance().Appearance;
        appearance = {settings.getCompactTabs(), settings.getShowFullTabTitles(),
                      settings.getWrapTabs(), qBound(96, settings.getMaximumTabWidth(), 320)};
        setExpanding(false);
        setUsesScrollButtons(!appearance.wrap);
        setElideMode(Qt::ElideNone);
        setElideMode(appearance.fullTitles ? Qt::ElideNone : Qt::ElideMiddle);
        scheduleLayout();
        updateGeometry();
        update();
    }

    QRect visualRect(int index) const
    {
        if (!appearance.wrap)
            return tabRect(index);
        for (auto* row : m_rows) {
            if (!row->isHidden() && index >= row->first && index < row->first + row->count())
                return row->tabRect(index - row->first).translated(row->pos());
        }
        return {};
    }

    int visualTabAt(QPoint point) const
    {
        if (!appearance.wrap)
            return tabAt(point);
        for (auto* row : m_rows) {
            if (!row->isHidden() && row->geometry().contains(point)) {
                const int local = row->tabAt(point - row->pos());
                return local < 0 ? -1 : row->first + local;
            }
        }
        return -1;
    }

    QSize sizeHint() const override
    {
        if (!appearance.wrap)
            return SizedTabBar::sizeHint();
        const int available = qMax(1, parentWidget()->contentsRect().width());
        int rows = count() > 0 ? 1 : 0, used = 0, height = 0;
        for (int index = 0; index < count(); ++index) {
            const QSize size = naturalSize(index);
            if (used && used + size.width() > available) {
                ++rows;
                used = 0;
            }
            used += size.width();
            height = qMax(height, size.height());
        }
        return QSize(available, rows * height);
    }
    QSize minimumSizeHint() const override
    { return appearance.wrap ? QSize(0, sizeHint().height()) : SizedTabBar::minimumSizeHint(); }

    void scheduleLayout()
    {
        if (m_pending)
            return;
        m_pending = true;
        QTimer::singleShot(0, this, [this] { m_pending = false; rebuildRows(); });
    }

protected:
    void resizeEvent(QResizeEvent* event) override
    { SizedTabBar::resizeEvent(event); scheduleLayout(); }
    void tabInserted(int index) override
    { SizedTabBar::tabInserted(index); scheduleLayout(); }
    void tabRemoved(int index) override
    { SizedTabBar::tabRemoved(index); scheduleLayout(); }
    void tabLayoutChange() override { scheduleLayout(); }
    void paintEvent(QPaintEvent* event) override
    {
        if (!appearance.wrap)
            SizedTabBar::paintEvent(event);
    }
private:
    QList<TabRow*> m_rows;
    bool m_pending = false;

    void rebuildRows()
    {
        if (!appearance.wrap) {
            for (auto* row : m_rows)
                row->hide();
        }
        for (int index = 0; index < count(); ++index)
            for (auto side : {QTabBar::LeftSide, QTabBar::RightSide})
                if (auto* button = tabButton(index, side))
                    button->setVisible(!appearance.wrap);
        if (!appearance.wrap)
            return;
        const int available = qMax(1, parentWidget()->contentsRect().width());
        int index = 0, rowNumber = 0, y = 0;
        while (index < count()) {
            TabRow* row;
            if (rowNumber < m_rows.size()) {
                row = m_rows[rowNumber];
            } else {
                row = new TabRow(this);
                row->customSelection = true;
                row->setExpanding(false);
                row->setUsesScrollButtons(false);
                row->setDocumentMode(true);
                row->setTabsClosable(true);
                row->setContextMenuPolicy(Qt::CustomContextMenu);
                connect(row, &QTabBar::tabBarClicked, this, [this, row](int tab) {
                    if (tab >= 0) setCurrentIndex(row->first + tab);
                    emit tabBarClicked(tab < 0 ? -1 : row->first + tab);
                });
                connect(row, &QTabBar::currentChanged, this, [this, row](int tab) {
                    if (tab >= 0) setCurrentIndex(row->first + tab);
                });
                connect(row, &QTabBar::tabBarDoubleClicked, this, [this, row](int tab) {
                    emit tabBarDoubleClicked(tab < 0 ? -1 : row->first + tab);
                });
                connect(row, &QTabBar::tabCloseRequested, this, [this, row](int tab) {
                    emit tabCloseRequested(row->first + tab);
                });
                connect(row, &QWidget::customContextMenuRequested, this, [this, row](QPoint point) {
                    auto* widget = qobject_cast<EditorTabWidget*>(parentWidget());
                    if (widget) emit widget->customContextMenuRequested(row->mapTo(widget, point));
                });
                row->middleClosed = [this](int tab) { emit tabCloseRequested(tab); };
                row->traversed = [this](int delta) {
                    if (count()) setCurrentIndex((currentIndex() + delta + count()) % count());
                };
                row->dropped = [this](int from, QPoint point) {
                    const int to = visualTabAt(point);
                    if (to >= 0 && to != from) moveTab(from, to);
                };
                m_rows.append(row);
            }
            QSignalBlocker blocker(row);
            row->appearance = appearance;
            row->setIconSize(iconSize());
            row->activeTab = currentIndex() - index;
            row->setElideMode(appearance.fullTitles ? Qt::ElideNone : Qt::ElideMiddle);
            while (row->count()) row->removeTab(0);
            row->first = index;
            int used = 0, height = 0;
            while (index < count()) {
                const QSize size = naturalSize(index);
                if (used && used + size.width() > available) break;
                const int tab = row->addTab(tabIcon(index), tabText(index));
                row->setTabData(tab, tabData(index));
                row->setTabToolTip(tab, tabToolTip(index));
                if (index == currentIndex()) row->setCurrentIndex(tab);
                used += size.width();
                height = qMax(height, size.height());
                ++index;
            }
            if (currentIndex() < row->first || currentIndex() >= index)
                row->setCurrentIndex(-1);
            row->setUsesScrollButtons(used > available);
            row->setGeometry(0, y, available, height);
            row->show();
            y += height;
            ++rowNumber;
        }
        for (int i = rowNumber; i < m_rows.size(); ++i) m_rows[i]->hide();
        updateGeometry();
        update();
    }
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

QRect EditorTabWidget::visualTabRect(int index) const
{ return static_cast<EditorTabBar*>(tabBar())->visualRect(index).translated(tabBar()->pos()); }

int EditorTabWidget::tabAtPosition(QPoint point) const
{ return static_cast<EditorTabBar*>(tabBar())->visualTabAt(point - tabBar()->pos()); }

void EditorTabWidget::on_tabColorChanged()
{
    auto* editor = qobject_cast<Editor*>(sender());
    const int index = indexOf(editor);
    if (index >= 0) {
        tabBar()->setTabData(index, editor->tabColor());
        static_cast<EditorTabBar*>(tabBar())->scheduleLayout();
        tabBar()->update();
    }
}

int EditorTabWidget::addEditorTab(bool setFocus, const QString& title)
{ return this->rawAddEditorTab(setFocus, title, 0, 0); }

void EditorTabWidget::connectEditorSignals(Editor* editor)
{
    connect(editor, &Editor::unsavedTitleChanged, this, &EditorTabWidget::on_unsavedTitleChanged);
    connect(editor, &Editor::cleanChanged, this, &EditorTabWidget::on_cleanChanged);
    connect(editor, &Editor::tabColorChanged, this, &EditorTabWidget::on_tabColorChanged);

    connect(editor, &Editor::gotFocus, this, &EditorTabWidget::gotFocus);

    connect(editor, &Editor::mouseWheel, this, &EditorTabWidget::on_editorMouseWheel);

    connect(editor, &Editor::fileNameChanged, this, &EditorTabWidget::on_fileNameChanged);
}

void EditorTabWidget::disconnectEditorSignals(Editor* editor)
{
    disconnect(editor, &Editor::unsavedTitleChanged, this, &EditorTabWidget::on_unsavedTitleChanged);
    disconnect(editor, &Editor::cleanChanged, this, &EditorTabWidget::on_cleanChanged);
    disconnect(editor, &Editor::tabColorChanged, this, &EditorTabWidget::on_tabColorChanged);
    disconnect(editor, &Editor::gotFocus, this, &EditorTabWidget::gotFocus);
    disconnect(editor, &Editor::mouseWheel, this, &EditorTabWidget::on_editorMouseWheel);
    disconnect(editor, &Editor::fileNameChanged, this, &EditorTabWidget::on_fileNameChanged);
}

int EditorTabWidget::indexOf(Editor* editor) const
{ return QTabWidget::indexOf(editor); }

void EditorTabWidget::on_unsavedTitleChanged()
{
    auto* editor = qobject_cast<Editor*>(sender());
    if (editor && indexOf(editor) >= 0)
        setTabText(editor, editor->unsavedTitle());
}

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
        int index = tabAtPosition(ev->position().toPoint());

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
