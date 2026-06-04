#include "database_service.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>

namespace app {

DatabaseService::DatabaseService(QObject *parent)
    : QObject(parent)
    , m_connectionName(QStringLiteral("app_db_%1").arg(reinterpret_cast<quintptr>(this)))
{
}

DatabaseService::~DatabaseService()
{
    if (m_db.isOpen()) {
        m_db.close();
    }
    m_db = {};
    QSqlDatabase::removeDatabase(m_connectionName);
}

bool DatabaseService::open(const QString &path)
{
    const QString dbPath = path.isEmpty() ? defaultDatabasePath() : path;
    QDir().mkpath(QFileInfo(dbPath).absolutePath());

    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    m_db.setDatabaseName(dbPath);
    if (!m_db.open()) {
        emit errorOccurred(QStringLiteral("打开 SQLite 失败: %1").arg(m_db.lastError().text()));
        return false;
    }
    return ensureSchema();
}

QJsonObject DatabaseService::queryExampleState() const
{
    QJsonObject result;
    if (!m_db.isOpen()) {
        result.insert(QStringLiteral("ok"), false);
        result.insert(QStringLiteral("error"), QStringLiteral("database is not open"));
        return result;
    }

    QSqlQuery query(m_db);
    if (!query.exec(QStringLiteral("select count(*) from app_events"))) {
        result.insert(QStringLiteral("ok"), false);
        result.insert(QStringLiteral("error"), query.lastError().text());
        return result;
    }
    query.next();
    result.insert(QStringLiteral("ok"), true);
    result.insert(QStringLiteral("eventCount"), query.value(0).toInt());
    return result;
}

void DatabaseService::handleEvent(const AppEvent &event)
{
    if (!event.flags.testFlag(EventFlag::Persist)) {
        return;
    }

    const qint64 id = insertEvent(event);
    if (id >= 0) {
        emit eventPersisted(event, id);
    }
}

bool DatabaseService::ensureSchema()
{
    QSqlQuery query(m_db);
    const bool ok = query.exec(QStringLiteral(
        "create table if not exists app_events ("
        "id integer primary key autoincrement,"
        "type text not null,"
        "source text not null,"
        "name text not null,"
        "occurred_at text not null,"
        "payload_json text not null"
        ")"));
    if (!ok) {
        emit errorOccurred(QStringLiteral("初始化 SQLite schema 失败: %1").arg(query.lastError().text()));
    }
    return ok;
}

qint64 DatabaseService::insertEvent(const AppEvent &event)
{
    if (!m_db.isOpen()) {
        emit errorOccurred(QStringLiteral("SQLite 未打开，无法写入事件"));
        return -1;
    }

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "insert into app_events(type, source, name, occurred_at, payload_json) "
        "values(:type, :source, :name, :occurred_at, :payload_json)"));
    query.bindValue(QStringLiteral(":type"), eventTypeName(event.type));
    query.bindValue(QStringLiteral(":source"), eventSourceName(event.source));
    query.bindValue(QStringLiteral(":name"), event.name);
    query.bindValue(QStringLiteral(":occurred_at"), event.occurredAt.toString(Qt::ISODateWithMs));
    query.bindValue(QStringLiteral(":payload_json"),
                    QString::fromUtf8(QJsonDocument(event.payload).toJson(QJsonDocument::Compact)));

    if (!query.exec()) {
        emit errorOccurred(QStringLiteral("写入事件失败: %1").arg(query.lastError().text()));
        return -1;
    }
    return query.lastInsertId().toLongLong();
}

QString DatabaseService::defaultDatabasePath() const
{
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dataDir.isEmpty()) {
        dataDir = QCoreApplication::applicationDirPath();
    }
    return QDir(dataDir).filePath(QStringLiteral("roco_helper.sqlite"));
}

} // namespace app
