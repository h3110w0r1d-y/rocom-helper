#include "map_catalog.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtMath>

#include <algorithm>

namespace app {
namespace {

GamePoint2D readPoint2D(const QJsonObject &object)
{
    return {object.value(QStringLiteral("x")).toDouble(), object.value(QStringLiteral("y")).toDouble()};
}

MapImageConfig readImage(const QJsonObject &object)
{
    MapImageConfig image;
    image.path = object.value(QStringLiteral("path")).toString();
    image.x = object.value(QStringLiteral("x")).toDouble();
    image.y = object.value(QStringLiteral("y")).toDouble();
    return image;
}

MapSpaceConfig readSpace(const QJsonObject &object)
{
    MapSpaceConfig space;
    const QJsonArray vertices = object.value(QStringLiteral("vertices")).toArray();
    for (const QJsonValue &value : vertices) {
        const QJsonArray vertex = value.toArray();
        if (vertex.size() < 3) {
            continue;
        }
        space.vertices.append(QVector3D(
            static_cast<float>(vertex.at(0).toDouble()),
            static_cast<float>(vertex.at(1).toDouble()),
            static_cast<float>(vertex.at(2).toDouble())));
    }
    return space;
}

MapAreaConfig readArea(const QJsonObject &object)
{
    MapAreaConfig area;
    area.id = object.value(QStringLiteral("id")).toString();
    area.name = object.value(QStringLiteral("name")).toString();
    const QJsonArray spaces = object.value(QStringLiteral("spaces")).toArray();
    for (const QJsonValue &value : spaces) {
        MapSpaceConfig space = readSpace(value.toObject());
        if (!space.vertices.isEmpty()) {
            area.spaces.append(space);
        }
    }
    return area;
}

MapLayerConfig readLayer(const QJsonObject &object)
{
    MapLayerConfig layer;
    layer.id = object.value(QStringLiteral("id")).toString();
    layer.name = object.value(QStringLiteral("name")).toString();
    layer.order = object.value(QStringLiteral("order")).toInt();
    layer.maskOpacity = object.value(QStringLiteral("mask_opacity")).toDouble(0.85);
    layer.defaultZ = object.value(QStringLiteral("default_z")).toInt();

    const QJsonArray images = object.value(QStringLiteral("images")).toArray();
    for (const QJsonValue &value : images) {
        layer.images.append(readImage(value.toObject()));
    }

    const QJsonArray areas = object.value(QStringLiteral("areas")).toArray();
    for (const QJsonValue &value : areas) {
        layer.areas.append(readArea(value.toObject()));
    }
    return layer;
}

} // namespace

MapCatalog MapCatalog::load(const QString &rootPath)
{
    MapCatalog catalog;
    catalog.m_rootPath = rootPath;

    QDir root(rootPath);
    const QStringList mapIds = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString &mapId : mapIds) {
        const QString indexPath = rootPath + QLatin1Char('/') + mapId + QStringLiteral("/index.json");
        MapConfig config;
        if (!loadMapConfig(mapId, indexPath, &config)) {
            continue;
        }
        catalog.m_orderedMapIds.append(config.id);
        catalog.m_maps.insert(config.id, config);
    }
    return catalog;
}

QString MapCatalog::firstMapId() const
{
    for (const QString &mapId : m_orderedMapIds) {
        if (m_maps.contains(mapId)) {
            return mapId;
        }
    }
    return m_maps.isEmpty() ? QString() : m_maps.firstKey();
}

const MapConfig *MapCatalog::mapById(const QString &mapId) const
{
    auto it = m_maps.constFind(mapId);
    return it == m_maps.constEnd() ? nullptr : &it.value();
}

QList<QPair<QString, QString>> MapCatalog::mapOptions() const
{
    QList<QPair<QString, QString>> result;
    for (const QString &mapId : m_orderedMapIds) {
        if (const MapConfig *config = mapById(mapId)) {
            result.append({config->id, config->name});
        }
    }
    return result;
}

const QStringList &MapCatalog::orderedMapIds() const
{
    return m_orderedMapIds;
}

bool MapCatalog::isEmpty() const
{
    return m_maps.isEmpty();
}

bool MapCatalog::loadMapConfig(const QString &mapId, const QString &indexPath, MapConfig *outConfig)
{
    QFile file(indexPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject() || outConfig == nullptr) {
        return false;
    }

    const QJsonObject object = doc.object();
    MapConfig config;
    config.version = object.value(QStringLiteral("version")).toInt(1);
    config.id = object.value(QStringLiteral("id")).toString(mapId);
    config.name = object.value(QStringLiteral("name")).toString(config.id);

    const QJsonArray bounds = object.value(QStringLiteral("game_bounds")).toArray();
    if (bounds.size() >= 2) {
        config.gameBoundsA = readPoint2D(bounds.at(0).toObject());
        config.gameBoundsB = readPoint2D(bounds.at(1).toObject());
    }

    const QJsonObject transform = object.value(QStringLiteral("transform")).toObject();
    config.transform.scaleX = transform.value(QStringLiteral("scale_x")).toDouble(1.0);
    config.transform.offsetX = transform.value(QStringLiteral("offset_x")).toDouble();
    config.transform.scaleY = transform.value(QStringLiteral("scale_y")).toDouble(1.0);
    config.transform.offsetY = transform.value(QStringLiteral("offset_y")).toDouble();

    const QJsonObject tile = object.value(QStringLiteral("tile")).toObject();
    config.tile.width = tile.value(QStringLiteral("width")).toInt(2048);
    config.tile.height = tile.value(QStringLiteral("height")).toInt(2048);
    config.tile.cols = tile.value(QStringLiteral("cols")).toInt(4);
    config.tile.rows = tile.value(QStringLiteral("rows")).toInt(4);
    config.topLayerId = object.value(QStringLiteral("top_layer_id")).toString(QStringLiteral("g"));
    config.backgroundColor = object.value(QStringLiteral("background_color"))
                                 .toString(QString::fromLatin1(DefaultMapBackgroundColor));
    config.rootPath = QStringLiteral(":/map/") + config.id;

    const QJsonArray layers = object.value(QStringLiteral("layers")).toArray();
    for (const QJsonValue &value : layers) {
        MapLayerConfig layer = readLayer(value.toObject());
        if (!layer.id.isEmpty()) {
            config.layers.append(layer);
        }
    }
    if (config.layers.isEmpty()) {
        MapLayerConfig top;
        top.id = config.topLayerId;
        top.name = QStringLiteral("顶层");
        config.layers.append(top);
    } else if (config.layerById(config.topLayerId) == nullptr) {
        MapLayerConfig top;
        top.id = config.topLayerId;
        top.name = QStringLiteral("顶层");
        config.layers.prepend(top);
    }

    *outConfig = config;
    return true;
}

