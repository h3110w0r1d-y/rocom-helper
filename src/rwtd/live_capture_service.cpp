#include "live_capture_service.h"

#include "record_decoder.h"

#include <QCoreApplication>
#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QMutexLocker>
#include <QStandardPaths>
#include <QTextStream>

#include <cstdio>

#include <Packet.h>
#include <PcapFilter.h>
#include <PcapLiveDevice.h>
#include <PcapLiveDeviceList.h>
#include <TcpReassembly.h>

#if defined(Q_OS_WIN)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <iphlpapi.h>
#endif

namespace rwtd {
namespace {

QString ipToString(const pcpp::IPAddress &ip)
{
    return QString::fromStdString(ip.toString());
}

QString defaultKeyPath()
{
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dataDir.isEmpty()) {
        dataDir = QCoreApplication::applicationDirPath();
    }
    return QDir(dataDir).filePath(QStringLiteral("traffic_key.json"));
}

QString defaultRouteInterfaceName()
{
#if defined(__APPLE__)
    FILE *pipe = popen("route -n get default 2>/dev/null", "r");
    if (pipe == nullptr) {
        return {};
    }
    char buffer[512];
    QString result;
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        const QString line = QString::fromUtf8(buffer).trimmed();
        if (line.startsWith(QStringLiteral("interface:"))) {
            result = line.section(QLatin1Char(':'), 1).trimmed();
            break;
        }
    }
    pclose(pipe);
    return result;
#elif defined(__linux__)
    QFile routeFile(QStringLiteral("/proc/net/route"));
    if (!routeFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    QTextStream in(&routeFile);
    in.readLine();
    while (!in.atEnd()) {
        const QStringList parts = in.readLine().split(QLatin1Char('\t'));
        if (parts.size() >= 2 && parts.at(1) == QStringLiteral("00000000")) {
            return parts.at(0);
        }
    }
    return {};
#else
    return {};
#endif
}

#if defined(Q_OS_WIN)
QString normalizedWindowsAdapterGuid(QString value)
{
    value = value.trimmed();
    const qsizetype openBrace = value.indexOf(QLatin1Char('{'));
    const qsizetype closeBrace = value.indexOf(QLatin1Char('}'), openBrace + 1);
    if (openBrace >= 0 && closeBrace > openBrace) {
        value = value.mid(openBrace + 1, closeBrace - openBrace - 1);
    }
    value.remove(QLatin1Char('{'));
    value.remove(QLatin1Char('}'));
    return value.toUpper();
}

QHash<QString, QString> windowsAdapterFriendlyNames()
{
    QHash<QString, QString> result;
    ULONG bufferLength = 15 * 1024;
    QByteArray buffer(static_cast<int>(bufferLength), Qt::Uninitialized);

    auto *addresses = reinterpret_cast<IP_ADAPTER_ADDRESSES *>(buffer.data());
    ULONG status = GetAdaptersAddresses(
        AF_UNSPEC,
        GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
        nullptr,
        addresses,
        &bufferLength);
    if (status == ERROR_BUFFER_OVERFLOW) {
        buffer.resize(static_cast<int>(bufferLength));
        addresses = reinterpret_cast<IP_ADAPTER_ADDRESSES *>(buffer.data());
        status = GetAdaptersAddresses(
            AF_UNSPEC,
            GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
            nullptr,
            addresses,
            &bufferLength);
    }
    if (status != NO_ERROR) {
        return result;
    }

    for (auto *adapter = addresses; adapter != nullptr; adapter = adapter->Next) {
        const QString guid = adapter->AdapterName == nullptr
            ? QString()
            : normalizedWindowsAdapterGuid(QString::fromLocal8Bit(adapter->AdapterName));
        if (guid.isEmpty()) {
            continue;
        }

        QString friendlyName = adapter->FriendlyName == nullptr
            ? QString()
            : QString::fromWCharArray(adapter->FriendlyName).trimmed();
        if (friendlyName.isEmpty() && adapter->Description != nullptr) {
            friendlyName = QString::fromWCharArray(adapter->Description).trimmed();
        }
        if (!friendlyName.isEmpty()) {
            result.insert(guid, friendlyName);
        }
    }
    return result;
}

QString windowsFriendlyNameForDevice(const QString &deviceName, const QHash<QString, QString> &friendlyNames)
{
    const QString guid = normalizedWindowsAdapterGuid(deviceName);
    if (guid.isEmpty()) {
        return {};
    }
    return friendlyNames.value(guid);
}
#endif

QString captureDeviceDisplayName(
    const pcpp::PcapLiveDevice *device,
#if defined(Q_OS_WIN)
    const QHash<QString, QString> &windowsFriendlyNames
#else
    const QHash<QString, QString> &
#endif
)
{
    if (device == nullptr) {
        return {};
    }

    const QString deviceName = QString::fromStdString(device->getName()).trimmed();
    const QString description = QString::fromStdString(device->getDesc()).trimmed();
#if defined(Q_OS_WIN)
    const QString friendlyName = windowsFriendlyNameForDevice(deviceName, windowsFriendlyNames);
    if (!friendlyName.isEmpty()) {
        return friendlyName;
    }
#endif
    if (!description.isEmpty()) {
        return description;
    }
    return deviceName;
}

bool isDefaultRouteDevice(const pcpp::PcapLiveDevice *device, const QString &defaultInterface)
{
    if (device == nullptr || device->getLoopback()) {
        return false;
    }
    if (device->getDefaultGateway() == pcpp::IPv4Address::Zero) {
        return false;
    }
    if (!defaultInterface.isEmpty()) {
        return device->getName() == defaultInterface.toStdString();
    }
    return true;
}

} // namespace

