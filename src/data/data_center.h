#pragma once

#include "events/app_event.h"
#include "map_catalog.h"
#include "map_types.h"
#include "runtime_context.h"

#include <QJsonArray>
#include <QFile>
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
    CatchState catchSnapshot() const;
    const RuntimeContext &runtimeContext() const;

    QList<QPair<QString, QString>> mapOptions() const;
    QVector<MapLayerConfig> layersForMap(const QString &mapId = {}) const;

    void setRuntimeContext(const RuntimeContext &context);
    void setFollowPlayerMap(bool enabled);
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
    PlayerState updatePlayer(bool visible, double rotation, int gameX, int gameY, int gameZ);

public slots:
    void handleEvent(const app::AppEvent &event);

signals:
    void baseStateLoaded(const app::BaseState &state);
    void baseStateChanged(const app::BaseState &state);
    void catchStateChanged(const app::CatchState &state);
    void catchRecordAdded(const app::CatchRecord &record);
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
    void shinyPetDetected(const QJsonObject &payload);
    void boxHintUpdated(const QJsonObject &payload);

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
    void applyCatchRecord(const QJsonObject &payload);
    static int intValue(const QJsonObject &object, const QString &key, int defaultValue = 0);
    static bool boolValue(const QJsonObject &object, const QString &key, bool defaultValue = false);

    MapCatalog m_catalog;
    MapResolver m_resolver;
    BaseState m_baseState;
    MapState m_mapState;
    CatchState m_catchState;
    RuntimeContext m_runtimeContext;
    bool m_baseDirty = false;
    bool m_followPlayerMap = true;
};

} // namespace app
