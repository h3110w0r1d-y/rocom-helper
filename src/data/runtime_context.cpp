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
        0x08, 0x16, 0x44, 0x3C, 0x8F, 0xE5, 0x57, 0xCF, 0x75, 0xFE, 0xE6, 0x40,
        0x42, 0x79, 0x29, 0xD4, 0x90, 0x9B, 0x33, 0x0B, 0x66, 0xA2, 0x6E, 0x9E,
        0xD9, 0xCC, 0x18, 0x04, 0xD3, 0xEC, 0xB2, 0xA2, 0x76, 0x62, 0x5C, 0x60,
        0xD0, 0xF4, 0x1F, 0xAE, 0x7B, 0xAB, 0x42, 0x8B, 0xCE, 0x88, 0xC9, 0x44,
        0x77, 0xF9, 0x67, 0x25, 0xB7, 0x39, 0xA6, 0x62, 0x42, 0x59, 0x80, 0xCE,
        0x3F, 0x3A, 0x22, 0x71, 0x9B, 0xE8, 0xC6, 0xBA, 0x57, 0x2D, 0x9D, 0x22,
        0xF1, 0x2A, 0x79, 0xFC, 0xD0, 0xCC, 0xF3, 0x00, 0x71, 0x3B, 0xA2, 0x90,
        0xAD, 0x22, 0xB1, 0x7D, 0x5C, 0x4E, 0xC7, 0x84, 0x34, 0x23, 0x33, 0x6B,
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
