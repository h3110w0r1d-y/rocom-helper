#pragma once

#include "opcode_filter.h"
#include "tgcp_parser.h"

#include <QObject>
#include <QHash>
#include <QMutex>
#include <QSet>

#include <memory>

namespace pcpp {
class PcapLiveDevice;
class RawPacket;
class TcpReassembly;
class TcpStreamData;
struct ConnectionData;
} // namespace pcpp

namespace rwtd {

class LiveCaptureService : public QObject {
    Q_OBJECT

public:
    explicit LiveCaptureService(QObject *parent = nullptr);
    ~LiveCaptureService() override;

    static QList<CaptureDeviceInfo> availableDevices();

    bool start(const QString &deviceName, quint16 port = DefaultPort);
    void stop();
    bool isRunning() const;
    void setOpcodeFilter(OpcodeFilter *filter);
    void preloadFlowKeys(const QHash<QString, QByteArray> &keys);

public slots:
    void updateEnabledOpcodes(const QSet<quint32> &opcodes);

signals:
    void actionDecoded(const rwtd::DecodedAction &action);
    void flowKeyEstablished(const QString &flowId, const QByteArray &key);
    void statusChanged(const QString &message);
    void errorOccurred(const QString &message);

private:
    struct DirectionInfo {
        QString flowId;
        TrafficDirection direction = TrafficDirection::Unknown;
        bool valid = false;
    };

    static void onPacketArrives(pcpp::RawPacket *packet, pcpp::PcapLiveDevice *device, void *userCookie);
    static void onTcpMessageReady(int8_t side, const pcpp::TcpStreamData &tcpData, void *userCookie);
    void handleTcpData(int8_t side, const pcpp::TcpStreamData &tcpData);
    void handleTgcpPacket(const TgcpPacket &packet);
    DirectionInfo classify(int8_t side, const pcpp::ConnectionData &connectionData) const;
    QString streamKey(const QString &flowId, TrafficDirection direction) const;
    QByteArray keyForFlow(const QString &flowId);
    void rememberFlowKey(const QString &flowId, const QByteArray &key);

    mutable QMutex m_mutex;
    std::unique_ptr<pcpp::PcapLiveDevice> m_ownedDevice;
    pcpp::PcapLiveDevice *m_device = nullptr;
    std::unique_ptr<pcpp::TcpReassembly> m_reassembly;
    quint16 m_port = DefaultPort;
    bool m_running = false;

    OpcodeFilter *m_opcodeFilter = nullptr;
    QSet<quint32> m_enabledOpcodes;
    QHash<QString, QByteArray> m_flowKeys;
    QHash<QString, TgcpStreamParser> m_streamParsers;
    QSet<QString> m_processedPackets;
};

} // namespace rwtd
