#include "database_service.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>

namespace app {
namespace {

QString nonNullString(const QString &value, const QString &fallback = QStringLiteral(""))
{
    return value.isNull() ? fallback : value;
}

} // namespace

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

bool DatabaseService::isOpen() const
{
    return m_db.isOpen();
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

QJsonArray DatabaseService::queryMapMarkers() const
{
    QJsonArray rows;
    if (!m_db.isOpen()) {
        return rows;
    }

    QSqlQuery query(m_db);
    if (!query.exec(QStringLiteral(
            "select id, marker_type, label, visible, game_x, game_y, game_z, extra_json "
            "from map_markers order by id"))) {
        return rows;
    }
    while (query.next()) {
        rows.append(QJsonObject{
            {QStringLiteral("id"), query.value(0).toString()},
            {QStringLiteral("marker_type"), query.value(1).toString()},
            {QStringLiteral("label"), query.value(2).toString()},
            {QStringLiteral("visible"), query.value(3).toBool()},
            {QStringLiteral("game_x"), query.value(4).toInt()},
            {QStringLiteral("game_y"), query.value(5).toInt()},
            {QStringLiteral("game_z"), query.value(6).toInt()},
            {QStringLiteral("extra_json"), query.value(7).toString()},
        });
    }
    return rows;
}

QJsonArray DatabaseService::queryPetInfo() const
{
    QJsonArray rows;
    if (!m_db.isOpen()) {
        return rows;
    }

    QSqlQuery query(m_db);
    if (!query.exec(QStringLiteral("select data from pet_info order by id"))) {
        return rows;
    }
    while (query.next()) {
        const QJsonDocument doc = QJsonDocument::fromJson(query.value(0).toString().toUtf8());
        if (doc.isObject()) {
            rows.append(doc.object());
        }
    }
    return rows;
}

QJsonArray DatabaseService::queryBoxInfo() const
{
    QJsonArray rows;
    if (!m_db.isOpen()) {
        return rows;
    }

    QSqlQuery query(m_db);
    if (!query.exec(QStringLiteral("select id, data from box order by id"))) {
        return rows;
    }
    while (query.next()) {
        const QJsonDocument doc = QJsonDocument::fromJson(query.value(1).toString().toUtf8());
        rows.append(QJsonObject{
            {QStringLiteral("id"), query.value(0).toInt()},
            {QStringLiteral("data"), doc.isArray() ? doc.array() : QJsonArray()},
        });
    }
    return rows;
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

    switch (event.type) {
    case EventType::PetInfoReload:
    case EventType::PetInfoChanged:
    case EventType::PetInfoDeleted:
        handlePetInfoEvent(event);
        break;
    case EventType::BoxInfoReload:
    case EventType::BoxInfoChanged:
        handleBoxInfoEvent(event);
        break;
    default:
        break;
    }
}

void DatabaseService::upsertMarker(const MapMarker &marker)
{
    if (!m_db.isOpen() || marker.temporary || marker.id.isEmpty()) {
        return;
    }

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "insert into map_markers(id, marker_type, label, visible, game_x, game_y, game_z, extra_json) "
        "values(:id, :marker_type, :label, :visible, :game_x, :game_y, :game_z, :extra_json) "
        "on conflict(id) do update set "
        "marker_type=excluded.marker_type,"
        "label=excluded.label,"
        "visible=excluded.visible,"
        "game_x=excluded.game_x,"
        "game_y=excluded.game_y,"
        "game_z=excluded.game_z,"
        "extra_json=excluded.extra_json"));
    query.bindValue(QStringLiteral(":id"), marker.id);
    query.bindValue(QStringLiteral(":marker_type"), nonNullString(marker.markerType, QString::fromLatin1(DefaultMarkerType)));
    query.bindValue(QStringLiteral(":label"), nonNullString(marker.label));
    query.bindValue(QStringLiteral(":visible"), marker.visible);
    query.bindValue(QStringLiteral(":game_x"), marker.gameX);
    query.bindValue(QStringLiteral(":game_y"), marker.gameY);
    query.bindValue(QStringLiteral(":game_z"), marker.gameZ);
    query.bindValue(QStringLiteral(":extra_json"), QString::fromUtf8(QJsonDocument(marker.extra).toJson(QJsonDocument::Compact)));
    if (!query.exec()) {
        emit errorOccurred(QStringLiteral("保存地图点位失败: %1").arg(query.lastError().text()));
    }
}

