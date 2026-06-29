#include "main_window.h"

#include "app_version.h"

#include <QCloseEvent>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QMessageBox>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QShowEvent>
#include <QVBoxLayout>
#include <QXmlStreamReader>

namespace app {
namespace {

QString pointsToPathData(const QString &points, bool closed)
{
    static const QRegularExpression numberRe(
        QStringLiteral("[-+]?(?:\\d*\\.\\d+|\\d+\\.?)(?:[eE][-+]?\\d+)?"));

    QStringList numbers;
    QRegularExpressionMatchIterator it = numberRe.globalMatch(points);
    while (it.hasNext()) {
        numbers.append(it.next().captured(0));
    }
    if (numbers.size() < 4 || numbers.size() % 2 != 0) {
        return {};
    }

    QString pathData = QStringLiteral("M %1 %2").arg(numbers.at(0), numbers.at(1));
    for (int index = 2; index < numbers.size(); index += 2) {
        pathData += QStringLiteral(" L %1 %2").arg(numbers.at(index), numbers.at(index + 1));
    }
    if (closed) {
        pathData += QStringLiteral(" Z");
    }
    return pathData;
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QWidget(parent)
    , m_mapWindow(new MapWindow(&m_dataCenter))
{
    setWindowTitle(appWindowTitle());
    resize(460, 420);

    buildUi();
    connectSignals();

    m_saveTimer.setParent(this);
    connect(&m_saveTimer, &QTimer::timeout, &m_dataCenter, &DataCenter::saveIfDirty);
    m_saveTimer.start(5000);

    m_dataCenter.load();
    connectToServer();
}

MainWindow::~MainWindow()
{
    m_apiClient.stop();
    m_dataCenter.close();
    delete m_mapWindow;
}

void MainWindow::buildUi()
{
    auto *serverGroup = new QGroupBox(QStringLiteral("HTTP 服务端"), this);
    auto *serverForm = new QFormLayout(serverGroup);
    serverForm->setVerticalSpacing(6);
    serverForm->setContentsMargins(8, 8, 8, 8);

    m_httpHostEdit = new QLineEdit(this);
    m_httpHostEdit->setPlaceholderText(QString::fromLatin1(DefaultHttpHost));
    m_httpPortSpin = new QSpinBox(this);
    m_httpPortSpin->setRange(MinHttpPort, MaxHttpPort);
    m_httpPortSpin->setValue(DefaultHttpPort);
    m_uidEdit = new QLineEdit(this);
    m_uidEdit->setPlaceholderText(QStringLiteral("0 表示所有用户"));
    m_connectButton = new QPushButton(QStringLiteral("连接"), this);
    m_disconnectButton = new QPushButton(QStringLiteral("断开"), this);
    m_statusLabel = new QLabel(QStringLiteral("未连接"), this);

    auto *endpointRow = new QHBoxLayout();
    endpointRow->setContentsMargins(0, 0, 0, 0);
    endpointRow->setSpacing(4);
    endpointRow->addWidget(m_httpHostEdit, 1);
    endpointRow->addWidget(m_httpPortSpin);
    serverForm->addRow(QStringLiteral("地址"), endpointRow);
    serverForm->addRow(QStringLiteral("UID"), m_uidEdit);

    auto *connectionRow = new QHBoxLayout();
    connectionRow->setContentsMargins(0, 0, 0, 0);
    connectionRow->setSpacing(4);
    connectionRow->addWidget(m_connectButton);
    connectionRow->addWidget(m_disconnectButton);
    connectionRow->addWidget(m_statusLabel, 1);
    serverForm->addRow(connectionRow);

    auto *mapGroup = new QGroupBox(QStringLiteral("小地图"), this);
    auto *mapLayout = new QGridLayout(mapGroup);
    m_showMapButton = new QPushButton(QStringLiteral("显示地图"), this);
    m_topCheckbox = new QCheckBox(QStringLiteral("置顶"), this);
    m_miniMapCheckbox = new QCheckBox(QStringLiteral("小地图模式"), this);
    m_mapCombo = new QComboBox(this);
    m_mapCombo->setToolTip(QStringLiteral("当前地图"));
    m_layerCombo = new QComboBox(this);
    m_layerCombo->setToolTip(QStringLiteral("当前层级"));
    m_trailCheckbox = new QCheckBox(QStringLiteral("记录轨迹"), this);
    m_trailWidthSpin = new QSpinBox(this);
    m_trailWidthSpin->setRange(1, 500);
    m_trailWidthSpin->setValue(260);
    m_trailWidthSpin->setSuffix(QStringLiteral(" px"));
    m_clearTrailButton = new QPushButton(QStringLiteral("清空轨迹"), this);
    m_exportMapButton = new QPushButton(QStringLiteral("导出地图"), this);
    m_importPathButton = new QPushButton(QStringLiteral("导入路径"), this);
    m_clearPathButton = new QPushButton(QStringLiteral("清空路径"), this);
    m_markerFilterPanel = new MarkerFilterPanel(this);

    mapLayout->addWidget(m_showMapButton, 0, 0);
    mapLayout->addWidget(m_topCheckbox, 0, 1);
    mapLayout->addWidget(m_miniMapCheckbox, 0, 2);
    mapLayout->addWidget(m_mapCombo, 1, 0);
    mapLayout->addWidget(m_layerCombo, 1, 1);
    mapLayout->addWidget(m_trailCheckbox, 2, 0);
    mapLayout->addWidget(m_trailWidthSpin, 2, 1);
    mapLayout->addWidget(m_clearTrailButton, 2, 2);
    mapLayout->addWidget(m_exportMapButton, 3, 0);
    mapLayout->addWidget(m_importPathButton, 3, 1);
    mapLayout->addWidget(m_clearPathButton, 3, 2);
    mapLayout->addWidget(m_markerFilterPanel, 4, 0, 1, 3);

    auto *pendingGroup = new QGroupBox(QStringLiteral("待接入功能"), this);
    auto *pendingLayout = new QGridLayout(pendingGroup);
    m_petFilterButton = new QPushButton(QStringLiteral("宠物管理"), this);
    m_showBoxHintButton = new QPushButton(QStringLiteral("盒子提示"), this);
    m_showCatchButton = new QPushButton(QStringLiteral("捕捉日志"), this);
    m_showEggTimeButton = new QPushButton(QStringLiteral("产蛋时间"), this);
    pendingLayout->addWidget(m_petFilterButton, 0, 0);
    pendingLayout->addWidget(m_showBoxHintButton, 0, 1);
    pendingLayout->addWidget(m_showCatchButton, 0, 2);
    pendingLayout->addWidget(m_showEggTimeButton, 1, 0);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(serverGroup);
    layout->addWidget(mapGroup);
    layout->addWidget(pendingGroup);
    layout->addStretch(1);
}

void MainWindow::connectSignals()
{
    connect(m_connectButton, &QPushButton::clicked, this, &MainWindow::connectToServer);
    connect(m_disconnectButton, &QPushButton::clicked, this, &MainWindow::disconnectFromServer);
    connect(m_showMapButton, &QPushButton::clicked, this, &MainWindow::showMap);
    connect(m_petFilterButton, &QPushButton::clicked, this, &MainWindow::showPendingFeature);
    connect(m_showBoxHintButton, &QPushButton::clicked, this, &MainWindow::showPendingFeature);
    connect(m_showCatchButton, &QPushButton::clicked, this, &MainWindow::showPendingFeature);
    connect(m_showEggTimeButton, &QPushButton::clicked, this, &MainWindow::showPendingFeature);
    connect(m_topCheckbox, &QCheckBox::toggled, m_mapWindow, &MapWindow::setAlwaysOnTop);
    connect(m_miniMapCheckbox, &QCheckBox::toggled, m_mapWindow, &MapWindow::setMiniMapMode);
    connect(m_mapWindow->miniMapCheckbox(), &QCheckBox::toggled, m_miniMapCheckbox, &QCheckBox::setChecked);
    connect(m_miniMapCheckbox, &QCheckBox::toggled, m_mapWindow->miniMapCheckbox(), &QCheckBox::setChecked);
    connect(m_mapCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, &MainWindow::selectMapFromCombo);
    connect(m_layerCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, &MainWindow::selectLayerFromCombo);
    connect(m_trailCheckbox, &QCheckBox::toggled, m_mapWindow, &MapWindow::setTrailRecordingEnabled);
    connect(m_trailWidthSpin, &QSpinBox::valueChanged, m_mapWindow, &MapWindow::setTrailWidth);
    connect(m_clearTrailButton, &QPushButton::clicked, m_mapWindow, &MapWindow::clearTrail);
    connect(m_exportMapButton, &QPushButton::clicked, m_mapWindow, &MapWindow::exportCurrentMapImage);
    connect(m_importPathButton, &QPushButton::clicked, this, &MainWindow::importPathOverlay);
    connect(m_clearPathButton, &QPushButton::clicked, m_mapWindow, &MapWindow::clearPathOverlays);
    connect(m_markerFilterPanel, &MarkerFilterPanel::typeVisibilityChanged, &m_dataCenter, &DataCenter::setMarkerTypeVisible);
    connect(m_markerFilterPanel, &MarkerFilterPanel::subtypeVisibilityChanged, &m_dataCenter, &DataCenter::setMarkerSubtypeVisible);

    connect(&m_dataCenter, &DataCenter::baseStateLoaded, this, &MainWindow::onBaseStateLoaded);
    connect(&m_dataCenter, &DataCenter::stateLoaded, this, &MainWindow::onMapStateLoaded);
    connect(&m_dataCenter, &DataCenter::mapChanged, this, &MainWindow::onMapChanged);
    connect(&m_dataCenter, &DataCenter::layerChanged, this, &MainWindow::onLayerChanged);
    connect(&m_dataCenter, &DataCenter::markerTypesChanged, this, &MainWindow::renderMarkerTypeControls);

    connect(&m_apiClient, &HttpApiClient::eventCreated, &m_eventDispatcher, &EventDispatcher::dispatch);
    connect(&m_apiClient, &HttpApiClient::statusChanged, m_statusLabel, &QLabel::setText);
    connect(&m_apiClient, &HttpApiClient::errorOccurred, this, [this](const QString &message) {
        m_statusLabel->setText(message);
    });
    connect(&m_eventDispatcher, &EventDispatcher::eventDispatched, &m_dataCenter, &DataCenter::handleEvent);
}

void MainWindow::connectToServer()
{
    applyServerEndpoint(true);
    if (!m_apiClient.isRunning()) {
        m_apiClient.start();
    }
}

void MainWindow::disconnectFromServer()
{
    m_apiClient.stop();
}

void MainWindow::applyServerEndpoint(bool restartClient)
{
    const QString host = m_httpHostEdit->text().trimmed().isEmpty()
        ? QString::fromLatin1(DefaultHttpHost)
        : m_httpHostEdit->text().trimmed();
    const int port = m_httpPortSpin->value();
    const quint64 uid = selectedUid();

    m_dataCenter.setHttpEndpoint(host, port);
    m_dataCenter.save();
    m_dataCenter.setSelectedUid(uid);
    m_apiClient.setEndpoint(host, port, uid);
    if (restartClient) {
        m_apiClient.restart();
    }
}

void MainWindow::showMap()
{
    m_mapWindow->show();
    m_mapWindow->raise();
    m_mapWindow->activateWindow();
}

void MainWindow::showPendingFeature()
{
    QMessageBox::information(this, QStringLiteral("待接入"), QStringLiteral("该功能等待后续 HTTP API 接入。"));
}

void MainWindow::importPathOverlay()
{
    const QString filePath = QFileDialog::getOpenFileName(this, QStringLiteral("导入路径"), QString(), QStringLiteral("SVG 文件 (*.svg);;所有文件 (*)"));
    if (filePath.isEmpty()) {
        return;
    }
    const QStringList paths = extractSvgPaths(filePath);
    if (paths.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("导入路径"), QStringLiteral("没有找到可导入的 path、polyline 或 polygon。"));
        return;
    }
    const int count = m_mapWindow->setPathOverlays(paths);
    m_statusLabel->setText(QStringLiteral("已导入 %1 条路径").arg(count));
}

QStringList MainWindow::extractSvgPaths(const QString &filePath) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(nullptr, QStringLiteral("导入失败"), QStringLiteral("读取 SVG 失败: %1").arg(file.errorString()));
        return {};
    }

