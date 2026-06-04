#pragma once

#include "rwtd_types.h"

#include <QList>

namespace rwtd {

class TgcpStreamParser {
public:
    QList<TgcpPacket> feed(const QString &flowId, TrafficDirection direction, const QByteArray &chunk);
    void reset();

private:
    QByteArray m_buffer;
    qsizetype m_streamBaseOffset = 0;
};

bool parseTgcpBaseHeader(const QByteArray &data, qsizetype offset, TgcpBaseHeader *outHeader);
QByteArray extractTgcpAckKey(const TgcpPacket &packet);

} // namespace rwtd
