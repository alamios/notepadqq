#include "include/EditorNS/editor.h"
#include "include/editoruicontroller.h"
#include "include/mainwindow.h"
#include "include/notepadqq.h"
#include "include/nqqsettings.h"
#include "include/docengine.h"
#include "include/comparisoncontroller.h"
#include "include/linediff.h"
#include "include/Sessions/sessions.h"
#include "include/Sessions/backupservice.h"
#include "include/Sessions/persistentcache.h"
#include "include/frmpreferences.h"

#include <QCloseEvent>
#include <QContextMenuEvent>
#include <QAction>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPointer>
#include <QSettings>
#include <QFile>
#include <QDir>
#include <QTabBar>
#include <QCheckBox>
#include <QComboBox>
#include <QToolBar>
#include <QToolButton>
#include <QSignalSpy>
#include <QSpinBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QScopeGuard>
#include <QMenu>
#include <QInputDialog>
#include <QTranslator>
#include <QFormLayout>
#include <QLabel>
#include <QProxyStyle>
#include <QStyleFactory>
#include <QStackedWidget>
#include <QTreeWidget>
#include <QString>
#include <QTemporaryDir>
#include <QTimer>
#include <QWebEngineView>
#include <QtTest>

#include <memory>

class NotepadqqTest : public QObject {
    Q_OBJECT

public:
    NotepadqqTest();

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void editorPathIsHtml();
    void acceptedCloseDisconnectsEditorUiController();
    void directWindowDeletionDestroysEditorUiControllerFirst();
    void toggleComment_data();
    void toggleComment();
    void toggleCommentShortcut();
    void compactTabsAndColors();
    void wrappingTabs();
    void tabInsertionPreview_data();
    void tabInsertionPreview();
    void lineDifferences_data();
    void lineDifferences();
    void comparisonUi();
    void comparisonCancellation();
    void comparisonRestoresViews_data();
    void comparisonRestoresViews();
    void comparisonRestoresMissingView();
    void forkTranslations_data();
    void forkTranslations();
    void removesLegacyTelemetrySettings();
    void tabColorSession_data();
    void tabColorSession();
    void legacyTabColorSession_data();
    void legacyTabColorSession();
    void tabAppearancePreferences();
    void appearancePreferencesAlignment();
    void tabColorAutosave();
    void tabColorContextMenu();
    void unsavedTabTitle();
    void unsavedTitleSession_data();
    void unsavedTitleSession();
    void unsavedTitleContextMenu_data();
    void unsavedTitleContextMenu();

private:
    QTemporaryDir m_settingsDirectory;
};

NotepadqqTest::NotepadqqTest() {}

void NotepadqqTest::initTestCase()
{
    QVERIFY(m_settingsDirectory.isValid());
    QCoreApplication::setOrganizationName("Notepadqq-tests");
    QCoreApplication::setApplicationName("Notepadqq-tests");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settingsDirectory.path());
    QVERIFY(NqqSettings::getInstance().Appearance.getShowFullTabTitles());
    QVERIFY(NqqSettings::getInstance().Appearance.getWrapTabs());
}

void NotepadqqTest::cleanupTestCase()
{
    EditorNS::Editor::invalidateEditorBuffer();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents();
}

void NotepadqqTest::editorPathIsHtml()
{ QVERIFY(Notepadqq::editorPath().endsWith(".html")); }

// Returns the real editor UI controller owned directly by the window.
static EditorUiController* editorUiController(MainWindow* window)
{
    for (QObject* child : window->children()) {
        if (auto* controller = dynamic_cast<EditorUiController*>(child))
            return controller;
    }
    return nullptr;
}

// Ensures accepted close disconnects container and direct-editor callbacks before deferred deletion.
void NotepadqqTest::acceptedCloseDisconnectsEditorUiController()
{
    auto* window = new MainWindow(QStringList());
    window->setAttribute(Qt::WA_DeleteOnClose, false);

    QPointer<EditorUiController> controller = editorUiController(window);
    QVERIFY(controller);
    EditorNS::Editor* editor = window->currentEditor();
    EditorTabWidget* tabWidget = window->topEditorContainer()->currentTabWidget();
    const int tab = tabWidget->currentIndex();
    QCloseEvent closeEvent;
    QCoreApplication::sendEvent(window, &closeEvent);
    QVERIFY(closeEvent.isAccepted());
    QVERIFY(controller);

    const QString sentinelTitle("closed-window-sentinel");
    window->setWindowTitle(sentinelTitle);
    QVERIFY(QMetaObject::invokeMethod(window->topEditorContainer(),
        "currentEditorChanged",
        Qt::DirectConnection,
        Q_ARG(EditorTabWidget*, tabWidget),
        Q_ARG(int, tab)));
    QCOMPARE(window->windowTitle(), sentinelTitle);

    QVERIFY(QMetaObject::invokeMethod(editor,
        "currentLanguageChanged",
        Qt::DirectConnection,
        Q_ARG(QString, QStringLiteral("plaintext")),
        Q_ARG(QString, QStringLiteral("Plain Text"))));
    QCOMPARE(window->windowTitle(), sentinelTitle);

    delete window;
}

// Ensures direct deletion tears down the controller before MainWindow's QObject destruction phase.
void NotepadqqTest::directWindowDeletionDestroysEditorUiControllerFirst()
{
    auto* window = new MainWindow(QStringList());
    EditorUiController* controller = editorUiController(window);
    QVERIFY(controller);

    QStringList destructionOrder;
    connect(controller, &QObject::destroyed, this, [&destructionOrder] { destructionOrder.append("controller"); });
    connect(window, &QObject::destroyed, this, [&destructionOrder] { destructionOrder.append("window"); });

    delete window;

    QCOMPARE(destructionOrder, QStringList({"controller", "window"}));
}

// Evaluates JavaScript in the real editor with a bounded wait and safe late callbacks.
static QVariant evaluateEditor(QWebEnginePage* page, const QString& script)
{
    auto result = std::make_shared<QVariant>();
    QEventLoop loop;
    QPointer<QEventLoop> guardedLoop(&loop);
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    page->runJavaScript(script, [result, guardedLoop](const QVariant& value) {
        *result = value;
        if (guardedLoop)
            guardedLoop->quit();
    });
    timeout.start(2000);
    loop.exec();
    return *result;
}

// Encodes a QString as a JavaScript string literal without manual escaping.
static QString jsString(const QString& value)
{
    const QByteArray array = QJsonDocument(QJsonArray{value}).toJson(QJsonDocument::Compact);
    return QString::fromUtf8(array.mid(1, array.size() - 2));
}

// Covers observable editing behavior across languages, selections, and undo boundaries.
void NotepadqqTest::toggleComment_data()
{
    QTest::addColumn<QString>("mode");
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("ranges");
    QTest::addColumn<QString>("expected");
    QTest::addColumn<bool>("readOnly");

    QTest::newRow("current-line") << QStringLiteral("javascript") << QStringLiteral("  let x = 1;\nlet y = 2;")
        << QStringLiteral("[[0,4,0,4]]") << QStringLiteral("  // let x = 1;\nlet y = 2;") << false;
    QTest::newRow("exclude-end-column-zero") << QStringLiteral("javascript") << QStringLiteral("one();\ntwo();\nthree();")
        << QStringLiteral("[[0,0,2,0]]") << QStringLiteral("// one();\n// two();\nthree();") << false;
    QTest::newRow("reversed-lines") << QStringLiteral("javascript") << QStringLiteral("one();\ntwo();")
        << QStringLiteral("[[1,6,0,0]]") << QStringLiteral("// one();\n// two();") << false;
    QTest::newRow("partial-block") << QStringLiteral("javascript") << QStringLiteral("let value = other + 1;")
        << QStringLiteral("[[0,12,0,17]]") << QStringLiteral("let value = /*other*/ + 1;") << false;
    QTest::newRow("partial-python-fallback") << QStringLiteral("python") << QStringLiteral("    value = other + 1")
        << QStringLiteral("[[0,12,0,17]]") << QStringLiteral("    # value = other + 1") << false;
    QTest::newRow("xml-block-only") << QStringLiteral("xml") << QStringLiteral("<root/>")
        << QStringLiteral("[[0,2,0,2]]") << QStringLiteral("<!-- <root/> -->") << false;
    QTest::newRow("css-partial") << QStringLiteral("css") << QStringLiteral("a { color: red; }")
        << QStringLiteral("[[0,4,0,15]]") << QStringLiteral("a { /*color: red;*/ }") << false;
    QTest::newRow("multiple-cursors-same-line") << QStringLiteral("javascript") << QStringLiteral("one();\ntwo();")
        << QStringLiteral("[[0,1,0,1],[0,4,0,4],[1,2,1,2]]") << QStringLiteral("// one();\n// two();") << false;
    QTest::newRow("overlapping-line-targets") << QStringLiteral("python") << QStringLiteral("value = other")
        << QStringLiteral("[[0,0,0,3],[0,8,0,13]]") << QStringLiteral("# value = other") << false;
    QTest::newRow("multiple-inline-blocks") << QStringLiteral("javascript") << QStringLiteral("first + second")
        << QStringLiteral("[[0,0,0,5],[0,8,0,14]]") << QStringLiteral("/*first*/ + /*second*/") << false;
    QTest::newRow("inline-and-line-targets") << QStringLiteral("javascript") << QStringLiteral("first + second + third")
        << QStringLiteral("[[0,0,0,5],[0,8,0,14],[0,19,0,19]]") << QStringLiteral("// first + second + third") << false;
    QTest::newRow("multiline-partial-block") << QStringLiteral("javascript") << QStringLiteral("before();\n  first();\n  second();\nafter();")
        << QStringLiteral("[[1,2,2,11]]") << QStringLiteral("before();\n  /*first();\n  second();*/\nafter();") << false;
    QTest::newRow("embedded-javascript") << QStringLiteral("htmlmixed") << QStringLiteral("<script>\none();\n</script>")
        << QStringLiteral("[[1,2,1,2]]") << QStringLiteral("<script>\n// one();\n</script>") << false;
    QTest::newRow("empty-document") << QStringLiteral("javascript") << QStringLiteral("")
        << QStringLiteral("[[0,0,0,0]]") << QStringLiteral("// ") << false;
    QTest::newRow("blank-line") << QStringLiteral("javascript") << QStringLiteral("one();\n\ntwo();")
        << QStringLiteral("[[0,0,2,6]]") << QStringLiteral("// one();\n\n// two();") << false;
    QTest::newRow("unsupported") << QStringLiteral("text/plain") << QStringLiteral("plain text")
        << QStringLiteral("[[0,0,0,10]]") << QStringLiteral("plain text") << false;
    QTest::newRow("read-only") << QStringLiteral("javascript") << QStringLiteral("one();")
        << QStringLiteral("[[0,0,0,6]]") << QStringLiteral("one();") << true;
}

