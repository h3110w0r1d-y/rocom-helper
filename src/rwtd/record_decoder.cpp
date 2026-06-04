#include "record_decoder.h"

#include <QtEndian>

#include <openssl/evp.h>

#include <memory>

namespace rwtd {
namespace {

using EvpCipherCtxPtr = std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)>;

quint16 readBe16(const QByteArray &data, qsizetype offset)
{
    return qFromBigEndian<quint16>(reinterpret_cast<const uchar *>(data.constData() + offset));
}

quint32 readBe32(const QByteArray &data, qsizetype offset)
{
    return qFromBigEndian<quint32>(reinterpret_cast<const uchar *>(data.constData() + offset));
}

quint32 normalizeClientOpcode(quint32 opcode)
{
    const quint32 low16 = opcode & 0xffff;
    if (opcode > 0xffff && (opcode >> 16) == 0x0001 && low16 != 0) {
        return low16;
    }
    return opcode;
}

qsizetype tsf4gPaddingLength(const QByteArray &data)
{
    static const QByteArray marker = QByteArrayLiteral("tsf4g");
    if (data.size() < marker.size() + 1 || data.lastIndexOf(marker) != data.size() - 6) {
        return 0;
    }

    const auto pad = static_cast<uchar>(data.at(data.size() - 1));
    if (marker.size() + 1 <= pad && pad <= 64 && data.size() >= pad) {
        return pad;
    }
    if (pad == 1) {
        return 1;
    }
    if (0 < pad && pad <= 16 && data.size() >= pad) {
        for (qsizetype i = data.size() - pad; i < data.size(); ++i) {
            if (static_cast<uchar>(data.at(i)) != pad) {
                return 0;
            }
        }
        return pad;
    }
    return 0;
}

QByteArray stripTsf4gPadding(const QByteArray &data)
{
    const qsizetype paddingLength = tsf4gPaddingLength(data);
    return paddingLength > 0 ? data.left(data.size() - paddingLength) : data;
}

} // namespace

std::optional<DecryptedRecord> DataRecordDecoder::decryptAndParse(
    const QByteArray &key,
    const TgcpPacket &packet,
    const QSet<quint32> &knownOpcodes)
{
    const QByteArray plaintext = decryptDataBody(key, packet.body);
    if (plaintext.isEmpty()) {
        return std::nullopt;
    }
    return parseDecryptedRecord(packet.direction, plaintext, knownOpcodes);
}

QByteArray DataRecordDecoder::decryptDataBody(const QByteArray &key, const QByteArray &body)
{
    if (key.size() != 16 || body.size() < 32) {
        return {};
    }

    const QByteArray iv = body.left(16);
    const QByteArray ciphertext = body.mid(16);
    if (ciphertext.size() % 16 != 0) {
        return {};
    }

    EvpCipherCtxPtr ctx(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
    if (!ctx) {
        return {};
    }
    if (EVP_DecryptInit_ex(ctx.get(), EVP_aes_128_cbc(), nullptr,
                           reinterpret_cast<const uchar *>(key.constData()),
                           reinterpret_cast<const uchar *>(iv.constData())) != 1) {
        return {};
    }
    EVP_CIPHER_CTX_set_padding(ctx.get(), 0);

    QByteArray plaintext(ciphertext.size() + EVP_CIPHER_block_size(EVP_aes_128_cbc()), Qt::Uninitialized);
    int outLength = 0;
    int finalLength = 0;
    if (EVP_DecryptUpdate(ctx.get(),
                          reinterpret_cast<uchar *>(plaintext.data()),
                          &outLength,
                          reinterpret_cast<const uchar *>(ciphertext.constData()),
                          ciphertext.size()) != 1) {
        return {};
    }
    if (EVP_DecryptFinal_ex(ctx.get(),
                            reinterpret_cast<uchar *>(plaintext.data()) + outLength,
                            &finalLength) != 1) {
        return {};
    }
    plaintext.resize(outLength + finalLength);
    return plaintext;
}

std::optional<DecryptedRecord> DataRecordDecoder::parseDecryptedRecord(
    const TrafficDirection direction,
    const QByteArray &plaintext,
    const QSet<quint32> &knownOpcodes)
{
    if (plaintext.size() >= 0x1e
        && plaintext.mid(4, 2) == QByteArray::fromRawData("\x55\xaa", 2)
        && plaintext.mid(24, 2) == QByteArray::fromRawData("\x39\x63", 2)) {
        DecryptedRecord record;
        record.payload = stripTsf4gPadding(plaintext.mid(30));
        if (direction == TrafficDirection::ClientToServer) {
            record.opcode = normalizeClientOpcode(readBe32(plaintext, 20));
        } else {
            record.opcode = readBe32(plaintext, 16) & 0xffff;
        }
        return record;
    }

    if (direction == TrafficDirection::ServerToClient
        && plaintext.size() >= 10
        && plaintext.mid(4, 2) == QByteArray::fromRawData("\x55\xaa", 2)) {
        return DecryptedRecord{readBe32(plaintext, 0), stripTsf4gPadding(plaintext.mid(10))};
    }

    if (direction == TrafficDirection::ClientToServer
        && plaintext.size() >= 14
        && plaintext.mid(8, 2) == QByteArray::fromRawData("\x39\x63", 2)) {
        return DecryptedRecord{normalizeClientOpcode(readBe32(plaintext, 4)), stripTsf4gPadding(plaintext.mid(14))};
    }

    if (direction == TrafficDirection::ClientToServer
        && plaintext.size() >= 16
        && plaintext.left(6) == QByteArray::fromRawData("\x00\x00\x00\x02\x00\x02", 6)
        && plaintext.mid(8, 2) == QByteArray::fromRawData("\x45\xf0", 2)) {
        return DecryptedRecord{readBe16(plaintext, 6), stripTsf4gPadding(plaintext.mid(14))};
    }

    if (direction == TrafficDirection::ClientToServer && plaintext.size() >= 14) {
        const quint32 opcode = readBe16(plaintext, 6);
        if (knownOpcodes.contains(opcode)) {
            return DecryptedRecord{opcode, stripTsf4gPadding(plaintext.mid(14))};
        }
    }

    return std::nullopt;
}

} // namespace rwtd
