#include <QApplication>
#include <QComboBox>
#include <QDebug>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QMainWindow>
#include <QPushButton>
#include <QSpinBox>
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
    void populateDevices()
    {
        const QList<rwtd::CaptureDeviceInfo> devices = rwtd::LiveCaptureService::availableDevices();
        int defaultIndex = -1;
        for (int i = 0; i < devices.size(); ++i) {
            const rwtd::CaptureDeviceInfo &device = devices.at(i);
            m_deviceCombo->addItem(device.name, device.name);
            if (device.isDefaultGateway && defaultIndex < 0) {
                defaultIndex = i;
            }
        }
        if (defaultIndex >= 0) {
            m_deviceCombo->setCurrentIndex(defaultIndex);
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
