#include "map_window.h"

#include "ui/overlay/overlay_host_controller.h"
#include "ui/window_flags.h"

#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QDebug>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QKeySequence>
#include <QMessageBox>
#include <QShowEvent>
#include <QHideEvent>
#include <QSizePolicy>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace app {

MapWindow::MapWindow(DataCenter *dataCenter, QWidget *parent)
    : QWidget(parent)
    , m_dataCenter(dataCenter)
    , m_viewer(new MapViewer(dataCenter != nullptr ? &dataCenter->catalog() : nullptr, this))
{
    setWindowTitle(m_baseTitle);
    resize(1000, 720);

#ifdef Q_OS_WIN
    OverlayHostOptions overlayOptions;
    overlayOptions.title = m_baseTitle;
    overlayOptions.requireStaysOnTopForOverlay = true;
    overlayOptions.canEnterOverlay = [this] {
        return m_miniMapMode;
    };
    m_overlayHost = new OverlayHostController(this, overlayOptions, this);
#else
    setCloseOnlyWindowControls(this);
#endif

    connect(m_viewer, &MapViewer::mapClicked, this, &MapWindow::createMarkerAt);
    connect(m_viewer, &MapViewer::blankMapClicked, this, &MapWindow::printCoordinate);
    connect(m_viewer, &MapViewer::markerMoved, this, &MapWindow::markerMoved);
    connect(m_viewer, &MapViewer::markerSelected, this, &MapWindow::setSelectedMarkers);
    connect(m_viewer, &MapViewer::markerJsonRequested, this, &MapWindow::copyMarkerJson);

    m_followCheckbox = new QCheckBox(QStringLiteral("跟踪角色"), this);
    m_followCheckbox->setChecked(true);
    connect(m_followCheckbox, &QCheckBox::toggled, this, &MapWindow::setFollowPlayer);

    m_miniMapCheckbox = new QCheckBox(QStringLiteral("小地图模式"), this);
    connect(m_miniMapCheckbox, &QCheckBox::toggled, this, &MapWindow::setMiniMapMode);

    m_mapCombo = new QComboBox(this);
    connect(m_mapCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] {
        selectMapFromCombo();
    });

    m_layerCombo = new QComboBox(this);
    connect(m_layerCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] {
        selectLayerFromCombo();
    });

    m_typeCombo = new QComboBox(this);
    m_typeCombo->setMinimumWidth(120);

    m_markModeCheckbox = new QCheckBox(QStringLiteral("标点模式"), this);
    m_markModeCheckbox->setToolTip(QStringLiteral("开启后，在地图上左键或右键点击即可添加当前类型点位。"));
    connect(m_markModeCheckbox, &QCheckBox::toggled, m_viewer, &MapViewer::setManualAddEnabled);

    m_temporaryMarkerCheckbox = new QCheckBox(QStringLiteral("临时标记"), this);
    m_clearTemporaryButton = new QPushButton(QStringLiteral("清空临时"), this);
    connect(m_clearTemporaryButton, &QPushButton::clicked, m_dataCenter, &DataCenter::clearTemporaryMarkers);

    m_centerButton = new QPushButton(QStringLiteral("居中"), this);
    connect(m_centerButton, &QPushButton::clicked, this, &MapWindow::centerPlayer);

    m_deleteButton = new QPushButton(QStringLiteral("删除选中"), this);
    m_deleteButton->setEnabled(false);
    connect(m_deleteButton, &QPushButton::clicked, this, &MapWindow::deleteSelectedMarkers);

    m_deleteShortcuts = {
        new QShortcut(QKeySequence(Qt::Key_Delete), this),
        new QShortcut(QKeySequence(Qt::Key_Backspace), this),
    };
    for (QShortcut *shortcut : m_deleteShortcuts) {
        shortcut->setContext(Qt::WindowShortcut);
        shortcut->setEnabled(false);
        connect(shortcut, &QShortcut::activated, this, &MapWindow::deleteSelectedMarkers);
    }

    m_editBar = new QWidget(this);
    auto *editLayout = new QHBoxLayout(m_editBar);
    editLayout->setContentsMargins(0, 0, 0, 0);
    editLayout->addWidget(m_mapCombo);
    editLayout->addWidget(m_layerCombo);
    editLayout->addWidget(m_typeCombo);
    editLayout->addWidget(m_markModeCheckbox);
    editLayout->addWidget(m_temporaryMarkerCheckbox);
    editLayout->addWidget(m_clearTemporaryButton);
    editLayout->addWidget(m_deleteButton);
    editLayout->addStretch(1);

    m_topBar = new QWidget(this);
    m_topBar->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    auto *topLayout = new QHBoxLayout(m_topBar);
    topLayout->setContentsMargins(8, 8, 8, 4);
    topLayout->addWidget(m_followCheckbox);
    topLayout->addWidget(m_miniMapCheckbox);
    topLayout->addWidget(m_centerButton);
    topLayout->addWidget(m_editBar, 1);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
