#pragma once

#include "data/data_center.h"
#include "map_viewer.h"

#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QRect>
#include <QShortcut>
#include <QWidget>

namespace app {

class OverlayHostController;

class MapWindow : public QWidget {
    Q_OBJECT

public:
    explicit MapWindow(DataCenter *dataCenter, QWidget *parent = nullptr);

    QCheckBox *miniMapCheckbox() const;
    void applyState(const MapState &state);
    void setAlwaysOnTop(bool enabled);
    void setMiniMapMode(bool enabled);
    void setTrailRecordingEnabled(bool enabled);
    void setTrailWidth(int width);
    void clearTrail();
    int setPathOverlays(const QStringList &pathData);
    void clearPathOverlays();

public slots:
    void markerMoved(const QString &markerId, double x, double y);
    void exportCurrentMapImage();

signals:
    void opcodeConsumerVisibilityChanged();

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    void populateMapCombo();
    void populateLayerCombo(const QString &mapId);
    void selectMapFromCombo();
    void selectLayerFromCombo();
    void onMapChanged(const QString &mapId);
    void onLayerChanged(const QString &layerId);
    void setMarkerTypes(const MarkerTypeMap &markerTypes);
    void onPlayerChanged(const PlayerState &player);
    void updateWindowTitle(const PlayerState &player);
    void onMarkerChanged(const MapMarker &marker);
    void onMarkerRemoved(const QString &markerId);
    void refreshMarkerVisibility();
    void renderAllMarkers(const MapState &state);
    bool markerVisibleByType(const MapMarker &marker, const MapState &state) const;
    void createMarkerAt(double x, double y);
    void printCoordinate(double x, double y);
    int currentLayerDefaultZ() const;
    void setFollowPlayer(bool enabled);
    void centerPlayer();
    void deleteSelectedMarkers();
    void setSelectedMarkers(const QSet<QString> &markerIds);
    void refreshDeleteEnabled();
    void copyMarkerJson(const QString &markerId);

    DataCenter *m_dataCenter = nullptr;
    MapViewer *m_viewer = nullptr;
    QCheckBox *m_followCheckbox = nullptr;
    QCheckBox *m_miniMapCheckbox = nullptr;
    QComboBox *m_mapCombo = nullptr;
    QComboBox *m_layerCombo = nullptr;
    QComboBox *m_typeCombo = nullptr;
    QCheckBox *m_markModeCheckbox = nullptr;
    QCheckBox *m_temporaryMarkerCheckbox = nullptr;
    QPushButton *m_clearTemporaryButton = nullptr;
    QPushButton *m_centerButton = nullptr;
    QPushButton *m_deleteButton = nullptr;
    QWidget *m_editBar = nullptr;
    QWidget *m_topBar = nullptr;
    QList<QShortcut *> m_deleteShortcuts;
    bool m_followPlayer = true;
    QString m_currentMapId;
    QString m_currentLayerId;
    QSet<QString> m_selectedMarkerIds;
    bool m_syncingMarkerSelection = false;
    QString m_baseTitle = QStringLiteral("小地图");
    OverlayHostController *m_overlayHost = nullptr;
    QRect m_savedMiniMapGeometry;
    bool m_miniMapMode = false;
};

} // namespace app
