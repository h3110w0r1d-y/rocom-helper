#pragma once

#include "data/data_center.h"
#include "events/event_dispatcher.h"
#include "http/http_server_service.h"
#include "http/sse_broadcaster.h"
#include "rwtd/live_capture_service.h"
#include "storage/database_service.h"
#include "traffic/traffic_event_mapper.h"
#include "ui/catch/catch_window.h"
#include "ui/flow_layout.h"
#include "ui/map/map_window.h"

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QWidget>

namespace app {

class MainWindow : public QWidget {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    void buildUi();
    void connectSignals();
    void initializeServices();
    void populateDevices();
    void toggleTraffic();
    void onTrafficError(const QString &message);
    void showMap();
    void showCatch();
    void importPathOverlay();
    QStringList extractSvgPaths(const QString &filePath) const;
    void onBaseStateLoaded(const BaseState &state);
    void onMapStateLoaded(const MapState &state);
    void populateMapCombo(const QString &currentMapId = {});
    void populateLayerCombo(const QString &mapId, const QString &currentLayerId = {});
    void selectMapFromCombo();
    void selectLayerFromCombo();
    void onMapChanged(const QString &mapId);
    void onLayerChanged(const QString &layerId);
    void renderMarkerTypeControls(const MarkerTypeMap &markerTypes);
    void scheduleTypeLayoutRefresh();
    void refreshTypeLayout();
    void showShinyAlert(const QJsonObject &payload);
    void forgetAlert(QMessageBox *dialog);

    DataCenter m_dataCenter;
    MapWindow *m_mapWindow = nullptr;
    CatchWindow *m_catchWindow = nullptr;
    rwtd::LiveCaptureService m_capture;
    EventDispatcher m_eventDispatcher;
    TrafficEventMapper m_trafficEventMapper;
    DatabaseService m_database;
    SseBroadcaster m_sseBroadcaster;
    HttpServerService m_httpServer{&m_database};

    QComboBox *m_ifaceCombo = nullptr;
    QPushButton *m_refreshIfacesButton = nullptr;
    QPushButton *m_trafficButton = nullptr;
    QLabel *m_statusLabel = nullptr;
    QPushButton *m_showMapButton = nullptr;
    QCheckBox *m_topCheckbox = nullptr;
    QCheckBox *m_miniMapCheckbox = nullptr;
    QComboBox *m_mapCombo = nullptr;
    QComboBox *m_layerCombo = nullptr;
    QCheckBox *m_playerPositionLogCheckbox = nullptr;
    QCheckBox *m_trailCheckbox = nullptr;
    QSpinBox *m_trailWidthSpin = nullptr;
    QPushButton *m_clearTrailButton = nullptr;
    QPushButton *m_importPathButton = nullptr;
    QPushButton *m_clearPathButton = nullptr;
    FlowWidget *m_typeContainer = nullptr;
    FlowLayout *m_typeLayout = nullptr;
    QPushButton *m_showCatchButton = nullptr;
    QTimer m_saveTimer;
    QList<QMessageBox *> m_alertWindows;
};

} // namespace app
