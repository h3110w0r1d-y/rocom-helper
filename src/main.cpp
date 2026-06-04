#include <QApplication>
#include <QComboBox>
#include <QDebug>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHostAddress>
#include <QJsonDocument>
#include <QMainWindow>
#include <QNetworkAddressEntry>
#include <QNetworkInterface>
#include <QPushButton>
#include <QSpinBox>
#include <QUdpSocket>
#include <QVBoxLayout>

#include "events/event_dispatcher.h"
#include "http/http_server_service.h"
#include "http/sse_broadcaster.h"
#include "rwtd/live_capture_service.h"
#include "storage/database_service.h"
#include "traffic/traffic_event_mapper.h"

class MainWindow : public QMainWindow {
public:
    MainWindow()
    {
        auto *central = new QWidget(this);
        auto *layout = new QVBoxLayout(central);

        auto *form = new QFormLayout();
        m_deviceCombo = new QComboBox(central);
        m_portSpin = new QSpinBox(central);
        m_portSpin->setRange(1, 65535);
        m_portSpin->setValue(rwtd::DefaultPort);

        form->addRow(QStringLiteral("网卡"), m_deviceCombo);
        form->addRow(QStringLiteral("端口"), m_portSpin);
        layout->addLayout(form);

        auto *buttonRow = new QHBoxLayout();
        m_startButton = new QPushButton(QStringLiteral("开始监听"), central);
        m_stopButton = new QPushButton(QStringLiteral("停止"), central);
        m_stopButton->setEnabled(false);
        buttonRow->addWidget(m_startButton);
        buttonRow->addWidget(m_stopButton);
        layout->addLayout(buttonRow);

        setCentralWidget(central);
        resize(520, 180);
        setWindowTitle(QStringLiteral("Roco Helper"));

        populateDevices();
        connectSignals();
        initializeServices();

        if (!m_capture.loadSchemas(QStringLiteral(":/rwtd"))) {
            qWarning().noquote() << QStringLiteral("schema 加载失败");
        }
    }

private:
    struct DefaultNetworkRoute {
        QString interfaceName;
        QString localAddress;
    };

    static DefaultNetworkRoute detectDefaultNetworkRoute()
    {
        DefaultNetworkRoute route;

        QUdpSocket socket;
        socket.connectToHost(QHostAddress(QStringLiteral("8.8.8.8")), 53);
        const QHostAddress localAddress = socket.localAddress();
        if (localAddress.isNull() || localAddress.isLoopback()) {
            return route;
        }

        route.localAddress = localAddress.toString();
        const QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
        for (const QNetworkInterface &interface : interfaces) {
            const QNetworkInterface::InterfaceFlags flags = interface.flags();
            if (!flags.testFlag(QNetworkInterface::IsUp)
                || !flags.testFlag(QNetworkInterface::IsRunning)
                || flags.testFlag(QNetworkInterface::IsLoopBack)) {
                continue;
            }

            for (const QNetworkAddressEntry &entry : interface.addressEntries()) {
                if (entry.ip() == localAddress) {
                    route.interfaceName = interface.name();
                    return route;
                }
            }
        }

        return route;
    }

    static QString displayNameForDevice(const rwtd::CaptureDeviceInfo &device)
    {
        const QNetworkInterface interface = QNetworkInterface::interfaceFromName(device.name);
        QString label = device.name;
        if (interface.isValid() && !interface.humanReadableName().isEmpty()
            && interface.humanReadableName() != device.name) {
            label += QStringLiteral(" - %1").arg(interface.humanReadableName());
        } else if (!device.description.isEmpty()) {
            label += QStringLiteral(" - %1").arg(device.description);
        }
        return label;
    }

    void populateDevices()
    {
        const QList<rwtd::CaptureDeviceInfo> devices = rwtd::LiveCaptureService::availableDevices();
        const DefaultNetworkRoute defaultRoute = detectDefaultNetworkRoute();
        int defaultDeviceIndex = -1;

        for (const rwtd::CaptureDeviceInfo &device : devices) {
            const QString label = displayNameForDevice(device);
            const QString tooltip = device.addresses.isEmpty()
                ? device.description
                : device.addresses.join(QStringLiteral(", "));
            m_deviceCombo->addItem(label, device.name);
            m_deviceCombo->setItemData(m_deviceCombo->count() - 1, tooltip, Qt::ToolTipRole);

            const bool matchesDefaultName = !defaultRoute.interfaceName.isEmpty()
                && device.name == defaultRoute.interfaceName;
            const bool matchesDefaultAddress = !defaultRoute.localAddress.isEmpty()
                && device.addresses.contains(defaultRoute.localAddress);
            if (defaultDeviceIndex < 0 && (matchesDefaultName || matchesDefaultAddress)) {
                defaultDeviceIndex = m_deviceCombo->count() - 1;
            }
        }

        if (defaultDeviceIndex >= 0) {
            m_deviceCombo->setCurrentIndex(defaultDeviceIndex);
            qInfo().noquote() << QStringLiteral("已默认选择路由网卡: %1").arg(m_deviceCombo->currentText());
        }
    }

