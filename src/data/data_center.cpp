#include "data_center.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QSet>
#include <QTextStream>
#include <QUuid>

namespace app {
namespace {

QJsonValue valueForKeys(const QJsonObject &object, std::initializer_list<const char *> keys)
{
    for (const char *key : keys) {
        const QJsonValue value = object.value(QString::fromLatin1(key));
        if (!value.isUndefined()) {
            return value;
        }
    }
    return {};
}

} // namespace

DataCenter::DataCenter(QObject *parent)
    : QObject(parent)
    , m_catalog(MapCatalog::load())
    , m_resolver(&m_catalog)
    , m_mapState(emptyMapState())
{
    qRegisterMetaType<app::BaseState>("app::BaseState");
    qRegisterMetaType<app::MapState>("app::MapState");
    qRegisterMetaType<app::PlayerPositionPayload>("app::PlayerPositionPayload");
    qRegisterMetaType<app::PlayerState>("app::PlayerState");
    qRegisterMetaType<app::MapMarker>("app::MapMarker");
    qRegisterMetaType<app::MarkerTypeMap>("app::MarkerTypeMap");
}

DataCenter::~DataCenter()
{
    close();
}

void DataCenter::load()
{
    loadBaseState();
    m_mapState = emptyMapState();
    emit baseStateLoaded(m_baseState);
    emit stateLoaded(m_mapState);
}

void DataCenter::loadPersistentMarkers(const QJsonArray &rows)
{
    m_mapState.markers.clear();
    for (const QJsonValue &value : rows) {
        const QJsonObject row = value.toObject();
        MapMarker marker;
        marker.id = row.value(QStringLiteral("id")).toString();
        marker.markerType = normalizeMarkerType(row.value(QStringLiteral("marker_type")).toString());
        marker.label = row.value(QStringLiteral("label")).toString();
        marker.visible = row.value(QStringLiteral("visible")).toBool(true);
        marker.temporary = false;
        marker.gameX = intValue(row, QStringLiteral("game_x"));
        marker.gameY = intValue(row, QStringLiteral("game_y"));
        marker.gameZ = intValue(row, QStringLiteral("game_z"));
        marker.extra = row.value(QStringLiteral("extra")).toObject();
        if (marker.extra.isEmpty() && row.value(QStringLiteral("extra_json")).isString()) {
            const QJsonDocument extraDoc = QJsonDocument::fromJson(row.value(QStringLiteral("extra_json")).toString().toUtf8());
            if (extraDoc.isObject()) {
                marker.extra = extraDoc.object();
            }
        }
        marker.hasLocation = resolveLocation(marker.gameX, marker.gameY, marker.gameZ, &marker.location);
        if (!marker.id.isEmpty()) {
            m_mapState.markers.insert(marker.id, marker);
        }
    }

    if (syncMarkerSubtypes()) {
        emit markerTypesChanged(m_mapState.markerTypes);
    }
    emit stateLoaded(m_mapState);
}

void DataCenter::save()
{
    QDir().mkpath(QFileInfo(baseConfigPath()).absolutePath());
    QJsonObject object{
        {QStringLiteral("version"), m_baseState.version},
        {QStringLiteral("http_host"), m_baseState.httpHost},
        {QStringLiteral("http_port"), m_baseState.httpPort},
        {QStringLiteral("selected_uid"), QString::number(m_baseState.selectedUid)},
    };
    QFile file(baseConfigPath());
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
        m_baseDirty = false;
    }
}

void DataCenter::saveIfDirty()
{
    if (m_baseDirty) {
        save();
    }
}

void DataCenter::close()
{
    saveIfDirty();
}

const MapCatalog &DataCenter::catalog() const
{
    return m_catalog;
}

const MapResolver &DataCenter::resolver() const
{
    return m_resolver;
}

BaseState DataCenter::baseSnapshot() const
{
    return m_baseState;
}