void NotepadqqTest::toggleComment()
{
    QFETCH(QString, mode);
    QFETCH(QString, input);
    QFETCH(QString, ranges);
    QFETCH(QString, expected);
    QFETCH(bool, readOnly);

    auto window = std::make_unique<MainWindow>(QStringList());
    auto* view = window->currentEditor()->findChild<QWebEngineView*>();
    QVERIFY(view);
    auto* page = view->page();
    QTRY_VERIFY_WITH_TIMEOUT(evaluateEditor(page, "typeof editor !== 'undefined' && !!editor").toBool(), 10000);
    auto* action = window->findChild<QAction*>("actionToggle_Comment");
    QVERIFY(action);
    QVERIFY(action->isEnabled());
    QCOMPARE(action->shortcut(), QKeySequence("Ctrl+Shift+C"));

    const QString setup = QString(
        "editor.setOption('mode', %1); editor.setValue(%2);"
        "editor.setSelections(%3.map(function(r) { return {anchor: CodeMirror.Pos(r[0],r[1]),"
        "head: CodeMirror.Pos(r[2],r[3])}; }));"
        "editor.setOption('readOnly', %4); editor.clearHistory();"
        "changeGeneration = editor.changeGeneration(true); forceDirty = false;"
        "window.commentCalls = 0; var actualToggle = toggleSelectedComments;"
        "toggleSelectedComments = function(cm) { actualToggle(cm); ++window.commentCalls; }; true;")
        .arg(jsString(mode), jsString(input), ranges, readOnly ? "true" : "false");
    QVERIFY(evaluateEditor(page, setup).toBool());
    const QString selectedBefore = evaluateEditor(page, "JSON.stringify(editor.getSelections())").toString();

    action->trigger();
    QTRY_COMPARE_WITH_TIMEOUT(evaluateEditor(page, "window.commentCalls").toInt(), 1, 5000);
    QCOMPARE(evaluateEditor(page, "editor.getValue()").toString(), expected);
    if (input == expected) {
        QCOMPARE(evaluateEditor(page, "editor.historySize().undo").toInt(), 0);
        QVERIFY(evaluateEditor(page, "isCleanOrForced(changeGeneration)").toBool());
        return;
    }
    QVERIFY(!evaluateEditor(page, "isCleanOrForced(changeGeneration)").toBool());
    evaluateEditor(page, "editor.undo()");
    QCOMPARE(evaluateEditor(page, "editor.getValue()").toString(), input);
    QCOMPARE(evaluateEditor(page, "JSON.stringify(editor.getSelections())").toString(), selectedBefore);
    evaluateEditor(page, "editor.redo()");
    QCOMPARE(evaluateEditor(page, "editor.getValue()").toString(), expected);

    action->trigger();
    QTRY_COMPARE_WITH_TIMEOUT(evaluateEditor(page, "window.commentCalls").toInt(), 2, 5000);
    QCOMPARE(evaluateEditor(page, "editor.getValue()").toString(), input);
    // Undoing the second toggle must preserve the first toggle as a separate step.
    evaluateEditor(page, "editor.undo()");
    QCOMPARE(evaluateEditor(page, "editor.getValue()").toString(), expected);
}

// Sends the actual key sequence through the focused WebEngine widget to the Qt action.
void NotepadqqTest::toggleCommentShortcut()
{
    auto window = std::make_unique<MainWindow>(QStringList());
    auto* view = window->currentEditor()->findChild<QWebEngineView*>();
    QVERIFY(view);
    window->show();
    window->activateWindow();
    QTRY_VERIFY_WITH_TIMEOUT(evaluateEditor(view->page(), "typeof editor !== 'undefined' && !!editor").toBool(), 10000);
    QVERIFY(evaluateEditor(view->page(),
        "editor.setOption('mode','javascript'); editor.setValue('one();'); editor.setCursor(0,2); true;").toBool());
    view->setFocus();
    QTRY_VERIFY(window->isActiveWindow());
    QWidget* receiver = view->focusProxy() ? view->focusProxy() : view;
    QTest::keyClick(receiver, Qt::Key_C, Qt::ControlModifier | Qt::ShiftModifier);
    QTRY_COMPARE_WITH_TIMEOUT(evaluateEditor(view->page(), "editor.getValue()").toString(), QString("// one();"), 5000);
}

void NotepadqqTest::compactTabsAndColors()
{
    auto& appearance = NqqSettings::getInstance().Appearance;
    appearance.setCompactTabs(true);
    appearance.setShowFullTabTitles(false);
    appearance.setWrapTabs(false);
    appearance.setMaximumTabWidth(160);
    TopEditorContainer container;
    container.resize(900, 400);
    auto* tabs = container.currentTabWidget();
    const QString longName = "a-very-long-document-name-that-should-be-elided.cpp";
    tabs->addEditorTab(false, longName);
    tabs->addEditorTab(false, "short.cpp");
    container.show();
    auto* bar = tabs->tabBar();
    QTRY_VERIFY(bar->tabRect(0).width() <= 160);
    QCOMPARE(bar->tabRect(0).width(), 160);
    const int compactWidth = bar->tabRect(0).width();
    QVERIFY(bar->tabRect(1).width() >= bar->fontMetrics().horizontalAdvance("short.cpp") + 40);
    QCOMPARE(tabs->tabToolTip(0), longName);
    QCOMPARE(tabs->tabText(0), longName);
    QVERIFY(bar->tabButton(0, QTabBar::RightSide) || bar->tabButton(0, QTabBar::LeftSide));

    appearance.setCompactTabs(false);
    tabs->refreshTabAppearance();
    QTRY_VERIFY(bar->tabRect(0).width() > compactWidth);
    appearance.setCompactTabs(true);
    appearance.setMaximumTabWidth(120);
    tabs->refreshTabAppearance();
    QTRY_VERIFY(bar->tabRect(0).width() <= 120);
    QCOMPARE(bar->tabRect(0).width(), 120);

    auto* document = tabs->editor(0);
    document->setTabColor("green");
    QCOMPARE(bar->tabData(0).toString(), QString("green"));
    const QImage rendered = bar->grab().toImage();
    const QPoint band = bar->tabRect(0).topLeft() + QPoint(8, 3);
    QCOMPARE(rendered.pixelColor(band * bar->devicePixelRatioF()), QColor("#48a868"));
    bar->moveTab(0, 1);
    QCOMPARE(tabs->editor(1), document);
    QCOMPARE(bar->tabData(1).toString(), QString("green"));
    auto* other = container.addTabWidget();
    other->transferEditorTab(false, tabs, 1);
    QCOMPARE(other->editor(0), document);
    QCOMPARE(other->tabBar()->tabData(0).toString(), QString("green"));
    document->setTabColor("blue");
    QCOMPARE(other->tabBar()->tabData(0).toString(), QString("blue"));
    QCOMPARE(bar->tabData(0).toString(), QString());
    document->setTabColor("invalid");
    QCOMPARE(document->tabColor(), QString());
    QCOMPARE(other->tabBar()->tabData(0).toString(), QString());
    appearance.setMaximumTabWidth(160);
}