    void connectSignals()
    {
        connect(m_startButton, &QPushButton::clicked, this, [this] {
            const QString deviceName = m_deviceCombo->currentData().toString();
            if (deviceName.isEmpty()) {
                qWarning().noquote() << QStringLiteral("没有可用网卡");
                return;
            }
            if (m_capture.start(deviceName, static_cast<quint16>(m_portSpin->value()))) {
                m_startButton->setEnabled(false);
                m_stopButton->setEnabled(true);
            }
        });

        connect(m_stopButton, &QPushButton::clicked, this, [this] {
            m_capture.stop();
            m_startButton->setEnabled(true);
            m_stopButton->setEnabled(false);
        });

        connect(&m_capture, &rwtd::LiveCaptureService::statusChanged, this, [this](const QString &message) {
            qInfo().noquote() << message;
        });

        connect(&m_capture, &rwtd::LiveCaptureService::errorOccurred, this, [this](const QString &message) {
            qWarning().noquote() << QStringLiteral("[错误] %1").arg(message);
        });

        connect(&m_capture, &rwtd::LiveCaptureService::actionDecoded,
                &m_trafficEventMapper, &app::TrafficEventMapper::mapDecodedAction);

        connect(&m_trafficEventMapper, &app::TrafficEventMapper::eventCreated,
                &m_eventDispatcher, &app::EventDispatcher::dispatch);
        connect(&m_httpServer, &app::HttpServerService::eventCreated,
                &m_eventDispatcher, &app::EventDispatcher::dispatch);

        connect(&m_eventDispatcher, &app::EventDispatcher::eventDispatched,
                &m_database, &app::DatabaseService::handleEvent);
        connect(&m_eventDispatcher, &app::EventDispatcher::eventDispatched,
                &m_sseBroadcaster, &app::SseBroadcaster::handleEvent);
        connect(&m_eventDispatcher, &app::EventDispatcher::eventDispatched,
                this, [this](const app::AppEvent &event) {
                    if (!event.flags.testFlag(app::EventFlag::UpdateUi)) {
                        return;
                    }
                    const QByteArray json = QJsonDocument(app::appEventToJson(event)).toJson(QJsonDocument::Compact);
                    qInfo().noquote() << QStringLiteral("[UI example] %1 %2")
                                             .arg(app::eventTypeName(event.type), QString::fromUtf8(json));
                });

        connect(&m_sseBroadcaster, &app::SseBroadcaster::ssePayloadReady,
                &m_httpServer, &app::HttpServerService::rememberLastSsePayload);

        connect(&m_database, &app::DatabaseService::eventPersisted,
                this, [this](const app::AppEvent &event, qint64 id) {
                    qInfo().noquote() << QStringLiteral("[DB example] event_id=%1 %2")
                                             .arg(id)
                                             .arg(app::eventTypeName(event.type));
                });
        connect(&m_database, &app::DatabaseService::errorOccurred, this, [this](const QString &message) {
            qWarning().noquote() << QStringLiteral("[数据库错误] %1").arg(message);
        });

        connect(&m_httpServer, &app::HttpServerService::statusChanged, this, [this](const QString &message) {
            qInfo().noquote() << message;
        });
        connect(&m_httpServer, &app::HttpServerService::errorOccurred, this, [this](const QString &message) {
            qWarning().noquote() << QStringLiteral("[HTTP错误] %1").arg(message);
        });
    }

    void initializeServices()
    {
        if (!m_database.open()) {
            return;
        }
        m_httpServer.start();
    }

    QComboBox *m_deviceCombo = nullptr;
    QSpinBox *m_portSpin = nullptr;
    QPushButton *m_startButton = nullptr;
    QPushButton *m_stopButton = nullptr;
    rwtd::LiveCaptureService m_capture;
    app::EventDispatcher m_eventDispatcher;
    app::TrafficEventMapper m_trafficEventMapper;
    app::DatabaseService m_database;
    app::SseBroadcaster m_sseBroadcaster;
    app::HttpServerService m_httpServer{&m_database};
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    MainWindow window;
    window.show();

    return app.exec();
}