MapState DataCenter::snapshot() const
{
    return m_mapState;
}

QList<QPair<QString, QString>> DataCenter::mapOptions() const
{
    return m_catalog.mapOptions();
}

QVector<MapLayerConfig> DataCenter::layersForMap(const QString &mapId) const
{
    const QString targetMapId = mapId.isEmpty() ? m_mapState.currentMapId : mapId;
    const MapConfig *config = m_catalog.mapById(targetMapId);
    return config == nullptr ? QVector<MapLayerConfig>() : config->layers;
}

void DataCenter::setFollowPlayerMap(bool enabled)
{
    m_followPlayerMap = enabled;
}

void DataCenter::setSelectedUid(quint64 uid)
{
    m_selectedUid = uid;
}

void DataCenter::setCurrentMap(const QString &mapId)
{
    const MapConfig *config = m_catalog.mapById(mapId);
    if (config == nullptr || m_mapState.currentMapId == config->id) {
        return;
    }

    const MapLayerConfig *topLayer = config->topLayer();
    m_mapState.currentMapId = config->id;
    m_mapState.currentLayerId = topLayer != nullptr ? topLayer->id : QString();
    emit mapChanged(m_mapState.currentMapId);
    emit layerChanged(m_mapState.currentLayerId);
}

void DataCenter::setCurrentLayer(const QString &layerId)
{
    const MapConfig *config = m_catalog.mapById(m_mapState.currentMapId);
    if (config == nullptr || config->layerById(layerId) == nullptr || m_mapState.currentLayerId == layerId) {
        return;
    }
    m_mapState.currentLayerId = layerId;
    emit layerChanged(layerId);
}

void DataCenter::setMarkerTypeVisible(const QString &markerType, bool visible)
{
    const QString key = normalizeMarkerType(markerType);
    auto it = m_mapState.markerTypes.find(key);
    if (it == m_mapState.markerTypes.end()) {
        return;
    }
    it->visible = visible;
    emit markerTypeVisibilityChanged(key, visible);
    emit markerTypesChanged(m_mapState.markerTypes);
}

void DataCenter::setMarkerSubtypeVisible(const QString &markerType, const QString &subtype, bool visible)
{
    const QString key = normalizeMarkerType(markerType);
    auto it = m_mapState.markerTypes.find(key);
    if (it == m_mapState.markerTypes.end()) {
        return;
    }
    const QString subtypeKey = subtype.trimmed();
    MarkerSubtypeConfig config = it->subtypes.value(subtypeKey);
    config.key = subtypeKey;
    config.visible = visible;
    it->subtypes.insert(subtypeKey, config);
    emit markerTypesChanged(m_mapState.markerTypes);
}

MapMarker DataCenter::createMarker(
    const QString &markerType,
    const QString &label,
    const QString &markerId,
    const QJsonObject &extra,
    int gameX,
    int gameY,
    int gameZ,
    bool visible,
    bool temporary)
{
    const QString id = markerId.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : markerId;
    MapMarker marker;
    marker.id = id;
    marker.markerType = normalizeMarkerType(markerType);
    marker.label = label.isNull() ? QStringLiteral("") : label;
    marker.visible = visible;
    marker.temporary = temporary;
    marker.gameX = gameX;
    marker.gameY = gameY;
    marker.gameZ = gameZ;
    marker.extra = extra;
    marker.hasLocation = resolveLocation(gameX, gameY, gameZ, &marker.location);

    MarkerMap &target = temporary ? m_mapState.temporaryMarkers : m_mapState.markers;
    const bool existed = target.contains(id);
    target.insert(id, marker);
    const bool subtypesChanged = syncMarkerSubtypes();
    if (subtypesChanged) {
        emit markerTypesChanged(m_mapState.markerTypes);
    }
    if (existed) {
        emit markerUpdated(marker);
    } else {
        emit markerAdded(marker);
    }
    return marker;
}