LiveCaptureService::LiveCaptureService(QObject *parent)
    : QObject(parent)
    , m_keyCache(defaultKeyPath())
{
    qRegisterMetaType<rwtd::DecodedAction>("rwtd::DecodedAction");
}

LiveCaptureService::~LiveCaptureService()
{
    stop();
}

QList<CaptureDeviceInfo> LiveCaptureService::availableDevices()
{
    QList<CaptureDeviceInfo> result;
    const auto &devices = pcpp::PcapLiveDeviceList::getInstance().getPcapLiveDevicesList();
    const QString defaultInterface = defaultRouteInterfaceName();
#if defined(Q_OS_WIN)
    const QHash<QString, QString> windowsFriendlyNames = windowsAdapterFriendlyNames();
#else
    const QHash<QString, QString> windowsFriendlyNames;
#endif
    for (const pcpp::PcapLiveDevice *device : devices) {
        CaptureDeviceInfo info;
        info.name = QString::fromStdString(device->getName());
        info.description = QString::fromStdString(device->getDesc());
        info.displayName = captureDeviceDisplayName(device, windowsFriendlyNames);
        for (const auto &address : device->getIPAddresses()) {
            info.addresses.append(ipToString(address));
        }
        info.loopback = device->getLoopback();
        info.isDefaultGateway = isDefaultRouteDevice(device, defaultInterface);
        result.append(info);
    }
    return result;
}

bool LiveCaptureService::loadSchemas(const QString &dataDir)
{
    QMutexLocker locker(&m_mutex);
    if (!m_protobuf.load(dataDir)) {
        emit errorOccurred(m_protobuf.errorString());
        return false;
    }
    m_knownOpcodes = m_protobuf.knownOpcodes();
    emit statusChanged(QStringLiteral("已加载 protobuf schema: %1 个 opcode").arg(m_knownOpcodes.size()));
    return true;
}

