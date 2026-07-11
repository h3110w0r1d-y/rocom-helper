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
        0xA7, 0x6B, 0x35, 0x2D, 0x99, 0xF8, 0x7C, 0x0E, 0x03, 0x4F, 0x66, 0x63,
        0xEF, 0x97, 0x30, 0x80, 0xAE, 0x6C, 0x10, 0x14, 0xDC, 0x32, 0x95, 0xDC,
        0x08, 0xA9, 0x4D, 0xAB, 0x85, 0x52, 0xC5, 0x8D, 0xD9, 0x1F, 0x2D, 0x71,
        0xC6, 0xE9, 0x34, 0x6F, 0x0D, 0x1A, 0xC2, 0xA8, 0x63, 0x66, 0xD0, 0x10,
        0x49, 0x0E, 0x44, 0x3A, 0x0D, 0xA9, 0x5D, 0x20, 0x93, 0x3C, 0xD5, 0x61,
        0x69, 0x84, 0x55, 0x5E, 0x34, 0x95, 0xB7, 0xAB, 0x41, 0x30, 0xB6, 0xE3,
        0x87, 0x9B, 0xF9, 0xDF, 0x7D, 0x22, 0xEA, 0x54, 0x4F, 0xCC, 0x81, 0x8F,
        0x17, 0xB2, 0x4A, 0x3F, 0x8D, 0x2B, 0x92, 0x2B, 0x62, 0x9D, 0x44, 0x44,
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