MapMarker DataCenter::updateMarker(
    const QString &markerId,
    bool hasGameX,
    int gameX,
    bool hasGameY,
    int gameY,
    bool hasGameZ,
    int gameZ,
    const QString &markerType,
    const QString &label,
    bool hasVisible,
    bool visible,
    const QJsonObject &extra)
{
    MapMarker *marker = findMarker(markerId);
    if (marker == nullptr) {
        return {};
    }
    if (hasGameX) {
        marker->gameX = gameX;
    }
    if (hasGameY) {
        marker->gameY = gameY;
    }
    if (hasGameZ) {
        marker->gameZ = gameZ;
    }
    if (hasGameX || hasGameY || hasGameZ) {
        marker->hasLocation = resolveLocation(marker->gameX, marker->gameY, marker->gameZ, &marker->location);
    }
    if (!markerType.isNull()) {
        marker->markerType = normalizeMarkerType(markerType);
    }
    if (!label.isNull()) {
        marker->label = label.isNull() ? QStringLiteral("") : label;
    }
    if (hasVisible) {
        marker->visible = visible;
    }
    for (auto it = extra.begin(); it != extra.end(); ++it) {
        marker->extra.insert(it.key(), it.value());
    }

    const MapMarker snapshot = *marker;
    const bool subtypesChanged = syncMarkerSubtypes();
    if (subtypesChanged) {
        emit markerTypesChanged(m_mapState.markerTypes);
    }
    emit markerUpdated(snapshot);
    return snapshot;
}

bool DataCenter::deleteMarker(const QString &markerId)
{
    if (m_mapState.temporaryMarkers.remove(markerId) <= 0 && m_mapState.markers.remove(markerId) <= 0) {
        return false;
    }
    const bool subtypesChanged = syncMarkerSubtypes();
    if (subtypesChanged) {
        emit markerTypesChanged(m_mapState.markerTypes);
    }
    emit markerRemoved(markerId);
    return true;
}

void DataCenter::clearTemporaryMarkers()
{
    const QStringList removedIds = m_mapState.temporaryMarkers.keys();
    m_mapState.temporaryMarkers.clear();
    const bool subtypesChanged = syncMarkerSubtypes();
    if (subtypesChanged) {
        emit markerTypesChanged(m_mapState.markerTypes);
    }
    for (const QString &markerId : removedIds) {
        emit markerRemoved(markerId);
    }
}

void DataCenter::setMarkerVisible(const QString &markerId, bool visible)
{
    QString normalizedId = markerId;
    MapMarker *marker = findMarker(normalizedId);
    if (marker == nullptr) {
        for (MapMarker &candidate : m_mapState.temporaryMarkers) {
            if (candidate.extra.value(QStringLiteral("actor_id")).toVariant().toString() == markerId) {
                marker = &candidate;
                normalizedId = candidate.id;
                break;
            }
        }
    }
    if (marker == nullptr) {
        for (MapMarker &candidate : m_mapState.markers) {
            if (candidate.extra.value(QStringLiteral("actor_id")).toVariant().toString() == markerId) {
                marker = &candidate;
                normalizedId = candidate.id;
                break;
            }
        }
    }
    if (marker == nullptr) {
        return;
    }
    marker->visible = visible;
    emit markerUpdated(*marker);
    emit markerVisibilityChanged(normalizedId, visible);
}

PlayerState DataCenter::updatePlayer(
    bool visible,
    double rotation,
    double ctrlRotation,
    int gameX,
    int gameY,
    int gameZ)
{
    PlayerState player;
    player.visible = visible;
    player.rotation = rotation;
    player.ctrlRotation = ctrlRotation;
    player.gameX = gameX;
    player.gameY = gameY;
    player.gameZ = gameZ;
    player.hasLocation = resolveLocation(gameX, gameY, gameZ, &player.location);

    QString mapToEmit;
    QString layerToEmit;
    m_mapState.player = player;
    if (m_followPlayerMap && player.hasLocation) {
        if (m_mapState.currentMapId != player.location.mapId) {
            m_mapState.currentMapId = player.location.mapId;
            mapToEmit = player.location.mapId;
        }
        if (m_mapState.currentLayerId != player.location.layerId) {
            m_mapState.currentLayerId = player.location.layerId;
            layerToEmit = player.location.layerId;
        }
    }

    if (!mapToEmit.isEmpty()) {
        emit mapChanged(mapToEmit);
    }
    if (!layerToEmit.isEmpty()) {
        emit layerChanged(layerToEmit);
    }
    emit playerChanged(player);
    return player;
}

