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
        const QSet<quint32> &knownOpcodes);

private:
    static QByteArray decryptDataBody(const QByteArray &key, const QByteArray &body);
    static std::optional<DecryptedRecord> parseDecryptedRecord(
        TrafficDirection direction,
        const QByteArray &plaintext,
        const QSet<quint32> &knownOpcodes);
};

} // namespace rwtd
