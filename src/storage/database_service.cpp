#include "database_service.h"

#include <QCoreApplication>
#include <QDateTime>
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

QString petTableName(quint64 uid)
{
    return QStringLiteral("pet_info_%1").arg(uid);
}

QString boxTableName(quint64 uid)
{
    return QStringLiteral("box_%1").arg(uid);
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
    if (!ensureSchema()) {
        return false;
    }
    loadUsers();
    return true;
}

bool DatabaseService::isOpen() const
{
    return m_db.isOpen();
}

void DatabaseService::resetMapMarkerVisibility()
{
    if (!m_db.isOpen()) {
        return;
    }
    QSqlQuery query(m_db);
    if (!query.exec(QStringLiteral("update map_markers set visible = 1"))) {
        emit errorOccurred(QStringLiteral("重置地图点位可见状态失败: %1").arg(query.lastError().text()));
    }
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

QJsonArray DatabaseService::queryPetInfo(quint64 uid) const
{
    QJsonArray rows;
    if (!m_db.isOpen() || uid == 0) {
        return rows;
    }

    QSqlQuery query(m_db);
    if (!query.exec(QStringLiteral("select data from %1 order by id").arg(petTableName(uid)))) {
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

QJsonArray DatabaseService::queryBoxInfo(quint64 uid) const
{
    QJsonArray rows;
    if (!m_db.isOpen() || uid == 0) {
        return rows;
    }

    QSqlQuery query(m_db);
    if (!query.exec(QStringLiteral("select id, data from %1 order by id").arg(boxTableName(uid)))) {
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

QJsonArray DatabaseService::queryUsers() const
{
    QJsonArray rows;
    if (!m_db.isOpen()) {
        return rows;
    }

    QSqlQuery query(m_db);
    if (!query.exec(QStringLiteral("select uid, name, avatar from users order by last_seen desc"))) {
        return rows;
    }
    while (query.next()) {
        rows.append(QJsonObject{
            {QStringLiteral("uid"), QString::number(query.value(0).toULongLong())},
            {QStringLiteral("name"), query.value(1).toString()},
            {QStringLiteral("avatar"), query.value(2).toInt()},
        });
    }
    return rows;
}

QHash<QString, QByteArray> DatabaseService::allFlowKeys() const
{
    return m_flowKeys;
}

quint64 DatabaseService::uidForFlow(const QString &flowId) const
{
    return m_flowUid.value(flowId, 0);
}

void DatabaseService::handleEvent(const AppEvent &event)
{
    if (!event.flags.testFlag(EventFlag::Persist)) {
        return;
    }

    switch (event.type) {
    case EventType::PetInfoReload:
    case EventType::PetInfoChanged:
    case EventType::PetInfoDeleted:
        if (event.uid == 0) {
            return;
        }
        handlePetInfoEvent(event);
        break;
    case EventType::BoxInfoReload:
    case EventType::BoxInfoChanged:
    case EventType::BoxInfoBoxReplaced:
        if (event.uid == 0) {
            return;
        }
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
            "create table if not exists users ("
            "uid integer primary key not null,"
            "name text not null default '',"
            "avatar integer not null default 0,"
            "flow_id text,"
            "aes_key text,"
            "last_seen integer not null default 0"
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

void DatabaseService::loadUsers()
{
    if (!m_db.isOpen()) {
        return;
    }
    m_flowKeys.clear();
    m_flowUid.clear();

    QSqlQuery query(m_db);
    if (!query.exec(QStringLiteral("select uid, flow_id, aes_key from users"))) {
        return;
    }
    while (query.next()) {
        const quint64 uid = query.value(0).toULongLong();
        const QString flowId = query.value(1).toString();
        const QByteArray key = QByteArray::fromHex(query.value(2).toString().toLatin1());
        if (uid == 0 || flowId.isEmpty()) {
            continue;
        }
        m_flowUid.insert(flowId, uid);
        if (key.size() == 16) {
            m_flowKeys.insert(flowId, key);
        }
        ensureUserTables(uid);
    }
}

void DatabaseService::ensureUserTables(quint64 uid)
{
    if (!m_db.isOpen() || uid == 0) {
        return;
    }
    QSqlQuery query(m_db);
    const QStringList statements = {
        QStringLiteral(
            "create table if not exists %1 ("
            "id integer primary key not null,"
            "data text not null"
            ")").arg(petTableName(uid)),
        QStringLiteral(
            "create table if not exists %1 ("
            "id integer primary key not null,"
            "data text not null default '[]'"
            ")").arg(boxTableName(uid)),
    };
    for (const QString &statement : statements) {
        if (!query.exec(statement)) {
            emit errorOccurred(QStringLiteral("初始化用户业务表失败: %1").arg(query.lastError().text()));
        }
    }
}

void DatabaseService::rememberFlowKey(const QString &flowId, const QByteArray &key)
{
    if (flowId.isEmpty() || key.size() != 16) {
        return;
    }
    m_flowKeys.insert(flowId, key);
}

void DatabaseService::registerUserLogin(const QString &flowId, quint64 uid, const QString &nameBase64, quint32 avatar)
{
    if (!m_db.isOpen() || flowId.isEmpty() || uid == 0) {
        return;
    }
    m_flowUid.insert(flowId, uid);
    ensureUserTables(uid);

    const QByteArray key = m_flowKeys.value(flowId);
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "insert into users(uid, name, avatar, flow_id, aes_key, last_seen) "
        "values(:uid, :name, :avatar, :flow_id, :aes_key, :last_seen) "
        "on conflict(uid) do update set "
        "name=excluded.name,"
        "avatar=excluded.avatar,"
        "flow_id=excluded.flow_id,"
        "aes_key=excluded.aes_key,"
        "last_seen=excluded.last_seen"));
    query.bindValue(QStringLiteral(":uid"), uid);
    query.bindValue(QStringLiteral(":name"), nonNullString(nameBase64));
    query.bindValue(QStringLiteral(":avatar"), avatar);
    query.bindValue(QStringLiteral(":flow_id"), flowId);
    query.bindValue(QStringLiteral(":aes_key"), key.isEmpty() ? QString() : QString::fromLatin1(key.toHex()));
    query.bindValue(QStringLiteral(":last_seen"), QDateTime::currentSecsSinceEpoch());
    if (!query.exec()) {
        emit errorOccurred(QStringLiteral("保存用户信息失败: %1").arg(query.lastError().text()));
        return;
    }
    emit usersChanged();
}

void DatabaseService::handlePetInfoEvent(const AppEvent &event)
{
    if (!m_db.isOpen()) {
        return;
    }
    const quint64 uid = event.uid;
    ensureUserTables(uid);
    if (event.type == EventType::PetInfoReload) {
        const int pageNo = event.payload.value(QStringLiteral("page_no")).toInt(1);
        if (pageNo == 1) {
            QSqlQuery clear(m_db);
            if (!clear.exec(QStringLiteral("delete from %1").arg(petTableName(uid)))) {
                emit errorOccurred(QStringLiteral("清空宠物信息失败: %1").arg(clear.lastError().text()));
                return;
            }
        }
        const QJsonArray data = event.payload.value(QStringLiteral("data")).toArray();
        for (const QJsonValue &value : data) {
            const QJsonObject pet = value.toObject();
            const int petId = pet.value(QStringLiteral("gid")).toInt();
            if (petId > 0) {
                savePetInfo(uid, petId, pet);
            }
        }
        return;
    }
    if (event.type == EventType::PetInfoChanged) {
        const int petId = event.payload.value(QStringLiteral("id")).toInt();
        const QJsonObject data = event.payload.value(QStringLiteral("data")).toObject();
        if (petId > 0 && !data.isEmpty()) {
            savePetInfo(uid, petId, data);
        }
        return;
    }
    if (event.type == EventType::PetInfoDeleted) {
        const QJsonArray ids = event.payload.value(QStringLiteral("ids")).toArray();
        QSqlQuery query(m_db);
        query.prepare(QStringLiteral("delete from %1 where id = :id").arg(petTableName(uid)));
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
    const quint64 uid = event.uid;
    ensureUserTables(uid);
    if (event.type == EventType::BoxInfoReload) {
        replaceBoxes(uid, event.payload.value(QStringLiteral("boxes")).toArray());
        return;
    }
    if (event.type == EventType::BoxInfoChanged) {
        changeBoxSlot(
            uid,
            event.payload.value(QStringLiteral("id")).toInt(),
            event.payload.value(QStringLiteral("pos")).toInt(),
            event.payload.value(QStringLiteral("pet_gid")).toInt());
        return;
    }
    if (event.type == EventType::BoxInfoBoxReplaced) {
        replaceBox(
            uid,
            event.payload.value(QStringLiteral("id")).toInt(),
            event.payload.value(QStringLiteral("data")).toArray());
    }
}

void DatabaseService::savePetInfo(quint64 uid, int petId, const QJsonObject &data)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "insert into %1(id, data) values(:id, :data) "
        "on conflict(id) do update set data = excluded.data").arg(petTableName(uid)));
    query.bindValue(QStringLiteral(":id"), petId);
    query.bindValue(QStringLiteral(":data"), QString::fromUtf8(QJsonDocument(data).toJson(QJsonDocument::Compact)));
    if (!query.exec()) {
        emit errorOccurred(QStringLiteral("保存宠物信息失败: %1").arg(query.lastError().text()));
    }
}

void DatabaseService::replaceBoxes(quint64 uid, const QJsonArray &boxes)
{
    QSqlQuery clear(m_db);
    if (!clear.exec(QStringLiteral("delete from %1").arg(boxTableName(uid)))) {
        emit errorOccurred(QStringLiteral("清空仓库信息失败: %1").arg(clear.lastError().text()));
        return;
    }

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("insert into %1(id, data) values(:id, :data)").arg(boxTableName(uid)));
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

void DatabaseService::replaceBox(quint64 uid, int boxId, const QJsonArray &data)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "insert into %1(id, data) values(:id, :data) "
        "on conflict(id) do update set data = excluded.data").arg(boxTableName(uid)));
    query.bindValue(QStringLiteral(":id"), boxId);
    query.bindValue(QStringLiteral(":data"), QString::fromUtf8(QJsonDocument(data).toJson(QJsonDocument::Compact)));
    if (!query.exec()) {
        emit errorOccurred(QStringLiteral("保存仓库信息失败: %1").arg(query.lastError().text()));
    }
}

void DatabaseService::changeBoxSlot(quint64 uid, int boxId, int pos, int value)
{
    if (pos < 0 || pos >= 30) {
        return;
    }

    QJsonArray data;
    QSqlQuery select(m_db);
    select.prepare(QStringLiteral("select data from %1 where id = :id").arg(boxTableName(uid)));
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
        "insert into %1(id, data) values(:id, :data) "
        "on conflict(id) do update set data = excluded.data").arg(boxTableName(uid)));
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
