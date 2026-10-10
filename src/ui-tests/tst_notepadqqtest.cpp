#include "include/EditorNS/editor.h"
#include "include/editoruicontroller.h"
#include "include/mainwindow.h"
#include "include/notepadqq.h"
#include "include/nqqsettings.h"
#include "include/docengine.h"
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
#include <QSpinBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QScopeGuard>
#include <QMenu>
#include <QInputDialog>
#include <QTranslator>
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
    void forkTranslations_data();
    void forkTranslations();
    void removesLegacyTelemetrySettings();
    void tabColorSession_data();
    void tabColorSession();
    void legacyTabColorSession_data();
    void legacyTabColorSession();
    void tabAppearancePreferences();
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
        "Set Tab Title...", "Set Tab Title", "Title:"})
        QVERIFY2(!translator.translate("MainWindow", source).isEmpty(), source);
    for (const char* source : {"Compact tabs", "Maximum compact tab width:",
        "Always show full tab titles", "Wrap tabs into multiple rows", " px"})
        QVERIFY2(!translator.translate("frmPreferences", source).isEmpty(), source);
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