MapResolver::MapResolver(const MapCatalog *catalog)
    : m_catalog(catalog)
{
}

void MapResolver::setCatalog(const MapCatalog *catalog)
{
    m_catalog = catalog;
}

ResolvedMapLocation MapResolver::resolve(int gameX, int gameY, int gameZ) const
{
    ResolvedMapLocation location;
    resolve(gameX, gameY, gameZ, &location);
    return location;
}

bool MapResolver::resolve(int gameX, int gameY, int gameZ, ResolvedMapLocation *outLocation) const
{
    if (m_catalog == nullptr || outLocation == nullptr) {
        return false;
    }
    const MapConfig *config = mapForGameXY(gameX, gameY);
    if (config == nullptr) {
        return false;
    }

    const QPointF mapPoint = config->transform.gameToMap(gameX, gameY);
    const auto pair = layerAndAreaForGamePoint(*config, gameX, gameY, gameZ);
    const MapLayerConfig *layer = pair.first != nullptr ? pair.first : config->topLayer();
    if (layer == nullptr) {
        return false;
    }

    outLocation->mapId = config->id;
    outLocation->mapName = config->name;
    outLocation->layerId = layer->id;
    outLocation->layerName = layer->name;
    outLocation->areaId = pair.second != nullptr ? pair.second->id : QString();
    outLocation->areaName = pair.second != nullptr ? pair.second->name : QString();
    outLocation->mapX = mapPoint.x();
    outLocation->mapY = mapPoint.y();
    return true;
}

QPointF MapResolver::mapToGame(const QString &mapId, double mapX, double mapY, bool *ok) const
{
    if (ok != nullptr) {
        *ok = false;
    }
    if (m_catalog == nullptr) {
        return {};
    }
    const MapConfig *config = m_catalog->mapById(mapId);
    if (config == nullptr) {
        return {};
    }
    if (ok != nullptr) {
        *ok = true;
    }
    return config->transform.mapToGame(mapX, mapY);
}

const MapConfig *MapResolver::mapForGameXY(double gameX, double gameY) const
{
    if (m_catalog == nullptr) {
        return nullptr;
    }
    for (const QString &mapId : m_catalog->orderedMapIds()) {
        const MapConfig *config = m_catalog->mapById(mapId);
        if (config != nullptr && config->containsGameXY(gameX, gameY)) {
            return config;
        }
    }
    return nullptr;
}

QPair<const MapLayerConfig *, const MapAreaConfig *> MapResolver::layerAndAreaForGamePoint(
    const MapConfig &config,
    double gameX,
    double gameY,
    double gameZ) const
{
    const MapLayerConfig *topLayer = config.topLayer();
    QVector<const MapLayerConfig *> floorLayers;
    for (const MapLayerConfig &layer : config.layers) {
        if (topLayer == nullptr || layer.id != topLayer->id) {
            floorLayers.append(&layer);
        }
    }
    std::sort(floorLayers.begin(), floorLayers.end(), [](const MapLayerConfig *a, const MapLayerConfig *b) {
        return a->order < b->order;
    });

    for (const MapLayerConfig *layer : floorLayers) {
        for (const MapAreaConfig &area : layer->areas) {
            if (pointInArea(gameX, gameY, gameZ, area)) {
                return {layer, &area};
            }
        }
    }
    return {topLayer, nullptr};
}

bool MapResolver::pointInArea(double gameX, double gameY, double gameZ, const MapAreaConfig &area)
{
    for (const MapSpaceConfig &space : area.spaces) {
        if (pointInSpace(gameX, gameY, gameZ, space)) {
            return true;
        }
    }
    return false;
}

bool MapResolver::pointInSpace(double gameX, double gameY, double gameZ, const MapSpaceConfig &space)
{
    if (space.vertices.isEmpty()) {
        return false;
    }

    double minX = space.vertices.first().x();
    double maxX = minX;
    double minY = space.vertices.first().y();
    double maxY = minY;
    double minZ = space.vertices.first().z();
    double maxZ = minZ;
    for (const QVector3D &vertex : space.vertices) {
        minX = qMin(minX, static_cast<double>(vertex.x()));
        maxX = qMax(maxX, static_cast<double>(vertex.x()));
        minY = qMin(minY, static_cast<double>(vertex.y()));
        maxY = qMax(maxY, static_cast<double>(vertex.y()));
        minZ = qMin(minZ, static_cast<double>(vertex.z()));
        maxZ = qMax(maxZ, static_cast<double>(vertex.z()));
    }
    return minX <= gameX && gameX <= maxX
        && minY <= gameY && gameY <= maxY
        && minZ <= gameZ && gameZ <= maxZ;
}

} // namespace app
