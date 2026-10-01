#include "CompanionLogService.h"

#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QStandardPaths>

CompanionLogService::CompanionLogService(QObject* parent) : QObject(parent)
{
    QString logDirPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(logDirPath);
    if (!dir.exists()) {
        dir.mkpath(logDirPath);
    }
    QString logFilePath = dir.filePath("thaco_companion_audit.log");
    _logFile.setFileName(logFilePath);
    if (_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        _logStream.setDevice(&_logFile);
    } else {
        qWarning() << "[CompanionLogService] Could not open audit log at:" << logFilePath;
    }
}

CompanionLogService::~CompanionLogService()
{
    if (_logFile.isOpen()) {
        _logStream.flush();
        _logFile.close();
    }
}

void CompanionLogService::_flushLogEntryToFile(const CompanionLogEntry& entry)
{
    if (_logFile.isOpen()) {
        _logStream << QStringLiteral("[%1] [%2] [%3] %4 (sev=%5)\n")
                          .arg(entry.timestamp, entry.category, entry.direction, entry.message)
                          .arg(entry.severity);
        _logStream.flush();
    }
}

void CompanionLogService::logMavlink(const QString& category, const QString& direction, const QString& message,
                                     int severity)
{
    QMutexLocker locker(&_mutex);
    CompanionLogEntry entry;
    entry.timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    entry.category = category;
    entry.direction = direction;
    entry.message = message;
    entry.severity = severity;

    if (_logEntries.size() >= kMaxLogEntries) {
        _logEntries.removeFirst();
    }
    _logEntries.append(entry);
    _flushLogEntryToFile(entry);

    locker.unlock();
    emit logMessageAdded(category, direction, message, severity);
}

bool CompanionLogService::matchesFilter(const QString& filter, const QString& category, const QString& message)
{
    if (filter.trimmed().isEmpty() || filter.compare(QStringLiteral("ALL"), Qt::CaseInsensitive) == 0) {
        return true;
    }
    for (const QString& rawToken : filter.split(',')) {
        const QString token = rawToken.trimmed();
        if (!token.isEmpty() &&
            (category.contains(token, Qt::CaseInsensitive) || message.contains(token, Qt::CaseInsensitive))) {
            return true;
        }
    }
    return false;
}

QVariantList CompanionLogService::getLogHistory(const QString& category) const
{
    QMutexLocker locker(&_mutex);
    QVariantList list;
    for (const auto& entry : _logEntries) {
        if (matchesFilter(category, entry.category, entry.message)) {
            QVariantMap map;
            map["timestamp"] = entry.timestamp;
            map["category"] = entry.category;
            map["direction"] = entry.direction;
            map["message"] = entry.message;
            map["severity"] = entry.severity;
            list.append(map);
        }
    }
    return list;
}

void CompanionLogService::clearLogHistory(const QString& category)
{
    QMutexLocker locker(&_mutex);
    if (category.trimmed().isEmpty() || category.compare(QStringLiteral("ALL"), Qt::CaseInsensitive) == 0) {
        _logEntries.clear();
    } else {
        auto it = _logEntries.begin();
        while (it != _logEntries.end()) {
            if (matchesFilter(category, it->category, it->message)) {
                it = _logEntries.erase(it);
            } else {
                ++it;
            }
        }
    }
}
