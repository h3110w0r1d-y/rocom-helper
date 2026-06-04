#include "main_window.h"

#include <QFileDialog>
#include <QCloseEvent>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QFile>
#include <QJsonDocument>
#include <QLayoutItem>
#include <QLineEdit>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QShowEvent>
#include <QVBoxLayout>
#include <QXmlStreamReader>

namespace app {
namespace {

const QSize MarkerTypeControlIconSize(20, 20);

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QWidget(parent)
    , m_mapWindow(new MapWindow(&m_dataCenter))
    , m_catchWindow(new CatchWindow(&m_dataCenter))
{
    setWindowTitle(QStringLiteral("洛克助手"));
    resize(420, 420);

    buildUi();
    connectSignals();
    initializeServices();

    if (!m_capture.loadSchemas(QStringLiteral(":/rwtd"))) {
        m_statusLabel->setText(QStringLiteral("schema 加载失败"));
    }

    m_saveTimer.setParent(this);
    connect(&m_saveTimer, &QTimer::timeout, &m_dataCenter, &DataCenter::saveIfDirty);
    m_saveTimer.start(5000);

    m_dataCenter.load();
    if (m_database.isOpen()) {
        m_dataCenter.loadPersistentMarkers(m_database.queryMapMarkers());
    }
    scheduleTypeLayoutRefresh();
    m_mapWindow->show();
    QTimer::singleShot(0, this, &MainWindow::toggleTraffic);
}

MainWindow::~MainWindow()
{
    m_capture.stop();
    m_httpServer.stop();
    m_dataCenter.close();
    delete m_mapWindow;
    delete m_catchWindow;
}

void MainWindow::buildUi()
{
    m_ifaceCombo = new QComboBox(this);
    m_ifaceCombo->setEditable(true);
    m_ifaceCombo->setInsertPolicy(QComboBox::NoInsert);
    m_ifaceCombo->lineEdit()->setPlaceholderText(QStringLiteral("例如 en0"));
    m_refreshIfacesButton = new QPushButton(QStringLiteral("刷新"), this);

    auto *trafficGroup = new QGroupBox(QStringLiteral("流量监控"), this);
    auto *trafficForm = new QFormLayout(trafficGroup);
    trafficForm->setVerticalSpacing(4);
    trafficForm->setContentsMargins(8, 8, 8, 8);
    auto *ifaceRow = new QHBoxLayout();
    ifaceRow->setContentsMargins(0, 0, 0, 0);
    ifaceRow->setSpacing(4);
    ifaceRow->addWidget(m_ifaceCombo, 1);
    ifaceRow->addWidget(m_refreshIfacesButton);
    trafficForm->addRow(QStringLiteral("网卡"), ifaceRow);

    m_trafficButton = new QPushButton(QStringLiteral("开启流量监控"), this);
    m_statusLabel = new QLabel(QStringLiteral("未运行"), this);
    auto *trafficButtons = new QHBoxLayout();
    trafficButtons->setContentsMargins(0, 0, 0, 0);
    trafficButtons->setSpacing(4);
    trafficButtons->addWidget(m_trafficButton);
    trafficButtons->addWidget(m_statusLabel, 1);
    trafficForm->addRow(trafficButtons);

    auto *mapGroup = new QGroupBox(QStringLiteral("小地图"), this);
    auto *mapLayout = new QGridLayout(mapGroup);
    m_showMapButton = new QPushButton(QStringLiteral("显示地图"), this);
    m_topCheckbox = new QCheckBox(QStringLiteral("置顶"), this);
    m_miniMapCheckbox = new QCheckBox(QStringLiteral("小地图模式"), this);
    m_mapCombo = new QComboBox(this);
    m_mapCombo->setToolTip(QStringLiteral("当前地图"));
    m_layerCombo = new QComboBox(this);
    m_layerCombo->setToolTip(QStringLiteral("当前层级"));
    m_playerPositionLogCheckbox = new QCheckBox(QStringLiteral("记录坐标日志"), this);
    m_trailCheckbox = new QCheckBox(QStringLiteral("显示轨迹"), this);
    m_trailWidthSpin = new QSpinBox(this);
    m_trailWidthSpin->setRange(1, 500);
    m_trailWidthSpin->setValue(260);
    m_trailWidthSpin->setSuffix(QStringLiteral(" px"));
    m_clearTrailButton = new QPushButton(QStringLiteral("清空轨迹"), this);
    m_importPathButton = new QPushButton(QStringLiteral("导入path"), this);
    m_clearPathButton = new QPushButton(QStringLiteral("清空path"), this);
    m_typeContainer = new FlowWidget(this);
    m_typeLayout = new FlowLayout(m_typeContainer, 0, 6);
    m_typeContainer->setLayout(m_typeLayout);

    mapLayout->addWidget(m_showMapButton, 0, 0);
    mapLayout->addWidget(m_topCheckbox, 0, 1);
    mapLayout->addWidget(m_miniMapCheckbox, 1, 0, 1, 2);
    mapLayout->addWidget(m_mapCombo, 2, 0);
    mapLayout->addWidget(m_layerCombo, 2, 1);
    mapLayout->addWidget(m_playerPositionLogCheckbox, 3, 0, 1, 2);
    mapLayout->addWidget(m_trailCheckbox, 4, 0, 1, 2);
    mapLayout->addWidget(new QLabel(QStringLiteral("轨迹宽度"), this), 5, 0);
    mapLayout->addWidget(m_trailWidthSpin, 5, 1);
    mapLayout->addWidget(m_clearTrailButton, 6, 0, 1, 2);
    mapLayout->addWidget(m_importPathButton, 7, 0);
    mapLayout->addWidget(m_clearPathButton, 7, 1);
    mapLayout->addWidget(m_typeContainer, 8, 0, 1, 2);

    auto *s2Group = new QGroupBox(QStringLiteral("S2赛季功能"), this);
    auto *s2Layout = new QGridLayout(s2Group);
    m_showCatchButton = new QPushButton(QStringLiteral("捕捉日志"), this);
    s2Layout->addWidget(m_showCatchButton, 0, 1);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(trafficGroup);
    layout->addWidget(mapGroup);
    layout->addWidget(s2Group);
    layout->addStretch(1);

    populateDevices();
}

void MainWindow::connectSignals()
{
    connect(m_trafficButton, &QPushButton::clicked, this, &MainWindow::toggleTraffic);
    connect(m_refreshIfacesButton, &QPushButton::clicked, this, &MainWindow::populateDevices);

    connect(m_showMapButton, &QPushButton::clicked, this, &MainWindow::showMap);
    connect(m_showCatchButton, &QPushButton::clicked, this, &MainWindow::showCatch);
    connect(m_topCheckbox, &QCheckBox::toggled, m_mapWindow, &MapWindow::setAlwaysOnTop);
    connect(m_miniMapCheckbox, &QCheckBox::toggled, m_mapWindow, &MapWindow::setMiniMapMode);
    connect(m_mapWindow->miniMapCheckbox(), &QCheckBox::toggled, m_miniMapCheckbox, &QCheckBox::setChecked);
    connect(m_miniMapCheckbox, &QCheckBox::toggled, m_mapWindow->miniMapCheckbox(), &QCheckBox::setChecked);
    connect(m_mapCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] {
        selectMapFromCombo();
    });
    connect(m_layerCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] {
        selectLayerFromCombo();
    });
    connect(m_playerPositionLogCheckbox, &QCheckBox::toggled, &m_dataCenter, &DataCenter::setPlayerPositionLogEnabled);
    connect(m_trailCheckbox, &QCheckBox::toggled, m_mapWindow, &MapWindow::setTrailRecordingEnabled);
    connect(m_trailWidthSpin, &QSpinBox::valueChanged, m_mapWindow, &MapWindow::setTrailWidth);
    connect(m_clearTrailButton, &QPushButton::clicked, m_mapWindow, &MapWindow::clearTrail);
    connect(m_importPathButton, &QPushButton::clicked, this, &MainWindow::importPathOverlay);
    connect(m_clearPathButton, &QPushButton::clicked, m_mapWindow, &MapWindow::clearPathOverlays);

