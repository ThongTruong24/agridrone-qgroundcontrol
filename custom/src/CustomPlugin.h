#pragma once

#include <QtQml/QQmlAbstractUrlInterceptor>

#include "QGCCorePlugin.h"

class QQmlApplicationEngine;

class CustomPlugin : public QGCCorePlugin
{
    Q_OBJECT

public:
    explicit CustomPlugin(QObject* parent = nullptr);

    static QGCCorePlugin* instance();

    const QVariantList& toolBarIndicators() override;

    QQmlApplicationEngine* createQmlApplicationEngine(QObject* parent) final;
    void destroyQmlApplicationEngine(QQmlApplicationEngine* qmlEngine) final;

private:
    QQmlAbstractUrlInterceptor* _urlInterceptor = nullptr;
};

class CustomOverrideInterceptor final : public QQmlAbstractUrlInterceptor
{
public:
    QUrl intercept(const QUrl& url, DataType type) final;
};
