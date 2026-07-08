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
        0xEB, 0xFE, 0x32, 0xE1, 0xF6, 0x4E, 0x53, 0xFF, 0x5F, 0x93, 0x1C, 0xC2,
        0x05, 0x03, 0xF9, 0x14, 0x1C, 0x1D, 0x9B, 0x64, 0xDA, 0x28, 0x52, 0x5F,
        0xAA, 0xF3, 0x83, 0x84, 0x08, 0xD1, 0x41, 0xBE, 0x95, 0x8A, 0x2A, 0xBD,
        0xA9, 0x5F, 0x1B, 0x9E, 0x51, 0xC6, 0xB8, 0x09, 0x89, 0xF2, 0x19, 0x84,
        0xFB, 0x7F, 0xCF, 0x4A, 0x0B, 0xB3, 0x9A, 0xA3, 0x31, 0x66, 0x1B, 0x4E,
        0xE4, 0x07, 0xD1, 0x6D, 0x78, 0x00, 0xB0, 0x67, 0x2E, 0x86, 0x99, 0x12,
        0xDB, 0x47, 0x83, 0x7E, 0x97, 0xB6, 0x23, 0xC0, 0xFD, 0xBD, 0x0A, 0xFF,
        0x11, 0xA8, 0x8D, 0xBC, 0x2F, 0x71, 0x5C, 0x04, 0xEF, 0x1E, 0xC0, 0x77,
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