    connect(&m_dataCenter, &DataCenter::baseStateLoaded, this, &MainWindow::onBaseStateLoaded);
    connect(&m_dataCenter, &DataCenter::stateLoaded, this, &MainWindow::onMapStateLoaded);
    connect(&m_dataCenter, &DataCenter::mapChanged, this, &MainWindow::onMapChanged);
    connect(&m_dataCenter, &DataCenter::layerChanged, this, &MainWindow::onLayerChanged);
    connect(&m_dataCenter, &DataCenter::markerTypesChanged, this, &MainWindow::renderMarkerTypeControls);
    connect(&m_dataCenter, &DataCenter::shinyPetDetected, this, &MainWindow::showShinyAlert);

    connect(&m_capture, &rwtd::LiveCaptureService::statusChanged, this, [this](const QString &message) {
        m_statusLabel->setText(message);
    });
    connect(&m_capture, &rwtd::LiveCaptureService::errorOccurred, this, &MainWindow::onTrafficError);
    connect(&m_capture, &rwtd::LiveCaptureService::actionDecoded, &m_trafficEventMapper, &TrafficEventMapper::mapDecodedAction);

    connect(&m_trafficEventMapper, &TrafficEventMapper::eventCreated, &m_eventDispatcher, &EventDispatcher::dispatch);
    connect(&m_httpServer, &HttpServerService::eventCreated, &m_eventDispatcher, &EventDispatcher::dispatch);
    connect(&m_eventDispatcher, &EventDispatcher::eventDispatched, &m_database, &DatabaseService::handleEvent);
    connect(&m_eventDispatcher, &EventDispatcher::eventDispatched, &m_sseBroadcaster, &SseBroadcaster::handleEvent);
    connect(&m_eventDispatcher, &EventDispatcher::eventDispatched, &m_dataCenter, &DataCenter::handleEvent);
    connect(&m_sseBroadcaster, &SseBroadcaster::ssePayloadReady, &m_httpServer, &HttpServerService::rememberLastSsePayload);
    connect(&m_dataCenter, &DataCenter::markerAdded, &m_database, &DatabaseService::upsertMarker);
    connect(&m_dataCenter, &DataCenter::markerUpdated, &m_database, &DatabaseService::upsertMarker);
    connect(&m_dataCenter, &DataCenter::markerRemoved, &m_database, &DatabaseService::deleteMarker);