bool LiveCaptureService::start(const QString &deviceName, quint16 port)
{
    QMutexLocker locker(&m_mutex);
    if (m_running) {
        return true;
    }
    if (!m_protobuf.isAvailable()) {
        emit errorOccurred(QStringLiteral("schema 尚未加载"));
        return false;
    }

    m_device = pcpp::PcapLiveDeviceList::getInstance().getDeviceByName(deviceName.toStdString());
    if (m_device == nullptr) {
        emit errorOccurred(QStringLiteral("找不到网卡: %1").arg(deviceName));
        return false;
    }

    pcpp::PcapLiveDevice::DeviceConfiguration config(
        pcpp::PcapLiveDevice::Promiscuous,
        100,
        4 * 1024 * 1024,
        pcpp::PcapLiveDevice::PCPP_INOUT);
    if (!m_device->open(config)) {
        emit errorOccurred(QStringLiteral("打开网卡失败，可能需要抓包权限: %1").arg(deviceName));
        m_device = nullptr;
        return false;
    }

    pcpp::BPFStringFilter portFilter(QStringLiteral("tcp port %1").arg(port).toStdString());
    if (!m_device->setFilter(portFilter)) {
        emit errorOccurred(QStringLiteral("设置抓包过滤器失败: tcp port %1").arg(port));
        m_device->close();
        m_device = nullptr;
        return false;
    }

    m_port = port;
    m_flowKeys.clear();
    m_streamParsers.clear();
    m_processedPackets.clear();
    m_reassembly = std::make_unique<pcpp::TcpReassembly>(
        &LiveCaptureService::onTcpMessageReady,
        this);

    if (!m_device->startCapture(&LiveCaptureService::onPacketArrives, this)) {
        emit errorOccurred(QStringLiteral("启动抓包失败: %1").arg(deviceName));
        m_reassembly.reset();
        m_device->close();
        m_device = nullptr;
        return false;
    }

    m_running = true;
    emit statusChanged(QStringLiteral("正在监听 %1，端口 %2").arg(deviceName).arg(port));
    return true;
}

void LiveCaptureService::stop()
{
    QMutexLocker locker(&m_mutex);
    if (!m_running && m_device == nullptr) {
        return;
    }

    if (m_device != nullptr) {
        m_device->stopCapture();
    }
    if (m_reassembly) {
        m_reassembly->closeAllConnections();
        m_reassembly.reset();
    }
    if (m_device != nullptr) {
        m_device->close();
        m_device = nullptr;
    }
    m_running = false;
    emit statusChanged(QStringLiteral("监听已停止"));
}

bool LiveCaptureService::isRunning() const
{
    QMutexLocker locker(&m_mutex);
    return m_running;
}

void LiveCaptureService::onPacketArrives(pcpp::RawPacket *packet, pcpp::PcapLiveDevice *, void *userCookie)
{
    auto *service = static_cast<LiveCaptureService *>(userCookie);
    if (service == nullptr || packet == nullptr) {
        return;
    }

    QMutexLocker locker(&service->m_mutex);
    if (service->m_reassembly) {
        service->m_reassembly->reassemblePacket(packet);
    }
}

void LiveCaptureService::onTcpMessageReady(int8_t side, const pcpp::TcpStreamData &tcpData, void *userCookie)
{
    auto *service = static_cast<LiveCaptureService *>(userCookie);
    if (service != nullptr) {
        service->handleTcpData(side, tcpData);
    }
}

void LiveCaptureService::handleTcpData(int8_t side, const pcpp::TcpStreamData &tcpData)
{
    const DirectionInfo info = classify(side, tcpData.getConnectionData());
    if (!info.valid || tcpData.getDataLength() == 0) {
        return;
    }

    const QString parserKey = streamKey(info.flowId, info.direction);
    if (tcpData.isBytesMissing()) {
        m_streamParsers[parserKey].reset();
        emit statusChanged(QStringLiteral("TCP 流缺失 %1 字节，已重置流缓冲: %2 %3")
                               .arg(tcpData.getMissingByteCount())
                               .arg(info.flowId, trafficDirectionName(info.direction)));
    }

    const QByteArray chunk(reinterpret_cast<const char *>(tcpData.getData()), static_cast<qsizetype>(tcpData.getDataLength()));
    QList<TgcpPacket> packets = m_streamParsers[parserKey].feed(info.flowId, info.direction, chunk);
    for (const TgcpPacket &packet : packets) {
        handleTgcpPacket(packet);
    }
}

