#include "CustomPlugin.h"

#include <QtCore/QApplicationStatic>
#include <QtCore/QFile>
#include <QtQml/QQmlApplicationEngine>

#include "CompanionShellController.h"
#include "QmlComponentInfo.h"

Q_APPLICATION_STATIC(CustomPlugin, _customPluginInstance);

CustomPlugin::CustomPlugin(QObject* parent) : QGCCorePlugin(parent) {}

QGCCorePlugin* CustomPlugin::instance()
{
    return _customPluginInstance();
}

void CustomPlugin::init()
{
    QGCCorePlugin::init();
    // Not in the constructor: the controller needs MAVLinkProtocol and MultiVehicleManager, which exist after start-up.
    companionShell();
}

CompanionShellController* CustomPlugin::companionShell() const
{
    if (_companionShell == nullptr) {
        _companionShell = new CompanionShellController(const_cast<CustomPlugin*>(this));
    }
    return _companionShell;
}

void CustomPlugin::cleanup()
{
    delete _companionShell;
    _companionShell = nullptr;
    QGCCorePlugin::cleanup();
}

const QVariantList& CustomPlugin::toolBarIndicators()
{
    static const QVariantList indicators = [this]() {
        auto result = QGCCorePlugin::toolBarIndicators();
        for (const auto &name : {"CameraStatusIndicator", "CompanionStatusIndicator"})
            result.append(QUrl(QString("qrc:/qml/Custom/AgriDrone/%1.qml").arg(name)));
        return result;
    }();
    return indicators;
}

const QVariantList& CustomPlugin::analyzePages()
{
    static const QVariantList pages = [this]() {
        auto result = QGCCorePlugin::analyzePages();
        // Works with no Vehicle: the shell reaches the companion host over its own MAVLink route.
        result.append(QVariant::fromValue(new QmlComponentInfo(
            QObject::tr("Companion Shell"), QUrl(QStringLiteral("qrc:/qml/Custom/AgriDrone/CompanionShellPage.qml")),
            QUrl::fromUserInput(QStringLiteral("qrc:/qmlimages/MAVLinkConsoleIcon.svg")), nullptr, false)));
        return result;
    }();
    return pages;
}

QQmlApplicationEngine* CustomPlugin::createQmlApplicationEngine(QObject* parent)
{
    QQmlApplicationEngine* const qmlEngine = QGCCorePlugin::createQmlApplicationEngine(parent);
    _urlInterceptor = new CustomOverrideInterceptor;
    qmlEngine->addUrlInterceptor(_urlInterceptor);

    return qmlEngine;
}

void CustomPlugin::destroyQmlApplicationEngine(QQmlApplicationEngine* qmlEngine)
{
    if (_urlInterceptor) {
        qmlEngine->removeUrlInterceptor(_urlInterceptor);
        delete _urlInterceptor;
        _urlInterceptor = nullptr;
    }

    QGCCorePlugin::destroyQmlApplicationEngine(qmlEngine);
}

QUrl CustomOverrideInterceptor::intercept(const QUrl& url, DataType type)
{
    if ((type == QmlFile) || (type == UrlString)) {
        if (url.scheme() == QStringLiteral("qrc")) {
            const QString overrideResource = QStringLiteral(":/Custom%1").arg(url.path());
            if (QFile::exists(overrideResource)) {
                QUrl result;
                result.setScheme(QStringLiteral("qrc"));
                result.setPath(overrideResource.sliced(1));
                return result;
            }
        }
    }

    return url;
}
