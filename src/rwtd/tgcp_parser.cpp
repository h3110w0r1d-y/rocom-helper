#include "tgcp_parser.h"

#include <QtEndian>

namespace rwtd {
namespace {

constexpr char Magic0 = '\x33';
constexpr char Magic1 = '\x66';
constexpr qsizetype BaseHeaderLength = 21;
constexpr quint32 MaxPacketLength = 4 * 1024 * 1024;

quint16 readBe16(const QByteArray &data, qsizetype offset)
{
    return qFromBigEndian<quint16>(reinterpret_cast<const uchar *>(data.constData() + offset));
}

quint32 readBe32(const QByteArray &data, qsizetype offset)
{
    return qFromBigEndian<quint32>(reinterpret_cast<const uchar *>(data.constData() + offset));
}

bool isValidHeader(const TgcpBaseHeader &header)
{
    if (header.command == 0) {
        return false;
    }
    if (header.headLength < BaseHeaderLength) {
        return false;
    }
    return header.headLength + header.bodyLength <= MaxPacketLength;
}

} // namespace

bool parseTgcpBaseHeader(const QByteArray &data, qsizetype offset, TgcpBaseHeader *outHeader)
{
    if (offset < 0 || offset + BaseHeaderLength > data.size()) {
        return false;
    }
    if (data.at(offset) != Magic0 || data.at(offset + 1) != Magic1) {
        return false;
    }

    TgcpBaseHeader header;
    header.headVersion = readBe16(data, offset + 2);
    header.bodyVersion = readBe16(data, offset + 4);
    header.command = readBe16(data, offset + 6);
    header.encrypted = data.at(offset + 8) != '\0';
    header.sequence = readBe32(data, offset + 9);
    header.headLength = readBe32(data, offset + 13);
    header.bodyLength = readBe32(data, offset + 17);

    if (!isValidHeader(header)) {
        return false;
    }
    if (outHeader != nullptr) {
        *outHeader = header;
    }
    return true;
}

QList<TgcpPacket> TgcpStreamParser::feed(const QString &flowId, TrafficDirection direction, const QByteArray &chunk)
{
    m_buffer.append(chunk);

    QList<TgcpPacket> packets;
    qsizetype offset = 0;
    while (true) {
        const qsizetype magicPos = m_buffer.indexOf(QByteArray::fromRawData("\x33\x66", 2), offset);
        if (magicPos < 0) {
            const qsizetype keepLength = qMin<qsizetype>(1, m_buffer.size() - offset);
            const qsizetype consumed = m_buffer.size() - keepLength;
            if (consumed > 0) {
                m_buffer.remove(0, consumed);
                m_streamBaseOffset += consumed;
            }
            return packets;
        }

        if (magicPos + BaseHeaderLength > m_buffer.size()) {
            if (magicPos > 0) {
                m_buffer.remove(0, magicPos);
                m_streamBaseOffset += magicPos;
            }
            return packets;
        }

        TgcpBaseHeader header;
        if (!parseTgcpBaseHeader(m_buffer, magicPos, &header)) {
            offset = magicPos + 2;
            continue;
        }

        const qsizetype packetLength = static_cast<qsizetype>(header.headLength + header.bodyLength);
        if (magicPos + packetLength > m_buffer.size()) {
            if (magicPos > 0) {
                m_buffer.remove(0, magicPos);
                m_streamBaseOffset += magicPos;
            }
            return packets;
        }

        TgcpPacket packet;
        packet.flowId = flowId;
        packet.direction = direction;
        packet.streamOffset = m_streamBaseOffset + magicPos;
        packet.base = header;
        packet.headExtend = m_buffer.mid(magicPos + BaseHeaderLength, header.headLength - BaseHeaderLength);
        packet.body = m_buffer.mid(magicPos + header.headLength, header.bodyLength);
        packets.append(packet);

        offset = magicPos + packetLength;
    }
}

void TgcpStreamParser::reset()
{
    m_buffer.clear();
    m_streamBaseOffset = 0;
}

QByteArray extractTgcpAckKey(const TgcpPacket &packet)
{
    if (packet.base.command != TgcpCommandAck || packet.headExtend.size() < 18) {
        return {};
    }
    return packet.headExtend.mid(2, 16);
}

} // namespace rwtd