void LiveCaptureService::handleTgcpPacket(const TgcpPacket &packet)
{
    const QString packetKey = QStringLiteral("%1|%2|%3")
                                  .arg(packet.flowId, trafficDirectionName(packet.direction))
                                  .arg(packet.streamOffset);
    if (m_processedPackets.contains(packetKey)) {
        return;
    }

    if (packet.base.command == TgcpCommandAck) {
        const QByteArray key = extractTgcpAckKey(packet);
        if (key.size() == 16) {
            rememberFlowKey(packet.flowId, key);
            emit statusChanged(QStringLiteral("已捕获 AES key: %1").arg(packet.flowId));
        }
        m_processedPackets.insert(packetKey);
        return;
    }

    if (packet.base.command != TgcpCommandData) {
        m_processedPackets.insert(packetKey);
        return;
    }

    const QByteArray key = keyForFlow(packet.flowId);
    if (key.size() != 16) {
        return;
    }

    const std::optional<DecryptedRecord> record = DataRecordDecoder::decryptAndParse(key, packet, m_knownOpcodes);
    m_processedPackets.insert(packetKey);
    if (!record.has_value()) {
        return;
    }

    QJsonObject payload;
    if (!m_protobuf.decode(record->opcode, record->payload, &payload)) {
        return;
    }

    DecodedAction action;
    action.flowId = packet.flowId;
    action.direction = packet.direction;
    action.opcode = record->opcode;
    action.messageName = m_protobuf.messageNameForOpcode(record->opcode);
    action.payload = payload;
    emit actionDecoded(action);
}

LiveCaptureService::DirectionInfo LiveCaptureService::classify(int8_t side, const pcpp::ConnectionData &connectionData) const
{
    QString srcIp;
    QString dstIp;
    quint16 srcPort = 0;
    quint16 dstPort = 0;

    if (side == 0) {
        srcIp = ipToString(connectionData.srcIP);
        dstIp = ipToString(connectionData.dstIP);
        srcPort = connectionData.srcPort;
        dstPort = connectionData.dstPort;
    } else {
        srcIp = ipToString(connectionData.dstIP);
        dstIp = ipToString(connectionData.srcIP);
        srcPort = connectionData.dstPort;
        dstPort = connectionData.srcPort;
    }

    DirectionInfo info;
    QString clientIp;
    QString serverIp;
    quint16 clientPort = 0;
    quint16 serverPort = 0;
    if (dstPort == m_port) {
        clientIp = srcIp;
        clientPort = srcPort;
        serverIp = dstIp;
        serverPort = dstPort;
        info.direction = TrafficDirection::ClientToServer;
    } else if (srcPort == m_port) {
        clientIp = dstIp;
        clientPort = dstPort;
        serverIp = srcIp;
        serverPort = srcPort;
        info.direction = TrafficDirection::ServerToClient;
    } else {
        return info;
    }

    info.flowId = QStringLiteral("%1:%2->%3:%4").arg(clientIp).arg(clientPort).arg(serverIp).arg(serverPort);
    info.valid = true;
    return info;
}

QString LiveCaptureService::streamKey(const QString &flowId, TrafficDirection direction) const
{
    return flowId + QLatin1Char('|') + trafficDirectionName(direction);
}

QByteArray LiveCaptureService::keyForFlow(const QString &flowId)
{
    QByteArray key = m_flowKeys.value(flowId);
    if (key.isEmpty()) {
        key = m_keyCache.keyForFlow(flowId);
        if (key.size() == 16) {
            m_flowKeys.insert(flowId, key);
        }
    }
    return key;
}

void LiveCaptureService::rememberFlowKey(const QString &flowId, const QByteArray &key)
{
    if (key.size() != 16) {
        return;
    }
    m_flowKeys.insert(flowId, key);
    m_keyCache.rememberKey(flowId, key);
}

} // namespace rwtd
