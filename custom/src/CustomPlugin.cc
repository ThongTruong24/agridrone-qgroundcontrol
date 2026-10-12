#include "CustomPlugin.h"

#include <QtCore/QApplicationStatic>
#include <QtCore/QFile>
#include <QtQml/QQmlApplicationEngine>

Q_APPLICATION_STATIC(CustomPlugin, _customPluginInstance);

CustomPlugin::CustomPlugin(QObject* parent) : QGCCorePlugin(parent) {}

QGCCorePlugin* CustomPlugin::instance()
{
    return _customPluginInstance();
}

const QVariantList& CustomPlugin::toolBarIndicators()
{
    static const QVariantList indicators = [this]() {
        auto result = QGCCorePlugin::toolBarIndicators();
        for (const auto &name : {"CameraStatusIndicator", "VisionStatusIndicator", "CompanionStatusIndicator"})
            result.append(QUrl(QString("qrc:/qml/Custom/AgriDrone/%1.qml").arg(name)));
        return result;
    }();
    return indicators;
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
