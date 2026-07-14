#pragma once

#include "data/data_center.h"
#include "events/event_dispatcher.h"
#include "http/http_api_client.h"
#include "ui/marker_filter_panel.h"
#include "ui/map/map_window.h"

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QList>
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
    void connectToServer();
    void disconnectFromServer();
    void applyServerEndpoint(bool restartClient);
    void showMap();
    void showPendingFeature();
    void importPathOverlay();
    QStringList extractSvgPaths(const QString &filePath) const;
    void onBaseStateLoaded(const BaseState &state);
    void onMapStateLoaded(const MapState &state);
    void populateMapCombo(const QString &currentMapId = {});
    void populateLayerCombo(const QString &mapId, const QString &currentLayerId = {});
    void selectMapFromCombo();
    void selectLayerFromCombo();
    quint64 selectedUid() const;
    void onMapChanged(const QString &mapId);
    void onLayerChanged(const QString &layerId);
    void renderMarkerTypeControls(const MarkerTypeMap &markerTypes);
    void showShinyAlert(const QJsonObject &payload);
    void forgetAlert(QMessageBox *dialog);

    DataCenter m_dataCenter;
    MapWindow *m_mapWindow = nullptr;
    EventDispatcher m_eventDispatcher;
    HttpApiClient m_apiClient;

    QLineEdit *m_httpHostEdit = nullptr;
    QSpinBox *m_httpPortSpin = nullptr;
    QLineEdit *m_uidEdit = nullptr;
    QPushButton *m_connectButton = nullptr;
    QPushButton *m_disconnectButton = nullptr;
    QLabel *m_statusLabel = nullptr;
    QPushButton *m_showMapButton = nullptr;
    QCheckBox *m_topCheckbox = nullptr;
    QCheckBox *m_miniMapCheckbox = nullptr;
    QComboBox *m_mapCombo = nullptr;
    QComboBox *m_layerCombo = nullptr;
    QCheckBox *m_trailCheckbox = nullptr;
    QSpinBox *m_trailWidthSpin = nullptr;
    QPushButton *m_clearTrailButton = nullptr;
    QPushButton *m_exportMapButton = nullptr;
    QPushButton *m_importPathButton = nullptr;
    QPushButton *m_clearPathButton = nullptr;
    MarkerFilterPanel *m_markerFilterPanel = nullptr;
    QList<QMessageBox *> m_alertWindows;
    QTimer m_saveTimer;
};

} // namespace app
