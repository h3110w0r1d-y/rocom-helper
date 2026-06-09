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
QString decodeRuntimeText(const unsigned char (&encoded)[Size], const QByteArray &key)
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
    return QString::fromLatin1(decoded);
}

} // namespace

bool RuntimeContext::isValid() const
{
    return seed.size() >= 16
        && httpKey.size() == 32
        && resourceKey.size() == 32
        && trafficKey.size() == 32;
}

QString RuntimeContext::webResourceRoot() const
{
    static constexpr unsigned char data[] = {
        0x6B, 0xFC, 0x60, 0xF6, 0xDA,
    };
    return isValid() ? decodeRuntimeText(data, resourceKey) : QString();
}

QString RuntimeContext::webIndexPath() const
{
    static constexpr unsigned char data[] = {
        0x7E, 0xBA, 0x79, 0xF7, 0xDD, 0x40, 0x35, 0x86, 0xE2, 0xF3, 0x92,
    };
    return isValid() ? decodeRuntimeText(data, resourceKey) : QString();
}

QString RuntimeContext::petFilterUrl() const
{
    static constexpr unsigned char data[] = {
        0x8D, 0xF7, 0x2A, 0xFE, 0xAB, 0x06, 0x13, 0x28, 0x3B, 0xBE, 0x0D,
        0xBB, 0x4A, 0x59, 0x2C, 0x25, 0xD5, 0xBB, 0x4B, 0x65, 0xEF, 0xB0,
    };
    return isValid() ? decodeRuntimeText(data, httpKey) : QString();
}

QString RuntimeContext::trafficSchemaRoot() const
{
    static constexpr unsigned char data[] = {
        0xA6, 0x0F, 0xB4, 0x84, 0x4F, 0x37,
    };
    return isValid() ? decodeRuntimeText(data, trafficKey) : QString();
}

RuntimeContext makeRuntimeContext(const QByteArray &seed)
{
    RuntimeContext context;
    if (seed.size() < 16) {
        return context;
    }
    context.seed = seed;
    context.httpKey = deriveKey(seed, "http");
    context.resourceKey = deriveKey(seed, "web-resource");
    context.trafficKey = deriveKey(seed, "traffic");
    return context;
}

} // namespace app
