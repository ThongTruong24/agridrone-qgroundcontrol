#include <QtCore/QCommandLineOption>
#include <QtCore/QCommandLineParser>
#include <QtCore/QDir>
#include <QtCore/QDirIterator>
#include <QtCore/QFileInfo>
#include <QtCore/QFileSystemWatcher>
#include <QtCore/QTimer>
#include <QtGui/QGuiApplication>
#include <QtQml/QQmlApplicationEngine>
#include <QtQml/QQmlContext>
#include <QtQuickControls2/QQuickStyle>

class PreviewController : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    Q_INVOKABLE void reload() { emit reloadRequested(); }

signals:
    void reloadRequested();
};

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("CC Telemetry Preview"));
    QGuiApplication::setOrganizationName(QStringLiteral("QGroundControl"));
    QGuiApplication::setQuitOnLastWindowClosed(false);
    QQuickStyle::setStyle(QStringLiteral("Fusion"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Standalone preview for the CC Telemetry QML UI"));
    parser.addHelpOption();
    parser.addOption({QStringLiteral("qml-dir"),
                      QStringLiteral("Load QML from <directory> instead of the configured source directory."),
                      QStringLiteral("directory")});
    parser.addOption({QStringLiteral("no-watch"), QStringLiteral("Disable automatic QML reload.")});
    parser.addOption({QStringLiteral("quit-after"),
                      QStringLiteral("Exit after <milliseconds>; useful for a headless smoke test."),
                      QStringLiteral("milliseconds")});
    parser.process(app);

    const QString configuredSourceDir = QStringLiteral(PREVIEW_QML_SOURCE_DIR);
    const QString sharedSourceDir = QDir::cleanPath(QStringLiteral(PREVIEW_SHARED_QML_SOURCE_DIR));
    const QString sourceDir = QDir::cleanPath(
        parser.isSet(QStringLiteral("qml-dir")) ? parser.value(QStringLiteral("qml-dir")) : configuredSourceDir);
    const QString mainFile = QDir(sourceDir).filePath(QStringLiteral("Main.qml"));

    if (!QFileInfo::exists(mainFile)) {
        qCritical("Preview entry point does not exist: %s", qPrintable(mainFile));
        return EXIT_FAILURE;
    }

    QQmlApplicationEngine engine;
    PreviewController previewController;
    QFileSystemWatcher watcher;
    QTimer reloadTimer;
    reloadTimer.setInterval(180);
    reloadTimer.setSingleShot(true);

    engine.rootContext()->setContextProperty(QStringLiteral("previewController"), &previewController);
    engine.rootContext()->setContextProperty(
        QStringLiteral("previewSharedPanelUrl"),
        QUrl::fromLocalFile(QDir(sharedSourceDir).filePath(QStringLiteral("CcTelemetryPanel.qml"))));

    const auto refreshWatchedPaths = [&watcher, &sourceDir, &sharedSourceDir]() {
        if (!watcher.files().isEmpty()) {
            watcher.removePaths(watcher.files());
        }
        if (!watcher.directories().isEmpty()) {
            watcher.removePaths(watcher.directories());
        }

        QStringList files;
        QStringList directories;
        const QStringList sourceDirectories{sourceDir, sharedSourceDir};
        for (const QString& watchedSourceDir : sourceDirectories) {
            directories.append(watchedSourceDir);
            QDirIterator iterator(watchedSourceDir, {QStringLiteral("*.qml")}, QDir::Files,
                                  QDirIterator::Subdirectories);
            while (iterator.hasNext()) {
                const QString file = iterator.next();
                files.append(file);
                const QString directory = QFileInfo(file).absolutePath();
                if (!directories.contains(directory)) {
                    directories.append(directory);
                }
            }
        }

        watcher.addPaths(files);
        watcher.addPaths(directories);
    };

    const auto loadPreview = [&engine, &mainFile]() {
        const auto rootObjects = engine.rootObjects();
        for (QObject* rootObject : rootObjects) {
            delete rootObject;
        }
        engine.clearComponentCache();
        engine.load(QUrl::fromLocalFile(mainFile));
    };

    QObject::connect(&reloadTimer, &QTimer::timeout, &app, [&loadPreview, &refreshWatchedPaths]() {
        refreshWatchedPaths();
        loadPreview();
    });
    QObject::connect(&previewController, &PreviewController::reloadRequested, &reloadTimer,
                     qOverload<>(&QTimer::start));

    if (!parser.isSet(QStringLiteral("no-watch"))) {
        QObject::connect(&watcher, &QFileSystemWatcher::fileChanged, &reloadTimer,
                         [&reloadTimer](const QString&) { reloadTimer.start(); });
        QObject::connect(&watcher, &QFileSystemWatcher::directoryChanged, &reloadTimer,
                         [&reloadTimer](const QString&) { reloadTimer.start(); });
        refreshWatchedPaths();
    }

    loadPreview();
    if (engine.rootObjects().isEmpty()) {
        return EXIT_FAILURE;
    }

    if (parser.isSet(QStringLiteral("quit-after"))) {
        bool durationIsValid = false;
        const int duration = parser.value(QStringLiteral("quit-after")).toInt(&durationIsValid);
        if (!durationIsValid || duration < 0) {
            qCritical("--quit-after expects a non-negative number of milliseconds");
            return EXIT_FAILURE;
        }
        QTimer::singleShot(duration, &app, &QCoreApplication::quit);
    }

    return app.exec();
}

#include "main.moc"