void NotepadqqTest::wrappingTabs()
{
    auto& appearance = NqqSettings::getInstance().Appearance;
    appearance.setShowFullTabTitles(true);
    appearance.setWrapTabs(true);
    appearance.setCompactTabs(true);
    TopEditorContainer container;
    container.resize(420, 500);
    auto* tabs = container.currentTabWidget();
    for (int i = 0; i < 8; ++i)
        tabs->addEditorTab(false, "Document " + QString::number(i));
    for (int i = 0; i < 8; ++i)
        tabs->editor(i)->setTabColor(EditorNS::Editor::tabColorIds()[i % 6]);
    container.show();
    QTRY_VERIFY(tabs->visualTabRect(7).top() > tabs->visualTabRect(0).top());
    for (int i = 0; i < tabs->count(); ++i) {
        const QRect rect = tabs->visualTabRect(i);
        QVERIFY(rect.width() >= tabs->fontMetrics().horizontalAdvance(tabs->tabText(i)) + 40);
        QCOMPARE(tabs->tabAtPosition(rect.center()), i);
    }
    auto rows = tabs->tabBar()->findChildren<QTabBar*>(QString(), Qt::FindDirectChildrenOnly);
    QTabBar* last = nullptr;
    for (auto* row : rows)
        if (row->isVisible() && (!last || row->y() > last->y())) last = row;
    QVERIFY(last);
    QTest::mouseClick(last, Qt::LeftButton, Qt::NoModifier, last->tabRect(last->count()-1).center());
    QCOMPARE(tabs->currentIndex(), 7);
    QTest::keyClick(last, Qt::Key_Right);
    QCOMPARE(tabs->currentIndex(), 0);
    QTest::keyClick(last, Qt::Key_Left);
    QCOMPARE(tabs->currentIndex(), 7);
    QSignalSpy context(&container, &TopEditorContainer::customTabContextMenuRequested);
    QTest::mouseClick(last, Qt::RightButton, Qt::NoModifier, last->tabRect(last->count()-1).center());
    // Some offscreen platforms synthesize context-menu events separately.
    QContextMenuEvent menuEvent(QContextMenuEvent::Mouse, last->tabRect(last->count()-1).center());
    QApplication::sendEvent(last, &menuEvent);
    QVERIFY(!context.isEmpty());
    QCOMPARE(context.last().at(2).toInt(), 7);
    QVERIFY(tabs->grab(QRect(0, 0, tabs->width(), tabs->tabBar()->height()))
        .save(QCoreApplication::applicationDirPath() + "/tab-rows-preview.png"));
    QSignalSpy close(tabs, &QTabWidget::tabCloseRequested);
    QTest::mouseClick(last, Qt::MiddleButton, Qt::NoModifier, last->tabRect(last->count()-1).center());
    QCOMPARE(close.count(), 1);
    QCOMPARE(close.takeFirst().first().toInt(), 7);
    QWidget* button = last->tabButton(last->count()-1, QTabBar::RightSide);
    if (!button) button = last->tabButton(last->count()-1, QTabBar::LeftSide);
    QVERIFY(button);
    QTest::mouseClick(button, Qt::LeftButton);
    QCOMPARE(close.count(), 1);
    QCOMPARE(close.takeFirst().first().toInt(), 7);
    // Drop the first tab on a tab in another row; the flat document order follows.
    auto* moved = tabs->editor(0);
    const QPoint source = tabs->visualTabRect(0).center();
    const QPoint target = tabs->visualTabRect(5).center();
    QTabBar* first = nullptr;
    for (auto* row : rows)
        if (row->isVisible() && row->y() == 0) first = row;
    QVERIFY(first);
    QTest::mousePress(first, Qt::LeftButton, Qt::NoModifier, first->mapFrom(tabs, source));
    QTest::mouseMove(first, first->mapFrom(tabs, target));
    QTest::mouseRelease(first, Qt::LeftButton, Qt::NoModifier, first->mapFrom(tabs, target));
    QCOMPARE(tabs->editor(5), moved);
    tabs->closeEditorTab(3);
    QTRY_VERIFY(!tabs->visualTabRect(6).isEmpty());
    QTRY_COMPARE(tabs->tabAtPosition(tabs->visualTabRect(6).center()), 6);
    container.resize(1600, 500);
    QTRY_COMPARE(tabs->visualTabRect(6).top(), tabs->visualTabRect(0).top());
    appearance.setWrapTabs(false);
    tabs->refreshTabAppearance();
    QTRY_VERIFY(tabs->tabBar()->usesScrollButtons());
    appearance.setShowFullTabTitles(false);
}

void NotepadqqTest::tabInsertionPreview_data()
{
    QTest::addColumn<bool>("wrap");
    QTest::addColumn<bool>("rtl");
    QTest::addColumn<int>("from");
    QTest::addColumn<int>("target");
    QTest::addColumn<int>("side"); // -1 before, 1 after, 2 end, 3 outside, 4 Escape
    QTest::addColumn<int>("expected");
    QTest::newRow("rows-before") << true << false << 0 << 4 << -1 << 3;
    QTest::newRow("rows-after") << true << false << 0 << 4 << 1 << 4;
    QTest::newRow("rows-backwards") << true << false << 7 << 1 << -1 << 1;
    QTest::newRow("rows-end") << true << false << 0 << 7 << 2 << 7;
    QTest::newRow("rows-no-op") << true << false << 2 << 2 << -1 << 2;
    QTest::newRow("rows-outside") << true << false << 0 << 4 << 3 << 0;
    QTest::newRow("rows-cancel") << true << false << 0 << 4 << 4 << 0;
    QTest::newRow("single-before") << false << false << 0 << 4 << -1 << 3;
    QTest::newRow("single-after") << false << false << 0 << 4 << 1 << 4;
    QTest::newRow("single-backwards") << false << false << 7 << 1 << 1 << 2;
    QTest::newRow("single-cancel") << false << false << 0 << 4 << 4 << 0;
    QTest::newRow("rows-rtl") << true << true << 0 << 4 << 1 << 4;
}

