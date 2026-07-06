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
        0x94, 0xEF, 0xC1, 0xBE, 0xA7, 0x6A, 0xC5, 0x24, 0x47, 0x16, 0xE8, 0x00,
        0x97, 0x3E, 0xC3, 0x8A, 0x72, 0x88, 0xB8, 0xF7, 0x46, 0x74, 0xC5, 0x25,
        0x67, 0x62, 0xAD, 0x22, 0xA5, 0xF7, 0x81, 0x3C, 0xEA, 0x9B, 0xD9, 0xE2,
        0xF8, 0x7B, 0x8D, 0x45, 0x49, 0x43, 0x4C, 0xCB, 0x1B, 0xCF, 0x23, 0x1A,
        0x95, 0xEA, 0xEC, 0xD9, 0x97, 0xEF, 0x0D, 0xD9, 0xFC, 0xF7, 0x35, 0xE8,
        0x49, 0x21, 0x11, 0xEF, 0x07, 0x11, 0x43, 0x38, 0x7F, 0xA2, 0x0F, 0xC9,
        0xC3, 0xC2, 0x77, 0xBC, 0x05, 0x8B, 0x19, 0x5E, 0x93, 0x28, 0x29, 0x6C,
        0x8D, 0xF4, 0x1A, 0xC6, 0xE2, 0xE0, 0x72, 0xA2, 0x42, 0x38, 0x00, 0xF5,
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