    connect(&m_database, &DatabaseService::errorOccurred, this, [this](const QString &message) {
        m_statusLabel->setText(QStringLiteral("数据库错误: %1").arg(message));
    });
    connect(&m_httpServer, &HttpServerService::errorOccurred, this, [this](const QString &message) {
        m_statusLabel->setText(QStringLiteral("HTTP错误: %1").arg(message));
    });
}

void MainWindow::initializeServices()
{
    if (m_database.open()) {
        m_httpServer.start();
    }
}

void MainWindow::populateDevices()
{
    const QString currentDevice = m_ifaceCombo->currentData().toString().isEmpty()
        ? m_ifaceCombo->currentText().trimmed()
        : m_ifaceCombo->currentData().toString();
    const QList<rwtd::CaptureDeviceInfo> devices = rwtd::LiveCaptureService::availableDevices();
    int defaultIndex = -1;

    QSignalBlocker blocker(m_ifaceCombo);
    m_ifaceCombo->clear();
    for (int i = 0; i < devices.size(); ++i) {
        const rwtd::CaptureDeviceInfo &device = devices.at(i);
        m_ifaceCombo->addItem(device.name, device.name);
        if (device.isDefaultGateway && defaultIndex < 0) {
            defaultIndex = i;
        }
    }
    int targetIndex = currentDevice.isEmpty() ? -1 : m_ifaceCombo->findData(currentDevice);
    if (targetIndex >= 0) {
        m_ifaceCombo->setCurrentIndex(targetIndex);
    } else if (defaultIndex >= 0) {
        m_ifaceCombo->setCurrentIndex(defaultIndex);
    } else if (!currentDevice.isEmpty()) {
        m_ifaceCombo->setEditText(currentDevice);
    }
}

void MainWindow::toggleTraffic()
{
    if (m_capture.isRunning()) {
        m_capture.stop();
        m_trafficButton->setText(QStringLiteral("开启流量监控"));
        m_statusLabel->setText(QStringLiteral("已停止"));
        return;
    }

    const QString deviceName = m_ifaceCombo->currentData().toString().isEmpty()
        ? m_ifaceCombo->currentText().trimmed()
        : m_ifaceCombo->currentData().toString();
    if (deviceName.isEmpty()) {
        m_statusLabel->setText(QStringLiteral("没有可用网卡"));
        return;
    }
    if (m_capture.start(deviceName)) {
        m_trafficButton->setText(QStringLiteral("停止流量监控"));
        m_statusLabel->setText(QStringLiteral("运行中"));
    }
}

void MainWindow::onTrafficError(const QString &message)
{
    m_statusLabel->setText(QStringLiteral("错误: %1").arg(message));
    m_trafficButton->setText(QStringLiteral("开启流量监控"));
}

void MainWindow::showMap()
{
    m_mapWindow->show();
    m_mapWindow->raise();
    m_mapWindow->activateWindow();
}

void MainWindow::showCatch()
{
    m_catchWindow->applyState(m_dataCenter.catchSnapshot());
    m_catchWindow->show();
    m_catchWindow->raise();
    m_catchWindow->activateWindow();
}

