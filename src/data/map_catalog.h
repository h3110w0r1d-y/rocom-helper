#pragma once

#include "map_types.h"

#include <QMap>
#include <QStringList>

namespace app {

class MapCatalog {
public:
    static MapCatalog load(const QString &rootPath = QStringLiteral(":/map"));

    QString firstMapId() const;
    const MapConfig *mapById(const QString &mapId) const;
    QList<QPair<QString, QString>> mapOptions() const;
    const QStringList &orderedMapIds() const;
    bool isEmpty() const;

private:
    static bool loadMapConfig(const QString &mapId, const QString &indexPath, MapConfig *outConfig);

    QString m_rootPath;
    QStringList m_orderedMapIds;
    QMap<QString, MapConfig> m_maps;
};

class MapResolver {
public:
    explicit MapResolver(const MapCatalog *catalog = nullptr);

    void setCatalog(const MapCatalog *catalog);
    ResolvedMapLocation resolve(int gameX, int gameY, int gameZ = 0) const;
    bool resolve(int gameX, int gameY, int gameZ, ResolvedMapLocation *outLocation) const;
    QPointF mapToGame(const QString &mapId, double mapX, double mapY, bool *ok = nullptr) const;

private:
    const MapConfig *mapForGameXY(double gameX, double gameY) const;
    QPair<const MapLayerConfig *, const MapAreaConfig *> layerAndAreaForGamePoint(
        const MapConfig &config,
        double gameX,
        double gameY,
        double gameZ) const;
    static bool pointInArea(double gameX, double gameY, double gameZ, const MapAreaConfig &area);
    static bool pointInSpace(double gameX, double gameY, double gameZ, const MapSpaceConfig &space);

    const MapCatalog *m_catalog = nullptr;
};

} // namespace app