    QStringList paths;
    QXmlStreamReader reader(&file);
    while (!reader.atEnd()) {
        reader.readNext();
        if (!reader.isStartElement()) {
            continue;
        }

        const QString tagName = reader.name().toString();
        if (tagName == QStringLiteral("path")) {
            const QString pathData = reader.attributes().value(QStringLiteral("d")).toString().trimmed();
            if (!pathData.isEmpty()) {
                paths.append(pathData);
            }
        } else if (tagName == QStringLiteral("polyline") || tagName == QStringLiteral("polygon")) {
            const QString pathData = pointsToPathData(
                reader.attributes().value(QStringLiteral("points")).toString(),
                tagName == QStringLiteral("polygon"));
            if (!pathData.isEmpty()) {
                paths.append(pathData);
            }
        }
    }
    if (reader.hasError()) {
        QMessageBox::warning(nullptr, QStringLiteral("导入失败"), QStringLiteral("读取 SVG 失败: %1").arg(reader.errorString()));
        return {};
    }
    return paths;
}

void MainWindow::onBaseStateLoaded(const BaseState &state)
{
    {
        QSignalBlocker blocker(m_httpHostEdit);
        m_httpHostEdit->setText(state.httpHost);
    }
    {
        QSignalBlocker blocker(m_httpPortSpin);
        m_httpPortSpin->setValue(state.httpPort);
    }
}