void DataCenter::handleEvent(const AppEvent &event)
{
    switch (event.type) {
    case EventType::PlayerPositionChanged:
        if (m_selectedUid != 0 && event.uid != m_selectedUid) {
            break;
        }
        if (event.playerPosition.has_value()) {
            const PlayerPositionPayload &position = *event.playerPosition;
            updatePlayer(
                position.visible,
                position.rotation,
                position.ctrlRotation,
                position.gameX,
                position.gameY,
                position.gameZ);
        }
        break;
    case EventType::MapMarkerAdded:
    case EventType::MapMarkerMoved:
    case EventType::MapMarkerUpdated:
    case EventType::MapMarkerDeleted:
    case EventType::MapMarkerVisibilityChanged:
    case EventType::MapMarkerTypeVisibilityChanged:
        applyMarkerEvent(event);
        break;
    default:
        break;
    }
}

MapState DataCenter::emptyMapState() const
{
    MapState state;
    state.currentMapId = m_catalog.firstMapId();
    if (const MapConfig *config = m_catalog.mapById(state.currentMapId)) {
        if (const MapLayerConfig *layer = config->topLayer()) {
            state.currentLayerId = layer->id;
        }
    }
    state.markerTypes = defaultMarkerTypes();
    return state;
}

void DataCenter::loadBaseState()
{
    QFile file(baseConfigPath());
    if (!file.open(QIODevice::ReadOnly)) {
        m_baseState = {};
        m_baseDirty = false;
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        m_baseState = {};
        m_baseDirty = false;
        return;
    }
    const QJsonObject object = doc.object();
    m_baseState.version = object.value(QStringLiteral("version")).toInt(1);
    m_baseState.httpHost = object.value(QStringLiteral("http_host")).toString(QString::fromLatin1(DefaultHttpHost)).trimmed();
    if (m_baseState.httpHost.isEmpty()) {
        m_baseState.httpHost = QString::fromLatin1(DefaultHttpHost);
    }
    m_baseState.httpPort = object.value(QStringLiteral("http_port")).toInt(DefaultHttpPort);
    if (m_baseState.httpPort < MinHttpPort || m_baseState.httpPort > MaxHttpPort) {
        m_baseState.httpPort = DefaultHttpPort;
    }
    bool uidOk = false;
    m_baseState.selectedUid = object.value(QStringLiteral("selected_uid")).toVariant().toULongLong(&uidOk);
    if (!uidOk) {
        m_baseState.selectedUid = 0;
    }
    m_baseDirty = false;
}

void DataCenter::setHttpHost(const QString &host)
{
    QString normalized = host.trimmed();
    if (normalized.isEmpty()) {
        normalized = QString::fromLatin1(DefaultHttpHost);
    }
    if (m_baseState.httpHost == normalized) {
        return;
    }
    m_baseState.httpHost = normalized;
    markBaseDirty();
    emit baseStateChanged(m_baseState);
}

void DataCenter::setHttpPort(int port)
{
    if (port < MinHttpPort || port > MaxHttpPort) {
        port = DefaultHttpPort;
    }
    if (m_baseState.httpPort == port) {
        return;
    }
    m_baseState.httpPort = port;
    markBaseDirty();
    emit baseStateChanged(m_baseState);
}