void DatabaseService::deleteMarker(const QString &markerId)
{
    if (!m_db.isOpen() || markerId.isEmpty()) {
        return;
    }
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("delete from map_markers where id = :id"));
    query.bindValue(QStringLiteral(":id"), markerId);
    if (!query.exec()) {
        emit errorOccurred(QStringLiteral("删除地图点位失败: %1").arg(query.lastError().text()));
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
    if (!ok) {
        return false;
    }

    const QStringList statements = {
        QStringLiteral(
            "create table if not exists map_markers ("
            "id text primary key,"
            "marker_type text not null,"
            "label text not null default '',"
            "visible integer not null default 1,"
            "game_x integer,"
            "game_y integer,"
            "game_z integer,"
            "extra_json text not null default '{}'"
            ")"),
        QStringLiteral(
            "create table if not exists pet_info ("
            "id integer primary key not null,"
            "data text not null"
            ")"),
        QStringLiteral(
            "create table if not exists box ("
            "id integer primary key not null,"
            "data text not null default '[]'"
            ")"),
    };
    for (const QString &statement : statements) {
        if (!query.exec(statement)) {
            emit errorOccurred(QStringLiteral("初始化业务表失败: %1").arg(query.lastError().text()));
            return false;
        }
    }
    return true;
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

void DatabaseService::handlePetInfoEvent(const AppEvent &event)
{
    if (!m_db.isOpen()) {
        return;
    }
    if (event.type == EventType::PetInfoReload) {
        const int pageNo = event.payload.value(QStringLiteral("page_no")).toInt(1);
        if (pageNo == 1) {
            QSqlQuery clear(m_db);
            if (!clear.exec(QStringLiteral("delete from pet_info"))) {
                emit errorOccurred(QStringLiteral("清空宠物信息失败: %1").arg(clear.lastError().text()));
                return;
            }
        }
        const QJsonArray data = event.payload.value(QStringLiteral("data")).toArray();
        for (const QJsonValue &value : data) {
            const QJsonObject pet = value.toObject();
            const int petId = pet.value(QStringLiteral("gid")).toInt();
            if (petId > 0) {
                savePetInfo(petId, pet);
            }
        }
        return;
    }
    if (event.type == EventType::PetInfoChanged) {
        const int petId = event.payload.value(QStringLiteral("id")).toInt();
        const QJsonObject data = event.payload.value(QStringLiteral("data")).toObject();
        if (petId > 0 && !data.isEmpty()) {
            savePetInfo(petId, data);
        }
        return;
    }
    if (event.type == EventType::PetInfoDeleted) {
        const QJsonArray ids = event.payload.value(QStringLiteral("ids")).toArray();
        QSqlQuery query(m_db);
        query.prepare(QStringLiteral("delete from pet_info where id = :id"));
        for (const QJsonValue &value : ids) {
            query.bindValue(QStringLiteral(":id"), value.toInt());
            if (!query.exec()) {
                emit errorOccurred(QStringLiteral("删除宠物信息失败: %1").arg(query.lastError().text()));
            }
        }
    }
}

void DatabaseService::handleBoxInfoEvent(const AppEvent &event)
{
    if (!m_db.isOpen()) {
        return;
    }
    if (event.type == EventType::BoxInfoReload) {
        replaceBoxes(event.payload.value(QStringLiteral("boxes")).toArray());
        return;
    }
    if (event.type == EventType::BoxInfoChanged) {
        changeBoxSlot(
            event.payload.value(QStringLiteral("id")).toInt(),
            event.payload.value(QStringLiteral("pos")).toInt(),
            event.payload.value(QStringLiteral("value")).toInt());
    }
}

void DatabaseService::savePetInfo(int petId, const QJsonObject &data)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "insert into pet_info(id, data) values(:id, :data) "
        "on conflict(id) do update set data = excluded.data"));
    query.bindValue(QStringLiteral(":id"), petId);
    query.bindValue(QStringLiteral(":data"), QString::fromUtf8(QJsonDocument(data).toJson(QJsonDocument::Compact)));
    if (!query.exec()) {
        emit errorOccurred(QStringLiteral("保存宠物信息失败: %1").arg(query.lastError().text()));
    }
}

void DatabaseService::replaceBoxes(const QJsonArray &boxes)
{
    QSqlQuery clear(m_db);
    if (!clear.exec(QStringLiteral("delete from box"))) {
        emit errorOccurred(QStringLiteral("清空仓库信息失败: %1").arg(clear.lastError().text()));
        return;
    }

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("insert into box(id, data) values(:id, :data)"));
    for (const QJsonValue &value : boxes) {
        const QJsonObject box = value.toObject();
        query.bindValue(QStringLiteral(":id"), box.value(QStringLiteral("id")).toInt());
        query.bindValue(QStringLiteral(":data"),
                        QString::fromUtf8(QJsonDocument(box.value(QStringLiteral("data")).toArray()).toJson(QJsonDocument::Compact)));
        if (!query.exec()) {
            emit errorOccurred(QStringLiteral("保存仓库信息失败: %1").arg(query.lastError().text()));
        }
    }
}

void DatabaseService::changeBoxSlot(int boxId, int pos, int value)
{
    if (pos < 0 || pos >= 30) {
        return;
    }

    QJsonArray data;
    QSqlQuery select(m_db);
    select.prepare(QStringLiteral("select data from box where id = :id"));
    select.bindValue(QStringLiteral(":id"), boxId);
    if (select.exec() && select.next()) {
        const QJsonDocument doc = QJsonDocument::fromJson(select.value(0).toString().toUtf8());
        if (doc.isArray()) {
            data = doc.array();
        }
    }
    while (data.size() < 30) {
        data.append(0);
    }
    data.replace(pos, value);

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "insert into box(id, data) values(:id, :data) "
        "on conflict(id) do update set data = excluded.data"));
    query.bindValue(QStringLiteral(":id"), boxId);
    query.bindValue(QStringLiteral(":data"), QString::fromUtf8(QJsonDocument(data).toJson(QJsonDocument::Compact)));
    if (!query.exec()) {
        emit errorOccurred(QStringLiteral("保存仓库变化失败: %1").arg(query.lastError().text()));
    }
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
