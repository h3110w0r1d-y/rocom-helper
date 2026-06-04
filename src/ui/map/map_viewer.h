#pragma once

#include "data/map_catalog.h"
#include "data/map_types.h"
#include "graphics_items.h"

#include <QGraphicsPathItem>
#include <QGraphicsPixmapItem>
#include <QGraphicsRectItem>
#include <QGraphicsTextItem>
#include <QGraphicsView>
#include <QMap>
#include <QPoint>
#include <QRubberBand>

namespace app {

class MapViewer : public QGraphicsView {
    Q_OBJECT

public:
    explicit MapViewer(const MapCatalog *catalog, QWidget *parent = nullptr);

    void setMap(const QString &mapId);
    void setActiveLayer(const QString &layerId);
    void setMarkerTypes(const MarkerTypeMap &markerTypes);
    void setPlayerPosition(const PlayerState &player, bool follow = false);
    void addOrUpdateMarker(const MapMarker &marker, bool typeVisible = true);
    void removeMarker(const QString &markerId);
    void setMarkerVisible(const QString &markerId, bool visible);
    void setEditEnabled(bool enabled);
    void setManualAddEnabled(bool enabled);
    void setTrailRecordingEnabled(bool enabled);
    void setTrailWidth(int width);
    void clearTrail();
    int setPathOverlays(const QStringList &pathData);
    void clearPathOverlays();
    void centerOnMapPoint(double x, double y);
    void setSelectedMarkers(const QSet<QString> &markerIds);
    QSet<QString> selectedMarkerIds() const;

    const QMap<QString, DraggableMarkerItem *> &markers() const;

signals:
    void mapClicked(double x, double y);
    void blankMapClicked(double x, double y);
    void markerMoved(const QString &markerId, double x, double y);
    void markerSelected(const QSet<QString> &markerIds);
    void markerJsonRequested(const QString &markerId);

protected:
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void showLoadingScene(const QString &message = QStringLiteral("地图加载中"));
    bool loadMapImages(const MapConfig &config, QImage *fullImage, QMap<QString, QList<QPair<MapImageConfig, QImage>>> *layerImages);
    QImage composeFullImage(const MapConfig &config) const;
    QList<MapImageConfig> fullTileConfigs(const MapConfig &config) const;
    QImage readImage(const QString &path, bool required) const;
    void renderPathOverlays();
    void clearPathOverlayItems();
    void flushSceneState();
    void applyEditFlags(DraggableMarkerItem *item);
    void recordTrailPosition(const QPointF &playerPos);
    void centerPixmapItem(QGraphicsPixmapItem *item);
    double minimumZoom() const;
    void fitMapToViewCover();
    void ensureMinimumZoom();
    void updateSceneRect();
    QPointF mapPosFromViewPos(const QPoint &viewPos) const;
    QString markerIdAt(const QPoint &viewPos) const;
    void toggleMarkerSelection(const QString &markerId, bool additive, QSet<QString> selection = {});
    void clearMarkerSelection();
    bool shiftSelectEnabled(QMouseEvent *event) const;
    void startRubberBand(const QPoint &origin);
    void updateRubberBand(const QPoint &current);
    void finishRubberBand(const QPoint &current);
    void resetPressState();
    void panViewTo(const QPoint &viewPos);

    const MapCatalog *m_catalog = nullptr;
    QGraphicsScene *m_scene = nullptr;
    const MapConfig *m_mapConfig = nullptr;
    QGraphicsRectItem *m_mapRootItem = nullptr;
    QGraphicsRectItem *m_maskItem = nullptr;
    TrailMaskItem *m_trailItem = nullptr;
    QList<QGraphicsPathItem *> m_pathOverlayItems;
    QStringList m_pathOverlayData;
    QMap<QString, QList<QGraphicsPixmapItem *>> m_layerItems;
    QGraphicsTextItem *m_loadingText = nullptr;
    QMap<QString, DraggableMarkerItem *> m_markers;
    MarkerTypeMap m_markerTypes;
    QGraphicsPixmapItem *m_playerItem = nullptr;
    QString m_activeMapId;
    QString m_activeLayerId;
    bool m_mapLoaded = false;
    bool m_trailRecordingEnabled = false;
    int m_trailWidth = 260;
    QPointF m_lastTrailPos;
    bool m_hasLastTrailPos = false;
    bool m_manualAddEnabled = false;
    bool m_editEnabled = true;
    bool m_updatingMarkerItem = false;
    QPoint m_pressViewPos;
    QPointF m_pressMapPos;
    QString m_pressMarkerId;
    QPoint m_lastPanPos;
    bool m_hasPress = false;
    bool m_pressDragged = false;
    QSet<QString> m_pressSelectedMarkerIds;
    QRubberBand *m_rubberBand = nullptr;
    QPoint m_rubberOrigin;
    bool m_rubberBandActive = false;
    double m_minZoom = 0.04;
    double m_maxZoom = 2.0;
};

} // namespace app
