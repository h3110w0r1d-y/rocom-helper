#include "runtime_context.h"

#include <QCryptographicHash>

#include <cstddef>

namespace app {
namespace {

QByteArray deriveKey(const QByteArray &seed, const char *purpose)
{
    QByteArray input = seed;
    input.append(purpose);
    return QCryptographicHash::hash(input, QCryptographicHash::Sha256);
}

template <size_t Size>
QString decodeRuntimeTextUtf8(const unsigned char (&encoded)[Size], const QByteArray &key)
{
    if (key.isEmpty()) {
        return {};
    }

    QByteArray decoded;
    decoded.reserve(static_cast<qsizetype>(Size));
    for (size_t i = 0; i < Size; ++i) {
        const auto keyByte = static_cast<unsigned char>(key.at(static_cast<qsizetype>(i % static_cast<size_t>(key.size()))));
        decoded.append(static_cast<char>(encoded[i] ^ keyByte));
    }
    return QString::fromUtf8(decoded);
}

} // namespace

bool RuntimeContext::isValid() const
{
    return seed.size() >= 16;
}

QString RuntimeContext::startupDisclaimer() const
{
    static constexpr unsigned char data[] = {
        0xA5, 0xF6, 0x78, 0xB9, 0x54, 0xFE, 0x20, 0xAF, 0xAA, 0xB8, 0xBC, 0x95,
        0xEE, 0xA7, 0xE5, 0x2D, 0xF4, 0x59, 0xAF, 0x27, 0x03, 0x15, 0xD3, 0x12,
        0xB7, 0x79, 0xF6, 0x77, 0x3A, 0x59, 0xFC, 0xAC, 0xDB, 0x82, 0x60, 0xE5,
        0x0B, 0xEF, 0x68, 0xCE, 0xA4, 0xED, 0x18, 0x5E, 0x62, 0x56, 0x05, 0xBD,
        0x13, 0x3B, 0xFB, 0x09, 0xD2, 0x8E, 0x1B, 0xEE, 0x2C, 0xEC, 0x6E, 0xBD,
        0xD6, 0x8F, 0x6C, 0x7F, 0x36, 0x08, 0xFA, 0x3F, 0x8C, 0x36, 0xEA, 0x42,
        0x2E, 0x6C, 0x23, 0x29, 0x7C, 0x12, 0x3F, 0xF9, 0x15, 0xF9, 0x3E, 0xBC,
        0xC8, 0x95, 0x0C, 0xF1, 0x32, 0xFB, 0x29, 0xF7, 0xDD, 0x96, 0x7D, 0x65,
    };
    if (!isValid()) {
        return {};
    }
    return decodeRuntimeTextUtf8(data, deriveKey(seed, "disclaimer"));
}

RuntimeContext makeRuntimeContext(const QByteArray &seed)
{
    RuntimeContext context;
    if (seed.size() < 16) {
        return context;
    }
    context.seed = seed;
    return context;
}

} // namespace app