#ifdef Q_OS_WIN
    m_overlayHost->install();
    layout->addWidget(m_overlayHost->captionBar());
#endif
    layout->addWidget(m_topBar);
    layout->addWidget(m_viewer, 1);

    if (m_dataCenter != nullptr) {
        connect(m_dataCenter, &DataCenter::stateLoaded, this, &MapWindow::applyState);
        connect(m_dataCenter, &DataCenter::mapChanged, this, &MapWindow::onMapChanged);
        connect(m_dataCenter, &DataCenter::layerChanged, this, &MapWindow::onLayerChanged);
        connect(m_dataCenter, &DataCenter::playerChanged, this, &MapWindow::onPlayerChanged);
        connect(m_dataCenter, &DataCenter::markerAdded, this, &MapWindow::onMarkerChanged);
        connect(m_dataCenter, &DataCenter::markerUpdated, this, &MapWindow::onMarkerChanged);
        connect(m_dataCenter, &DataCenter::markerRemoved, this, &MapWindow::onMarkerRemoved);
        connect(m_dataCenter, &DataCenter::markerTypesChanged, this, &MapWindow::setMarkerTypes);
        connect(m_dataCenter, &DataCenter::markerTypeVisibilityChanged, this, &MapWindow::refreshMarkerVisibility);
    }

    populateMapCombo();
}

QCheckBox *MapWindow::miniMapCheckbox() const
{
    return m_miniMapCheckbox;
}

void MapWindow::applyState(const MapState &state)
{
    populateMapCombo();
    onMapChanged(state.currentMapId);
    onLayerChanged(state.currentLayerId);
    setMarkerTypes(state.markerTypes);
    m_viewer->setPlayerPosition(state.player, m_followPlayer);
    renderAllMarkers(state);

    QSet<QString> markerIds;
    for (const QString &id : state.markers.keys()) {
        markerIds.insert(id);
    }
    for (const QString &id : state.temporaryMarkers.keys()) {
        markerIds.insert(id);
    }
    QSet<QString> selected = m_selectedMarkerIds;
    selected.intersect(markerIds);
    setSelectedMarkers(selected);
}

void MapWindow::setAlwaysOnTop(bool enabled)
{
#ifdef Q_OS_WIN
    if (m_overlayHost != nullptr) {
        m_overlayHost->setStaysOnTop(enabled);
        show();
        return;
    }
#endif
    setCloseOnlyWindowControls(this, enabled);
    show();
}

void MapWindow::setMiniMapMode(bool enabled)
{
#ifdef Q_OS_WIN
    if (!enabled && m_overlayHost != nullptr && m_overlayHost->isOverlayEnabled()) {
        m_overlayHost->setOverlayEnabled(false);
    }
    if (!enabled && m_miniMapMode) {
        m_savedMiniMapGeometry = geometry();
    }
#endif

    m_miniMapMode = enabled;
    m_topBar->setVisible(!enabled);
    m_viewer->setEditEnabled(!enabled);
    if (enabled) {
        m_markModeCheckbox->setChecked(false);
        if (m_savedMiniMapGeometry.isValid()) {
            setGeometry(m_savedMiniMapGeometry);
        } else {
            resize(420, 320);
        }
    }

#ifdef Q_OS_WIN
    if (m_overlayHost != nullptr) {
        m_overlayHost->refreshOverlayButton();
    }
#endif
}

void MapWindow::setTrailRecordingEnabled(bool enabled)
{
    m_viewer->setTrailRecordingEnabled(enabled);
}

void MapWindow::setTrailWidth(int width)
{
    m_viewer->setTrailWidth(width);
}

void MapWindow::clearTrail()
{
    m_viewer->clearTrail();
}

int MapWindow::setPathOverlays(const QStringList &pathData)
{
    return m_viewer->setPathOverlays(pathData);
}

void MapWindow::clearPathOverlays()
{
    m_viewer->clearPathOverlays();
}

