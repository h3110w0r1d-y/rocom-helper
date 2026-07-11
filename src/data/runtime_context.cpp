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
        0xA0, 0x33, 0xF8, 0xA4, 0xA7, 0x67, 0x7E, 0xD2, 0x2F, 0xFF, 0xE7, 0xF2,
        0x56, 0x86, 0xFC, 0x87, 0xD5, 0xB9, 0xBA, 0x6A, 0x16, 0x02, 0xA8, 0x34,
        0x4F, 0xCE, 0x6E, 0x5C, 0x88, 0x43, 0x99, 0xBE, 0xDE, 0x47, 0xE0, 0xF8,
        0xF8, 0x76, 0x36, 0xB3, 0x21, 0xAA, 0x43, 0x39, 0xDA, 0x77, 0x1C, 0x17,
        0x32, 0xDB, 0xEE, 0x44, 0xC7, 0x99, 0x60, 0xC8, 0xD4, 0x5B, 0xF6, 0x96,
        0x64, 0x95, 0x09, 0x6D, 0x33, 0xCD, 0x7A, 0x22, 0x7F, 0xAF, 0xB4, 0x3F,
        0xAB, 0x2B, 0x78, 0x4E, 0xC4, 0x33, 0x26, 0x53, 0x34, 0x19, 0x2B, 0xF1,
        0xDD, 0x82, 0x77, 0xD7, 0xCA, 0x4C, 0xB1, 0xDC, 0x6F, 0x8C, 0x18, 0x77,
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
