#include "ShowroomSessionStore.h"
#include "ShowroomLog.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

namespace {

QString sessionFilePath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (dir.isEmpty())
        return {};

    QDir().mkpath(dir);
    return dir + QStringLiteral("/session.json");
}

QStringList candidateSessionPaths()
{
    QStringList paths;
    const auto addUnique = [&paths](const QString &path) {
        if (!path.isEmpty() && !paths.contains(path))
            paths.append(path);
    };

    const QString primary = sessionFilePath();
    addUnique(primary);

    const QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (!configDir.isEmpty()) {
        const QDir dir(configDir);
        addUnique(QDir::cleanPath(dir.absoluteFilePath(QStringLiteral("ShowroomPlayer/session.json"))));
        addUnique(QDir::cleanPath(dir.absoluteFilePath(QStringLiteral("../session.json"))));
    }

    return paths;
}

QString readSrIdFromFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
        qCWarning(lcShowroomAuth) << "Invalid session file format:" << path;
        return {};
    }

    const QString srId = document.object().value(QLatin1String("sr_id")).toString().trimmed();
    if (srId.isEmpty()) {
        qCWarning(lcShowroomAuth) << "Session file missing sr_id:" << path;
        return {};
    }

    qCInfo(lcShowroomAuth) << "Loaded sr_id from" << path;
    return srId;
}

} // namespace

QString ShowroomSessionStore::loadSrId()
{
    const QString primaryPath = sessionFilePath();
    const QStringList paths = candidateSessionPaths();

    QString foundSrId;
    QString foundPath;

    for (const QString &path : paths) {
        const QString srId = readSrIdFromFile(path);
        if (!srId.isEmpty()) {
            foundSrId = srId;
            foundPath = path;
            break;
        }
    }

    if (foundSrId.isEmpty()) {
        qCInfo(lcShowroomAuth) << "No saved session in" << paths.join(QStringLiteral(", "));
        return {};
    }

    if (foundPath != primaryPath && !primaryPath.isEmpty()) {
        qCInfo(lcShowroomAuth) << "Migrating session file to" << primaryPath;
        saveSrId(foundSrId);
    }

    return foundSrId;
}

bool ShowroomSessionStore::saveSrId(const QString &srId)
{
    const QString path = sessionFilePath();
    if (path.isEmpty() || srId.trimmed().isEmpty())
        return false;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qCWarning(lcShowroomAuth) << "Failed to write session file:" << path;
        return false;
    }

    const QJsonObject object{{QStringLiteral("sr_id"), srId.trimmed()}};
    file.write(QJsonDocument(object).toJson(QJsonDocument::Compact));
    qCInfo(lcShowroomAuth) << "Saved sr_id to" << path;
    return true;
}

bool ShowroomSessionStore::clear()
{
    bool ok = true;
    bool removedAny = false;

    for (const QString &path : candidateSessionPaths()) {
        if (!QFile::exists(path))
            continue;

        removedAny = true;
        ok = QFile::remove(path) && ok;
    }

    if (!removedAny)
        return true;

    if (ok)
        qCInfo(lcShowroomAuth) << "Removed session file(s)";
    else
        qCWarning(lcShowroomAuth) << "Failed to remove one or more session files";

    return ok;
}