void MapWindow::markerMoved(const QString &markerId, double x, double y)
{
    if (m_dataCenter == nullptr || m_currentMapId.isEmpty()) {
        return;
    }
    bool ok = false;
    const QPointF gamePos = m_dataCenter->resolver().mapToGame(m_currentMapId, x, y, &ok);
    if (!ok) {
        return;
    }
    m_dataCenter->updateMarker(markerId, true, qRound(gamePos.x()), true, qRound(gamePos.y()), false, 0);
}

void MapWindow::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    emit opcodeConsumerVisibilityChanged();
}

void MapWindow::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    emit opcodeConsumerVisibilityChanged();
}

#ifdef Q_OS_WIN
bool MapWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
    if (m_overlayHost != nullptr && m_overlayHost->handleNativeEvent(eventType, message, result)) {
        return true;
    }
    return QWidget::nativeEvent(eventType, message, result);
}
#endif

void MapWindow::closeEvent(QCloseEvent *event)
{
#ifdef Q_OS_WIN
    if (m_overlayHost != nullptr && m_overlayHost->isOverlayEnabled()) {
        m_overlayHost->setOverlayEnabled(false);
    }
    if (m_miniMapMode) {
        m_savedMiniMapGeometry = geometry();
    }
#endif
    if (m_dataCenter != nullptr) {
        m_dataCenter->saveIfDirty();
    }
    event->ignore();
    hide();
}

void MapWindow::populateMapCombo()
{
    if (m_dataCenter == nullptr) {
        return;
    }
    const QString current = m_mapCombo->currentData().toString().isEmpty() ? m_currentMapId : m_mapCombo->currentData().toString();
    QSignalBlocker blocker(m_mapCombo);
    m_mapCombo->clear();
    for (const auto &option : m_dataCenter->mapOptions()) {
        m_mapCombo->addItem(option.second, option.first);
    }
    if (!current.isEmpty()) {
        const int index = m_mapCombo->findData(current);
        if (index >= 0) {
            m_mapCombo->setCurrentIndex(index);
        }
    }
}

void MapWindow::populateLayerCombo(const QString &mapId)
{
    if (m_dataCenter == nullptr) {
        return;
    }
    const QString current = m_currentLayerId;
    QSignalBlocker blocker(m_layerCombo);
    m_layerCombo->clear();
    for (const MapLayerConfig &layer : m_dataCenter->layersForMap(mapId)) {
        m_layerCombo->addItem(layer.name, layer.id);
    }
    if (!current.isEmpty()) {
        const int index = m_layerCombo->findData(current);
        if (index >= 0) {
            m_layerCombo->setCurrentIndex(index);
        }
    }
}

void MapWindow::selectMapFromCombo()
{
    if (m_dataCenter != nullptr && m_mapCombo->currentData().isValid()) {
        m_dataCenter->setCurrentMap(m_mapCombo->currentData().toString());
    }
}

void MapWindow::selectLayerFromCombo()
{
    if (m_dataCenter != nullptr && m_layerCombo->currentData().isValid()) {
        m_dataCenter->setCurrentLayer(m_layerCombo->currentData().toString());
    }
}

void MapWindow::onMapChanged(const QString &mapId)
{
    if (mapId.isEmpty()) {
        return;
    }
    m_currentMapId = mapId;
    m_viewer->setMap(mapId);
    const int index = m_mapCombo->findData(mapId);
    if (index >= 0 && m_mapCombo->currentIndex() != index) {
        QSignalBlocker blocker(m_mapCombo);
        m_mapCombo->setCurrentIndex(index);
    }
    populateLayerCombo(mapId);
    if (m_dataCenter != nullptr) {
        renderAllMarkers(m_dataCenter->snapshot());
    }
}

void MapWindow::onLayerChanged(const QString &layerId)
{
    m_currentLayerId = layerId;
    m_viewer->setActiveLayer(layerId);
    const int index = m_layerCombo->findData(layerId);
    if (index >= 0 && m_layerCombo->currentIndex() != index) {
        QSignalBlocker blocker(m_layerCombo);
        m_layerCombo->setCurrentIndex(index);
    }
    refreshMarkerVisibility();
    if (m_dataCenter != nullptr) {
        onPlayerChanged(m_dataCenter->snapshot().player);
    }
}

