#pragma once

#include <QJsonObject>
#include <QMap>
#include <QMetaType>
#include <QPointF>
#include <QString>
#include <QVector>
#include <QVector3D>

namespace app {

constexpr double MapIconScale = 0.5;
constexpr const char *DefaultMarkerType = "fruit";
constexpr const char *DefaultMapBackgroundColor = "#E9DED1";

struct GamePoint2D {
    double x = 0.0;
    double y = 0.0;
};

struct CoordinateTransform {
    double scaleX = 1.0;
    double offsetX = 0.0;
    double scaleY = 1.0;
    double offsetY = 0.0;

    QPointF gameToMap(double gameX, double gameY) const;
    QPointF mapToGame(double mapX, double mapY) const;
};

struct MapImageConfig {
    QString path;
    double x = 0.0;
    double y = 0.0;
};

struct MapTileConfig {
    int width = 2048;
    int height = 2048;
    int cols = 4;
    int rows = 4;
};

struct MapSpaceConfig {
    QVector<QVector3D> vertices;
};

struct MapAreaConfig {
    QString id;
    QString name;
    QVector<MapSpaceConfig> spaces;
};

struct MapLayerConfig {
    QString id;
    QString name;
    int order = 0;
    double maskOpacity = 0.85;
    int defaultZ = 0;
    QVector<MapImageConfig> images;
    QVector<MapAreaConfig> areas;
};

struct MapConfig {
    int version = 1;
    QString id;
    QString name;
    GamePoint2D gameBoundsA;
    GamePoint2D gameBoundsB;
    CoordinateTransform transform;
    MapTileConfig tile;
    QString topLayerId = QStringLiteral("g");
    QString backgroundColor = QString::fromLatin1(DefaultMapBackgroundColor);
    QVector<MapLayerConfig> layers;
    QString rootPath;

    int width() const;
    int height() const;
    const MapLayerConfig *topLayer() const;
    const MapLayerConfig *layerById(const QString &layerId) const;
    bool containsGameXY(double gameX, double gameY) const;
    QString resolvePath(const QString &relativePath) const;
};

struct ResolvedMapLocation {
    QString mapId;
    QString mapName;
    QString layerId;
    QString layerName;
    QString areaId;
    QString areaName;
    double mapX = 0.0;
    double mapY = 0.0;
};

struct MapLocation {
    QString mapId;
    QString mapName;
    QString layerId;
    QString layerName;
    QString areaId;
    QString areaName;
    double mapX = 0.0;
    double mapY = 0.0;
};

struct PlayerPositionPayload {
    bool visible = true;
    double rotation = 0.0;
    double ctrlRotation = 0.0;
    int gameX = 0;
    int gameY = 0;
    int gameZ = 0;
};

struct PlayerState {
    bool visible = true;
    double rotation = 0.0;
    double ctrlRotation = 0.0;
    int gameX = 0;
    int gameY = 0;
    int gameZ = 0;
    bool hasLocation = false;
    MapLocation location;
};

struct MarkerSubtypeConfig {
    QString key;
    bool visible = true;
};

struct MarkerTypeConfig {
    QString key;
    QString name;
    QString icon;
    bool visible = true;
    QMap<QString, MarkerSubtypeConfig> subtypes;
};

using MarkerTypeMap = QMap<QString, MarkerTypeConfig>;

struct MapMarker {
    QString id;
    QString markerType = QString::fromLatin1(DefaultMarkerType);
    QString label;
    bool visible = true;
    bool temporary = false;
    int gameX = 0;
    int gameY = 0;
    int gameZ = 0;
    bool hasLocation = false;
    MapLocation location;
    QJsonObject extra;
};

using MarkerMap = QMap<QString, MapMarker>;

struct MapState {
    QString currentMapId;
    QString currentLayerId;
    PlayerState player;
    MarkerTypeMap markerTypes;
    MarkerMap markers;
    MarkerMap temporaryMarkers;
};

constexpr int DefaultHttpPort = 4939;
constexpr const char *DefaultHttpHost = "127.0.0.1";
constexpr int MinHttpPort = 1024;
constexpr int MaxHttpPort = 65535;

struct BaseState {
    int version = 1;
    QString httpHost = QString::fromLatin1(DefaultHttpHost);
    int httpPort = DefaultHttpPort;
};

MarkerTypeMap defaultMarkerTypes();
QString normalizeMarkerType(const QString &markerType);
QString normalizeMarkerSubtype(const QJsonValue &value);
QJsonObject mapMarkerToJson(const MapMarker &marker);

} // namespace app

Q_DECLARE_METATYPE(app::GamePoint2D)
Q_DECLARE_METATYPE(app::CoordinateTransform)
Q_DECLARE_METATYPE(app::MapImageConfig)
Q_DECLARE_METATYPE(app::MapTileConfig)
Q_DECLARE_METATYPE(app::MapSpaceConfig)
Q_DECLARE_METATYPE(app::MapAreaConfig)
Q_DECLARE_METATYPE(app::MapLayerConfig)
Q_DECLARE_METATYPE(app::MapConfig)
Q_DECLARE_METATYPE(app::ResolvedMapLocation)
Q_DECLARE_METATYPE(app::MapLocation)
Q_DECLARE_METATYPE(app::PlayerPositionPayload)
Q_DECLARE_METATYPE(app::PlayerState)
Q_DECLARE_METATYPE(app::MarkerSubtypeConfig)
Q_DECLARE_METATYPE(app::MarkerTypeConfig)
Q_DECLARE_METATYPE(app::MarkerTypeMap)
Q_DECLARE_METATYPE(app::MapMarker)
Q_DECLARE_METATYPE(app::MarkerMap)
Q_DECLARE_METATYPE(app::MapState)
Q_DECLARE_METATYPE(app::BaseState)
