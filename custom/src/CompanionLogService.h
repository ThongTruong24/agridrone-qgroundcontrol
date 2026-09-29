#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QList>
#include <QtCore/QFile>
#include <QtCore/QTextStream>
#include <QtCore/QDateTime>
#include <QtCore/QVariantList>
#include <QtCore/QVariantMap>
#include <QtCore/QMutex>

struct CompanionLogEntry {
    QString timestamp;
    QString category;
    QString direction;
    QString message;
    int severity;
};

/**
 * @brief Service responsible for MAVLink command & event audit logging (SOLID: Single Responsibility)
 */
class CompanionLogService : public QObject {
    Q_OBJECT
public:
    explicit CompanionLogService(QObject* parent = nullptr);
    ~CompanionLogService() override;

    void logMavlink(const QString& category, const QString& direction, const QString& message, int severity = 6);
    QVariantList getLogHistory(const QString& category) const;
    void clearLogHistory(const QString& category);

signals:
    void logEntriesChanged();
    void logMessageAdded(const QString& category, const QString& direction, const QString& message, int severity);

private:
    void _loadLogFile();
    void _flushLogEntryToFile(const CompanionLogEntry& entry);

    mutable QMutex _mutex;
    QList<CompanionLogEntry> _logEntries;
    QFile _logFile;
    QTextStream _logStream;
    static constexpr int kMaxLogEntries = 500;
};
