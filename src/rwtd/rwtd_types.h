#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QMetaType>
#include <QString>
#include <QStringList>

#include <cstdint>

namespace rwtd {

constexpr quint16 DefaultPort = 8195;
constexpr quint16 TgcpCommandAck = 0x1002;
constexpr quint16 TgcpCommandData = 0x4013;

enum class TrafficDirection {
    Unknown,
    ClientToServer,
    ServerToClient,
};

inline QString trafficDirectionName(TrafficDirection direction)
{
    switch (direction) {
    case TrafficDirection::ClientToServer:
        return QStringLiteral("c2s");
    case TrafficDirection::ServerToClient:
        return QStringLiteral("s2c");
    case TrafficDirection::Unknown:
        break;
    }
    return QStringLiteral("unknown");
}

struct TgcpBaseHeader {
    quint16 headVersion = 0;
    quint16 bodyVersion = 0;
    quint16 command = 0;
    bool encrypted = false;
    quint32 sequence = 0;
    quint32 headLength = 0;
    quint32 bodyLength = 0;
};

struct TgcpPacket {
    QString flowId;
    TrafficDirection direction = TrafficDirection::Unknown;
    qsizetype streamOffset = 0;
    TgcpBaseHeader base;
    QByteArray headExtend;
    QByteArray body;
};

struct DecryptedRecord {
    quint32 opcode = 0;
    QByteArray payload;
};

struct DecodedAction {
    QString flowId;
    TrafficDirection direction = TrafficDirection::Unknown;
    quint32 opcode = 0;
    QString messageName;
    QJsonObject payload;
};

struct CaptureDeviceInfo {
    QString name;
    QString description;
    QStringList addresses;
    bool loopback = false;
};

} // namespace rwtd

Q_DECLARE_METATYPE(rwtd::DecodedAction)
Q_DECLARE_METATYPE(rwtd::CaptureDeviceInfo)
Q_DECLARE_METATYPE(rwtd::TrafficDirection)
