#include "include/EditorNS/editor.h"
#include "include/editoruicontroller.h"
#include "include/mainwindow.h"
#include "include/notepadqq.h"

#include <QCloseEvent>
#include <QAction>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPointer>
#include <QSettings>
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

QTEST_MAIN(NotepadqqTest)

#include "tst_notepadqqtest.moc"
