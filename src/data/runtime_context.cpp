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
        0x31, 0xBB, 0xFA, 0x75, 0x70, 0x99, 0x3E, 0x19, 0xDA, 0x71, 0x6F, 0x3C,
        0x72, 0x41, 0x96, 0x64, 0x42, 0xA6, 0xB0, 0x04, 0xCB, 0xA8, 0x2B, 0x37,
        0x9D, 0xA9, 0x58, 0x53, 0x86, 0x3A, 0x9F, 0x0F, 0x4F, 0xCF, 0xE2, 0x29,
        0x2F, 0x88, 0x76, 0x78, 0xD4, 0x24, 0xCB, 0xF7, 0xFE, 0xB0, 0x76, 0xF4,
        0xA5, 0xC4, 0xE4, 0x2A, 0x1A, 0x33, 0xE3, 0xCB, 0x06, 0x3C, 0xC0, 0x99,
        0x6A, 0xEC, 0x0F, 0xDC, 0xA2, 0x45, 0x78, 0xF3, 0xA8, 0x51, 0xF4, 0xF4,
        0x5E, 0xA5, 0xF0, 0x80, 0xE0, 0xF4, 0x4C, 0xB0, 0xA3, 0x06, 0x21, 0x9F,
        0x00, 0x28, 0xF4, 0xD4, 0x18, 0x2B, 0x87, 0xD3, 0x61, 0xF5, 0x1E, 0xC6,
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