void NotepadqqTest::tabInsertionPreview()
{
    QFETCH(bool, wrap);
    QFETCH(bool, rtl);
    QFETCH(int, from);
    QFETCH(int, target);
    QFETCH(int, side);
    QFETCH(int, expected);
    auto& appearance = NqqSettings::getInstance().Appearance;
    appearance.setShowFullTabTitles(true);
    appearance.setWrapTabs(wrap);
    TopEditorContainer container;
    container.setLayoutDirection(rtl ? Qt::RightToLeft : Qt::LeftToRight);
    container.resize(wrap ? 420 : 1400, 500);
    auto* tabs = container.currentTabWidget();
    for (int i = 0; i < 8; ++i)
        tabs->addEditorTab(false, "Document " + QString::number(i));
    container.show();
    QTRY_VERIFY(!tabs->visualTabRect(target).isEmpty());
    auto* marker = tabs->findChild<QWidget*>("tabInsertionMarker");
    QVERIFY(marker);
    QVERIFY(!marker->isVisible());
    const QPoint origin = tabs->visualTabRect(from).center();
    const QRect destination = tabs->visualTabRect(target);
    QPoint point = destination.center();
    const bool before = side == -1;
    point.setX((before != rtl) ? destination.left() + destination.width()/4 :
                                destination.right() - destination.width()/4);
    if (side == 2)
        point.setX(rtl ? 4 : tabs->width() - 4);
    if (side == 3)
        point.setX(tabs->width() + 20);
    QTabBar* receiver = tabs->tabBar();
    if (wrap) {
        for (auto* row : tabs->tabBar()->findChildren<QTabBar*>(QString(), Qt::FindDirectChildrenOnly))
            if (row->isVisible() && row->geometry().contains(origin - tabs->tabBar()->pos())) receiver = row;
    }
    auto* document = tabs->editor(from);
    QTest::mousePress(receiver, Qt::LeftButton, Qt::NoModifier, receiver->mapFrom(tabs, origin));
    QVERIFY(!marker->isVisible());
    const QPoint local = receiver->mapFrom(tabs, point);
    QMouseEvent move(QEvent::MouseMove, QPointF(local), QPointF(receiver->mapToGlobal(local)),
        Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(receiver, &move);
    QCOMPARE(tabs->editor(from), document); // Preview never changes document order.
    if (side == 3) {
        QVERIFY(!marker->isVisible());
    } else {
        QVERIFY(marker->isVisible());
        QCOMPARE(marker->width(), 4);
        const QImage painted = marker->grab().toImage();
        QCOMPARE(painted.pixelColor(1, 1), QApplication::palette().highlight().color());
        if (side == -1 || side == 1 || side == 4) {
            const bool visualBefore = (side == -1) != rtl;
            const int boundary = visualBefore ? destination.left() : destination.right() + 1;
            QVERIFY(qAbs(marker->mapTo(tabs, QPoint(2, 0)).x() - boundary) <= 2);
        }
        if (wrap && !rtl && side == 1 && from == 0) {
            const QPixmap preview = tabs->grab(QRect(0, 0, tabs->width(), tabs->tabBar()->height()));
            const QPoint pixel = marker->mapTo(tabs, QPoint(2, 2));
            QCOMPARE(preview.toImage().pixelColor(pixel * preview.devicePixelRatio()), QApplication::palette().highlight().color());
            QVERIFY(preview.save(QCoreApplication::applicationDirPath() + "/tab-drag-preview.png"));
        }
    }
    if (side == 4) {
        QTest::keyClick(tabs->editor(from), Qt::Key_Escape);
        QVERIFY(!marker->isVisible());
    }
    QTest::mouseRelease(receiver, Qt::LeftButton, Qt::NoModifier, local);
    QVERIFY(!marker->isVisible());
    QCOMPARE(tabs->editor(expected), document);
    QCOMPARE(tabs->count(), 8);
    appearance.setWrapTabs(false);
    appearance.setShowFullTabTitles(false);
}

void NotepadqqTest::lineDifferences_data()
{
    QTest::addColumn<QString>("left");
    QTest::addColumn<QString>("right");
    QTest::addColumn<QVector<int>>("removed");
    QTest::addColumn<QVector<int>>("added");
    QTest::newRow("equal") << QString("a\nb\n") << QString("a\nb\n") << QVector<int>{} << QVector<int>{};
    QTest::newRow("insert") << QString("a\nb") << QString("a\nx\nb") << QVector<int>{} << QVector<int>{1};
    QTest::newRow("delete") << QString("a\nx\nb") << QString("a\nb") << QVector<int>{1} << QVector<int>{};
    QTest::newRow("replace") << QString("a\nx\nb") << QString("a\ny\nb") << QVector<int>{1} << QVector<int>{1};
    QTest::newRow("duplicates") << QString("x\na\nx") << QString("x\nx") << QVector<int>{1} << QVector<int>{};
    QTest::newRow("empty") << QString() << QString() << QVector<int>{} << QVector<int>{};
    QTest::newRow("empty-left") << QString() << QString("x") << QVector<int>{} << QVector<int>{0};
    QTest::newRow("empty-right") << QString("x") << QString() << QVector<int>{0} << QVector<int>{};
    QTest::newRow("unicode") << QString("猫\nα") << QString("猫\nβ") << QVector<int>{1} << QVector<int>{1};
    QTest::newRow("eol") << QString("a\r\nb\r\n") << QString("a\nb\n") << QVector<int>{} << QVector<int>{};
    QTest::newRow("final-newline") << QString("a") << QString("a\n") << QVector<int>{} << QVector<int>{1};
    QTest::newRow("blank-lines") << QString("a\n\nb") << QString("a\nb") << QVector<int>{1} << QVector<int>{};
}

void NotepadqqTest::lineDifferences()
{
    QFETCH(QString, left); QFETCH(QString, right);
    QFETCH(QVector<int>, removed); QFETCH(QVector<int>, added);
    std::atomic_bool canceled(false);
    const auto result = LineDiff::compare(left, right, canceled);
    QVERIFY(!result.limited);
    QCOMPARE(result.removed, removed);
    QCOMPARE(result.added, added);
}

void NotepadqqTest::comparisonUi()
{
    const auto oldToolbar = NqqSettings::getInstance().MainWindow.getToolBarItems();
    const bool oldMigrated = NqqSettings::getInstance().MainWindow.getComparisonToolbarAdded();
    auto restore = qScopeGuard([&] {
        NqqSettings::getInstance().MainWindow.setToolBarItems(oldToolbar);
        NqqSettings::getInstance().MainWindow.setComparisonToolbarAdded(oldMigrated);
    });
    NqqSettings::getInstance().MainWindow.setToolBarItems("actionNew|actionSave");
    NqqSettings::getInstance().MainWindow.setComparisonToolbarAdded(false);
    MainWindow window(QStringList{});
    window.resize(1000, 600);
    window.show();
    auto* container = window.topEditorContainer();
    auto* pane = container->currentTabWidget();
    auto* left = window.currentEditor();
    pane->setTabText(left, "same title");
    pane->addEditorTab(false, "same title");
    auto* right = pane->editor(1);
    auto* lp = left->findChild<QWebEngineView*>()->page();
    auto* rp = right->findChild<QWebEngineView*>()->page();
    QTRY_VERIFY_WITH_TIMEOUT(evaluateEditor(rp, "typeof editor !== 'undefined' && !!editor").toBool(), 10000);
    left->setValue("same\nold\ntail").wait();
    right->setValue("same\nnew\ntail").wait();
    left->markClean().wait();
    evaluateEditor(lp, "editor.replaceRange('!', {line:0,ch:4}); editor.setCursor(1,1)");
    const QString history = evaluateEditor(lp, "JSON.stringify(editor.getHistory())").toString();
    const auto cursor = evaluateEditor(lp, "JSON.stringify(editor.getCursor())");
    const auto clean = evaluateEditor(lp, "isCleanOrForced(changeGeneration)");
    evaluateEditor(rp, "editor.setOption('readOnly', 'nocursor')");
    auto* compare = window.findChild<QAction*>("actionCompare_Documents");
    auto* end = window.findChild<QAction*>("actionEnd_Comparison");
    auto* controller = container->findChild<ComparisonController*>();
    QVERIFY(compare && end && controller);
    QVERIFY(compare->isEnabled());
    QVERIFY(window.getToolBar()->actions().contains(compare));
    QVERIFY(!compare->icon().isNull());
    // Invoke the same action used by the toolbar and accept its real picker.
    QTimer::singleShot(0, &window, [&] {
        auto* dialog = window.findChild<QDialog*>("comparisonDialog");
        QVERIFY(dialog);
        auto* choices = dialog->findChild<QComboBox*>("comparisonDocuments");
        QVERIFY(choices);
        QCOMPARE(choices->count(), 1);
        QVERIFY(choices->itemText(0).contains("same title"));
        dialog->accept();
    });
    compare->trigger();
    QVERIFY(controller->active());
    QVERIFY(end->isEnabled());
    QCOMPARE(container->tabWidget(0)->currentEditor(), left);
    QCOMPARE(container->tabWidget(1)->currentEditor(), right);
    QTRY_VERIFY_WITH_TIMEOUT(evaluateEditor(lp, "diffLines.length > 0 && diffLines[0].style === 'nqq-diff-removed'").toBool(), 5000);
    QTRY_VERIFY_WITH_TIMEOUT(evaluateEditor(rp, "diffLines.length > 0 && diffLines[0].style === 'nqq-diff-added'").toBool(), 5000);
    QCOMPARE(evaluateEditor(lp, "editor.getOption('readOnly')").toBool(), true);
    QTRY_VERIFY(evaluateEditor(lp, "!!document.querySelector('.nqq-diff-removed')").toBool());
    QCOMPARE(evaluateEditor(lp, "getComputedStyle(document.querySelector('.nqq-diff-removed')).backgroundColor").toString(),
        QString("rgba(230, 65, 65, 0.28)"));
    QCOMPARE(evaluateEditor(rp, "editor.lineInfo(1).bgClass").toString(), QString("nqq-diff-added"));
    QTest::qWait(250); // WebEngine must submit the frame after decoration updates.
    QVERIFY(window.grab().save(QCoreApplication::applicationDirPath() + "/comparison-preview.png"));
    const auto value = left->value();
    // Menu edits, history commands and direct edits must all preserve history.
    left->sendMessage("C_CMD_UNDO");
    left->sendMessage("C_CMD_REDO");
    left->sendMessage("C_CMD_DELETE_LINE");
    left->sendMessage("C_CMD_SET_VALUE", "overwrite");
    QCOMPARE(evaluateEditor(lp, "UiDriver.messageReceived('C_FUN_REPLACE_ALL', ['old', 'g', 'new', 0])").toInt(), 0);
    evaluateEditor(lp, "editor.replaceRange('blocked', {line:0,ch:0}); editor.undo(); editor.clearHistory()");
    QCOMPARE(left->value(), value);
    QCOMPARE(evaluateEditor(lp, "JSON.stringify(editor.getHistory())").toString(), history);
    QCOMPARE(evaluateEditor(lp, "JSON.stringify(editor.getCursor())"), cursor);
    QCOMPARE(evaluateEditor(lp, "isCleanOrForced(changeGeneration)"), clean);
    // Revision validation rejects old results, without altering live marks.
    evaluateEditor(lp, "UiDriver.messageReceived('C_CMD_DIFF_MARKS', {revision:diffRevision-1, side:'added', lines:[0]})");
    QCOMPARE(evaluateEditor(lp, "diffLines[0].style").toString(), QString("nqq-diff-removed"));
    QVERIFY(compare->isChecked());
    QCOMPARE(compare->text(), QString("End Comparison"));
    auto* button = qobject_cast<QToolButton*>(window.getToolBar()->widgetForAction(compare));
    QVERIFY(button);
    QTest::mouseClick(button, Qt::LeftButton);
    QVERIFY(!controller->active());
    QVERIFY(!compare->isChecked());
    QCOMPARE(compare->text(), QString("Compare with..."));
    QCOMPARE(container->count(), 1);
    QCOMPARE(container->currentTabWidget(), pane);
    QCOMPARE(pane->editor(0), left);
    QCOMPARE(pane->editor(1), right);
    QTRY_COMPARE(evaluateEditor(lp, "diffLines.length").toInt(), 0);
    QTRY_COMPARE(evaluateEditor(lp, "editor.getOption('readOnly')").toBool(), false);
    QTRY_COMPARE(evaluateEditor(rp, "editor.getOption('readOnly')").toString(), QString("nocursor"));
    QCOMPARE(evaluateEditor(lp, "JSON.stringify(editor.getHistory())").toString(), history);
    // A restarted comparison is tied to document identity, not tab indices.
    QVERIFY(controller->start(left, right));
    container->tabWidget(1)->closeEditorTab(container->tabWidget(1)->indexOf(right));
    QTRY_VERIFY(!controller->active());
    QTRY_COMPARE(evaluateEditor(lp, "editor.getOption('readOnly')").toBool(), false);
    // Once migrated, user customization may remove the toolbar entry.
    NqqSettings::getInstance().MainWindow.setToolBarItems("actionNew|actionSave");
    window.loadToolBar();
    QVERIFY(!window.getToolBar()->actions().contains(compare));
}

void NotepadqqTest::comparisonCancellation()
{
    std::atomic_bool canceled(false);
    QVERIFY(LineDiff::compare(QString(5 * 1024 * 1024, 'x'), "x", canceled).limited);
    QVERIFY(LineDiff::compare(QString("a\n").repeated(2500), QString("b\n").repeated(2500), canceled).limited);
    const auto local = LineDiff::compare(QString("a\n").repeated(10000) + "old", QString("a\n").repeated(10000) + "new", canceled);
    QVERIFY(!local.limited);
    QCOMPARE(local.removed, QVector<int>{10000});
    canceled.store(true);
    const auto stopped = LineDiff::compare("a\nb", "c\nd", canceled);
    QVERIFY(stopped.removed.isEmpty() && stopped.added.isEmpty());
    TopEditorContainer container;
    auto* pane = container.currentTabWidget();
    pane->addEditorTab(false, "left"); pane->addEditorTab(false, "right");
    auto* left = pane->editor(0); auto* right = pane->editor(1);
    auto* lp = left->findChild<QWebEngineView*>()->page();
    auto* rp = right->findChild<QWebEngineView*>()->page();
    QTRY_VERIFY_WITH_TIMEOUT(evaluateEditor(rp, "typeof editor !== 'undefined' && !!editor").toBool(), 10000);
    left->setValue("a").wait(); right->setValue("b").wait();
    ComparisonController controller(&container);
    QSignalSpy updated(&controller, &ComparisonController::updated);
    QVERIFY(controller.start(left, right));
    controller.stop();
    QTest::qWait(300);
    QCOMPARE(updated.count(), 0);
    QTRY_COMPARE(evaluateEditor(lp, "diffLines.length").toInt(), 0);
    QTRY_COMPARE(evaluateEditor(lp, "editor.getOption('readOnly')").toBool(), false);
    QSignalSpy started(&controller, &ComparisonController::calculationStarted);
    const auto cancelConnection = connect(&controller, &ComparisonController::calculationStarted,
        &controller, &ComparisonController::stop);
    QVERIFY(controller.start(left, right));
    QTRY_COMPARE_WITH_TIMEOUT(started.count(), 1, 5000);
    QVERIFY(!controller.active());
    QTest::qWait(100);
    QCOMPARE(updated.count(), 0); // A worker already running cannot repaint after exit.
    disconnect(cancelConnection);
    QVERIFY(controller.start(left, right));
    QTRY_VERIFY_WITH_TIMEOUT(updated.count() > 0, 5000);
    // Moving both documents into one pane terminates the comparison safely.
    pane->transferEditorTab(false, container.tabWidget(1), 0);
    QTRY_VERIFY(!controller.active());
    QTRY_COMPARE(evaluateEditor(rp, "editor.getOption('readOnly')").toBool(), false);
    auto ephemeral = std::make_unique<ComparisonController>(&container);
    QVERIFY(ephemeral->start(left, right));
    QSignalSpy running(ephemeral.get(), &ComparisonController::calculationStarted);
    QTRY_VERIFY_WITH_TIMEOUT(running.count() > 0, 5000);
    ephemeral.reset();
    QTRY_COMPARE(evaluateEditor(lp, "diffLines.length").toInt(), 0);
    QTRY_COMPARE(evaluateEditor(lp, "editor.getOption('readOnly')").toBool(), false);
    QTRY_COMPARE(evaluateEditor(rp, "editor.getOption('readOnly')").toBool(), false);
    QSignalSpy status(&controller, &ComparisonController::statusChanged);
    left->setValue(QString("a\n").repeated(2500)).wait();
    right->setValue(QString("b\n").repeated(2500)).wait();
    QVERIFY(controller.start(left, right));
    QTRY_VERIFY_WITH_TIMEOUT(!controller.active(), 5000);
    QVERIFY(status.last().at(0).toString().contains("limit"));
    QTRY_COMPARE(evaluateEditor(lp, "editor.getOption('readOnly')").toBool(), false);
    QTRY_COMPARE(evaluateEditor(rp, "editor.getOption('readOnly')").toBool(), false);
    left->setValue("equal").wait(); right->setValue("equal").wait();
    QVERIFY(controller.start(left, right));
    QTRY_COMPARE_WITH_TIMEOUT(status.last().at(0).toString(), QString("Documents are identical."), 5000);
    QVERIFY(controller.active());
    QCOMPARE(evaluateEditor(lp, "diffLines.length").toInt(), 0);
    QCOMPARE(evaluateEditor(rp, "diffLines.length").toInt(), 0);
}

void NotepadqqTest::comparisonRestoresViews_data()
{
    QTest::addColumn<int>("views");
    QTest::addColumn<int>("leftView");
    QTest::addColumn<int>("rightView");
    QTest::newRow("single-view") << 1 << 0 << 0;
    QTest::newRow("both-in-left") << 2 << 0 << 0;
    QTest::newRow("both-in-right") << 2 << 1 << 1;
    QTest::newRow("already-side-by-side") << 2 << 0 << 1;
    QTest::newRow("swapped-views") << 2 << 1 << 0;
    QTest::newRow("third-view") << 3 << 2 << 2;
    QTest::newRow("third-and-first") << 3 << 2 << 0;
}

void NotepadqqTest::comparisonRestoresViews()
{
    QFETCH(int, views); QFETCH(int, leftView); QFETCH(int, rightView);
    TopEditorContainer container;
    QList<EditorTabWidget*> panes;
    QList<QList<Editor*>> original;
    for (int view = 0; view < views; ++view) {
        auto* pane = view ? container.addTabWidget() : container.tabWidget(0);
        panes.append(pane);
        QList<Editor*> editors;
        for (int tab = 0; tab < 4; ++tab) {
            pane->addEditorTab(false, QString("View %1 tab %2").arg(view).arg(tab));
            editors.append(pane->editor(tab));
        }
        original.append(editors);
    }
    auto* left = original[leftView][3];
    auto* right = original[rightView][1];
    left->setTabColor("blue");
    right->setUnsavedTitle("Comparison title");
    ComparisonController controller(&container);
    QVERIFY(controller.start(left, right));
    QCOMPARE(container.tabWidget(0)->currentEditor(), left);
    QCOMPARE(container.tabWidget(1)->currentEditor(), right);
    controller.stop();
    QCOMPARE(container.count(), views);
    QCOMPARE(container.currentTabWidget(), panes[leftView]);
    QCOMPARE(container.currentTabWidget()->currentEditor(), left);
    for (int view = 0; view < views; ++view) {
        QCOMPARE(container.tabWidget(view), panes[view]);
        QCOMPARE(panes[view]->count(), 4);
        for (int tab = 0; tab < 4; ++tab) QCOMPARE(panes[view]->editor(tab), original[view][tab]);
    }
    QCOMPARE(left->tabColor(), QString("blue"));
    QCOMPARE(right->unsavedTitle(), QString("Comparison title"));
    // Stop is idempotent; beginning again captures restored origins afresh.
    controller.stop();
    QVERIFY(controller.start(left, right));
    controller.stop();
    QCOMPARE(container.count(), views);
    QCOMPARE(panes[leftView]->indexOf(left), 3);
    QCOMPARE(panes[rightView]->indexOf(right), 1);
}

void NotepadqqTest::comparisonRestoresMissingView()
{
    TopEditorContainer container;
    auto* origin = container.addTabWidget();
    origin->addEditorTab(false, "left"); origin->addEditorTab(false, "right");
    auto* left = origin->editor(0); auto* right = origin->editor(1);
    ComparisonController controller(&container);
    QVERIFY(controller.start(left, right));
    // Simulate closing an emptied original pane during a manual rearrangement.
    container.tabWidget(0)->transferEditorTab(false, origin, origin->indexOf(right));
    QPointer<EditorTabWidget> oldOrigin(origin);
    delete origin;
    QVERIFY(!oldOrigin);
    controller.stop();
    QCOMPARE(container.count(), 2);
    QCOMPARE(container.tabWidget(1)->editor(0), left);
    QCOMPARE(container.tabWidget(1)->editor(1), right);
    QCOMPARE(container.currentTabWidget(), container.tabWidget(1));
    // A document moved into another window is never stolen back on exit.
    TopEditorContainer other;
    QVERIFY(controller.start(left, right));
    auto* rightPane = container.tabWidgetFromEditor(right);
    other.tabWidget(0)->transferEditorTab(false, rightPane, rightPane->indexOf(right));
    controller.stop();
    QCOMPARE(other.tabWidget(0)->editor(0), right);
    QCOMPARE(container.tabWidget(1)->editor(0), left);
    QCOMPARE(container.currentTabWidget(), container.tabWidget(1));
    // Parent destruction must not restore tabs into an already-destroyed splitter.
    auto* dying = new TopEditorContainer;
    auto* dyingPane = dying->tabWidget(0);
    dyingPane->addEditorTab(false, "left"); dyingPane->addEditorTab(false, "right");
    QPointer<ComparisonController> child = new ComparisonController(dying);
    QVERIFY(child->start(dyingPane->editor(0), dyingPane->editor(1)));
    delete dying;
    QVERIFY(!child);
}

void NotepadqqTest::forkTranslations_data()
{
    QTest::addColumn<QString>("locale");
    for (const QString& locale : {"de", "es", "fr", "hu", "it", "ja", "nl", "pl", "pt", "ru", "sl", "sv", "uk", "zh"})
        QTest::newRow(qPrintable(locale)) << locale;
}

void NotepadqqTest::forkTranslations()
{
    QFETCH(QString, locale);
    QTranslator translator;
    QVERIFY(translator.load(QCoreApplication::applicationDirPath() + "/notepadqq_" + locale + ".qm"));
    for (const char* source : {"Toggle &Comment", "Comment or uncomment the current line or selected code",
        "Tab Color", "Default", "Red", "Orange", "Yellow", "Green", "Blue", "Purple",
        "Set Tab Title...", "Set Tab Title", "Title:",
        "Compare with...", "Compare two open documents side by side", "End Comparison",
        "Remove differences and restore editing", "Compare Documents", "Reference (left): %1",
        "Compared document (right):", "%1 (view %2, tab %3)"})
        QVERIFY2(!translator.translate("MainWindow", source).isEmpty(), source);
    for (const char* source : {"Compact tabs", "Maximum compact tab width:",
        "Always show full tab titles", "Wrap tabs into multiple rows", "Show line numbers", " px"})
        QVERIFY2(!translator.translate("frmPreferences", source).isEmpty(), source);
    for (const char* source : {"Comparison exceeds the size or work limit.", "Documents are identical."})
        QVERIFY2(!translator.translate("ComparisonController", source).isEmpty(), source);
    if (locale == "es") {
        QCoreApplication::installTranslator(&translator);
        QCOMPARE(QCoreApplication::translate("MainWindow", "Tab Color"), QString("Color de la pestaña"));
        QCoreApplication::removeTranslator(&translator);
    }
}

void NotepadqqTest::removesLegacyTelemetrySettings()
{
    QSettings settings;
    settings.setValue("NotepadqqVersion", "2.2.0");
    settings.setValue("CollectStatistics", true);
    settings.setValue("LastStatisticTransmissionTime", 123456);
    settings.setValue("StatisticsDialogShown", 1);
    settings.setValue("Appearance/MaximumTabWidth", 160);
    settings.sync();
    NqqSettings::ensureBackwardsCompatibility();
    settings.sync();
    QVERIFY(!settings.contains("CollectStatistics"));
    QVERIFY(!settings.contains("LastStatisticTransmissionTime"));
    QVERIFY(!settings.contains("StatisticsDialogShown"));
    QCOMPARE(settings.value("Appearance/MaximumTabWidth").toInt(), 160);
    QCOMPARE(settings.value("NotepadqqVersion").toString(), QString("2.2.0"));
}

void NotepadqqTest::tabColorSession_data()
{
    QTest::addColumn<bool>("savedFile");
    QTest::addColumn<bool>("modified");
    QTest::newRow("clean-file-session") << true << false;
    QTest::newRow("modified-file-backup") << true << true;
    QTest::newRow("unsaved-document-backup") << false << true;
}

void NotepadqqTest::tabColorSession()
{
    QFETCH(bool, savedFile);
    QFETCH(bool, modified);
    QTemporaryDir files;
    QVERIFY(files.isValid());
    const QString filePath = files.filePath("document.txt");
    const QString sessionPath = files.filePath("session.xml");
    const QString content = modified ? "unsaved changes\n" : "saved text\n";
    if (savedFile) {
        QFile file(filePath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("saved text\n");
    }

    {
        TopEditorContainer source;
        DocEngine engine(&source);
        auto* tabs = source.currentTabWidget();
        tabs->addEditorTab(false, "document.txt");
        auto* editor = tabs->editor(0);
        auto* view = editor->findChild<QWebEngineView*>();
        QVERIFY(view);
        QTRY_VERIFY_WITH_TIMEOUT(evaluateEditor(view->page(), "typeof editor !== 'undefined' && !!editor").toBool(), 10000);
        editor->setValue(content).wait();
        if (!modified)
            editor->markClean().wait();
        if (savedFile)
            editor->setFilePath(QUrl::fromLocalFile(filePath));
        const bool cleanBefore = editor->isClean();
        editor->setTabColor("purple");
        QCOMPARE(editor->isClean(), cleanBefore);
        QVERIFY(Sessions::saveSession(&engine, &source, sessionPath,
            modified ? files.filePath("backup") : QString()));
    }

    TopEditorContainer restored;
    DocEngine engine(&restored);
    Sessions::loadSession(&engine, &restored, sessionPath);
    auto* tabs = restored.currentTabWidget();
    QCOMPARE(tabs->count(), 1);
    auto* editor = tabs->editor(0);
    QCOMPARE(editor->tabColor(), QString("purple"));
    QCOMPARE(tabs->tabBar()->tabData(0).toString(), QString("purple"));
    QTRY_COMPARE_WITH_TIMEOUT(editor->value(), content, 10000);
    QTRY_COMPARE(editor->isClean(), !modified);
}

void NotepadqqTest::legacyTabColorSession_data()
{
    QTest::addColumn<QString>("attribute");
    QTest::newRow("old-format") << QString();
    QTest::newRow("unknown-color") << QString(" tabColor=\"not-a-color\"");
}

void NotepadqqTest::legacyTabColorSession()
{
    QFETCH(QString, attribute);
    QTemporaryDir files;
    QFile document(files.filePath("document.txt"));
    QVERIFY(document.open(QIODevice::WriteOnly));
    document.write("legacy\n");
    document.close();
    QFile session(files.filePath("session.xml"));
    QVERIFY(session.open(QIODevice::WriteOnly));
    session.write(QString("<Notepadqq><View><Tab filePath=\"%1\"%2/></View></Notepadqq>")
                      .arg(document.fileName().toHtmlEscaped(), attribute).toUtf8());
    session.close();
    TopEditorContainer restored;
    DocEngine engine(&restored);
    Sessions::loadSession(&engine, &restored, session.fileName());
    auto* tabs = restored.currentTabWidget();
    QCOMPARE(tabs->count(), 1);
    QCOMPARE(tabs->editor(0)->tabColor(), QString());
    QTRY_COMPARE_WITH_TIMEOUT(tabs->editor(0)->value(), QString("legacy\n"), 10000);
}

void NotepadqqTest::tabAppearancePreferences()
{
    auto window = std::make_unique<MainWindow>(QStringList());
    auto* view = window->currentEditor()->findChild<QWebEngineView*>();
    QVERIFY(view);
    QTRY_VERIFY_WITH_TIMEOUT(evaluateEditor(view->page(), "typeof editor !== 'undefined' && !!editor").toBool(), 10000);
    auto* tabs = window->topEditorContainer()->currentTabWidget();
    tabs->setTabText(0, "a-very-long-name-for-the-preferences-test.txt");
    window->resize(900, 400);
    window->show();
    frmPreferences preferences(window->topEditorContainer(), window.get());
    QVERIFY(!preferences.findChild<QCheckBox*>("chkCollectStatistics"));
    auto* full = preferences.findChild<QCheckBox*>("chkShowFullTabTitles");
    auto* wrap = preferences.findChild<QCheckBox*>("chkWrapTabs");
    QVERIFY(full && wrap);
    full->setChecked(false);
    wrap->setChecked(false);
    auto* compact = preferences.findChild<QCheckBox*>("chkCompactTabs");
    auto* width = preferences.findChild<QSpinBox*>("spnMaximumTabWidth");
    auto* buttons = preferences.findChild<QDialogButtonBox*>("buttonBox");
    QVERIFY(compact && width && buttons);
    compact->setChecked(false);
    QVERIFY(!width->isEnabled());
    buttons->button(QDialogButtonBox::Apply)->click();
    QVERIFY(!NqqSettings::getInstance().Appearance.getCompactTabs());
    QTRY_VERIFY(tabs->tabBar()->tabRect(0).width() > 160);
    compact->setChecked(true);
    QVERIFY(width->isEnabled());
    width->setValue(120);
    buttons->button(QDialogButtonBox::Apply)->click();
    QCOMPARE(NqqSettings::getInstance().Appearance.getMaximumTabWidth(), 120);
    QTRY_VERIFY(tabs->tabBar()->tabRect(0).width() <= 120);
    full->setChecked(true);
    QVERIFY(!width->isEnabled());
    buttons->button(QDialogButtonBox::Apply)->click();
    QTRY_VERIFY(tabs->tabBar()->tabRect(0).width() > 120);
    full->setChecked(false);
    width->setValue(160);
    buttons->button(QDialogButtonBox::Apply)->click();
}

void NotepadqqTest::appearancePreferencesAlignment()
{
    class RightAlignedFormsStyle : public QProxyStyle {
    public:
        RightAlignedFormsStyle() : QProxyStyle(QStyleFactory::create("Fusion")) {}
        int styleHint(StyleHint hint, const QStyleOption* option = nullptr,
                      const QWidget* widget = nullptr, QStyleHintReturn* result = nullptr) const override
        {
            if (hint == SH_FormLayoutLabelAlignment)
                return Qt::AlignRight | Qt::AlignVCenter;
            return QProxyStyle::styleHint(hint, option, widget, result);
        }
    };

    QTranslator translator;
    QVERIFY(translator.load(QCoreApplication::applicationDirPath() + "/notepadqq_es.qm"));
    QCoreApplication::installTranslator(&translator);
    auto cleanup = qScopeGuard([&translator] { QCoreApplication::removeTranslator(&translator); });
    auto window = std::make_unique<MainWindow>(QStringList());
    frmPreferences preferences(window->topEditorContainer(), window.get());
    auto* style = new RightAlignedFormsStyle;
    style->setParent(&preferences);
    preferences.setStyle(style);
    auto* page = preferences.findChild<QWidget*>("pageAppearance");
    auto* stack = preferences.findChild<QStackedWidget*>("stackedWidget");
    auto* categories = preferences.findChild<QTreeWidget*>("treeWidget");
    QVERIFY(page && stack && categories);
    categories->setCurrentItem(categories->topLevelItem(1));
    QCOMPARE(stack->currentWidget(), page);
    preferences.resize(980, 720);
    preferences.show();
    QCoreApplication::processEvents();
    auto* form = preferences.findChild<QFormLayout*>("formLayout_3");
    auto* full = preferences.findChild<QCheckBox*>("chkShowFullTabTitles");
    auto* widthLabel = preferences.findChild<QLabel*>("lblMaximumTabWidth");
    auto* width = preferences.findChild<QSpinBox*>("spnMaximumTabWidth");
    QVERIFY(form && full && widthLabel && width);
    QCOMPARE(form->labelAlignment(), Qt::AlignLeft | Qt::AlignVCenter);
    const int left = full->mapTo(page, QPoint()).x();
    for (const char* name : {"chkOverrideFontFamily", "chkOverrideFontSize", "chkOverrideLineHeight",
                            "chkShowLineNumbers", "chkCompactTabs", "chkWrapTabs"}) {
        auto* checkbox = preferences.findChild<QCheckBox*>(name);
        QVERIFY(checkbox);
        QCOMPARE(checkbox->mapTo(page, QPoint()).x(), left);
        QVERIFY(checkbox->geometry().width() >= checkbox->sizeHint().width());
    }
    QCOMPARE(widthLabel->mapTo(page, QPoint()).x(), left);
    QVERIFY(widthLabel->mapTo(page, QPoint()).y() > full->mapTo(page, QPoint()).y());
    for (const char* name : {"cmbFontFamilies", "spnFontSize", "spnLineHeight"}) {
        auto* field = preferences.findChild<QWidget*>(name);
        QVERIFY(field);
        QCOMPARE(field->mapTo(page, QPoint()).x(), width->mapTo(page, QPoint()).x());
    }
    QCOMPARE(preferences.findChild<QCheckBox*>("chkShowLineNumbers")->text(), QString("Mostrar números de línea"));
    QVERIFY(width->width() < 150);
    auto* preview = preferences.findChild<QWebEngineView*>();
    QVERIFY(preview);
    QTRY_VERIFY_WITH_TIMEOUT(evaluateEditor(preview->page(), "typeof editor !== 'undefined' && !!editor").toBool(), 10000);
    QVERIFY(preferences.grab().save(QCoreApplication::applicationDirPath() + "/preferences-appearance-preview.png"));
}

void NotepadqqTest::tabColorAutosave()
{
    BackupService::clearBackupData();
    auto cleanup = qScopeGuard([] { BackupService::disableAutosave(); });
    auto window = std::make_unique<MainWindow>(QStringList());
    auto* editor = window->currentEditor();
    auto* view = editor->findChild<QWebEngineView*>();
    QVERIFY(view);
    QTRY_VERIFY_WITH_TIMEOUT(evaluateEditor(view->page(), "typeof editor !== 'undefined' && !!editor").toBool(), 10000);
    editor->setValue("autosaved text\n").wait();
    const QString backupPath = PersistentCache::backupDirPath() +
        QString("/window_%1/window.xml").arg(reinterpret_cast<uintptr_t>(window.get()));
    auto backupXml = [&backupPath] {
        QFile file(backupPath);
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    };
    // MainWindow starts the configured timer; replace it with the test interval.
    BackupService::disableAutosave();
    BackupService::enableAutosave(1);
    QTRY_VERIFY_WITH_TIMEOUT(!backupXml().isEmpty(), 5000);
    const QString originalContent = editor->value();
    editor->setTabColor("red");
    QTRY_VERIFY_WITH_TIMEOUT(backupXml().contains("tabColor=\"red\""), 5000);
    editor->setTabColor(QString());
    QTRY_VERIFY_WITH_TIMEOUT(!backupXml().contains("tabColor="), 5000);
    editor->setUnsavedTitle("Backup title");
    QTRY_VERIFY_WITH_TIMEOUT(backupXml().contains("unsavedTitle=\"Backup title\""), 5000);
    editor->setUnsavedTitle("Updated title");
    QTRY_VERIFY_WITH_TIMEOUT(backupXml().contains("unsavedTitle=\"Updated title\""), 5000);
    QCOMPARE(editor->value(), originalContent);
}

void NotepadqqTest::unsavedTabTitle()
{
    auto window = std::make_unique<MainWindow>(QStringList());
    auto* tabs = window->topEditorContainer()->currentTabWidget();
    auto* editor = window->currentEditor();
    auto* view = editor->findChild<QWebEngineView*>();
    QVERIFY(view);
    QTRY_VERIFY_WITH_TIMEOUT(evaluateEditor(view->page(), "typeof editor !== 'undefined' && !!editor").toBool(), 10000);
    editor->markClean().wait();
    const QString undoBefore = evaluateEditor(view->page(), "JSON.stringify(editor.historySize())").toString();
    editor->setUnsavedTitle("  Research notes  ");
    QCOMPARE(tabs->tabText(0), QString("Research notes"));
    QCOMPARE(tabs->tabToolTip(0), QString("Research notes"));
    QVERIFY(window->windowTitle().startsWith("Research notes - "));
    QVERIFY(editor->filePath().isEmpty());
    QVERIFY(editor->isClean());
    QCOMPARE(evaluateEditor(view->page(), "JSON.stringify(editor.historySize())").toString(), undoBefore);
    editor->setUnsavedTitle("   ");
    QCOMPARE(editor->unsavedTitle(), QString("Research notes"));
    tabs->addEditorTab(false, "another");
    tabs->tabBar()->moveTab(0, 1);
    QCOMPARE(tabs->tabText(1), QString("Research notes"));
    auto* other = window->topEditorContainer()->addTabWidget();
    other->transferEditorTab(false, tabs, 1);
    editor->setUnsavedTitle("Moved notes");
    QCOMPARE(other->tabText(0), QString("Moved notes"));
    QCOMPARE(tabs->tabText(0), QString("another"));
    QTemporaryDir files;
    QCOMPARE(window->getDocEngine()->saveDocument(other, 0,
        QUrl::fromLocalFile(files.filePath("copy.txt")), true), DocEngine::saveFileResult_Saved);
    QCOMPARE(other->tabText(0), QString("Moved notes"));
    QVERIFY(editor->filePath().isEmpty());
    QCOMPARE(window->getDocEngine()->saveDocument(other, 0,
        QUrl::fromLocalFile(files.filePath("saved.txt"))), DocEngine::saveFileResult_Saved);
    QCOMPARE(other->tabText(0), QString("saved.txt"));
    QVERIFY(editor->unsavedTitle().isEmpty());
    editor->setUnsavedTitle("Ignored");
    QCOMPARE(other->tabText(0), QString("saved.txt"));
}

void NotepadqqTest::unsavedTitleSession_data()
{
    QTest::addColumn<QString>("content");
    QTest::newRow("modified-document") << QString("draft\n");
    QTest::newRow("clean-empty-document") << QString();
}

void NotepadqqTest::unsavedTitleSession()
{
    QFETCH(QString, content);
    QTemporaryDir files;
    QVERIFY(files.isValid());
    const QString title = "Ideas / ../ café & <notes>";
    const QString sessionPath = files.filePath("session.xml");
    const QString cachePath = files.filePath("cache");
    {
        TopEditorContainer source;
        DocEngine engine(&source);
        auto* tabs = source.currentTabWidget();
        // Identical arbitrary titles must still have distinct, safe cache paths.
        for (int i = 0; i < 2; ++i) {
            tabs->addEditorTab(false, "new " + QString::number(i));
            auto* editor = tabs->editor(i);
            auto* view = editor->findChild<QWebEngineView*>();
            QTRY_VERIFY_WITH_TIMEOUT(evaluateEditor(view->page(), "typeof editor !== 'undefined' && !!editor").toBool(), 10000);
            editor->setValue(content).wait();
            if (content.isEmpty())
                editor->markClean().wait();
            editor->setUnsavedTitle(title);
            editor->setTabColor("green");
        }
        QVERIFY(Sessions::saveSession(&engine, &source, sessionPath, cachePath));
        const QStringList cached = QDir(cachePath).entryList(QDir::Files);
        QCOMPARE(cached.size(), 2);
        for (const QString& name : cached)
            QVERIFY(name.startsWith("untitled."));
    }
    TopEditorContainer restored;
    DocEngine engine(&restored);
    QSignalSpy loaded(&engine, &DocEngine::documentLoaded);
    Sessions::loadSession(&engine, &restored, sessionPath);
    auto* tabs = restored.currentTabWidget();
    QCOMPARE(tabs->count(), 2);
    QTRY_COMPARE_WITH_TIMEOUT(loaded.count(), 2, 10000);
    for (int i = 0; i < 2; ++i) {
        auto* editor = tabs->editor(i);
        QCOMPARE(tabs->tabText(i), title);
        QTRY_COMPARE_WITH_TIMEOUT(editor->value(), content, 10000);
        QTRY_COMPARE(editor->isClean(), content.isEmpty());
        QCOMPARE(editor->unsavedTitle(), title);
        QCOMPARE(tabs->tabToolTip(i), title);
        QVERIFY(editor->filePath().isEmpty());
        QCOMPARE(editor->tabColor(), QString("green"));
    }
}

void NotepadqqTest::unsavedTitleContextMenu_data()
{
    QTest::addColumn<bool>("accept");
    QTest::addColumn<bool>("saved");
    QTest::newRow("accept") << true << false;
    QTest::newRow("cancel") << false << false;
    QTest::newRow("saved-file-disabled") << false << true;
}

void NotepadqqTest::unsavedTitleContextMenu()
{
    QFETCH(bool, accept);
    QFETCH(bool, saved);
    auto window = std::make_unique<MainWindow>(QStringList());
    auto* tabs = window->topEditorContainer()->currentTabWidget();
    auto* editor = window->currentEditor();
    auto* view = editor->findChild<QWebEngineView*>();
    QTRY_VERIFY_WITH_TIMEOUT(evaluateEditor(view->page(), "typeof editor !== 'undefined' && !!editor").toBool(), 10000);
    editor->setUnsavedTitle("Original title");
    QTemporaryDir files;
    if (saved)
        editor->setFilePath(QUrl::fromLocalFile(files.filePath("saved.txt")));
    bool found = false;
    bool enabled = false;
    QTimer::singleShot(0, window.get(), [&] {
        auto* popup = qobject_cast<QMenu*>(QApplication::activePopupWidget());
        if (!popup)
            return;
        for (auto* action : popup->actions()) {
            if (action->text() != "Set Tab Title...")
                continue;
            found = true;
            enabled = action->isEnabled();
            if (!enabled)
                break;
            QTimer::singleShot(0, window.get(), [&] {
                auto* input = qobject_cast<QInputDialog*>(QApplication::activeModalWidget());
                if (input) {
                    input->setTextValue("New title");
                    accept ? input->accept() : input->reject();
                }
            });
            action->trigger();
            break;
        }
        popup->close();
    });
    QTimer::singleShot(2000, window.get(), [] {
        if (auto* modal = qobject_cast<QDialog*>(QApplication::activeModalWidget()))
            modal->reject();
        if (auto* popup = QApplication::activePopupWidget())
            popup->close();
    });
    QVERIFY(QMetaObject::invokeMethod(window.get(), "on_customTabContextMenuRequested", Qt::DirectConnection,
        Q_ARG(QPoint, QPoint(10, 10)), Q_ARG(EditorTabWidget*, tabs), Q_ARG(int, 0)));
    QVERIFY(found);
    QCOMPARE(enabled, !saved);
    QCOMPARE(tabs->tabText(0), saved ? QString("saved.txt") : accept ? QString("New title") : QString("Original title"));
}

void NotepadqqTest::tabColorContextMenu()
{
    auto window = std::make_unique<MainWindow>(QStringList());
    auto* tabs = window->topEditorContainer()->currentTabWidget();
    tabs->currentEditor()->setTabColor("blue");
    bool foundMenu = false;
    bool checkedColor = false;
    QTimer::singleShot(0, window.get(), [&] {
        auto* popup = qobject_cast<QMenu*>(QApplication::activePopupWidget());
        if (!popup)
            return;
        for (auto* action : popup->actions()) {
            auto* submenu = action->menu();
            if (!submenu || submenu->title() != "Tab Color")
                continue;
            foundMenu = true;
            QAction* green = nullptr;
            for (auto* color : submenu->actions()) {
                if (color->text() == "Blue")
                    checkedColor = color->isChecked();
                if (color->text() == "Green")
                    green = color;
            }
            if (green)
                green->trigger();
        }
        popup->close();
    });
    // Bound even an unexpected menu failure, so CI never waits for user input.
    QTimer::singleShot(2000, window.get(), [] {
        if (auto* popup = QApplication::activePopupWidget())
            popup->close();
    });
    QVERIFY(QMetaObject::invokeMethod(window.get(), "on_customTabContextMenuRequested", Qt::DirectConnection,
        Q_ARG(QPoint, QPoint(10, 10)), Q_ARG(EditorTabWidget*, tabs), Q_ARG(int, 0)));
    QVERIFY(foundMenu);
    QVERIFY(checkedColor);
    QCOMPARE(tabs->currentEditor()->tabColor(), QString("green"));
}

QTEST_MAIN(NotepadqqTest)

#include "tst_notepadqqtest.moc"
