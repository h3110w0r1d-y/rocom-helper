#pragma once

#include "rwtd_types.h"

#include <QSet>

#include <optional>

namespace rwtd {

class DataRecordDecoder {
public:
    static std::optional<DecryptedRecord> decryptAndParse(
        const QByteArray &key,
        const TgcpPacket &packet,
        const QSet<quint32> &enabledOpcodes);

private:
    struct PlaintextHeader {
        quint32 opcode = 0;
        qsizetype payloadOffset = 0;
    };

    static QByteArray decryptDataBody(const QByteArray &key, const QByteArray &body);
    static std::optional<PlaintextHeader> parsePlaintextHeader(
        TrafficDirection direction,
        const QByteArray &plaintext);
    static std::optional<DecryptedRecord> parseDecryptedRecord(
        TrafficDirection direction,
        const QByteArray &plaintext,
        const QSet<quint32> &enabledOpcodes);
};

} // namespace rwtd
