#pragma once

#include <QtQml/QQmlAbstractUrlInterceptor>

#include "QGCCorePlugin.h"

class QQmlApplicationEngine;
class CompanionShellController;

class CustomPlugin : public QGCCorePlugin
{
    Q_OBJECT
    /// The one MAVLink shell session of this app. Owned here, never by a page: closing and reopening the page keeps it.
    Q_PROPERTY(CompanionShellController* companionShell READ companionShell CONSTANT)

public:
    explicit CustomPlugin(QObject* parent = nullptr);

    static QGCCorePlugin* instance();

    void init() override;
    void cleanup() override;
    /// Created on first use (unit tests never run init()); init() creates it at start-up to hear heartbeats early.
    CompanionShellController* companionShell() const;

    const QVariantList& toolBarIndicators() override;
    const QVariantList& analyzePages() override;

    QQmlApplicationEngine* createQmlApplicationEngine(QObject* parent) final;
    void destroyQmlApplicationEngine(QQmlApplicationEngine* qmlEngine) final;

private:
    QQmlAbstractUrlInterceptor* _urlInterceptor = nullptr;
    mutable CompanionShellController* _companionShell = nullptr;
};

class CustomOverrideInterceptor final : public QQmlAbstractUrlInterceptor
{
public:
    QUrl intercept(const QUrl& url, DataType type) final;
};