void MainWindow::onMapStateLoaded(const MapState &state)
{
    populateMapCombo(state.currentMapId);
    populateLayerCombo(state.currentMapId, state.currentLayerId);
    renderMarkerTypeControls(state.markerTypes);
}

void MainWindow::populateMapCombo(const QString &currentMapId)
{
    const QString current = currentMapId.isEmpty() ? m_mapCombo->currentData().toString() : currentMapId;
    QSignalBlocker blocker(m_mapCombo);
    m_mapCombo->clear();
    for (const auto &option : m_dataCenter.mapOptions()) {
        m_mapCombo->addItem(option.second, option.first);
    }
    if (!current.isEmpty()) {
        const int index = m_mapCombo->findData(current);
        if (index >= 0) {
            m_mapCombo->setCurrentIndex(index);
        }
    }
}

void MainWindow::populateLayerCombo(const QString &mapId, const QString &currentLayerId)
{
    const QString current = currentLayerId.isEmpty() ? m_layerCombo->currentData().toString() : currentLayerId;
    QSignalBlocker blocker(m_layerCombo);
    m_layerCombo->clear();
    for (const MapLayerConfig &layer : m_dataCenter.layersForMap(mapId)) {
        m_layerCombo->addItem(layer.name, layer.id);
    }
    if (!current.isEmpty()) {
        const int index = m_layerCombo->findData(current);
        if (index >= 0) {
            m_layerCombo->setCurrentIndex(index);
        }
    }
}

