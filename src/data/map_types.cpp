#include "map_types.h"

#include <QJsonDocument>
#include <QJsonValue>
#include <QtMath>

namespace app {

QPointF CoordinateTransform::gameToMap(double gameX, double gameY) const
{
    return QPointF(gameX * scaleX + offsetX, gameY * scaleY + offsetY);
}

QPointF CoordinateTransform::mapToGame(double mapX, double mapY) const
{
    return QPointF((mapX - offsetX) / scaleX, (mapY - offsetY) / scaleY);
}

int MapConfig::width() const
{
    return tile.width * tile.cols;
}

int MapConfig::height() const
{
    return tile.height * tile.rows;
}

const MapLayerConfig *MapConfig::topLayer() const
{
    if (const MapLayerConfig *layer = layerById(topLayerId)) {
        return layer;
    }
    return layers.isEmpty() ? nullptr : &layers.first();
}

const MapLayerConfig *MapConfig::layerById(const QString &layerId) const
{
    for (const MapLayerConfig &layer : layers) {
        if (layer.id == layerId) {
            return &layer;
        }
    }
    return nullptr;
}

bool MapConfig::containsGameXY(double gameX, double gameY) const
{
    const double minX = qMin(gameBoundsA.x, gameBoundsB.x);
    const double maxX = qMax(gameBoundsA.x, gameBoundsB.x);
    const double minY = qMin(gameBoundsA.y, gameBoundsB.y);
    const double maxY = qMax(gameBoundsA.y, gameBoundsB.y);
    return minX <= gameX && gameX <= maxX && minY <= gameY && gameY <= maxY;
}

QString MapConfig::resolvePath(const QString &relativePath) const
{
    if (relativePath.startsWith(QLatin1Char(':')) || relativePath.startsWith(QLatin1Char('/'))) {
        return relativePath;
    }
    return rootPath + QLatin1Char('/') + relativePath;
}

MarkerTypeMap defaultMarkerTypes()
{
    const QList<QPair<QString, QString>> entries = {
        {QStringLiteral("fruit"), QStringLiteral("果子")},
        {QStringLiteral("chest"), QStringLiteral("宝箱")},
        {QStringLiteral("plant"), QStringLiteral("花朵")},
        {QStringLiteral("ore"), QStringLiteral("矿石")},
        {QStringLiteral("star"), QStringLiteral("星星")},
        {QStringLiteral("task"), QStringLiteral("任务")},
        {QStringLiteral("shining"), QStringLiteral("异色")},
        {QStringLiteral("glass"), QStringLiteral("炫彩")},
    };

    MarkerTypeMap result;
    for (const auto &entry : entries) {
        MarkerTypeConfig config;
        config.key = entry.first;
        config.name = entry.second;
        config.icon = entry.first + QStringLiteral(".png");
        config.visible = true;
        result.insert(config.key, config);
    }
    return result;
}

QString normalizeMarkerType(const QString &markerType)
{
    const QString key = markerType.trimmed();
    const MarkerTypeMap defaults = defaultMarkerTypes();
    return defaults.contains(key) ? key : QString::fromLatin1(DefaultMarkerType);
}

QString normalizeMarkerSubtype(const QJsonValue &value)
{
    if (value.isUndefined() || value.isNull()) {
        return {};
    }
    if (value.isString()) {
        return value.toString().trimmed();
    }
    if (value.isDouble()) {
        return QString::number(value.toInt());
    }
    return QString::fromUtf8(QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact)).trimmed();
}

QJsonObject mapMarkerToJson(const MapMarker &marker)
{
    QJsonObject location;
    if (marker.hasLocation) {
        location.insert(QStringLiteral("map_id"), marker.location.mapId);
        location.insert(QStringLiteral("map_name"), marker.location.mapName);
        location.insert(QStringLiteral("layer_id"), marker.location.layerId);
        location.insert(QStringLiteral("layer_name"), marker.location.layerName);
        if (!marker.location.areaId.isEmpty()) {
            location.insert(QStringLiteral("area_id"), marker.location.areaId);
        }
        if (!marker.location.areaName.isEmpty()) {
            location.insert(QStringLiteral("area_name"), marker.location.areaName);
        }
        location.insert(QStringLiteral("map_x"), marker.location.mapX);
        location.insert(QStringLiteral("map_y"), marker.location.mapY);
    }

    QJsonObject object{
        {QStringLiteral("id"), marker.id},
        {QStringLiteral("marker_type"), marker.markerType},
        {QStringLiteral("label"), marker.label},
        {QStringLiteral("visible"), marker.visible},
        {QStringLiteral("temporary"), marker.temporary},
        {QStringLiteral("game_x"), marker.gameX},
        {QStringLiteral("game_y"), marker.gameY},
        {QStringLiteral("game_z"), marker.gameZ},
        {QStringLiteral("extra"), marker.extra},
    };
    if (marker.hasLocation) {
        object.insert(QStringLiteral("location"), location);
    }
    return object;
}

} // namespace app
