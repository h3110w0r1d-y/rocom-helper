#pragma once

#include "events/app_event.h"
#include "map_catalog.h"
#include "map_types.h"

#include <QFile>
#include <QJsonArray>
#include <QObject>
#include <QTimer>

namespace app {

class DataCenter : public QObject {
    Q_OBJECT

public:
    explicit DataCenter(QObject *parent = nullptr);
    ~DataCenter() override;

    void load();
    void loadPersistentMarkers(const QJsonArray &rows);
    void save();
    void saveIfDirty();
    void close();

    const MapCatalog &catalog() const;
    const MapResolver &resolver() const;

    BaseState baseSnapshot() const;
    MapState snapshot() const;

    void setHttpHost(const QString &host);
    void setHttpPort(int port);
    void setHttpEndpoint(const QString &host, int port);
    void setDefaultUid(quint64 uid);

    QList<QPair<QString, QString>> mapOptions() const;
    QVector<MapLayerConfig> layersForMap(const QString &mapId = {}) const;

    void setFollowPlayerMap(bool enabled);
    void setSelectedUid(quint64 uid);
    void setCurrentMap(const QString &mapId);
    void setCurrentLayer(const QString &layerId);
    void setMarkerTypeVisible(const QString &markerType, bool visible);
    void setMarkerSubtypeVisible(const QString &markerType, const QString &subtype, bool visible);

    MapMarker createMarker(
        const QString &markerType = QString::fromLatin1(DefaultMarkerType),
        const QString &label = {},
        const QString &markerId = {},
        const QJsonObject &extra = {},
        int gameX = 0,
        int gameY = 0,
        int gameZ = 0,
        bool visible = true,
        bool temporary = false);
    MapMarker updateMarker(
        const QString &markerId,
        bool hasGameX,
        int gameX,
        bool hasGameY,
        int gameY,
        bool hasGameZ,
        int gameZ,
        const QString &markerType = {},
        const QString &label = {},
        bool hasVisible = false,
        bool visible = true,
        const QJsonObject &extra = {});
    bool deleteMarker(const QString &markerId);
    void clearTemporaryMarkers();
    void setMarkerVisible(const QString &markerId, bool visible);
    PlayerState updatePlayer(
        bool visible,
        double rotation,
        double ctrlRotation,
        int gameX,
        int gameY,
        int gameZ);

public slots:
    void handleEvent(const app::AppEvent &event);

signals:
    void baseStateLoaded(const app::BaseState &state);
    void baseStateChanged(const app::BaseState &state);
    void stateLoaded(const app::MapState &state);
    void playerChanged(const app::PlayerState &player);
    void markerAdded(const app::MapMarker &marker);
    void markerUpdated(const app::MapMarker &marker);
    void markerRemoved(const QString &markerId);
    void markerVisibilityChanged(const QString &markerId, bool visible);
    void markerTypeVisibilityChanged(const QString &markerType, bool visible);
    void markerTypesChanged(const app::MarkerTypeMap &markerTypes);
    void mapChanged(const QString &mapId);
    void layerChanged(const QString &layerId);

private:
    MapState emptyMapState() const;
    void loadBaseState();
    QString baseConfigPath() const;
    void markBaseDirty();
    MapMarker *findMarker(const QString &markerId, bool *temporary = nullptr);
    const MapMarker *findMarker(const QString &markerId, bool *temporary = nullptr) const;
    MapLocation locationFromResolved(const ResolvedMapLocation &resolved) const;
    bool resolveLocation(int gameX, int gameY, int gameZ, MapLocation *outLocation) const;
    bool syncMarkerSubtypes();
    void applyMarkerEvent(const AppEvent &event);
    static int intValue(const QJsonObject &object, const QString &key, int defaultValue = 0);
    static bool boolValue(const QJsonObject &object, const QString &key, bool defaultValue = false);

    MapCatalog m_catalog;
    MapResolver m_resolver;
    BaseState m_baseState;
    MapState m_mapState;
    bool m_baseDirty = false;
    bool m_followPlayerMap = true;
    quint64 m_selectedUid = 0;
};

} // namespace app