void MapWindow::setMarkerTypes(const MarkerTypeMap &markerTypes)
{
    m_viewer->setMarkerTypes(markerTypes);
    const QString current = m_typeCombo->currentData().toString();
    QSignalBlocker blocker(m_typeCombo);
    m_typeCombo->clear();
    for (auto it = markerTypes.begin(); it != markerTypes.end(); ++it) {
        m_typeCombo->addItem(it->name, it.key());
    }
    if (!current.isEmpty()) {
        const int index = m_typeCombo->findData(current);
        if (index >= 0) {
            m_typeCombo->setCurrentIndex(index);
        }
    }
    refreshMarkerVisibility();
}

void MapWindow::onPlayerChanged(const PlayerState &player)
{
    updateWindowTitle(player);
    m_viewer->setPlayerPosition(player, m_followPlayer);
}

void MapWindow::updateWindowTitle(const PlayerState &player)
{
    QString title = m_baseTitle;
    if (player.visible) {
        const QString area = player.hasLocation && !player.location.areaName.isEmpty()
            ? QStringLiteral(" %1").arg(player.location.areaName)
            : QString();
        title = QStringLiteral("%1 (%2, %3, %4)%5")
                    .arg(m_baseTitle)
                    .arg(player.gameX)
                    .arg(player.gameY)
                    .arg(player.gameZ)
                    .arg(area);
    }

    setWindowTitle(title);
#ifdef Q_OS_WIN
    if (m_overlayHost != nullptr) {
        m_overlayHost->setTitle(title);
    }
#endif
}

void MapWindow::onMarkerChanged(const MapMarker &marker)
{
    if (m_dataCenter == nullptr) {
        return;
    }
    const MapState state = m_dataCenter->snapshot();
    m_viewer->addOrUpdateMarker(marker, markerVisibleByType(marker, state));
}

void MapWindow::onMarkerRemoved(const QString &markerId)
{
    m_viewer->removeMarker(markerId);
    if (m_selectedMarkerIds.contains(markerId)) {
        m_selectedMarkerIds.remove(markerId);
        setSelectedMarkers(m_selectedMarkerIds);
    }
}

void MapWindow::refreshMarkerVisibility()
{
    if (m_dataCenter == nullptr) {
        return;
    }
    const MapState state = m_dataCenter->snapshot();
    auto refresh = [this, &state](const MarkerMap &markers) {
        for (const MapMarker &marker : markers) {
            const bool visible = marker.visible
                && markerVisibleByType(marker, state)
                && marker.hasLocation
                && marker.location.mapId == m_currentMapId
                && marker.location.layerId == m_currentLayerId;
            m_viewer->setMarkerVisible(marker.id, visible);
        }
    };
    refresh(state.markers);
    refresh(state.temporaryMarkers);
}

void MapWindow::renderAllMarkers(const MapState &state)
{
    const QStringList existingIds = m_viewer->markers().keys();
    for (const QString &markerId : existingIds) {
        m_viewer->removeMarker(markerId);
    }
    for (const MapMarker &marker : state.markers) {
        m_viewer->addOrUpdateMarker(marker, markerVisibleByType(marker, state));
    }
    for (const MapMarker &marker : state.temporaryMarkers) {
        m_viewer->addOrUpdateMarker(marker, markerVisibleByType(marker, state));
    }
}

bool MapWindow::markerVisibleByType(const MapMarker &marker, const MapState &state) const
{
    const MarkerTypeConfig markerType = state.markerTypes.value(
        marker.markerType,
        state.markerTypes.value(QString::fromLatin1(DefaultMarkerType)));
    if (!markerType.visible) {
        return false;
    }
    const QString subtype = normalizeMarkerSubtype(marker.extra.value(QStringLiteral("type")));
    if (!markerType.subtypes.contains(subtype)) {
        return true;
    }
    return markerType.subtypes.value(subtype).visible;
}

void MapWindow::createMarkerAt(double x, double y)
{
    if (m_dataCenter == nullptr || m_currentMapId.isEmpty()) {
        return;
    }
    bool ok = false;
    const QPointF gamePos = m_dataCenter->resolver().mapToGame(m_currentMapId, x, y, &ok);
    if (!ok) {
        return;
    }
    const MapMarker marker = m_dataCenter->createMarker(
        m_typeCombo->currentData().toString().isEmpty()
            ? QString::fromLatin1(DefaultMarkerType)
            : m_typeCombo->currentData().toString(),
        QString(),
        QString(),
        QJsonObject(),
        qRound(gamePos.x()),
        qRound(gamePos.y()),
        currentLayerDefaultZ(),
        true,
        m_temporaryMarkerCheckbox->isChecked());
    setSelectedMarkers({marker.id});
}

