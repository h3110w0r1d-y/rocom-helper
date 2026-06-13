#include "runtime_context.h"

#include "map_types.h"

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

int clampHttpPort(int port)
{
    if (port < MinHttpPort || port > MaxHttpPort) {
        return DefaultHttpPort;
    }
    return port;
}

} // namespace

bool RuntimeContext::isValid() const
{
    return seed.size() >= 16
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

QString RuntimeContext::trafficSchemaRoot() const
{
    static constexpr unsigned char data[] = {
        0xA6, 0x0F, 0xB4, 0x84, 0x4F, 0x37,
    };
    return isValid() ? decodeRuntimeText(data, trafficKey) : QString();
}

QString RuntimeContext::startupDisclaimer() const
{
    static constexpr unsigned char data[] = {
        0x8B, 0x51, 0x38, 0x6D, 0x83, 0x0B, 0x77, 0xF2, 0x85, 0x87, 0x46, 0x49,
        0xA6, 0x8D, 0x44, 0xC0, 0xD4, 0x5F, 0x20, 0xB8, 0xA2, 0x94, 0x8C, 0x0D,
        0xA6, 0xCD, 0x6B, 0x0F, 0x35, 0x8A, 0x72, 0xC0, 0xF5, 0x25, 0x20, 0x31,
        0xDC, 0x1A, 0x3F, 0x93, 0x8B, 0xD2, 0xE2, 0x82, 0x2A, 0x7C, 0xA4, 0x50,
        0x33, 0x3D, 0x74, 0x96, 0x73, 0x0F, 0x44, 0xF1, 0x3D, 0x58, 0xF3, 0xC5,
        0xD9, 0x5C, 0xE2, 0x13, 0x18, 0xAF, 0xBA, 0xEB, 0x5B, 0xC3, 0xBD, 0x1F,
        0x01, 0x53, 0xD9, 0xF5, 0x34, 0x38, 0x9E, 0x14, 0x35, 0xFF, 0xB1, 0x23,
        0x69, 0x14, 0x53, 0xEE, 0x23, 0x4F, 0xB4, 0x8F, 0xD2, 0x45, 0xF3, 0x09,
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
    context.resourceKey = deriveKey(seed, "web-resource");
    context.trafficKey = deriveKey(seed, "traffic");
    return context;
}

QString localPetFilterUrl(int port)
{
    return QStringLiteral("http://127.0.0.1:%1/").arg(clampHttpPort(port));
}

} // namespace app