void DataCenter::setHttpEndpoint(const QString &host, int port)
{
    bool changed = false;
    QString normalizedHost = host.trimmed();
    if (normalizedHost.isEmpty()) {
        normalizedHost = QString::fromLatin1(DefaultHttpHost);
    }
    if (port < MinHttpPort || port > MaxHttpPort) {
        port = DefaultHttpPort;
    }
    if (m_baseState.httpHost != normalizedHost) {
        m_baseState.httpHost = normalizedHost;
        changed = true;
    }
    if (m_baseState.httpPort != port) {
        m_baseState.httpPort = port;
        changed = true;
    }
    if (!changed) {
        return;
    }
    markBaseDirty();
    emit baseStateChanged(m_baseState);
}

void DataCenter::setDefaultUid(quint64 uid)
{
    if (m_baseState.selectedUid == uid) {
        return;
    }
    m_baseState.selectedUid = uid;
    markBaseDirty();
    emit baseStateChanged(m_baseState);
}

QString DataCenter::baseConfigPath() const
{
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dataDir.isEmpty()) {
        dataDir = QCoreApplication::applicationDirPath();
    }
    return QDir(dataDir).filePath(QStringLiteral("base_config.json"));
}

void DataCenter::markBaseDirty()
{
    m_baseDirty = true;
}

MapMarker *DataCenter::findMarker(const QString &markerId, bool *temporary)
{
    auto tempIt = m_mapState.temporaryMarkers.find(markerId);
    if (tempIt != m_mapState.temporaryMarkers.end()) {
        if (temporary != nullptr) {
            *temporary = true;
        }
        return &tempIt.value();
    }
    auto it = m_mapState.markers.find(markerId);
    if (it == m_mapState.markers.end()) {
        return nullptr;
    }
    if (temporary != nullptr) {
        *temporary = false;
    }
    return &it.value();
}

const MapMarker *DataCenter::findMarker(const QString &markerId, bool *temporary) const
{
    auto tempIt = m_mapState.temporaryMarkers.constFind(markerId);
    if (tempIt != m_mapState.temporaryMarkers.constEnd()) {
        if (temporary != nullptr) {
            *temporary = true;
        }
        return &tempIt.value();
    }
    auto it = m_mapState.markers.constFind(markerId);
    if (it == m_mapState.markers.constEnd()) {
        return nullptr;
    }
    if (temporary != nullptr) {
        *temporary = false;
    }
    return &it.value();
}

MapLocation DataCenter::locationFromResolved(const ResolvedMapLocation &resolved) const
{
    MapLocation location;
    location.mapId = resolved.mapId;
    location.mapName = resolved.mapName;
    location.layerId = resolved.layerId;
    location.layerName = resolved.layerName;
    location.areaId = resolved.areaId;
    location.areaName = resolved.areaName;
    location.mapX = resolved.mapX;
    location.mapY = resolved.mapY;
    return location;
}

bool DataCenter::resolveLocation(int gameX, int gameY, int gameZ, MapLocation *outLocation) const
{
    ResolvedMapLocation resolved;
    if (!m_resolver.resolve(gameX, gameY, gameZ, &resolved)) {
        return false;
    }
    if (outLocation != nullptr) {
        *outLocation = locationFromResolved(resolved);
    }
    return true;
}

bool DataCenter::syncMarkerSubtypes()
{
    QMap<QString, QSet<QString>> usedSubtypes;
    for (const QString &key : m_mapState.markerTypes.keys()) {
        usedSubtypes.insert(key, {});
    }

    auto collect = [&usedSubtypes](const MarkerMap &markers) {
        for (const MapMarker &marker : markers) {
            usedSubtypes[marker.markerType].insert(normalizeMarkerSubtype(marker.extra.value(QStringLiteral("type"))));
        }
    };
    collect(m_mapState.markers);
    collect(m_mapState.temporaryMarkers);

    bool changed = false;
    for (auto it = m_mapState.markerTypes.begin(); it != m_mapState.markerTypes.end(); ++it) {
        QMap<QString, MarkerSubtypeConfig> nextSubtypes;
        QStringList subtypeKeys = usedSubtypes.value(it.key()).values();
        subtypeKeys.sort();
        for (const QString &subtype : subtypeKeys) {
            MarkerSubtypeConfig config = it->subtypes.value(subtype);
            config.key = subtype;
            nextSubtypes.insert(subtype, config);
        }
        if (it->subtypes.keys() != nextSubtypes.keys()) {
            changed = true;
        }
        it->subtypes = nextSubtypes;
    }
    return changed;
}