void MapWindow::printCoordinate(double x, double y)
{
    if (m_dataCenter == nullptr || m_currentMapId.isEmpty()) {
        return;
    }
    bool ok = false;
    const QPointF gamePos = m_dataCenter->resolver().mapToGame(m_currentMapId, x, y, &ok);
    if (!ok) {
        qInfo("map coordinate cannot be converted");
        return;
    }
    qInfo("map坐标: %.2f,%.2f; 游戏坐标: %d,%d,%d", x, y, qRound(gamePos.x()), qRound(gamePos.y()), currentLayerDefaultZ());
}

int MapWindow::currentLayerDefaultZ() const
{
    if (m_dataCenter == nullptr) {
        return 0;
    }
    for (const MapLayerConfig &layer : m_dataCenter->layersForMap(m_currentMapId)) {
        if (layer.id == m_currentLayerId) {
            return layer.defaultZ;
        }
    }
    return 0;
}

void MapWindow::setFollowPlayer(bool enabled)
{
    m_followPlayer = enabled;
    if (m_dataCenter != nullptr) {
        m_dataCenter->setFollowPlayerMap(enabled);
    }
}

void MapWindow::centerPlayer()
{
    if (m_dataCenter == nullptr) {
        return;
    }
    const PlayerState player = m_dataCenter->snapshot().player;
    if (!player.visible || !player.hasLocation) {
        return;
    }
    if (player.location.mapId != m_currentMapId) {
        m_dataCenter->setCurrentMap(player.location.mapId);
    }
    if (player.location.layerId != m_currentLayerId) {
        m_dataCenter->setCurrentLayer(player.location.layerId);
    }
    m_viewer->centerOnMapPoint(player.location.mapX, player.location.mapY);
}

void MapWindow::deleteSelectedMarkers()
{
    if (m_dataCenter == nullptr || m_selectedMarkerIds.isEmpty()) {
        return;
    }
    const QSet<QString> ids = m_selectedMarkerIds;
    for (const QString &markerId : ids) {
        m_dataCenter->deleteMarker(markerId);
    }
    setSelectedMarkers({});
}

void MapWindow::exportCurrentMapImage()
{
    QString baseName = m_mapCombo->currentText().trimmed();
    if (baseName.isEmpty()) {
        baseName = m_currentMapId.isEmpty() ? QStringLiteral("map") : m_currentMapId;
    }
    const QString invalidChars = QStringLiteral("\\/:*?\"<>|");
    for (qsizetype i = 0; i < baseName.size(); ++i) {
        if (invalidChars.contains(baseName.at(i))) {
            baseName[i] = QLatin1Char('_');
        }
    }

    const QString defaultPath = QDir::home().filePath(QStringLiteral("%1.png").arg(baseName));
    QString filePath = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("导出地图图片"),
        defaultPath,
        QStringLiteral("PNG 图片 (*.png);;JPEG 图片 (*.jpg *.jpeg);;所有文件 (*)"));
    if (filePath.isEmpty()) {
        return;
    }

    if (QFileInfo(filePath).suffix().isEmpty()) {
        filePath += QStringLiteral(".png");
    }
    if (!m_viewer->exportFullImage(filePath)) {
        QMessageBox::warning(this, QStringLiteral("导出失败"), QStringLiteral("地图图片导出失败，请检查保存路径。"));
    }
}

void MapWindow::setSelectedMarkers(const QSet<QString> &markerIds)
{
    m_selectedMarkerIds = markerIds;
    refreshDeleteEnabled();
    if (m_syncingMarkerSelection) {
        return;
    }
    m_syncingMarkerSelection = true;
    m_viewer->setSelectedMarkers(markerIds);
    m_syncingMarkerSelection = false;
}

void MapWindow::refreshDeleteEnabled()
{
    const bool enabled = !m_selectedMarkerIds.isEmpty();
    m_deleteButton->setEnabled(enabled);
    for (QShortcut *shortcut : m_deleteShortcuts) {
        shortcut->setEnabled(enabled);
    }
}

void MapWindow::copyMarkerJson(const QString &markerId)
{
    if (m_dataCenter == nullptr) {
        return;
    }
    const MapState state = m_dataCenter->snapshot();
    const MapMarker marker = state.markers.value(markerId, state.temporaryMarkers.value(markerId));
    if (marker.id.isEmpty()) {
        return;
    }
    QApplication::clipboard()->setText(QString::fromUtf8(QJsonDocument(mapMarkerToJson(marker)).toJson(QJsonDocument::Indented)));
}

} // namespace app