void MainWindow::selectMapFromCombo()
{
    if (m_mapCombo->currentData().isValid()) {
        m_dataCenter.setCurrentMap(m_mapCombo->currentData().toString());
    }
}

void MainWindow::selectLayerFromCombo()
{
    if (m_layerCombo->currentData().isValid()) {
        m_dataCenter.setCurrentLayer(m_layerCombo->currentData().toString());
    }
}

quint64 MainWindow::selectedUid() const
{
    bool ok = false;
    const quint64 uid = m_uidEdit->text().trimmed().toULongLong(&ok);
    return ok ? uid : 0;
}

void MainWindow::onMapChanged(const QString &mapId)
{
    const int index = m_mapCombo->findData(mapId);
    if (index >= 0 && m_mapCombo->currentIndex() != index) {
        QSignalBlocker blocker(m_mapCombo);
        m_mapCombo->setCurrentIndex(index);
    }
    populateLayerCombo(mapId);
}

void MainWindow::onLayerChanged(const QString &layerId)
{
    const int index = m_layerCombo->findData(layerId);
    if (index >= 0 && m_layerCombo->currentIndex() != index) {
        QSignalBlocker blocker(m_layerCombo);
        m_layerCombo->setCurrentIndex(index);
    }
}

void MainWindow::renderMarkerTypeControls(const MarkerTypeMap &markerTypes)
{
    if (m_markerFilterPanel != nullptr) {
        m_markerFilterPanel->setMarkerTypes(markerTypes);
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    m_apiClient.stop();
    m_dataCenter.close();
    m_mapWindow->hide();
    event->accept();
}

void MainWindow::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
}

} // namespace app