void DataCenter::applyMarkerEvent(const AppEvent &event)
{
    const QJsonObject payload = event.payload;
    const QString id = valueForKeys(payload, {"id", "marker_id"}).toVariant().toString();
    if (event.type == EventType::MapMarkerDeleted) {
        deleteMarker(id);
        return;
    }
    if (event.type == EventType::MapMarkerTypeVisibilityChanged) {
        setMarkerTypeVisible(payload.value(QStringLiteral("marker_type")).toString(), boolValue(payload, QStringLiteral("visible"), true));
        return;
    }
    if (event.type == EventType::MapMarkerVisibilityChanged) {
        setMarkerVisible(id, boolValue(payload, QStringLiteral("visible"), true));
        return;
    }

    const bool hasGameX = payload.contains(QStringLiteral("game_x"));
    const bool hasGameY = payload.contains(QStringLiteral("game_y"));
    const bool hasGameZ = payload.contains(QStringLiteral("game_z"));
    if (event.type == EventType::MapMarkerAdded) {
        createMarker(
            payload.value(QStringLiteral("marker_type")).toString(QString::fromLatin1(DefaultMarkerType)),
            payload.value(QStringLiteral("label")).toString(),
            id,
            payload.value(QStringLiteral("extra")).toObject(),
            intValue(payload, QStringLiteral("game_x")),
            intValue(payload, QStringLiteral("game_y")),
            intValue(payload, QStringLiteral("game_z")),
            boolValue(payload, QStringLiteral("visible"), true),
            boolValue(payload, QStringLiteral("temporary"), false));
        return;
    }

    updateMarker(
        id,
        hasGameX,
        intValue(payload, QStringLiteral("game_x")),
        hasGameY,
        intValue(payload, QStringLiteral("game_y")),
        hasGameZ,
        intValue(payload, QStringLiteral("game_z")),
        payload.contains(QStringLiteral("marker_type")) ? payload.value(QStringLiteral("marker_type")).toString() : QString(),
        payload.contains(QStringLiteral("label")) ? payload.value(QStringLiteral("label")).toString() : QString(),
        payload.contains(QStringLiteral("visible")),
        boolValue(payload, QStringLiteral("visible"), true),
        payload.value(QStringLiteral("extra")).toObject());
}

int DataCenter::intValue(const QJsonObject &object, const QString &key, int defaultValue)
{
    const QJsonValue value = object.value(key);
    if (value.isDouble()) {
        return qRound(value.toDouble());
    }
    if (value.isString()) {
        bool ok = false;
        const double number = value.toString().toDouble(&ok);
        return ok ? qRound(number) : defaultValue;
    }
    return defaultValue;
}

bool DataCenter::boolValue(const QJsonObject &object, const QString &key, bool defaultValue)
{
    const QJsonValue value = object.value(key);
    if (value.isBool()) {
        return value.toBool();
    }
    if (value.isDouble()) {
        return !qFuzzyIsNull(value.toDouble());
    }
    if (value.isString()) {
        const QString text = value.toString().trimmed().toLower();
        if (text == QStringLiteral("true") || text == QStringLiteral("1")) {
            return true;
        }
        if (text == QStringLiteral("false") || text == QStringLiteral("0")) {
            return false;
        }
    }
    return defaultValue;
}

} // namespace app
