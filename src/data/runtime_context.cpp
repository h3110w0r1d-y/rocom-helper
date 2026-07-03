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
        0xE3, 0x8F, 0x54, 0x7A, 0xCE, 0x77, 0xD2, 0x23, 0x65, 0x08, 0x8F, 0xFE,
        0xE5, 0x21, 0x4A, 0x40, 0xAA, 0xA7, 0x5F, 0x78, 0x70, 0x4A, 0xE3, 0x49,
        0xD6, 0x92, 0x1D, 0x96, 0x07, 0x6D, 0x9F, 0xC2, 0x9D, 0xFB, 0x4C, 0x26,
        0x91, 0x66, 0x9A, 0x42, 0x6B, 0x5D, 0x2B, 0x35, 0x69, 0xD0, 0xAA, 0xD0,
        0x4D, 0xC5, 0x0B, 0x56, 0xA1, 0xD1, 0x2B, 0xB5, 0x4D, 0x07, 0x85, 0x5C,
        0xEB, 0xBB, 0x0F, 0x11, 0x70, 0x71, 0xD6, 0xFC, 0x16, 0xBF, 0x18, 0xCE,
        0xE1, 0xDC, 0x10, 0x42, 0x77, 0x94, 0x90, 0x94, 0x4B, 0x07, 0xCE, 0xE3,
        0xBB, 0xCA, 0x3C, 0xAA, 0x53, 0x10, 0xC2, 0x16, 0xE0, 0xA2, 0x1E, 0x0B,
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