void MainWindow::importPathOverlay()
{
    const QString filePath = QFileDialog::getOpenFileName(this, QStringLiteral("导入 path"), QString(), QStringLiteral("SVG 文件 (*.svg);;所有文件 (*)"));
    if (filePath.isEmpty()) {
        return;
    }
    const QStringList paths = extractSvgPaths(filePath);
    if (paths.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("导入 path"), QStringLiteral("没有找到可导入的 path。"));
        return;
    }
    const int count = m_mapWindow->setPathOverlays(paths);
    m_statusLabel->setText(QStringLiteral("已导入 %1 条 path").arg(count));
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
        if (reader.isStartElement() && reader.name().toString().endsWith(QStringLiteral("path"))) {
            const QString pathData = reader.attributes().value(QStringLiteral("d")).toString().trimmed();
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
    QSignalBlocker blocker(m_playerPositionLogCheckbox);
    m_playerPositionLogCheckbox->setChecked(state.playerPositionLogEnabled);
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
    while (m_typeLayout->count() > 0) {
        QLayoutItem *item = m_typeLayout->takeAt(0);
        if (item != nullptr) {
            if (QWidget *widget = item->widget()) {
                widget->deleteLater();
            }
            delete item;
        }
    }

    for (const MarkerTypeConfig &markerType : markerTypes) {
        auto *checkbox = new QCheckBox(markerType.name, this);
        checkbox->setIcon(QIcon(QStringLiteral(":/icon/") + markerType.icon));
        checkbox->setIconSize(MarkerTypeControlIconSize);
        checkbox->setChecked(markerType.visible);
        connect(checkbox, &QCheckBox::toggled, this, [this, key = markerType.key](bool checked) {
            m_dataCenter.setMarkerTypeVisible(key, checked);
        });
        m_typeLayout->addWidget(checkbox);

        for (const MarkerSubtypeConfig &subtype : markerType.subtypes) {
            const QString subtypeName = subtype.key.isEmpty() ? QStringLiteral("默认") : subtype.key;
            auto *subtypeCheckbox = new QCheckBox(subtypeName, this);
            subtypeCheckbox->setToolTip(QStringLiteral("%1/%2").arg(markerType.name, subtypeName));
            subtypeCheckbox->setChecked(subtype.visible);
            connect(subtypeCheckbox, &QCheckBox::toggled, this, [this, key = markerType.key, subtypeKey = subtype.key](bool checked) {
                m_dataCenter.setMarkerSubtypeVisible(key, subtypeKey, checked);
            });
            m_typeLayout->addWidget(subtypeCheckbox);
        }
    }
    m_typeContainer->refreshHeight();
    scheduleTypeLayoutRefresh();
}

void MainWindow::scheduleTypeLayoutRefresh()
{
    QTimer::singleShot(0, this, &MainWindow::refreshTypeLayout);
    QTimer::singleShot(50, this, &MainWindow::refreshTypeLayout);
}

void MainWindow::refreshTypeLayout()
{
    m_typeContainer->refreshHeight();
    if (layout() != nullptr) {
        layout()->activate();
    }
}

void MainWindow::showShinyAlert(const QJsonObject &payload)
{
    const QString message = payload.value(QStringLiteral("message")).toString(QStringLiteral("发现异色！！"));
    QStringList details;
    details << message;
    if (payload.contains(QStringLiteral("base_conf_id"))) {
        details << QStringLiteral("宠物配置 ID: %1").arg(payload.value(QStringLiteral("base_conf_id")).toInt());
    }
    if (!payload.value(QStringLiteral("attr_name")).toString().isEmpty()) {
        details << QStringLiteral("加成属性: %1").arg(payload.value(QStringLiteral("attr_name")).toString());
    }
    if (payload.contains(QStringLiteral("pet_rarity_type"))) {
        details << QStringLiteral("稀有类型: %1").arg(payload.value(QStringLiteral("pet_rarity_type")).toInt());
    }
    if (payload.contains(QStringLiteral("pet_mutation_type"))) {
        details << QStringLiteral("变异类型: %1").arg(payload.value(QStringLiteral("pet_mutation_type")).toInt());
    }

    auto *box = new QMessageBox(this);
    box->setIcon(QMessageBox::Warning);
    box->setWindowTitle(payload.value(QStringLiteral("title")).toString(QStringLiteral("异色提示")));
    box->setText(QStringLiteral("发现异色！！"));
    box->setInformativeText(details.join(QLatin1Char('\n')));
    box->setStandardButtons(QMessageBox::Ok);
    box->setWindowModality(Qt::NonModal);
    box->setWindowFlag(Qt::WindowStaysOnTopHint, true);
    connect(box, &QMessageBox::finished, this, [this, box] {
        forgetAlert(box);
    });
    m_alertWindows.append(box);
    box->show();
    box->raise();
    box->activateWindow();
}

void MainWindow::forgetAlert(QMessageBox *dialog)
{
    m_alertWindows.removeAll(dialog);
    dialog->deleteLater();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    m_capture.stop();
    m_dataCenter.close();
    m_mapWindow->hide();
    m_catchWindow->hide();
    event->accept();
}

void MainWindow::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    scheduleTypeLayoutRefresh();
}

} // namespace app
